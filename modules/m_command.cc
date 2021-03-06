// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/util/params.h>

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Server Command"
};

static std::pair<string_view, string_view>
execute_command(const mutable_buffer &buf,
                const m::user &user,
                const m::room &room,
                const string_view &cmd);

static void
handle_command(const m::event &event,
               m::vm::eval &eval);

m::hookfn<m::vm::eval &>
command_hook
{
	handle_command,
	{
		{ "_site",    "vm.effect"       },
		{ "type",     "ircd.cmd"        },
		{ "origin",   my_host()         },
	}
};

void
handle_command(const m::event &event,
               m::vm::eval &eval)
try
{
	const m::user user
	{
		at<"sender"_>(event)
	};

	if(!m::my(user.user_id))
		return;

	const json::object &content
	{
		json::get<"content"_>(event)
	};

	const m::user::room user_room
	{
		user
	};

	if(json::get<"room_id"_>(event) != user_room.room_id)
		return;

	const m::room::id &room_id
	{
		unquote(content.at("room_id"))
	};

	const json::string &input
	{
		content.at("body")
	};

	if(!startswith(input, "\\\\"))
		return;

	// View of the command string without prefix
	string_view cmd{input};
	cmd = lstrip(cmd, "\\\\");

	// Determine if there's a bang after the prefix; if so the response's
	// sender will be the user, and will be broadcast publicly to the room.
	// Otherwise the response comes from the server and is only visible in
	// the user's timeline.
	const bool public_response(startswith(cmd, '!'));
	cmd = lstrip(cmd, '!');

	log::debug
	{
		m::log, "Server command from %s in %s public:%b :%s",
		string_view{room_id},
		string_view{user.user_id},
		public_response,
		cmd
	};

	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	const auto &[html, alt]
	{
		execute_command(buf, user, room_id, cmd)
	};

	if(!html && !alt)
		return;

	const auto &response_sender
	{
		public_response? user : m::user(m::me())
	};

	const auto &response_room
	{
		public_response? room_id : user_room
	};

	const auto &response_type
	{
		public_response? "m.room.message" : "ircd.cmd.result"
	};

	m::send(response_room, response_sender, response_type,
	{
		{ "msgtype",         "m.notice"                },
		{ "format",          "org.matrix.custom.html"  },
		{ "body",            { alt,  json::STRING }    },
		{ "formatted_body",  { html, json::STRING }    },
		{ "room_id",         room_id                   },
		{ "input",           input                     },
    });
}
catch(const std::exception &e)
{
	throw;
}

static std::pair<string_view, string_view>
command__ping(const mutable_buffer &buf,
              const m::user &user,
              const m::room &room,
              const string_view &cmd);

static std::pair<string_view, string_view>
command__dash(const mutable_buffer &buf,
              const m::user &user,
              const m::room &room,
              const string_view &cmd);

static std::pair<string_view, string_view>
command__read(const mutable_buffer &buf,
              const m::user &user,
              const m::room &room,
              const string_view &cmd);

static std::pair<string_view, string_view>
command__version(const mutable_buffer &buf,
                 const m::user &user,
                 const m::room &room,
                 const string_view &cmd);

std::pair<string_view, string_view>
execute_command(const mutable_buffer &buf,
                const m::user &user,
                const m::room &room,
                const string_view &cmd)
try
{
	switch(hash(token(cmd, ' ', 0)))
	{
		case "version"_:
			return command__version(buf, user, room, cmd);

		case "read"_:
			return command__read(buf, user, room, cmd);

		case "dash"_:
			return command__dash(buf, user, room, cmd);

		case "ping"_:
			return command__ping(buf, user, room, cmd);

		default:
			break;
	}

	const string_view out{fmt::sprintf
	{
		buf, "unknown command :%s", cmd
	}};

	const string_view alt
	{
		out
	};

	return { out, alt };
}
catch(const m::error &e)
{
	const json::object &error
	{
		e.content
	};

	log::error
	{
		m::log, "Server command from %s in %s '%s' :%s :%s :%s",
		string_view{room.room_id},
		string_view{user.user_id},
		cmd,
		e.what(),
		unquote(error.get("errcode")),
		unquote(error.get("error")),
	};

	std::ostringstream out;
	pubsetbuf(out, buf);

	const string_view fg[] {"#FCFCFC", "#FFFFFF"};
	const string_view bg[] {"#A01810", "#C81810"};
	const string_view sp {"&nbsp;"};
	out
	    << "<h5>"
	    << "<font color=\"" << fg[0] << "\" data-mx-bg-color=\"" << bg[0] << "\">"
	    << "<b>"
	    << sp << sp << e.what() << sp << sp
	    << "</b>"
	    << "</font> "
	    << "<font color=\"" << fg[1] << "\" data-mx-bg-color=\"" << bg[1] << "\">"
	    << "<b>"
	    << sp << sp << unquote(error.get("errcode")) << sp << sp
	    << "</b>"
	    << "</font> "
	    << "</h5>"
	    << "<pre>"
	    << unquote(error.get("error"))
	    << "</pre>"
	    ;

	return
	{
		view(out, buf), e.what()
	};
}
catch(const http::error &e)
{
	log::error
	{
		m::log, "Server command from %s in %s '%s' :%s :%s",
		string_view{room.room_id},
		string_view{user.user_id},
		cmd,
		e.what(),
		e.content
	};

	std::ostringstream out;
	pubsetbuf(out, buf);

	const string_view fg[] {"#FCFCFC"};
	const string_view bg[] {"#A01810"};
	const string_view sp {"&nbsp;"};
	out
	    << "<h5>"
	    << "<font color=\"" << fg[0] << "\" data-mx-bg-color=\"" << bg[0] << "\">"
	    << "<b>"
	    << sp << sp << e.what() << sp << sp
	    << "</b>"
	    << "</font> "
	    << "</h5>"
	    << "<pre>"
	    << e.content
	    << "</pre>"
	    ;

	return
	{
		view(out, buf), e.what()
	};
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "Server command from %s in %s '%s' :%s",
		string_view{room.room_id},
		string_view{user.user_id},
		cmd,
		e.what()
	};

	const size_t len
	{
		copy(buf, string_view(e.what()))
	};

	return
	{
		{ data(buf), len },
		{ data(buf), len }
	};
}

std::pair<string_view, string_view>
command__version(const mutable_buffer &buf,
                 const m::user &user,
                 const m::room &room,
                 const string_view &cmd)
{
	std::ostringstream out;
	pubsetbuf(out, buf);

	const string_view sp {"&nbsp;"};
	out
	    << "<h1>"
	    << info::name
	    << "</h1>"
	    << "<pre><code>"
	    << info::version
	    << "</code></pre>"
	    ;

	const string_view alt
	{
		info::version
	};

	return { view(out, buf), alt };
}

std::pair<string_view, string_view>
command__read(const mutable_buffer &buf,
              const m::user &user,
              const m::room &room,
              const string_view &cmd)
{
	const params param{tokens_after(cmd, ' ', 0), " ",
	{
		"arg", "[time]"
	}};

	const string_view &arg
	{
		param["arg"]
	};

	const time_t &ms
	{
		param.at("[time]", ircd::time<milliseconds>())
	};

	std::ostringstream out;
	pubsetbuf(out, buf);

	if(m::valid(m::id::EVENT, arg))
	{
		const m::event::id::buf event_id
		{
			arg?
				m::event::id::buf{arg}:
				m::head(room)
		};

		m::receipt::read(room, user, event_id, json::strung{json::members
		{
			{ "ts", ms },
		}});

		return {};
	}
	else if(m::valid(m::id::ROOM, arg) || m::valid(m::id::ROOM_ALIAS, arg))
	{
		const auto room_id
		{
			m::room_id(arg)
		};

		const m::room room
		{
			room_id
		};

		const m::event::id::buf event_id
		{
			m::head(room)
		};

		m::receipt::read(room, user, event_id, json::strung{json::members
		{
			{ "ts", ms },
		}});

		return {};
	}

	// We don't accept no-argument as a wildcard to prevent a naive user
	// just probing the command interface from performing the following large
	// operation on all their rooms...
	if(!arg)
		return {};

	// The string argument can be a globular expression of room tags, like
	// `m.*` or just `*`.
	const globular_imatch match
	{
		arg
	};

	// Iterate all joined rooms for a user. For each room we'll test if the
	// room's tag matches the arg string globular expression.
	const m::user::rooms user_rooms
	{
		user
	};

	const string_view fg[] {"#FFFFFF"};
	const string_view bg[] {"#000000"};
	out
	<< "<pre>"
	<< "<font color=\"" << fg[0] << "\" data-mx-bg-color=\"" << bg[0] << "\">"
	<< "<table>"
	;

	size_t matched(0);
	user_rooms.for_each("join", [&user, &ms, &match, &out, &matched]
	(const m::room::id &room_id, const string_view &)
	{
		const m::user::room_tags room_tags
		{
			user, room_id
		};

		// return true if the expression is not matched for this room.
		const auto without_match{[&match]
		(const string_view &key, const json::object &object)
		{
			return !match(key);
		}};

		// for_each returns true if it didn't break from the loop, which means
		// no match and skip actions for this room.
		if(match.expr != "*")
			if(room_tags.for_each(without_match))
				return;

		// Get the room head (if there are multiple, the best is selected for
		// us) which will be the target of our receipt.
		const m::event::id::buf event_id
		{
			m::head(std::nothrow, room_id)
		};

		// Nothing to send a receipt for.
		if(!event_id)
			return;

		const auto put{[&out]
		(const string_view &room_id, const string_view &event_id)
		{
			out
			<< "<tr>"
			<< "<td>"
			<< "<b>"
			<< room_id
			<< "</b>"
			<< "</td>"
			<< "<td>"
			<< event_id
			<< "</td>"
			<< "</tr>"
			;
		}};

		// Check if event_id is more recent than the last receipt's event_id.
		if(!m::receipt::freshest(room_id, user, event_id))
		{
			put(room_id, "You already read this or a later event in the room.");
			return;
		}

		// Check if user wants to prevent sending receipts to this room.
		if(m::receipt::ignoring(user, room_id))
		{
			put(room_id, "You have configured to not send receipts to this room.");
			return;
		}

		// Check if user wants to prevent based on this event's specifics.
		if(m::receipt::ignoring(user, event_id))
		{
			put(room_id, "You have configured to not send receipts for this event.");
			return;
		}

		// Commit the receipt.
		m::receipt::read(room_id, user, event_id, json::strung{json::members
		{
			{ "ts",        ms   },
			{ "m.hidden",  true },
		}});

		put(room_id, event_id);
		++matched;
	});

	out
	<< "</table>"
	<< "</font>"
	<< "<br />*** Marked "
	<< matched
	<< " rooms as read.<br />"
	<< "</pre>"
	;

	return
	{
		view(out, buf), "TODO: alt text."
	};
}

static std::pair<string_view, string_view>
command__ping__room(const mutable_buffer &buf,
                    const m::user &user,
                    const m::room &room,
                    const string_view &cmd);

std::pair<string_view, string_view>
command__ping(const mutable_buffer &buf,
              const m::user &user,
              const m::room &room,
              const string_view &cmd)
{
	const params param{tokens_after(cmd, ' ', 0), " ",
	{
		"target"
	}};

	const string_view &target
	{
		param["target"]
	};

	const bool room_ping
	{
		!target
		|| m::valid(m::id::ROOM, target)
		|| m::valid(m::id::ROOM_ALIAS, target)
	};

	if(room_ping)
		return command__ping__room(buf, user, room, cmd);

	m::fed::version::opts opts;
	if(m::valid(m::id::USER, target))
		opts.remote = m::user::id(target).host();
	else
		opts.remote = target;

	const unique_buffer<mutable_buffer> http_buf
	{
		8_KiB
	};

	util::timer timer;
	m::fed::version request
	{
		http_buf, std::move(opts)
	};

	std::exception_ptr eptr; try
	{
		request.wait(seconds(10));
		const auto code(request.get());
	}
	catch(const std::exception &e)
	{
		eptr = std::current_exception();
	}

	const auto time
	{
		timer.at<nanoseconds>()
	};

	static const string_view
	sp{"&nbsp;"}, fg{"#e8e8e8"}, host_bg{"#181b21"},
	online_bg{"#008000"}, offline_bg{"#A01810"};

	const string_view
	bg{eptr? offline_bg : online_bg},
	status{eptr? "FAILED " : "ONLINE"};

	std::ostringstream out;
	pubsetbuf(out, buf);
	thread_local char tmbuf[32];
	out
	    << " <font color=\"" << fg << "\" data-mx-bg-color=\"" << bg << "\">"
	    << " <b>"
	    << sp << sp << status << sp << sp
	    << " </b>"
	    << " </font>"
	    << " <font color=\"" << fg << "\" data-mx-bg-color=\"" << host_bg << "\">"
	    << sp << sp << " " << target << " " << sp
	    << " </font> "
	    ;

	if(!eptr)
		out << " <b>"
		    << pretty(tmbuf, time)
		    << " </b>"
		    << " application layer round-trip time.";

	if(eptr)
		out << "<pre>"
		     << what(eptr)
		     << "</pre>";

	const string_view rich
	{
		view(out, buf)
	};

	const string_view alt{fmt::sprintf
	{
		buf + size(rich), "response in %s", pretty(tmbuf, time)
	}};

	return { view(out, buf), alt };
}

std::pair<string_view, string_view>
command__ping__room(const mutable_buffer &buf,
                    const m::user &user,
                    const m::room &room,
                    const string_view &cmd)
{
	const params param{tokens_after(cmd, ' ', 0), " ",
	{
		"target"
	}};

	const string_view &target
	{
		param["target"]
	};

	m::feds::opts opts;
	opts.op = m::feds::op::version;
	opts.room_id = room.room_id;
	opts.closure_cached_errors = true;
	opts.timeout = seconds(10); //TODO: conf

	thread_local char tmbuf[32];
	std::ostringstream out;
	pubsetbuf(out, buf);

	util::timer timer;
	size_t responses{0};
	m::feds::execute(opts, [&timer, &responses, &buf, &out]
	(const auto &result)
	{
		++responses;

		static const string_view
		sp{"&nbsp;"}, fg{"#e8e8e8"}, host_bg{"#181b21"},
		online_bg{"#008000"}, offline_bg{"#A01810"};

		const string_view
		bg{result.eptr? offline_bg : online_bg},
		status{result.eptr? "FAILED " : "ONLINE"};

		out
		    << " <font color=\"" << fg << "\" data-mx-bg-color=\"" << bg << "\">"
		    << " <b>"
		    << sp << sp << status << sp << sp
		    << " </b>"
		    << " </font>"
		    << " <font color=\"" << fg << "\" data-mx-bg-color=\"" << host_bg << "\">"
		    << sp << sp << " " << result.origin << " " << sp
		    << " </font> "
		    ;

		if(!result.eptr)
			out << " <b>"
			    << pretty(tmbuf, timer.at<nanoseconds>())
			    << " </b>"
			    << " application layer round-trip time."
			    << "<br />";

		if(result.eptr)
			out << "<code>"
			    << what(result.eptr)
			    << "</code>"
			    << "<br />";

		return true;
	});

	const string_view rich
	{
		view(out, buf)
	};

	const string_view alt{fmt::sprintf
	{
		buf + size(rich), "%zu responses in %s",
		responses,
		pretty(tmbuf, timer.at<nanoseconds>())
	}};

	return { view(out, buf), alt };
}

std::pair<string_view, string_view>
command__dash(const mutable_buffer &buf,
              const m::user &user,
              const m::room &room,
              const string_view &cmd)
{
	std::ostringstream out;
	pubsetbuf(out, buf);

	const string_view fg[] {"#3EA6FF", "#FFFFFF"};
	const string_view bg[] {"#000000", "#008000"};
	const string_view sp {"&nbsp;"};
	out
	    << "<h5>"
	    << "<font color=\"" << fg[0] << "\" data-mx-bg-color=\"" << bg[0] << "\">"
	    << "<b>"
	    << sp << sp << " CONSTRUCT STATUS " << sp << sp
	    << "</b>"
	    << "</font> "
	    << "<font color=\"" << fg[1] << "\" data-mx-bg-color=\"" << bg[1] << "\">"
	    << "<b>"
	    << sp << sp << " OK " << sp << sp
	    << "</b>"
	    << "</font> "
	    << "</h5>"
	    << "<pre>"
	    << " "
	    << "</pre>"
	    ;

	const string_view alt
	{
		"no alt text"
	};

	return { view(out, buf), alt };
}
