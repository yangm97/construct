// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "media.h"

struct ircd::m::media::magick
{
	module modules
	{
		"magick"
	};
};

ircd::mapi::header
IRCD_MODULE
{
	"11.7 :Content respository",
	ircd::m::media::init,
	ircd::m::media::fini
};

decltype(ircd::m::media::magick_support)
ircd::m::media::magick_support;

decltype(ircd::m::media::log)
ircd::m::media::log
{
	"m.media"
};

decltype(ircd::m::media::blocks_cache_enable)
ircd::m::media::blocks_cache_enable
{
	{ "name",     "ircd.media.blocks.cache.enable"  },
	{ "default",  true                              },
};

decltype(ircd::m::media::blocks_cache_comp_enable)
ircd::m::media::blocks_cache_comp_enable
{
	{ "name",     "ircd.media.blocks.cache_comp.enable"  },
	{ "default",  false                                  },
};

// Blocks column
decltype(ircd::m::media::blocks_descriptor)
ircd::m::media::blocks_descriptor
{
	// name
	"blocks",

	// explain
	R"(
	Key-value store of blocks belonging to files. The key is a hash of
	the block. The key is plaintext sha256-b58 and the block is binary
	up to 32768 bytes.
	)",

	// typing
	{
		typeid(string_view), typeid(string_view)
	},

	{},      // options
	{},      // comparaor
	{},      // prefix transform
	false,   // drop column

	bool(blocks_cache_enable)? -1 : 0,

	bool(blocks_cache_comp_enable)? -1 : 0,

	// bloom_bits
	10,

	// expect hit
	true,

	// block_size
	32_KiB,

	// meta block size
	512,

	// compression
	""s // no compression
};

decltype(ircd::m::media::description)
ircd::m::media::description
{
	{ "default" }, // requirement of RocksDB

	blocks_descriptor,
};

decltype(ircd::m::media::blocks_cache_size)
ircd::m::media::blocks_cache_size
{
	{
		{ "name",     "ircd.media.blocks.cache.size" },
		{ "default",  long(48_MiB)                   },
	}, []
	{
		if(!blocks)
			return;

		const size_t &value{blocks_cache_size};
		db::capacity(db::cache(blocks), value);
	}
};

decltype(ircd::m::media::blocks_cache_comp_size)
ircd::m::media::blocks_cache_comp_size
{
	{
		{ "name",     "ircd.media.blocks.cache_comp.size" },
		{ "default",  long(16_MiB)                        },
	}, []
	{
		if(!blocks)
			return;

		const size_t &value{blocks_cache_comp_size};
		db::capacity(db::cache_compressed(blocks), value);
	}
};

decltype(ircd::m::media::events_prefetch)
ircd::m::media::events_prefetch
{
	{ "name",     "ircd.media.file.prefetch.events" },
	{ "default",  16L                               },
};

decltype(ircd::m::media::database)
ircd::m::media::database;

decltype(ircd::m::media::blocks)
ircd::m::media::blocks;

decltype(ircd::m::media::downloading)
ircd::m::media::downloading;

decltype(ircd::m::media::downloading_dock)
ircd::m::media::downloading_dock;

//
// init
//

void
ircd::m::media::init()
{
	static const std::string dbopts;
	database = std::make_shared<db::database>("media", dbopts, description);
	blocks = db::column{*database, "blocks"};

	// The conf setter callbacks must be manually executed after
	// the database was just loaded to set the cache size.
	conf::reset("ircd.media.blocks.cache.size");
	conf::reset("ircd.media.blocks.cache_comp.size");

	// conditions to load the magick.so module
	const bool enable_magick
	{
		// used by the thumbnailer
		thumbnail::enable

		// support is available
		&& mods::available("magick")
	};

	if(enable_magick)
		magick_support.reset(new media::magick{});
	else
		log::warning
		{
			log, "GraphicsMagick support is disabled or unavailable."
		};
}

void
ircd::m::media::fini()
{
	magick_support.reset();

	// The database close contains pthread_join()'s within RocksDB which
	// deadlock under certain conditions when called during a dlclose()
	// (i.e static destruction of this module). Therefor we must manually
	// close the db here first.
	database = std::shared_ptr<db::database>{};
}

//
// media::file
//

ircd::m::room::id::buf
IRCD_MODULE_EXPORT
ircd::m::media::file::download(const mxc &mxc,
                               const m::user::id &user_id,
                               const net::hostport &remote)
{
	const m::room::id::buf room_id
	{
		file::room_id(mxc)
	};

	thread_local char rembuf[256];
	if(remote && my_host(string(rembuf, remote)))
		return room_id;

	if(!remote && my_host(mxc.server))
		return room_id;

	download(mxc, user_id, remote, room_id);
	return room_id;
}

ircd::m::room
IRCD_MODULE_EXPORT
ircd::m::media::file::download(const mxc &mxc,
                               const m::user::id &user_id,
                               const net::hostport &remote,
                               const m::room::id &room_id)
try
{
	auto iit
	{
		downloading.emplace(room_id)
	};

	if(!iit.second)
	{
		downloading_dock.wait([&room_id]
		{
			return !downloading.count(room_id);
		});

		return room_id;
	}

	const unwind uw{[&iit]
	{
		downloading.erase(iit.first);
		downloading_dock.notify_all();
	}};

	if(exists(room_id))
		return room_id;

	const unique_buffer<mutable_buffer> buf
	{
		16_KiB
	};

	const auto pair
	{
		download(buf, mxc, remote)
	};

	const auto &head
	{
		pair.first
	};

	const const_buffer &content
	{
		pair.second
	};

	char mime_type_buf[64];
	const auto &content_type
	{
		magic::mime(mime_type_buf, content)
	};

	if(content_type != head.content_type) log::dwarning
	{
		log, "Server %s claims thumbnail %s is '%s' but we think it is '%s'",
		string(remote),
		mxc.mediaid,
		head.content_type,
		content_type,
	};

	m::vm::copts vmopts;
	vmopts.history = false;
	const m::room room
	{
		room_id, &vmopts
	};

	create(room, user_id, "file");
	const unwind::exceptional purge{[&room]
	{
		m::room::purge(room);
	}};

	const size_t written
	{
		file::write(room, user_id, content, content_type)
	};

	return room;
}
catch(const ircd::server::unavailable &e)
{
	thread_local char rembuf[256];
	throw m::error
	{
		http::BAD_GATEWAY, "M_MEDIA_UNAVAILABLE",
		"Server '%s' is not available for media for '%s/%s' :%s",
		string(rembuf, remote),
		mxc.server,
		mxc.mediaid,
		e.what()
	};
}

decltype(ircd::m::media::download_timeout)
ircd::m::media::download_timeout
{
	{ "name",     "ircd.media.download.timeout" },
	{ "default",  15L                           },
};

std::pair
<
	ircd::http::response::head,
	ircd::unique_buffer<ircd::mutable_buffer>
>
IRCD_MODULE_EXPORT
ircd::m::media::file::download(const mutable_buffer &head_buf,
                               const mxc &mxc,
                               net::hostport remote,
                               server::request::opts *const opts)
{
	thread_local char rembuf[256];
	assert(remote || !my_host(mxc.server));
	assert(!remote || !my_host(string(rembuf, remote)));

	if(!remote)
		remote = mxc.server;

	window_buffer wb{head_buf};
	thread_local char uri[4_KiB];
	http::request
	{
		wb, host(remote), "GET", fmt::sprintf
		{
			uri, "/_matrix/media/r0/download/%s/%s",
			mxc.server,
			mxc.mediaid,
		}
	};

	const const_buffer out_head
	{
		wb.completed()
	};

	// Remaining space in buffer is used for received head
	const mutable_buffer in_head
	{
		data(head_buf) + size(out_head), size(head_buf) - size(out_head)
	};

	//TODO: --- This should use the progress callback to build blocks
	server::request remote_request
	{
		remote, { out_head }, { in_head, {} }, opts
	};

	if(!remote_request.wait(seconds(download_timeout), std::nothrow))
		throw m::error
		{
			http::GATEWAY_TIMEOUT, "M_MEDIA_DOWNLOAD_TIMEOUT",
			"Server '%s' did not respond with media for '%s/%s' in time",
			string(rembuf, remote),
			mxc.server,
			mxc.mediaid
		};

	const auto &code
	{
		remote_request.get()
	};

	if(code != http::OK)
		return {};

	parse::buffer pb{remote_request.in.head};
	parse::capstan pc{pb};
	pc.read += size(remote_request.in.head);
	return std::pair<http::response::head, unique_buffer<mutable_buffer>>
	{
		pc, std::move(remote_request.in.dynamic)
	};
}

size_t
IRCD_MODULE_EXPORT
ircd::m::media::file::write(const m::room &room,
                            const m::user::id &user_id,
                            const const_buffer &content,
                            const string_view &content_type)
{
	//TODO: TXN
	send(room, user_id, "ircd.file.stat", "size", json::members
	{
		{ "value", long(size(content)) }
	});

	//TODO: TXN
	send(room, user_id, "ircd.file.stat", "type", json::members
	{
		{ "value", content_type }
	});

	size_t off{0}, wrote{0};
	while(off < size(content))
	{
		const size_t blksz
		{
			std::min(size(content) - off, size_t(32_KiB))
		};

		const const_buffer &block
		{
			data(content) + off, blksz
		};

		block::set(room, user_id, block);
		wrote += size(block);
		off += blksz;
	}

	assert(off == size(content));
	assert(wrote == off);
	return wrote;
}

size_t
IRCD_MODULE_EXPORT
ircd::m::media::file::read(const m::room &room,
                           const closure &closure)
{
	static const event::fetch::opts fopts
	{
		event::keys::include { "content", "type" }
	};

	size_t ret{0};
	room::events it
	{
		room, 1, &fopts
	};

	if(!it)
		return ret;

	room::events it_pf
	{
		room, 1, &fopts
	};

	// Block buffer
	const unique_buffer<mutable_buffer> buf
	{
		64_KiB
	};

	size_t prefetched(0), fetched(0);
	for(; bool(it); ++it, ++fetched)
	{
		for(; it_pf && prefetched < fetched + events_prefetch; ++it_pf)
			prefetched += m::prefetch(it_pf.event_idx(), fopts);

		++fetched;
		const m::event &event
		{
			*it
		};

		if(at<"type"_>(event) != "ircd.file.block")
			continue;

		const json::string &hash
		{
			at<"content"_>(event).at("hash")
		};

		const auto &blksz
		{
			at<"content"_>(event).get<size_t>("size")
		};

		const const_buffer &block
		{
			block::get(buf, hash)
		};

		if(unlikely(size(block) != blksz)) throw error
		{
			"File [%s] block [%s] (%s) blksz %zu != %zu",
			string_view{room.room_id},
			string_view{m::get(std::nothrow, it.event_idx(), "event_id", buf)},
			hash,
			blksz,
			size(block)
		};

		assert(size(block) == blksz);
		ret += size(block);
		closure(block);
	}

	return ret;
}

//
// media::file
//

ircd::m::room::id::buf
IRCD_MODULE_EXPORT
ircd::m::media::file::room_id(const mxc &mxc)
{
	m::room::id::buf ret;
	room_id(ret, mxc);
	return ret;
}

ircd::m::room::id
IRCD_MODULE_EXPORT
ircd::m::media::file::room_id(room::id::buf &out,
                              const mxc &mxc)
{
	thread_local char buf[512];
	const auto path
	{
		mxc.path(buf)
	};

	const sha256::buf hash
	{
		sha256{path}
	};

	out =
	{
		b58encode(buf, hash), my_host()
	};

	return out;
}

//
// media::block
//

ircd::m::event::id::buf
IRCD_MODULE_EXPORT
ircd::m::media::block::set(const m::room &room,
                           const m::user::id &user_id,
                           const const_buffer &block)
{
	static constexpr const auto bufsz
	{
		b58encode_size(sha256::digest_size)
	};

	char b58buf[bufsz];
	const auto hash
	{
		set(mutable_buffer{b58buf}, block)
	};

	return send(room, user_id, "ircd.file.block", json::members
	{
		{ "size",  long(size(block))  },
		{ "hash",  hash               }
	});
}

ircd::string_view
IRCD_MODULE_EXPORT
ircd::m::media::block::set(const mutable_buffer &b58buf,
                           const const_buffer &block)
{
	const sha256::buf hash
	{
		sha256{block}
	};

	const string_view b58hash
	{
		b58encode(b58buf, hash)
	};

	set(b58hash, block);
	return b58hash;
}

void
IRCD_MODULE_EXPORT
ircd::m::media::block::set(const string_view &b58hash,
                           const const_buffer &block)
{
	db::write(blocks, b58hash, block);
}

ircd::const_buffer
IRCD_MODULE_EXPORT
ircd::m::media::block::get(const mutable_buffer &out,
                           const string_view &b58hash)
{
	return db::read(blocks, b58hash, out);
}

bool
IRCD_MODULE_EXPORT
ircd::m::media::block::prefetch(const string_view &b58hash)
{
	return db::prefetch(blocks, b58hash);
}

//
// media::mxc
//

IRCD_MODULE_EXPORT
ircd::m::media::mxc::mxc(const string_view &server,
                         const string_view &mediaid)
:server
{
	split(lstrip(server, "mxc://"), '/').first
}
,mediaid
{
	mediaid?: rsplit(server, '/').second
}
{
	if(empty(server))
		throw m::BAD_REQUEST
		{
			"Invalid MXC: missing server parameter."
		};

	if(empty(mediaid))
		throw m::BAD_REQUEST
		{
			"Invalid MXC: missing mediaid parameter."
		};
}

IRCD_MODULE_EXPORT
ircd::m::media::mxc::mxc(const string_view &uri)
:server
{
	split(lstrip(uri, "mxc://"), '/').first
}
,mediaid
{
	rsplit(uri, '/').second
}
{
	if(empty(server))
		throw m::BAD_REQUEST
		{
			"Invalid MXC: missing server parameter."
		};

	if(empty(mediaid))
		throw m::BAD_REQUEST
		{
			"Invalid MXC: missing mediaid parameter."
		};
}

ircd::string_view
IRCD_MODULE_EXPORT
ircd::m::media::mxc::uri(const mutable_buffer &out)
const
{
	return fmt::sprintf
	{
		out, "mxc://%s/%s", server, mediaid
	};
}

ircd::string_view
IRCD_MODULE_EXPORT
ircd::m::media::mxc::path(const mutable_buffer &out)
const
{
	return fmt::sprintf
	{
		out, "%s/%s", server, mediaid
	};
}
