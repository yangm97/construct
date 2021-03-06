// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::vm
{
	template<class... args> static fault handle_error(const opts &, const fault &, const string_view &fmt, args&&... a);
	template<class T> static void call_hook(hook::site<T> &, eval &, const event &, T&& data);
	static size_t calc_txn_reserve(const opts &, const event &);
	static void write_commit(eval &);
	static void write_append(eval &, const event &);
	static void write_prepare(eval &, const event &);
	static fault execute_edu(eval &, const event &);
	static fault execute_pdu(eval &, const event &);
	static fault execute_du(eval &, const event &);
	static fault inject3(eval &, json::iov &, const json::iov &);
	static fault inject1(eval &, json::iov &, const json::iov &);
	static void fini();
	static void init();

	extern hook::site<eval &> issue_hook;    ///< Called when this server is issuing event
	extern hook::site<eval &> conform_hook;  ///< Called for static evaluations of event
	extern hook::site<eval &> access_hook;   ///< Called for access control checking
	extern hook::site<eval &> fetch_hook;    ///< Called to resolve dependencies
	extern hook::site<eval &> eval_hook;     ///< Called for final event evaluation
	extern hook::site<eval &> post_hook;     ///< Called to apply effects pre-notify
	extern hook::site<eval &> notify_hook;   ///< Called to broadcast successful eval
	extern hook::site<eval &> effect_hook;   ///< Called to apply effects post-notify

	extern conf::item<bool> log_commit_debug;
	extern conf::item<bool> log_accept_debug;
	extern conf::item<bool> log_accept_info;
}

decltype(ircd::m::vm::log_commit_debug)
ircd::m::vm::log_commit_debug
{
	{ "name",     "ircd.m.vm.log.commit.debug" },
	{ "default",  true                         },
};

decltype(ircd::m::vm::log_accept_debug)
ircd::m::vm::log_accept_debug
{
	{ "name",     "ircd.m.vm.log.accept.debug" },
	{ "default",  true                         },
};

decltype(ircd::m::vm::log_accept_info)
ircd::m::vm::log_accept_info
{
	{ "name",     "ircd.m.vm.log.accept.info" },
	{ "default",  false                       },
};

decltype(ircd::m::vm::issue_hook)
ircd::m::vm::issue_hook
{
	{ "name", "vm.issue" }
};

decltype(ircd::m::vm::conform_hook)
ircd::m::vm::conform_hook
{
	{ "name", "vm.conform" }
};

decltype(ircd::m::vm::access_hook)
ircd::m::vm::access_hook
{
	{ "name", "vm.access" }
};

decltype(ircd::m::vm::fetch_hook)
ircd::m::vm::fetch_hook
{
	{ "name", "vm.fetch" }
};

decltype(ircd::m::vm::eval_hook)
ircd::m::vm::eval_hook
{
	{ "name", "vm.eval" }
};

decltype(ircd::m::vm::post_hook)
ircd::m::vm::post_hook
{
	{ "name", "vm.post" }
};

decltype(ircd::m::vm::notify_hook)
ircd::m::vm::notify_hook
{
	{ "name",        "vm.notify"  },
	{ "exceptions",  false        },
};

decltype(ircd::m::vm::effect_hook)
ircd::m::vm::effect_hook
{
	{ "name",        "vm.effect"  },
	{ "exceptions",  false        },
};

//
// execute
//

ircd::m::vm::fault
ircd::m::vm::execute(eval &eval,
                     const event &event)
try
{
	// This assertion is tripped if the end of your context's stack is
	// danger close; try increasing your stack size.
	const ctx::stack_usage_assertion sua;

	// m::vm bookkeeping that someone entered this function
	const scope_count executing
	{
		eval::executing
	};

	const scope_notify notify
	{
		vm::dock
	};

	assert(eval.opts);
	const auto &opts
	{
		*eval.opts
	};

	// Set a member to the room_id for convenient access, without stepping on
	// any room_id reference that exists there for whatever reason.
	const scope_restore eval_room_id
	{
		eval.room_id,
		eval.room_id?
			eval.room_id:
		valid(id::ROOM, json::get<"room_id"_>(event))?
			string_view(json::get<"room_id"_>(event)):
			eval.room_id,
	};

	// Procure the room version.
	char room_version_buf[room::VERSION_MAX_SIZE];
	const scope_restore eval_room_version
	{
		eval.room_version,

		// The room version was supplied by the user in the options structure
		// because they know better.
		eval.opts->room_version?
			eval.opts->room_version:

		// The room version was already computed; probably by vm::inject().
		eval.room_version?
			eval.room_version:

		// There's no room version because there's no room!
		!eval.room_id?
			string_view{}:

		// Make a query for the room version into the stack buffer.
		m::version(room_version_buf, room{eval.room_id}, std::nothrow)
	};

	// Determine if this is an internal room creation event
	const bool is_internal_room_create
	{
		json::get<"type"_>(event) == "m.room.create" &&
		json::get<"sender"_>(event) &&
		m::myself(json::get<"sender"_>(event))
	};

	// Query for whether the room apropos is an internal room.
	const scope_restore room_internal
	{
		eval.room_internal,
		is_internal_room_create?
			true:
		eval.room_id?
			m::internal(eval.room_id):
			false
	};

	// We have to set the event_id in the event instance if it didn't come
	// with the event JSON.
	if(!opts.edu && !event.event_id)
		const_cast<m::event &>(event).event_id = eval.room_version == "3"?
			event::id{event::id::v3{eval.event_id, event}}:
			event::id{event::id::v4{eval.event_id, event}};

	// If we set the event_id in the event instance we have to unset
	// it so other contexts don't see an invalid reference.
	const unwind restore_event_id{[&event]
	{
		const_cast<m::event &>(event).event_id = json::get<"event_id"_>(event)?
			event.event_id:
			m::event::id{};
	}};

	// If the event is already being evaluated, wait here until the other
	// evaluation is finished. If the other was successful, the exists()
	// check will skip this, otherwise we have to try again here because
	// this evaluator might be using different options/credentials.
	if(likely(opts.unique) && event.event_id)
		sequence::dock.wait([&event]
		{
			return eval::count(event.event_id) <= 1;
		});

	// We can elide a lot of grief here by not proceeding further and simply
	// returning fault::EXISTS after an existence check. If we had to wait for
	// a duplicate eval this check will indicate its success.
	if(likely(!opts.replays && opts.nothrows & fault::EXISTS) && event.event_id)
		if(m::exists(event.event_id))
			return fault::EXISTS;

	// Set a member pointer to the event currently being evaluated. This
	// allows other parallel evals to have deep access to this eval. It also
	// will be used to count this event as currently being evaluated.
	assert(!eval.event_);
	const scope_restore eval_event
	{
		eval.event_, &event
	};

	// Ensure the member pointer/buffer to the eval's event_id is set in case
	// anything needs this to be in sync with event.event_id. This may be also
	// be used to onsider this event as currently being evaluated.
	const scope_restore<event::id> eval_event_id
	{
		eval.event_id,
		event.event_id?
			event.event_id:
			eval.event_id
	};

	return execute_du(eval, event);
}
catch(const vm::error &e)
{
	throw; // propagate from execute_du
}
catch(const m::error &e)
{
	const json::object &content
	{
		e.content
	};

	assert(eval.opts);
	return handle_error
	(
		*eval.opts, fault::GENERAL,
		"eval %s %s :%s :%s :%s",
		event.event_id?
			string_view{event.event_id}:
			"<unknown>"_sv,
		json::get<"room_id"_>(event)?
			string_view{json::get<"room_id"_>(event)}:
			"<unknown>"_sv,
		e.what(),
		json::string{content["errcode"]},
		json::string{content["error"]}
	);
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	assert(eval.opts);
	return handle_error
	(
		*eval.opts, fault::GENERAL,
		"eval %s %s (General Protection) :%s",
		event.event_id?
			string_view{event.event_id}:
			"<unknown>"_sv,
		json::get<"room_id"_>(event)?
			string_view{json::get<"room_id"_>(event)}:
			"<unknown>"_sv,
		e.what()
	);
}

ircd::m::vm::fault
ircd::m::vm::execute_du(eval &eval,
                        const event &event)
try
{
	assert(eval.id);
	assert(eval.ctx);
	assert(eval.opts);
	assert(eval.opts->edu || event.event_id);
	assert(eval.opts->edu || eval.event_id);
	assert(eval.event_id == event.event_id);
	assert(eval.event_);
	const auto &opts
	{
		*eval.opts
	};

	// The issue hook is only called when this server is injecting a newly
	// created event.
	if(eval.copts && eval.copts->issue)
		call_hook(issue_hook, eval, event, eval);

	// Branch on whether the event is an EDU or a PDU
	const fault ret
	{
		event.event_id?
			execute_pdu(eval, event):
			execute_edu(eval, event)
	};

	// ret can be a fault code if the user masked the exception from being
	// thrown. If there's an error code here nothing further is done.
	if(ret != fault::ACCEPT)
		return ret;

	if(opts.debuglog_accept || bool(log_accept_debug))
		log::debug
		{
			log, "%s", pretty_oneline(event)
		};

	// The event was executed; now we broadcast the good news. This will
	// include notifying client `/sync` and the federation sender.
	if(likely(opts.notify))
		call_hook(notify_hook, eval, event, eval);

	// The "effects" of the event are created by listeners on the effect hook.
	// These can include the creation of even more events, such as creating a
	// PDU out of an EDU, etc. Unlike the post_hook in execute_pdu(), the
	// notify for the event at issue here has already been made.
	if(likely(opts.effects))
		call_hook(effect_hook, eval, event, eval);

	if(opts.infolog_accept || bool(log_accept_info))
		log::info
		{
			log, "%s", pretty_oneline(event)
		};

	return ret;
}
catch(const vm::error &e) // VM FAULT CODE
{
	const json::object &content{e.content};
	const json::string &error
	{
		content["error"]
	};

	return handle_error
	(
		*eval.opts, e.code,
		"execute %s %s :%s",
		event.event_id?
			string_view{event.event_id}:
			"<edu>"_sv,
		eval.room_id?
			eval.room_id:
			"<edu>"_sv,
		error
	);
}
catch(const m::error &e) // GENERAL MATRIX ERROR
{
	const json::object &content{e.content};
	const json::string error[]
	{
		content["errcode"],
		content["error"]
	};

	return handle_error
	(
		*eval.opts, fault::GENERAL,
		"execute %s %s :%s :%s :%s",
		event.event_id?
			string_view{event.event_id}:
			"<edu>"_sv,
		eval.room_id?
			eval.room_id:
			"<edu>"_sv,
		e.what(),
		error[0],
		error[1]
	);
}
catch(const ctx::interrupted &e) // INTERRUPTION
{
	throw;
}
catch(const std::exception &e) // ALL OTHER ERRORS
{
	return handle_error
	(
		*eval.opts, fault::GENERAL,
		"execute %s %s (General Protection) :%s",
		event.event_id?
			string_view{event.event_id}:
			"<edu>"_sv,
		eval.room_id?
			eval.room_id:
			"<edu>"_sv,
		e.what()
	);
}

ircd::m::vm::fault
ircd::m::vm::execute_edu(eval &eval,
                         const event &event)
{
	if(likely(eval.opts->eval))
		call_hook(eval_hook, eval, event, eval);

	if(likely(eval.opts->post))
		call_hook(post_hook, eval, event, eval);

	return fault::ACCEPT;
}

ircd::m::vm::fault
ircd::m::vm::execute_pdu(eval &eval,
                         const event &event)
{
	const scope_count pending
	{
		sequence::pending
	};

	const scope_restore remove_txn
	{
		eval.txn, std::shared_ptr<db::txn>{}
	};

	const scope_notify sequence_dock
	{
		sequence::dock, scope_notify::all
	};

	assert(eval.opts);
	const auto &opts
	{
		*eval.opts
	};

	const m::event::id &event_id
	{
		event.event_id
	};

	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const string_view &type
	{
		at<"type"_>(event)
	};

	const bool authenticate
	{
		opts.auth && !eval.room_internal
	};

	// The conform hook runs static checks on an event's formatting and
	// composure; these checks only require the event data itself.
	if(likely(opts.conform))
	{
		const ctx::critical_assertion ca;
		call_hook(conform_hook, eval, event, eval);
	}

	if(unlikely(eval.room_internal && !my(event)))
		throw error
		{
			fault::GENERAL, "Internal room event denied from external source."
		};

	// Wait for any pending duplicate evals before proceeding.
	assert(eval::count(event_id));
	if(likely(opts.unique))
		sequence::dock.wait([&event_id]
		{
			return eval::count(event_id) <= 1;
		});

	if(likely(opts.unique) && unlikely(eval::count(event_id) > 1))
		throw error
		{
			fault::EXISTS, "Event is already being evaluated."
		};

	if(likely(!opts.replays) && m::exists(event_id))
		throw error
		{
			fault::EXISTS, "Event has already been evaluated."
		};

	if(likely(opts.access))
		call_hook(access_hook, eval, event, eval);

	if(likely(opts.verify) && !verify(event))
		throw m::BAD_SIGNATURE
		{
			"Signature verification failed"
		};

	// Fetch dependencies
	if(likely(opts.fetch))
		call_hook(fetch_hook, eval, event, eval);

	// Evaluation by auth system; throws
	if(likely(authenticate))
		room::auth::check_static(event);

	// Obtain sequence number here.
	const auto *const &top(eval::seqmax());
	eval.sequence_shared[0] = 0;
	eval.sequence_shared[1] = 0;
	eval.sequence = 0;
	eval.sequence =
	{
		top?
			std::max(sequence::get(*top) + 1, sequence::committed + 1):
			sequence::committed + 1
	};

	log::debug
	{
		log, "%s | event sequenced", loghead(eval)
	};

	// Wait until this is the lowest sequence number
	sequence::dock.wait([&eval]
	{
		return eval::seqnext(sequence::uncommitted) == &eval;
	});

	if(likely(authenticate))
		room::auth::check_relative(event);

	log::debug
	{
		log, "%s | event committing", loghead(eval)
	};

	assert(eval.sequence != 0);
	assert(sequence::uncommitted <= sequence::get(eval));
	assert(sequence::committed < sequence::get(eval));
	assert(sequence::retired < sequence::get(eval));
	assert(eval::sequnique(sequence::get(eval)));
	sequence::uncommitted = sequence::get(eval);

	// Wait until this is the lowest sequence number
	sequence::dock.wait([&eval]
	{
		return eval::seqnext(sequence::committed) == &eval;
	});

	// Reevaluation of auth against the present state of the room.
	if(likely(authenticate))
		room::auth::check_present(event);

	// Evaluation by module hooks
	if(likely(opts.eval))
		call_hook(eval_hook, eval, event, eval);

	log::debug
	{
		log, "%s | event committed", loghead(eval)
	};

	assert(sequence::committed < sequence::get(eval));
	assert(sequence::retired < sequence::get(eval));
	sequence::committed = sequence::get(eval);

	if(likely(opts.write))
		write_prepare(eval, event);

	if(likely(opts.write))
		write_append(eval, event);

	// Generate post-eval/pre-notify effects. This function may conduct
	// an entire eval of several more events recursively before returning.
	if(likely(opts.post))
		call_hook(post_hook, eval, event, eval);

	// Commit the transaction to database iff this eval is at the stack base.
	if(likely(opts.write) && !eval.sequence_shared[0])
		write_commit(eval);

	// Wait for sequencing only if this is the stack base, otherwise we'll
	// never return back to that stack base.
	if(likely(!eval.sequence_shared[0]))
	{
		sequence::dock.wait([&eval]
		{
			return eval::seqnext(sequence::retired) == &eval;
		});

		log::debug
		{
			log, "%s | retire %lu:%lu",
			loghead(eval),
			sequence::get(eval),
			eval.sequence_shared[1],
		};

		assert(sequence::retired < sequence::get(eval));
		sequence::retired = std::max(eval.sequence_shared[1], sequence::get(eval));
	}

	return fault::ACCEPT;
}

void
ircd::m::vm::write_prepare(eval &eval,
                           const event &event)

{
	assert(eval.opts);
	const auto &opts{*eval.opts};

	// Share a transaction with any other unretired evals on this stack. This
	// should mean the bottom-most/lowest-sequence eval on this ctx.
	const auto get_other_txn{[&eval]
	(auto &other)
	{
		if(&other == &eval)
			return true;

		if(!other.txn)
			return true;

		if(sequence::get(other) <= sequence::retired)
			return true;

		other.sequence_shared[1] = std::max(other.sequence_shared[1], sequence::get(eval));
		eval.sequence_shared[0] = sequence::get(other);
		eval.txn = other.txn;
		return false;
	}};

	// If we broke from the iteration then this eval is sharing a transaction
	// from another eval on this stack.
	if(!eval.for_each(eval.ctx, get_other_txn))
		return;

	eval.txn = std::make_shared<db::txn>
	(
		*dbs::events, db::txn::opts
		{
			calc_txn_reserve(opts, event),   // reserve_bytes
			0,                               // max_bytes (no max)
		}
	);
}

void
ircd::m::vm::write_append(eval &eval,
                          const event &event)

{
	assert(eval.opts);
	const auto &opts
	{
		*eval.opts
	};

	assert(eval.txn);
	auto &txn
	{
		*eval.txn
	};

	m::dbs::write_opts wopts(opts.wopts);
	wopts.event_idx = eval.sequence;
	wopts.json_source = opts.json_source;
	wopts.appendix.set(dbs::appendix::ROOM_STATE_SPACE, opts.history);

	// Don't update or resolve the room head with this shit.
	const bool dummy_event(json::get<"type"_>(event) == "org.matrix.dummy_event");
	wopts.appendix.set(dbs::appendix::ROOM_HEAD, opts.room_head && !dummy_event);
	wopts.appendix.set(dbs::appendix::ROOM_HEAD_RESOLVE, opts.room_head_resolve);

	if(opts.present && json::get<"state_key"_>(event))
	{
		const room room
		{
			at<"room_id"_>(event)
		};

		const room::state state
		{
			room
		};

		//XXX
		const auto pres_idx
		{
			state.get(std::nothrow, at<"type"_>(event), at<"state_key"_>(event))
		};

		//XXX
		int64_t pres_depth(0);
		const auto fresher
		{
			!pres_idx ||
			m::get<int64_t>(pres_idx, "depth", pres_depth) < json::get<"depth"_>(event)
		};

		if(fresher)
		{
			//XXX
			const auto &[pass, fail]
			{
				opts.auth && !eval.room_internal?
					room::auth::check(event, room):
					room::auth::passfail{true, {}}
			};

			if(fail)
				log::dwarning
				{
					log, "%s | fails auth for present state of %s :%s",
					loghead(eval),
					string_view{room.room_id},
					what(fail),
				};

			wopts.appendix.set(dbs::appendix::ROOM_STATE, pass);
			wopts.appendix.set(dbs::appendix::ROOM_JOINED, pass);
		}
	}

	dbs::write(*eval.txn, event, wopts);

	log::debug
	{
		log, "%s | write buffered",
		loghead(eval),
	};
}

void
ircd::m::vm::write_commit(eval &eval)
{
	assert(eval.txn);
	assert(eval.txn.use_count() == 1);
	assert(eval.sequence_shared[0] == 0);
	auto &txn
	{
		*eval.txn
	};

	#ifdef RB_DEBUG
	const auto db_seq_before(db::sequence(*m::dbs::events));
	#endif

	txn();

	#ifdef RB_DEBUG
	const auto db_seq_after(db::sequence(*m::dbs::events));

	log::debug
	{
		log, "%s | wrote  %lu:%lu | db seq %lu:%lu %zu cells in %zu bytes to events database ...",
		loghead(eval),
		sequence::get(eval),
		eval.sequence_shared[1],
		db_seq_before,
		db_seq_after,
		txn.size(),
		txn.bytes()
	};
	#endif
}

size_t
ircd::m::vm::calc_txn_reserve(const opts &opts,
                              const event &event)
{
	const size_t reserve_event
	{
		opts.reserve_bytes == size_t(-1)?
			size_t(json::serialized(event) * 1.66):
			opts.reserve_bytes
	};

	return reserve_event + opts.reserve_index;
}

template<class T>
void
ircd::m::vm::call_hook(hook::site<T> &hook,
                       eval &eval,
                       const event &event,
                       T&& data)
try
{
	#if 0
	log::debug
	{
		log, "%s | phase:%s enter",
		loghead(eval),
		hook.name(),
	};
	#endif

	assert(!eval.phase);
	const scope_restore hook_phase
	{
		eval.phase, std::addressof(hook)
	};

	hook(event, std::forward<T>(data));

	#if 0
	log::debug
	{
		log, "%s | phase:%s leave",
		loghead(eval),
		hook.name(),
	};
	#endif
}
catch(const m::error &e)
{
	log::derror
	{
		log, "%s | phase:%s :%s :%s :%s",
		loghead(eval),
		hook.name(),
		e.what(),
		e.errcode(),
		e.errstr(),
	};

	throw;
}
catch(const http::error &e)
{
	log::derror
	{
		log, "%s | phase:%s :%s :%s",
		loghead(eval),
		hook.name(),
		e.what(),
		e.content,
	};

	throw;
}
catch(const std::exception &e)
{
	log::derror
	{
		log, "%s | phase:%s :%s",
		loghead(eval),
		hook.name(),
		e.what(),
	};

	throw;
}

template<class... args>
ircd::m::vm::fault
ircd::m::vm::handle_error(const vm::opts &opts,
                          const vm::fault &code,
                          const string_view &fmt,
                          args&&... a)
{
	if(opts.errorlog & code)
		log::error
		{
			log, fmt, std::forward<args>(a)...
		};
	else if(~opts.warnlog & code)
		log::derror
		{
			log, fmt, std::forward<args>(a)...
		};

	if(opts.warnlog & code)
		log::warning
		{
			log, fmt, std::forward<args>(a)...
		};

	if(~opts.nothrows & code)
		throw error
		{
			code, fmt, std::forward<args>(a)...
		};

	return code;
}
