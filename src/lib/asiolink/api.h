// Copyright (C) 2015 Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#ifndef ASIOLINK_API_H
#define ASIOLINK_API_H 1

#include <config.h>

#ifndef _WIN32
#ifndef HAVE_ATTRIBUTE_VISIBILITY
#define KEA_ASIOLINK_API
#else
#define KEA_ASIOLINK_API __attribute__((visibility("default")))
#endif
#else
#ifdef KEA_ASIOLINK_EXPORT
#define KEA_ASIOLINK_API __declspec(dllexport)
#else
#define KEA_ASIOLINK_API __declspec(dllimport)
#endif
#endif

#endif // ASIOLINK_API_H

// Local Variables: 
// mode: c++
// End: 
