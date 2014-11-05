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
// priv2_graph.h
//     Based on the work of Dan Tsafrir and David Wagner
//     Subsequent work by Mark Dittmer
//
// See also
//     http://code.google.com/p/change-process-identity
//     http://www.usenix.org/publications/login/2008-06/pdfs/tsafrir.pdf
//##############################################################################

#ifndef PRIV2_DEBUG_H__
#define PRIV2_DEBUG_H__


#ifdef DEBUG

#include <stdio.h>

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 5
#endif

// DebugDo
#define DD(code) code
// DebugDo-level-N
#define DDN(level, code) if (level <= DEBUG_LEVEL) { code }

// DebugPrint
#define DP(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
// DebugPrintString
#define DPS(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
// DebugPrint-level-N
#define DPN(level, fmt, ...) if (level <= DEBUG_LEVEL)  \
        fprintf(stderr, fmt "\n", ##__VA_ARGS__)
// DebugPrintString-level-N
#define DPSN(level, fmt, ...) if (level <= DEBUG_LEVEL)  \
        fprintf(stderr, fmt, ##__VA_ARGS__)

#else

#ifdef DEBUG_LEVEL
#undef DEBUG_LEVEL
#define DEBUG_LEVEL -1
#endif


#define DD(...) do {} while(0) /* DebugDo */
#define DDN(...) do {} while(0) /* DebugDo-level-N */
#define DP(...) do {} while(0) /* DebugPrint */
#define DPS(...) do {} while(0) /* DebugPrintString */
#define DPN(...) do {} while(0) /* DebugPrint-level-N */
#define DPSN(...) do {} while(0) /* DebugPrintString-level-N */

#endif


#endif /* PRIV2_DEBUG_H__ */

//##############################################################################
//                                   EOF
//##############################################################################
