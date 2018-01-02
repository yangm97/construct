/*
 * Copyright (C) 2017 Matrix Construct Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
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

///////////////////////////////////////////////////////////////////////////////
//
// IRCd's namespacing is as follows:
//
// IRCD_     #define and macro namespace
// RB_       #define and macro namespace (legacy, low-level)
// ircd_     C namespace and demangled bindings
// ircd::    C++ namespace
//

///////////////////////////////////////////////////////////////////////////////
//
// Standard includes
//
// This header includes almost everything we use out of the standard library.
// This is a pre-compiled header. Project build time is significantly reduced
// by doing things this way and C++ std headers have very little namespace
// pollution and risk of conflicts.
//
// * If any project header file requires standard library symbols we try to
//   list it here, not in our header files.
//
// * Rare one-off's #includes isolated to a specific .cc file may not always be
//   listed here but can be.
//
// * Third party / dependency / non-std includes, are NEVER listed here.
//   Instead we include those in .cc files, and use forward declarations if
//   we require a symbol in our API to them.
//

#define HAVE_IRCD_STDINC_H

// Generated by ./configure
#include "config.h"

extern "C" {

#include <RB_INC_ASSERT_H
#include <RB_INC_STDARG_H
#include <RB_INC_SYS_TIME_H
#include <RB_INC_SYS_RESOURCE_H

} // extern "C"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <RB_INC_WINDOWS_H
#include <RB_INC_WINSOCK2_H
#include <RB_INC_WS2TCPIP_H
#include <RB_INC_IPHLPAPI_H
#endif

#include <RB_INC_CSTDDEF
#include <RB_INC_CSTDINT
#include <RB_INC_LIMITS
#include <RB_INC_TYPE_TRAITS
#include <RB_INC_TYPEINDEX
#include <RB_INC_VARIANT
#include <RB_INC_CERRNO
#include <RB_INC_UTILITY
#include <RB_INC_FUNCTIONAL
#include <RB_INC_ALGORITHM
#include <RB_INC_NUMERIC
#include <RB_INC_CMATH
#include <RB_INC_MEMORY
#include <RB_INC_EXCEPTION
#include <RB_INC_SYSTEM_ERROR
#include <RB_INC_ARRAY
#include <RB_INC_VECTOR
#include <RB_INC_STACK
#include <RB_INC_STRING
#include <RB_INC_CSTRING
#include <RB_INC_STRING_VIEW
#include <RB_INC_LOCALE
#include <RB_INC_CODECVT
#include <RB_INC_MAP
#include <RB_INC_SET
#include <RB_INC_LIST
#include <RB_INC_FORWARD_LIST
#include <RB_INC_UNORDERED_MAP
#include <RB_INC_DEQUE
#include <RB_INC_QUEUE
#include <RB_INC_SSTREAM
#include <RB_INC_FSTREAM
#include <RB_INC_IOSTREAM
#include <RB_INC_IOMANIP
#include <RB_INC_CSTDIO
#include <RB_INC_CHRONO
#include <RB_INC_CTIME
#include <RB_INC_ATOMIC
#include <RB_INC_THREAD
#include <RB_INC_MUTEX
#include <RB_INC_SHARED_MUTEX
#include <RB_INC_CONDITION_VARIABLE
#include <RB_INC_RANDOM
#include <RB_INC_BITSET
#include <RB_INC_OPTIONAL

#include <RB_INC_EXPERIMENTAL_STRING_VIEW
#include <RB_INC_EXPERIMENTAL_OPTIONAL

///////////////////////////////////////////////////////////////////////////////
//
// Pollution
//
// This section lists all of the items introduced outside of our namespace
// which may conflict with your project.
//

// Common branch prediction macros
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

// Legacy attribute format printf macros
#define AFP(a, b)       __attribute__((format(printf, a, b)))
#define AFGP(a, b)      __attribute__((format(gnu_printf, a, b)))

// Experimental std::string_view
#if __cplusplus <= 201703 //TODO: refine
namespace std
{
	using experimental::string_view;
}
#endif

// Experimental std::optional
#if __cplusplus <= 20170519 //TODO: refine
namespace std
{
	using experimental::optional;
}
#endif

// OpenSSL
// Additional forward declarations in the extern namespace are introduced
// by ircd/openssl.h

///////////////////////////////////////////////////////////////////////////////
//
// libircd API
//

// Some items imported into our namespace.
namespace ircd
{
	using std::nullptr_t;
	using std::begin;
	using std::end;
	using std::get;
	using std::static_pointer_cast;
	using std::dynamic_pointer_cast;
	using std::const_pointer_cast;
	using ostream = std::ostream;
	namespace ph = std::placeholders;
	using namespace std::string_literals;
	using namespace std::literals::chrono_literals;
	template<class... T> using ilist = std::initializer_list<T...>;

	using int128_t = signed __int128;
	using uint128_t = unsigned __int128;
}

namespace ircd
{
	enum class runlevel :int;
	using runlevel_handler = std::function<void (const enum runlevel &)>;

	constexpr size_t BUFSIZE { 512 };
	extern const enum runlevel &runlevel;
	extern const std::string &conf;
	extern bool debugmode; ///< Toggle; available only ifdef RB_DEBUG

	std::string demangle(const std::string &symbol);
	template<class T> std::string demangle();
}

#include "util.h"
#include "exception.h"
#include "string_view.h"
#include "vector_view.h"
#include "array_view.h"
#include "byte_view.h"
#include "tuple.h"
#include "allocator.h"
#include "buffer.h"
#include "date.h"
#include "timer.h"
#include "logger.h"
#include "nacl.h"
#include "rand.h"
#include "hash.h"
#include "ed25519.h"
#include "info.h"
#include "localee.h"
#include "life_guard.h"
#include "color.h"
#include "lex_cast.h"
#include "stringops.h"
#include "tokens.h"
#include "params.h"
#include "iov.h"
#include "parse.h"
#include "rfc1459.h"
#include "json/json.h"
#include "openssl.h"
#include "http.h"
#include "fmt.h"
#include "fs.h"
#include "ios.h"
#include "ctx/ctx.h"
#include "db/db.h"
#include "js.h"
#include "mods.h"
#include "rfc3986.h"
#include "net/net.h"
#include "server.h"
#include "client.h"
#include "resource.h"
