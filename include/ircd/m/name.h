// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_NAME_H

/// All strings used for json::tuple keys in ircd::m (matrix protocol) are
/// contained within this namespace. These strings allow constant time access
/// to JSON in a tuple using recursive constexpr function inlining provided by
/// ircd::json. There can't be any duplicates, so they're all aggregated here.
///
namespace ircd::m::name
{
	constexpr const char *const auth_events {"auth_events"};
	constexpr const char *const content {"content"};
	constexpr const char *const depth {"depth"};
	constexpr const char *const destination {"destination"};
	constexpr const char *const event_id {"event_id"};
	constexpr const char *const hashes {"hashes"};
	constexpr const char *const membership {"membership"};
	constexpr const char *const method {"method"};
	constexpr const char *const origin {"origin"};
	constexpr const char *const origin_server_ts {"origin_server_ts"};
	constexpr const char *const prev_events {"prev_events"};
	constexpr const char *const prev_state {"prev_state"};
	constexpr const char *const room_id {"room_id"};
	constexpr const char *const sender {"sender"};
	constexpr const char *const signatures {"signatures"};
	constexpr const char *const state_key {"state_key"};
	constexpr const char *const type {"type"};
	constexpr const char *const unsigned_ {"unsigned"};
	constexpr const char *const uri {"uri"};
}
