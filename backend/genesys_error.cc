/* sane - Scanner Access Now Easy.

   Copyright (C) 2019 Povilas Kanapickas <povilas@radix.lt>

   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.
*/

#define DEBUG_DECLARE_ONLY

#include "genesys_error.h"

#if (defined(__GNUC__) || defined(__CLANG__)) && (defined(__linux__) || defined(__APPLE__))
extern "C" char* __cxa_get_globals();
#endif

static unsigned num_uncaught_exceptions()
{
#if __cplusplus >= 201703L
    int count = std::uncaught_exceptions();
    return count >= 0 ? count : 0;
#elif (defined(__GNUC__) || defined(__CLANG__)) && (defined(__linux__) || defined(__APPLE__))
    // the format of the __cxa_eh_globals struct is enshrined into the Itanium C++ ABI and it's
    // very unlikely we'll get issues referencing it directly
    char* cxa_eh_globals_ptr = __cxa_get_globals();
    return *reinterpret_cast<unsigned*>(cxa_eh_globals_ptr + sizeof(void*));
#else
    return std::uncaught_exception() ? 1 : 0;
#endif
}

DebugMessageHelper::DebugMessageHelper(const char* func)
{
    func_ = func;
    num_exceptions_on_enter_ = num_uncaught_exceptions();
    DBG(DBG_proc, "%s: start", func_);
}

DebugMessageHelper::~DebugMessageHelper()
{
    if (num_exceptions_on_enter_ < num_uncaught_exceptions()) {
        if (status_) {
            DBG(DBG_error, "%s: failed during %s", func_, status_);
        } else {
            DBG(DBG_error, "%s: failed", func_);
        }
    } else {
        DBG(DBG_proc, "%s: completed", func_);
    }
}
