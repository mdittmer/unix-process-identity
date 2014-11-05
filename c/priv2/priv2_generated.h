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
#ifdef __linux__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

/** Include **/

#include <sys/types.h>

/** Constants **/

#define MAX_NORMALIZED_IDS 6
#define NEG_ONE_IS_SUPPORTED 0

/** Globals **/

extern uid_t const** adjacency_matrix[343][343];
extern unsigned const predecessor_matrix[343][343];
extern uid_t const* effective_privileged_states[50];
extern uid_t const* priv_jumps[343];

/** Functions **/

int state_idx_lookup(uid_t const ruid, uid_t const euid, uid_t const svuid);
unsigned const get_num_function_params(uid_t const call);
int execute_uids_function(uid_t const  call, uid_t const* params);
int execute_gids_function(uid_t const  call, uid_t const* params);
