// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
//
// fed/groups.h
//

ircd::m::fed::groups::publicised::publicised(const string_view &node,
                                             const vector_view<const id::user> &user_ids,
                                             const mutable_buffer &buf_,
                                             opts opts)
:request{[&]
{
	if(likely(!opts.remote))
		opts.remote = node;

	if(likely(!defined(json::get<"uri"_>(opts.request))))
		json::get<"uri"_>(opts.request) = "/_matrix/federation/v1/get_groups_publicised";

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "POST";

	mutable_buffer buf{buf_};
	const string_view user_ids_
	{
		json::stringify(buf, user_ids.data(), user_ids.data() + user_ids.size())
	};

	assert(!defined(json::get<"content"_>(opts.request)));
	json::get<"content"_>(opts.request) = stringify(buf, json::members
	{
		{ "user_ids", user_ids_ }
	});

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/send.h
//

void
ircd::m::fed::send::response::for_each_pdu(const pdus_closure &closure)
const
{
	const json::object &pdus
	{
		this->get("pdus")
	};

	for(const auto &[event_id, error] : pdus)
		closure(event_id, error);
}

ircd::m::fed::send::send(const txn::array &pdu,
                         const txn::array &edu,
                         const mutable_buffer &buf_,
                         opts opts)
:send{[&]
{
	assert(!!opts.remote);

	mutable_buffer buf{buf_};
	const string_view &content
	{
		txn::create(buf, pdu, edu)
	};

	consume(buf, size(content));
	const string_view &txnid
	{
		txn::create_id(buf, content)
	};

	consume(buf, size(txnid));
	return send
	{
		txnid, content, buf, std::move(opts)
	};
}()}
{
}

ircd::m::fed::send::send(const string_view &txnid,
                         const const_buffer &content,
                         const mutable_buffer &buf_,
                         opts opts)
:request{[&]
{
	assert(!!opts.remote);

	assert(!size(opts.out.content));
	assert(!defined(json::get<"content"_>(opts.request)));
	json::get<"content"_>(opts.request) = json::object
	{
		content
	};

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "PUT";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char txnidbuf[256];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/send/%s/",
			url::encode(txnidbuf, txnid),
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/rooms.h
//

ircd::m::fed::rooms::complexity::complexity(const m::id::room &room_id,
                                            const mutable_buffer &buf_,
                                            opts opts)
:request{[&]
{
	if(!opts.remote)
		opts.remote = room_id.host();

	window_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/unstable/rooms/%s/complexity",
			url::encode(ridbuf, room_id)
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/public_rooms.h
//

ircd::m::fed::public_rooms::public_rooms(const string_view &remote,
                                         const mutable_buffer &buf_,
                                         opts opts)
:request{[&]
{
	if(!opts.remote)
		opts.remote = remote;

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char query[2048], tpid[1024], since[1024];
		std::stringstream qss;
		pubsetbuf(qss, query);

		if(opts.since)
			qss << "&since="
			    << url::encode(since, opts.since);

		if(opts.third_party_instance_id)
			qss << "&third_party_instance_id="
			    << url::encode(tpid, opts.third_party_instance_id);

		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/publicRooms?limit=%zu%s%s",
			opts.limit,
			opts.include_all_networks? "&include_all_networks=true" : "",
			view(qss, query)
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/frontfill.h
//

ircd::m::fed::frontfill::frontfill(const room::id &room_id,
                                   const span &span,
                                   const mutable_buffer &buf,
                                   opts opts)
:frontfill
{
	room_id,
	ranges { vector(&span.first, 1), vector(&span.second, 1) },
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::frontfill::frontfill(const room::id &room_id,
                                   const ranges &pair,
                                   const mutable_buffer &buf_,
                                   opts opts)
:request{[&]
{
	assert(!!opts.remote);

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "POST";

	window_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/get_missing_events/%s/",
			url::encode(ridbuf, room_id)
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	if(likely(!defined(json::get<"content"_>(opts.request))))
	{
		buf([&pair, &opts](const mutable_buffer &buf)
		{
			return make_content(buf, pair, opts);
		});

		json::get<"content"_>(opts.request) = json::object
		{
			buf.completed()
		};
	}

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

ircd::const_buffer
ircd::m::fed::frontfill::make_content(const mutable_buffer &buf,
	                                  const ranges &pair,
	                                  const opts &opts)
{
	json::stack out{buf};
	{
		// note: This object must be in abc order
		json::stack::object top{out};
		{
			json::stack::member earliest{top, "earliest_events"};
			json::stack::array array{earliest};
			for(const auto &id : pair.first)
				array.append(id);
		}
		{
			json::stack::member latest{top, "latest_events"};
			json::stack::array array{latest};
			for(const auto &id : pair.second)
				array.append(id);
		}
		json::stack::member{top, "limit", json::value(int64_t(opts.limit))};
		json::stack::member{top, "min_depth", json::value(int64_t(opts.min_depth))};
	}

	return out.completed();
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/backfill.h
//

ircd::m::fed::backfill::backfill(const room::id &room_id,
                                 const mutable_buffer &buf_,
                                 opts opts)
:request{[&]
{
	m::event::id::buf event_id_buf;
	if(!opts.event_id)
	{
		event_id_buf = fetch_head(room_id, opts.remote);
		opts.event_id = event_id_buf;
	}

	if(!opts.remote)
		opts.remote = room_id.host();

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768], eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/backfill/%s/?limit=%zu&v=%s",
			url::encode(ridbuf, room_id),
			opts.limit,
			url::encode(eidbuf, opts.event_id),
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/state.h
//

ircd::m::fed::state::state(const room::id &room_id,
                           const mutable_buffer &buf_,
                           opts opts)
:request{[&]
{
	m::event::id::buf event_id_buf;
	if(!opts.event_id)
	{
		event_id_buf = fetch_head(room_id, opts.remote);
		opts.event_id = event_id_buf;
	}

	if(!opts.remote)
		opts.remote = room_id.host();

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768], eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/%s/%s/?event_id=%s%s",
			opts.ids_only? "state_ids" : "state",
			url::encode(ridbuf, room_id),
			url::encode(eidbuf, opts.event_id),
			opts.ids_only? "&auth_chain_ids=0"_sv : ""_sv,
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/query_auth.h
//

ircd::m::fed::query_auth::query_auth(const m::room::id &room_id,
                                     const m::event::id &event_id,
                                     const json::object &content,
                                     const mutable_buffer &buf_,
                                     opts opts)
:request{[&]
{
	if(!opts.remote && event_id.version() == "1")
		opts.remote = event_id.host();

	if(likely(!defined(json::get<"content"_>(opts.request))))
		json::get<"content"_>(opts.request) = content;

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "POST";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768], eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/query_auth/%s/%s",
			url::encode(ridbuf, room_id),
			url::encode(eidbuf, event_id),
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	assert(!!opts.remote);
	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/event_auth.h
//

ircd::m::fed::event_auth::event_auth(const m::room::id &room_id,
                                     const m::event::id &event_id,
                                     const mutable_buffer &buf_,
                                     opts opts)
:request{[&]
{
	if(!opts.remote && event_id.version() == "1")
		opts.remote = event_id.host();

	if(!opts.remote)
		opts.remote = room_id.host();

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768], eidbuf[768];
		if(opts.ids_only)
			json::get<"uri"_>(opts.request) = fmt::sprintf
			{
				buf, "/_matrix/federation/v1/state_ids/%s/?event_id=%s&pdu_ids=0",
				url::encode(ridbuf, room_id),
				url::encode(eidbuf, event_id),
			};
		else
			json::get<"uri"_>(opts.request) = fmt::sprintf
			{
				buf, "/_matrix/federation/v1/event_auth/%s/%s",
				url::encode(ridbuf, room_id),
				url::encode(eidbuf, event_id),
			};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	assert(!!opts.remote);
	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/event.h
//

ircd::m::fed::event::event(const m::event::id &event_id,
                           const mutable_buffer &buf_,
                           opts opts)
:request{[&]
{
	if(!opts.remote && event_id.version() == "1")
		opts.remote = event_id.host();

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/event/%s/",
			url::encode(eidbuf, event_id),
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	assert(!!opts.remote);
	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/invite.h
//

ircd::m::fed::invite::invite(const room::id &room_id,
                             const id::event &event_id,
                             const json::object &content,
                             const mutable_buffer &buf_,
                             opts opts)
:request{[&]
{
	assert(!size(opts.out.content));
	assert(!defined(json::get<"content"_>(opts.request)));
	json::get<"content"_>(opts.request) = content;

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "PUT";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768], eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/invite/%s/%s",
			url::encode(ridbuf, room_id),
			url::encode(eidbuf, event_id)
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	assert(!!opts.remote);
	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/invite2.h
//

ircd::m::fed::invite2::invite2(const room::id &room_id,
                               const id::event &event_id,
                               const json::object &content,
                               const mutable_buffer &buf_,
                               opts opts)
:request{[&]
{
	assert(!size(opts.out.content));
	assert(!defined(json::get<"content"_>(opts.request)));
	json::get<"content"_>(opts.request) = content;

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "PUT";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768], eidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v2/invite/%s/%s",
			url::encode(ridbuf, room_id),
			url::encode(eidbuf, event_id)
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	assert(!!opts.remote);
	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/send_join.h
//

ircd::m::fed::send_join::send_join(const room::id &room_id,
                                   const id::event &event_id,
                                   const const_buffer &content,
                                   const mutable_buffer &buf_,
                                   opts opts)
:request{[&]
{
	assert(!!opts.remote);

	assert(!size(opts.out.content));
	assert(!defined(json::get<"content"_>(opts.request)));
	json::get<"content"_>(opts.request) = json::object
	{
		content
	};

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "PUT";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char ridbuf[768], uidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/send_join/%s/%s",
			url::encode(ridbuf, room_id),
			url::encode(uidbuf, event_id)
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/make_join.h
//

ircd::m::fed::make_join::make_join(const room::id &room_id,
                                   const id::user &user_id_,
                                   const mutable_buffer &buf_,
                                   opts opts)
:request{[&]
{
	if(!opts.remote)
		opts.remote = room_id.host();

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		id::user::buf user_id_buf;
		const id::user &user_id
		{
			user_id_?: id::user
			{
				user_id_buf, id::generate, my_host()
			}
		};

		thread_local char ridbuf[768], uidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/make_join/%s/%s"
			"?ver=1"
			"&ver=2"
			"&ver=3"
			"&ver=4"
			"&ver=5"
			"&ver=6"
			"&ver=7"
			"&ver=8"
			"&ver=org.matrix.msc2432"
			,url::encode(ridbuf, room_id)
			,url::encode(uidbuf, user_id)
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/user_keys.h
//

//
// query
//

ircd::m::fed::user::keys::query::query(const m::user::id &user_id,
                                       const mutable_buffer &buf,
                                       opts opts)
:query
{
	user_id,
	string_view{},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::user::keys::query::query(const m::user::id &user_id,
                                       const string_view &device_id,
                                       const mutable_buffer &buf,
                                       opts opts)
:query
{
	user_devices
	{
		user_id, vector_view<const string_view>
		{
			&device_id, !empty(device_id)
		}
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::user::keys::query::query(const user_devices &v,
                                       const mutable_buffer &buf,
                                       opts opts)
:query
{
	vector_view<const user_devices>
	{
		&v, 1
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::user::keys::query::query(const users_devices &v,
                                       const mutable_buffer &buf,
                                       opts opts)
:query{[&v, &buf, &opts]
{
	const json::object &content
	{
		make_content(buf, v)
	};

	return query
	{
		content, buf + size(string_view(content)), std::move(opts)
	};
}()}
{
}

ircd::m::fed::user::keys::query::query(const users_devices_map &m,
                                       const mutable_buffer &buf,
                                       opts opts)
:query{[&m, &buf, &opts]
{
	const json::object &content
	{
		make_content(buf, m)
	};

	return query
	{
		content, buf + size(string_view(content)), std::move(opts)
	};
}()}
{
}

ircd::m::fed::user::keys::query::query(const json::object &content,
                                       const mutable_buffer &buf,
                                       opts opts)
:request{[&]
{
	assert(!!opts.remote);

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "POST";

	if(likely(!defined(json::get<"uri"_>(opts.request))))
		json::get<"uri"_>(opts.request) = "/_matrix/federation/v1/user/keys/query";

	if(likely(!defined(json::get<"content"_>(opts.request))))
		json::get<"content"_>(opts.request) = content;

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

ircd::json::object
ircd::m::fed::user::keys::query::make_content(const mutable_buffer &buf,
                                              const users_devices &v)
{
	json::stack out{buf};
	{
		json::stack::object top{out};
		json::stack::object device_keys
		{
			top, "device_keys"
		};

		for(const auto &[user_id, devices] : v)
		{
			json::stack::array array
			{
				device_keys, user_id
			};

			for(const auto &device_id : devices)
				array.append(device_id);
		}
	}

	return out.completed();
}

ircd::json::object
ircd::m::fed::user::keys::query::make_content(const mutable_buffer &buf,
                                              const users_devices_map &m)
{
	json::stack out{buf};
	{
		json::stack::object top{out};
		json::stack::object device_keys
		{
			top, "device_keys"
		};

		for(const auto &[user_id, devices] : m)
			json::stack::member
			{
				device_keys, user_id, devices
			};
	}

	return out.completed();
}

//
// claim
//

ircd::m::fed::user::keys::claim::claim(const m::user::id &user_id,
                                       const string_view &device_id,
                                       const string_view &algorithm,
                                       const mutable_buffer &buf,
                                       opts opts)
:claim
{
	user_id,
	device
	{
		device_id, algorithm
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::user::keys::claim::claim(const m::user::id &user_id,
                                       const device &device,
                                       const mutable_buffer &buf,
                                       opts opts)
:claim
{
	user_devices
	{
		user_id, { &device, 1 }
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::user::keys::claim::claim(const user_devices &ud,
                                       const mutable_buffer &buf,
                                       opts opts)
:claim
{
	vector_view<const user_devices>
	{
		&ud, 1
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::user::keys::claim::claim(const users_devices &v,
                                       const mutable_buffer &buf,
                                       opts opts)
:claim{[&v, &buf, &opts]
{
	const json::object &content
	{
		make_content(buf, v)
	};

	return claim
	{
		content, buf + size(string_view(content)), std::move(opts)
	};
}()}
{
}

ircd::m::fed::user::keys::claim::claim(const users_devices_map &m,
                                       const mutable_buffer &buf,
                                       opts opts)
:claim{[&m, &buf, &opts]
{
	const json::object &content
	{
		make_content(buf, m)
	};

	return claim
	{
		content, buf + size(string_view(content)), std::move(opts)
	};
}()}
{
}

ircd::m::fed::user::keys::claim::claim(const json::object &content,
                                       const mutable_buffer &buf,
                                       opts opts)
:request{[&]
{
	assert(!!opts.remote);

	assert(!defined(json::get<"content"_>(opts.request)));
	json::get<"content"_>(opts.request) = content;

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "POST";

	if(likely(!defined(json::get<"uri"_>(opts.request))))
		json::get<"uri"_>(opts.request) = "/_matrix/federation/v1/user/keys/claim";

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

ircd::json::object
ircd::m::fed::user::keys::claim::make_content(const mutable_buffer &buf,
                                              const users_devices &v)
{
	json::stack out{buf};
	{
		json::stack::object top{out};
		json::stack::object one_time_keys
		{
			top, "one_time_keys"
		};

		for(const auto &[user_id, devices] : v)
		{
			json::stack::object user
			{
				one_time_keys, user_id
			};

			for(const auto &[device_id, algorithm_name] : devices)
				json::stack::member
				{
					user, device_id, algorithm_name
				};
		}
	}

	return out.completed();
}

ircd::json::object
ircd::m::fed::user::keys::claim::make_content(const mutable_buffer &buf,
                                              const users_devices_map &v)
{
	json::stack out{buf};
	{
		json::stack::object top{out};
		json::stack::object one_time_keys
		{
			top, "one_time_keys"
		};

		for(const auto &[user_id, devices] : v)
		{
			json::stack::object user
			{
				one_time_keys, user_id
			};

			for(const auto &[device_id, algorithm_name] : devices)
				json::stack::member
				{
					user, device_id, algorithm_name
				};
		}
	}

	return out.completed();
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/user.h
//

ircd::m::fed::user::devices::devices(const id::user &user_id,
                                     const mutable_buffer &buf_,
                                     opts opts)
:request{[&]
{
	if(!opts.remote)
		opts.remote = user_id.host();

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		thread_local char uidbuf[768];
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/user/devices/%s",
			url::encode(uidbuf, user_id)
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/query.h
//

namespace ircd::m::fed
{
	thread_local char query_arg_buf[1024];
	thread_local char query_url_buf[1024];
}

ircd::m::fed::query::directory::directory(const id::room_alias &room_alias,
                                          const mutable_buffer &buf,
                                          opts opts)
:query
{
	"directory",
	fmt::sprintf
	{
		query_arg_buf, "room_alias=%s",
		url::encode(query_url_buf, room_alias)
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::query::profile::profile(const id::user &user_id,
                                      const mutable_buffer &buf,
                                      opts opts)
:query
{
	"profile",
	fmt::sprintf
	{
		query_arg_buf, "user_id=%s",
		url::encode(query_url_buf, user_id)
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::query::profile::profile(const id::user &user_id,
                                      const string_view &field,
                                      const mutable_buffer &buf,
                                      opts opts)
:query
{
	"profile",
	fmt::sprintf
	{
		query_arg_buf, "user_id=%s%s%s",
		url::encode(query_url_buf, string_view{user_id}),
		!empty(field)? "&field="_sv: string_view{},
		field
	},
	buf,
	std::move(opts)
}
{
}

ircd::m::fed::query::query(const string_view &type,
                           const string_view &args,
                           const mutable_buffer &buf_,
                           opts opts)
:request{[&]
{
	assert(!!opts.remote);

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		json::get<"uri"_>(opts.request) = fmt::sprintf
		{
			buf, "/_matrix/federation/v1/query/%s%s%s",
			type,
			args? "?"_sv: string_view{},
			args
		};

		consume(buf, size(json::get<"uri"_>(opts.request)));
	}

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/key.h
//

ircd::m::fed::key::keys::keys(const string_view &server_name,
                              const mutable_buffer &buf,
                              opts opts)
:keys
{
	server_key{server_name, ""}, buf, std::move(opts)
}
{
}

ircd::m::fed::key::keys::keys(const server_key &server_key,
                              const mutable_buffer &buf_,
                              opts opts)
:request{[&]
{
	const auto &[server_name, key_id]
	{
		server_key
	};

	if(likely(!opts.remote))
		opts.remote = server_name;

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	mutable_buffer buf{buf_};
	if(likely(!defined(json::get<"uri"_>(opts.request))))
	{
		if(!empty(key_id))
		{
			json::get<"uri"_>(opts.request) = fmt::sprintf
			{
				buf, "/_matrix/key/v2/server/%s/",
				key_id
			};

			consume(buf, size(json::get<"uri"_>(opts.request)));
		}
		else json::get<"uri"_>(opts.request) = "/_matrix/key/v2/server/";
	}

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

namespace ircd::m::fed
{
	static const_buffer
	_make_server_keys(const vector_view<const key::server_key> &,
	                  const mutable_buffer &);
}

ircd::m::fed::key::query::query(const vector_view<const server_key> &keys,
                                const mutable_buffer &buf_,
                                opts opts)
:request{[&]
{
	assert(!!opts.remote);

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "POST";

	if(likely(!defined(json::get<"uri"_>(opts.request))))
		json::get<"uri"_>(opts.request) = "/_matrix/key/v2/query";

	window_buffer buf{buf_};
	if(likely(!defined(json::get<"content"_>(opts.request))))
	{
		buf([&keys](const mutable_buffer &buf)
		{
			return _make_server_keys(keys, buf);
		});

		json::get<"content"_>(opts.request) = json::object
		{
			buf.completed()
		};
	}

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

static ircd::const_buffer
ircd::m::fed::_make_server_keys(const vector_view<const key::server_key> &keys,
                                const mutable_buffer &buf)
{
	json::stack out{buf};
	json::stack::object top{out};
	json::stack::object server_keys
	{
		top, "server_keys"
	};

	for(const auto &[server_name, key_id] : keys)
	{
		json::stack::object server_object
		{
			server_keys, server_name
		};

		if(key_id)
		{
			json::stack::object key_object
			{
				server_object, key_id
			};

			//json::stack::member mvut
			//{
			//	key_object, "minimum_valid_until_ts", json::value(0L)
			//};
		}
	}

	server_keys.~object();
	top.~object();
	return out.completed();
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/version.h
//

ircd::m::fed::version::version(const mutable_buffer &buf,
                               opts opts)
:request{[&]
{
	assert(!!opts.remote);

	if(likely(!defined(json::get<"method"_>(opts.request))))
		json::get<"method"_>(opts.request) = "GET";

	if(likely(!defined(json::get<"uri"_>(opts.request))))
		json::get<"uri"_>(opts.request) = "/_matrix/federation/v1/version";

	return request
	{
		buf, std::move(opts)
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/request.h
//

//
// request::request
//

ircd::m::fed::request::request(const mutable_buffer &buf_,
                               opts &&opts)
:server::request{[&]
{
	// Requestor must always provide a remote by this point.
	assert(!!opts.remote);

	// Requestor must always generate a uri by this point
	assert(defined(json::get<"uri"_>(opts.request)));

	// Default the origin to my primary homeserver
	if(likely(!defined(json::get<"origin"_>(opts.request))))
		json::get<"origin"_>(opts.request) = my_host();

	// Default the destination to the remote origin
	if(likely(!defined(json::get<"destination"_>(opts.request))))
		json::get<"destination"_>(opts.request) = opts.remote;

	// Set the outgoing HTTP content from the request's content field.
	if(likely(defined(json::get<"content"_>(opts.request))))
		opts.out.content = json::get<"content"_>(opts.request);

	// Allows for the reverse to ensure these values are set.
	if(!defined(json::get<"content"_>(opts.request)))
		json::get<"content"_>(opts.request) = json::object{opts.out.content};

	// Defaults the method as a convenience if none specified.
	if(!defined(json::get<"method"_>(opts.request)))
		json::get<"method"_>(opts.request) = "GET";

	// Perform well-known query; note that we hijack the user's buffer to make
	// this query and receive the result at the front of it. When there's no
	// well-known this fails silently by just returning the input (likely).
	const string_view remote
	{
		// We don't query for well-known if the origin has an explicit port.
		!port(net::hostport(opts.remote))?
			well_known(buf_, opts.remote):
			opts.remote
	};

	// Devote the remaining buffer for HTTP as otherwise intended.
	const mutable_buffer buf
	{
		buf_ + size(remote)
	};

	// Generate the request head including the X-Matrix into buffer.
	opts.out.head = opts.request(buf,
	{
		// Note that we override the HTTP Host header with the well-known
		// remote; otherwise default is the destination above which may differ.
		{ "Host", remote }
	});

	// Setup some buffering features which can optimize the server::request
	if(!size(opts.in))
	{
		opts.in.head = buf + size(opts.out.head);
		opts.in.content = opts.dynamic?
			mutable_buffer{}:  // server::request will allocate new mem
			opts.in.head;      // server::request will auto partition
	}

	// Launch the request
	return server::request
	{
		matrix_service(remote),
		std::move(opts.out),
		std::move(opts.in),
		opts.sopts
	};
}()}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// fed/fed.h
//

//
// fetch_head util
//

ircd::conf::item<ircd::milliseconds>
fetch_head_timeout
{
	{ "name",     "ircd.m.v1.fetch_head.timeout" },
	{ "default",  30 * 1000L                     },
};

ircd::m::event::id::buf
ircd::m::fed::fetch_head(const id::room &room_id,
                         const string_view &remote)
{
	const m::room room
	{
		room_id
	};

	// When no user_id is supplied and the room exists locally we attempt
	// to find the user_id of one of our users with membership in the room.
	// This satisfies synapse's requirements for whether we have access
	// to the response. If user_id remains blank then make_join will later
	// generate a random one from our host as well.
	m::user::id::buf user_id
	{
		any_user(room, my_host(), "join")
	};

	// Make another attempt to find an invited user because that carries some
	// value (this query is not as fast as querying join memberships).
	if(!user_id)
		user_id = any_user(room, my_host(), "invite");

	return fetch_head(room_id, remote, user_id);
}

ircd::m::event::id::buf
ircd::m::fed::fetch_head(const id::room &room_id,
                         const string_view &remote,
                         const id::user &user_id)
{
	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	make_join::opts opts;
	opts.remote = remote;
	opts.dynamic = false;
	make_join request
	{
		room_id, user_id, buf, std::move(opts)
	};

	request.wait(milliseconds(fetch_head_timeout));
	request.get();

	const json::object proto
	{
		request.in.content
	};

	const json::object event
	{
		proto.at("event")
	};

	const m::event::prev prev
	{
		event
	};

	const auto &prev_event_id
	{
		prev.prev_event(0)
	};

	return prev_event_id;
}

//
// well-known matrix server
//

ircd::log::log
well_known_log
{
	"m.well-known"
};

ircd::conf::item<ircd::seconds>
well_known_cache_default
{
	{ "name",     "ircd.m.fed.well-known.cache.default" },
	{ "default",  24 * 60 * 60L                         },
};

ircd::conf::item<ircd::seconds>
well_known_cache_error
{
	{ "name",     "ircd.m.fed.well-known.cache.error" },
	{ "default",  36 * 60 * 60L                       },
};

///NOTE: not yet used until HTTP cache headers in response are respected.
ircd::conf::item<ircd::seconds>
well_known_cache_max
{
	{ "name",     "ircd.m.fed.well-known.cache.max" },
	{ "default",  48 * 60 * 60L                     },
};

ircd::conf::item<ircd::seconds>
well_known_fetch_timeout
{
	{ "name",     "ircd.m.fed.well-known.fetch.timeout" },
	{ "default",  8L                                    },
};

ircd::conf::item<size_t>
well_known_fetch_redirects
{
	{ "name",     "ircd.m.fed.well-known.fetch.redirects" },
	{ "default",  2L                                      },
};

ircd::string_view
ircd::m::fed::well_known(const mutable_buffer &buf,
                         const string_view &origin)
try
{
	static const string_view type
	{
		"well-known.matrix.server"
	};

	const m::room::id::buf room_id
	{
		"dns", m::my_host()
	};

	const m::room room
	{
		room_id
	};

	const m::event::idx event_idx
	{
		room.get(std::nothrow, type, origin)
	};

	const milliseconds origin_server_ts
	{
		m::get<time_t>(std::nothrow, event_idx, "origin_server_ts", time_t(0))
	};

	const json::object content
	{
		origin_server_ts > 0ms?
			m::get(std::nothrow, event_idx, "content", buf):
			const_buffer{}
	};

	const json::string cached
	{
		content["m.server"]
	};

	const seconds ttl
	{
		content.get<time_t>("ttl", time_t(0))
	};

	const system_point expires
	{
		origin_server_ts + ttl
	};

	const bool expired
	{
		ircd::now<system_point>() > expires
		|| empty(cached)
	};

	// Crucial value that will provide us with a return string for this
	// function in any case. This is obtained by either using the value
	// found in cache or making a network query for a new value. expired=true
	// when a network query needs to be made, otherwise we can return the
	// cached value. If the network query fails, this value is still defaulted
	// as the origin string to return and we'll also cache that too.
	assert(expired || !empty(cached));
	const string_view delegated
	{
		expired?
			fetch_well_known(buf, origin):

			// Move the returned string to the front of the buffer; this overwrites
			// other data fetched by the cache query to focus on just the result.
			string_view
			{
				data(buf), move(buf, cached)
			}
	};

	// Branch on valid cache hit to return result.
	if(!expired)
	{
		thread_local char tmbuf[48];
		log::debug
		{
			well_known_log, "%s found in cache delegated to %s event_idx:%u expires %s",
			origin,
			delegated,
			event_idx,
			timef(tmbuf, expires, localtime),
		};

		return delegated;
	}

	// Any time the well-known result is the same as the origin (that
	// includes legitimate errors where fetch_well_known() returns the
	// origin to default) we consider that an error and use the error
	// TTL value. Sorry, no exponential backoff implemented yet.
	const auto cache_ttl
	{
		origin == delegated?
			seconds(well_known_cache_error).count():
			seconds(well_known_cache_default).count()
	};

	// Write our record to the cache room; note that this doesn't really
	// match the format of other DNS records in this room since it's a bit
	// simpler, but we don't share the ircd.dns.rr type prefix anyway.
	const auto cache_id
	{
		m::send(room, m::me(), type, origin, json::members
		{
			{ "ttl",       cache_ttl  },
			{ "m.server",  delegated  },
		})
	};

	log::debug
	{
		well_known_log, "%s caching delegation to %s to cache in %s",
		origin,
		delegated,
		string_view{cache_id},
	};

	return delegated;
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		well_known_log, "%s :%s",
		origin,
		e.what(),
	};

	return origin;
}

namespace ircd::m::fed
{
	static std::tuple<http::code, string_view, string_view>
	fetch_well_known(const mutable_buffer &out,
	                 const net::hostport &remote,
	                 const string_view &url);
}

ircd::string_view
ircd::m::fed::fetch_well_known(const mutable_buffer &buf,
                               const string_view &origin)
try
{
	static const string_view path
	{
		"/.well-known/matrix/server"
	};

	rfc3986::uri uri;
	uri.remote = origin;
	uri.path = path;
	json::object response;
	unique_mutable_buffer carry;
	for(size_t i(0); i < well_known_fetch_redirects; ++i)
	{
		const auto &[code, location, content]
		{
			fetch_well_known(buf, uri.remote, uri.path)
		};

		// Successful error; bail
		if(code >= 400)
			return origin;

		// Successful result; response content handled after loop.
		if(code < 300)
		{
			response = content;
			break;
		}

		// Indirection code, but no location response header
		if(!location)
			return origin;

		// Redirection; carry over the new target by copying it because it's
		// in the buffer which we'll be overwriting for the new request.
		carry = unique_mutable_buffer{location};
		uri = string_view{carry};

		// Indirection code, bad location header.
		if(!uri.path || !uri.remote)
			return origin;
	}

	const json::string &m_server
	{
		response["m.server"]
	};

	// This construction validates we didn't get a junk string
	volatile const net::hostport ret
	{
		m_server
	};

	thread_local char rembuf[rfc3986::DOMAIN_BUFSIZE * 2];
	log::debug
	{
		well_known_log, "%s query to %s found delegation to %s",
		origin,
		uri.remote,
		m_server,
	};

	// Move the returned string to the front of the buffer; this overwrites
	// any other incoming content to focus on just the unquoted string.
	return string_view
	{
		data(buf), move(buf, m_server)
	};
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::derror
	{
		well_known_log, "%s in network query :%s",
		origin,
		e.what(),
	};

	return origin;
}

/// Return a tuple of the HTTP code, any Location header, and the response
/// content. These values are unconditional if this function doesn't throw,
/// but if there's no Location header and/or content then those will be empty
/// string_view's. This function is intended to be run in a loop by the caller
/// to chase redirection. No HTTP codes will throw from here; server and
/// network errors (and others) will.
static std::tuple<ircd::http::code, ircd::string_view, ircd::string_view>
ircd::m::fed::fetch_well_known(const mutable_buffer &buf,
	                           const net::hostport &remote,
	                           const string_view &url)
{
	// Hard target https service; do not inherit any matrix service from remote.
	const net::hostport target
	{
		host(remote), "https", port(remote)
	};

	window_buffer wb{buf};
	http::request
	{
		wb, host(target), "GET", url
	};

	const const_buffer out_head
	{
		wb.completed()
	};

	// Remaining space in buffer is used for received head; note that below
	// we specify this same buffer for in.content, but that's a trick
	// recognized by ircd::server to place received content directly after
	// head in this buffer without any additional dynamic allocation.
	const mutable_buffer in_head
	{
		buf + size(out_head)
	};

	server::request::opts opts;
	opts.http_exceptions = false; // 3xx/4xx/5xx response won't throw.
	server::request request
	{
		target,
		{ out_head },
		{ in_head, in_head },
		&opts
	};

	const auto code
	{
		request.get(seconds(well_known_fetch_timeout))
	};

	thread_local char rembuf[rfc3986::DOMAIN_BUFSIZE * 2];
	log::debug
	{
		well_known_log, "fetch from %s %s :%u %s",
		string(rembuf, target),
		url,
		uint(code),
		http::status(code)
	};

	const http::response::head head
	{
		request.in.gethead(request)
	};

	return
	{
		code,
		head.location,
		request.in.content,
	};
}
