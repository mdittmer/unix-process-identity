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
#pragma once

#if defined(__sun) && defined(__SVR4)
#define IS_SOLARIS 1
#else
#define IS_SOLARIS 0
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__)
#define IS_BSD 1
#else
#define IS_BSD 0
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__) || (defined(__APPLE__) && defined(__MACH__))
#define IS_BSD_LIKE 1
#else
#define IS_BSD_LIKE 0
#endif

#ifdef __linux__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define IS_LINUX 1

#else

#define IS_LINUX 0

#endif


// If DEBUG_SETRESUID is defined, then HAS_SETRESUID should be set manually
#ifndef DEBUG_SETRESUID

#if !(defined(__APPLE__) && defined(__MACH__))
#define HAS_SETRESUID 1
#else
#define HAS_SETRESUID 0
#endif

#endif

#define WITH_SETRESUID if (HAS_SETRESUID)

// NOTE: Solaris "has setresuid" insofar as it can be simulated with the /proc
// filesystem
#if IS_SOLARIS

#include <unistd.h>

int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
int setresuid(uid_t ruid, uid_t euid, uid_t suid);

#endif
