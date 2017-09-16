/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once
#define HAVE_IRCD_DB_DELTA_H

namespace ircd::db
{
	enum op
	{
		GET,                     // no-op sentinel, do not use (debug asserts)
		SET,                     // (batch.Put)
		MERGE,                   // (batch.Merge)
		DELETE,                  // (batch.Delete)
		DELETE_RANGE,            // (batch.DeleteRange)
		SINGLE_DELETE,           // (batch.SingleDelete)
	};

	struct delta;

	// Indicates an op uses both a key and value for its operation. Some only use
	// a key name so an empty value argument in a delta is okay when false.
	bool value_required(const op &op);
}

/// Update a database cell without `cell`, `column` or row `references`.
///
/// The cell is found by name string. This is the least efficient of the deltas
/// for many updates to the same column or cell when a reference to those can
/// be pre-resolved. This delta has to resolve those references every single
/// time it's iterated over; but that's okay for some transactions.
///
struct ircd::db::delta
:std::tuple<op, string_view, string_view, string_view>
{
	delta(const string_view &col, const string_view &key, const string_view &val = {}, const enum op &op = op::SET)
	:std::tuple<enum op, string_view, string_view, string_view>{op, col, key, val}
	{}

	delta(const enum op &op, const string_view &col, const string_view &key, const string_view &val = {})
	:std::tuple<enum op, string_view, string_view, string_view>{op, col, key, val}
	{}

	delta() = default;
};
