// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "sender.h"

std::list<txn> txns;
std::map<std::string, node, std::less<>> nodes;

void remove_node(const node &);
static void recv_timeout(txn &, node &);
static void recv_timeouts();
static bool recv_handle(txn &, node &);
static void recv();
static void recv_worker();
ctx::dock recv_action;

static void send_from_user(const m::event &, const m::user::id &user_id);
static void send_to_user(const m::event &, const m::user::id &user_id);
static void send_to_room(const m::event &, const m::room::id &room_id);
static void send(const m::event &);
static void send_worker();

static void handle_notify(const m::event &, m::vm::eval &);

context
sender
{
	"m.fedsnd.S", 1_MiB, &send_worker, context::POST,
};

context
receiver
{
	"m.fedsnd.R", 1_MiB, &recv_worker, context::POST,
};

mapi::header
IRCD_MODULE
{
	"federation sender",
	nullptr, []
	{
		sender.terminate();
		receiver.terminate();
		sender.join();
		receiver.join();
	}
};

std::deque<std::pair<std::string, m::event::id::buf>>
notified_queue;

ctx::dock
notified_dock;

m::hookfn<m::vm::eval &>
notified
{
	handle_notify,
	{
		{ "_site",  "vm.notify" },
	}
};

void
handle_notify(const m::event &event,
              m::vm::eval &eval)
try
{
	if(!my(event))
		return;

	assert(eval.opts);
	if(!eval.opts->notify_servers)
		return;

	const m::event::id::buf &event_id
	{
		event.event_id?
			m::event::id::buf{event.event_id}:
			m::event::id::buf{}
	};

	notified_queue.emplace_back(json::strung{event}, event_id);
	notified_dock.notify_all();
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::critical
	{
		m::log, "Federation sender notify handler :%s",
		e.what()
	};
}

void
__attribute__((noreturn))
send_worker()
{
	while(1) try
	{
		notified_dock.wait([]
		{
			return !notified_queue.empty();
		});

		const unwind pop{[]
		{
			assert(!notified_queue.empty());
			notified_queue.pop_front();
		}};

		const auto &[event_, event_id]
		{
			notified_queue.front()
		};

		const m::event event
		{
			json::object{event_}, event_id
		};

		send(event);
	}
	catch(const std::exception &e)
	{
		log::error
		{
			"sender worker: %s", e.what()
		};
	}
}

void
send(const m::event &event)
{
	const auto &type
	{
		json::get<"type"_>(event)
	};

	const auto &sender
	{
		json::get<"sender"_>(event)
	};

	const auto &room_id
	{
		json::get<"room_id"_>(event)
	};

	const json::object &content
	{
		json::get<"content"_>(event)
	};

	if(json::get<"depth"_>(event) == json::undefined_number)
		return;

	// target is every remote server in a room
	if(valid(m::id::ROOM, room_id))
		return send_to_room(event, m::room::id{room_id});

	// target is remote server hosting user/device
	if(type == "m.direct_to_device")
	{
		const json::string &target
		{
			content.get("target")
		};

		if(valid(m::id::USER, target))
			return send_to_user(event, m::user::id(target));
	}

	// target is every remote server from every room a user is joined to.
	if(valid(m::id::USER, sender))
		return send_from_user(event, m::user::id{sender});
}

/// EDU and PDU path where the target is a room
void
send_to_room(const m::event &event,
             const m::room::id &room_id)
{
	// Unit is not allocated until we find another server in the room.
	std::shared_ptr<struct unit> unit;

	const m::room room{room_id};
	const m::room::origins origins{room};
	origins.for_each([&unit, &event]
	(const string_view &origin)
	{
		if(my_host(origin))
			return;

		auto it{nodes.lower_bound(origin)};
		if(it == end(nodes) || it->first != origin)
		{
			if(server::errant(m::fed::matrix_service(origin)))
				return;

			it = nodes.emplace_hint(it, origin, origin);
		}

		auto &node{it->second};
		if(node.err)
			return;

		if(!unit)
			unit = std::make_shared<struct unit>(event);

		node.push(unit);
		node.flush();
	});
}

/// EDU path where the target is a user/device
void
send_to_user(const m::event &event,
             const m::user::id &user_id)
{
	const string_view &remote
	{
		user_id.host()
	};

	if(my_host(remote))
		return;

	auto it{nodes.lower_bound(remote)};
	if(it == end(nodes) || it->first != remote)
	{
		if(server::errant(m::fed::matrix_service(remote)))
			return;

		it = nodes.emplace_hint(it, remote, remote);
	}

	auto &node{it->second};
	if(node.err)
		return;

	auto unit
	{
		std::make_shared<struct unit>(event)
	};

	node.push(std::move(unit));
	node.flush();
}

/// EDU path where the he target is every server from every room the sender
/// is joined to.
void
send_from_user(const m::event &event,
               const m::user::id &user_id)
{
	const m::user::servers servers
	{
		user_id
	};

	// Iterate all of the servers visible in this user's joined rooms.
	servers.for_each("join", [&user_id, &event]
	(const string_view &origin)
	{
		if(my_host(origin))
			return true;

		auto it{nodes.lower_bound(origin)};
		if(it == end(nodes) || it->first != origin)
		{
			if(server::errant(m::fed::matrix_service(origin)))
				return true;

			it = nodes.emplace_hint(it, origin, origin);
		}

		auto &node{it->second};
		if(node.err)
			return true;

		auto unit
		{
			std::make_shared<struct unit>(event)
		};

		node.push(unit);
		node.flush();
		return true;
	});
}

void
node::push(std::shared_ptr<unit> su)
{
	q.emplace_back(std::move(su));
}

bool
node::flush()
try
{
	if(q.empty())
		return true;

	if(curtxn)
		return true;

	size_t pdus{0}, edus{0};
	for(const auto &unit : q) switch(unit->type)
	{
		case unit::PDU:   ++pdus;  break;
		case unit::EDU:   ++edus;  break;
		default:                   break;
	}

	size_t pc(0), ec(0);
	std::vector<json::value> units(pdus + edus);
	for(const auto &unit : q) switch(unit->type)
	{
		case unit::PDU:
			units.at(pc++) = string_view{unit->s};
			break;

		case unit::EDU:
			units.at(pdus + ec++) = string_view{unit->s};
			break;

		default:
			break;
	}

	m::fed::send::opts opts;
	opts.remote = remote;
	opts.sopts = &sopts;

	const vector_view<const json::value> pduv
	{
		units.data(), units.data() + pc
	};

	const vector_view<const json::value> eduv
	{
		units.data() + pdus, units.data() + pdus + ec
	};

	std::string content
	{
		m::txn::create(pduv, eduv)
	};

	txns.emplace_back(*this, std::move(content), std::move(opts));
	const unwind_nominal_assertion na;
	curtxn = &txns.back();
	q.clear();
	log::debug
	{
		m::log, "sending txn %s pdus:%zu edus:%zu to '%s'",
		curtxn->txnid,
		pdus,
		edus,
		this->remote,
	};

	recv_action.notify_one();
	return true;
}
catch(const std::exception &e)
{
	log::error
	{
		"flush error to %s :%s", remote, e.what()
	};

	err = true;
	return false;
}

void
__attribute__((noreturn))
recv_worker()
{
	while(1)
	{
		recv_action.wait([]
		{
			return !txns.empty();
		});

		recv();
		recv_timeouts();
	}
}

void
recv()
try
{
	auto next
	{
		ctx::when_any(begin(txns), end(txns))
	};

	if(!next.wait(seconds(2), std::nothrow))   //TODO: conf
		return;

	const auto it
	{
		next.get()
	};

	if(unlikely(it == end(txns)))
	{
		assert(0);
		return;
	}

	auto &txn
	{
		*it
	};

	assert(txn.node);
	auto &node{*txn.node};
	const auto ret
	{
		recv_handle(txn, node)
	};

	node.curtxn = nullptr;
	txns.erase(it);

	if(node.err)
		return remove_node(node);

	if(!ret)
		return;

	node.flush();
}
catch(const std::exception &e)
{
	ircd::panicking(e);
}

bool
recv_handle(txn &txn,
            node &node)
try
{
	const auto code
	{
		txn.get()
	};

	const json::object obj
	{
		txn
	};

	const m::fed::send::response resp
	{
		obj
	};

	if(code != http::OK) log::dwarning
	{
		"%u %s from %s for %s",
		ushort(code),
		http::status(code),
		node.remote,
		txn.txnid
	};

	resp.for_each_pdu([&txn, &node]
	(const m::event::id &event_id, const json::object &error)
	{
		if(empty(error))
			return;

		log::error
		{
			"Error from %s in %s for %s :%s",
			node.remote,
			txn.txnid,
			string_view{event_id},
			string_view{error}
		};
	});

	return true;
}
catch(const http::error &e)
{
	log::derror
	{
		"%u %s from %s for %s :%s",
		ushort(e.code),
		http::status(e.code),
		node.remote,
		txn.txnid,
		e.what()
	};

	node.err = true;
	return false;
}
catch(const std::exception &e)
{
	log::derror
	{
		"Error from %s for %s :%s",
		node.remote,
		txn.txnid,
		e.what()
	};

	node.err = true;
	return false;
}

void
recv_timeouts()
{
	const auto &now
	{
		ircd::now<steady_point>()
	};

	auto it(begin(txns));
	for(; it != end(txns); ++it)
	{
		auto &txn(*it);
		assert(txn.node);
		if(txn.node->err)
			continue;

		if(txn.timeout + seconds(45) < now) //TODO: conf
			recv_timeout(txn, *txn.node);
	}
}

void
recv_timeout(txn &txn,
             node &node)
{
	log::dwarning
	{
		"Timeout to %s for txn %s",
		node.remote,
		txn.txnid
	};

	cancel(txn);
	node.err = true;
}

void
remove_node(const node &node)
{
	const auto it
	{
		nodes.find(node.remote)
	};

	assert(it != end(nodes));
	nodes.erase(it);
}
