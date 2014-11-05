// Copyright (c) 2014 Mark S. Dittmer
//
// This file is part of Priv2.
//
// Priv2 is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Priv2 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Priv2.  If not, see <http://www.gnu.org/licenses/>.
//##############################################################################
// priv2_setuid.h
//     Based on the work of Dan Tsafrir and David Wagner
//     Subsequent work by Mark Dittmer
//
// See also
//     http://code.google.com/p/change-process-identity
//     http://www.usenix.org/publications/login/2008-06/pdfs/tsafrir.pdf
//##############################################################################

#ifndef PRIV2_SETUID_H__
#define PRIV2_SETUID_H__

#include "priv2.h"


// Permanently change identity of a process to that of the given user.
int change_uid_permanently(uid_t const uid);


// Switch to the given user, but maintain the ability to regain the
// current effective identity.
int change_uid_temporarily(uid_t const uid);

#ifdef MULTITHREADED
// Must be called exactly once before any other change_uid...() call
int change_uid_pthread_setup();

// Should be called exactly once after all other change_uid...() calls
int change_uid_pthread_teardown();
#endif

#endif /* PRIV2_H__ */

//##############################################################################
//                                   EOF
//##############################################################################
