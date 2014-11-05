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

#ifndef PRIV2_GRAPH_H__
#define PRIV2_GRAPH_H__

#include "priv.h"
#include "priv2.h"
#include "priv2_generated.h"


#define MAX_SYSCALL_PARAMS 3

// Real IDs being used by the caller
typedef struct actual_pcred_ids {
    uids_t uids;
    gids_t gids;
} actual_ids_t;

// Normalized IDs from the graph
typedef struct normalized_pcred_ids {
    uids_t uids;
    uids_t gids; // Store gids as uids for lookup in state graph
} normalized_ids_t;

#define ID_MAP_SIZE MAX_NORMALIZED_IDS // From priv2_generated.h

// Mapping from real to normalized UIDs; ids-index + 1 is the normalized id;
// actual ids are stored in the ids array
typedef struct process_uids_map {
    unsigned counter;
    uid_t ids[ID_MAP_SIZE];
} uids_map_t;

// As above for groups; the graph is indexed by uid_t so the ids-index + 1 is
// converted to a uid_t as above, but the actual ids are gid_t for groups
typedef struct process_gids_map {
    unsigned counter;
    gid_t ids[ID_MAP_SIZE];
} gids_map_t;

// Normalized credentials with a copy of actual and normalized ids (as well as
// sups)
// NOTE: storing normalized ids is somewhat redundant, givan that we have a
// structure for mapping actual to normalized ids, but there are contexts where
// direct lookup in this structure makes things much more straightforward
typedef struct normalized_process_credentals {
    actual_ids_t actual;
    normalized_ids_t normalized;
    sups_t sups;
} norm_pcred_t;

// A pair of normalized identities for computing id state paths from one
// identity to another; this structure also stores id mappings
typedef struct normalized_process_credential_pair {
    norm_pcred_t prev;
    norm_pcred_t next;
    uids_map_t uids_map;
    gids_map_t gids_map;
} norm_pcred_pair_t;


// Get a pair of credentials that contains actual and normalized ids
norm_pcred_pair_t const get_normalized_pcred_pair(pcred_pair_t const* p);

// Test: Can this process go from current_uids to new_uids via a state with
// euid = super-user
bool suid_privilege_is_attainable(
    uids_t const current_uids, // Normalized previous (current) uids
    uids_t const new_uids);    // Normalized target (next) uids

// Get uids where euid = super-user such that target state in pair is still
// reachable
uids_t const get_suid_for_identity_change(norm_pcred_pair_t const* p);

// Test: Can we transition a process from previous state to next state within
// pair?
bool can_set_uids_from_graph(norm_pcred_pair_t const* p);
bool can_set_gids_from_graph(norm_pcred_pair_t const* p);


// Transition process previous state to next state within pair
// NOTE: Assumes this is possible without error
int set_uids_from_graph(norm_pcred_pair_t const* p);
int set_gids_from_graph(norm_pcred_pair_t const* p);

// Denormalize a list of ids
// NOTE: This is mostly used internally but must be exported for an edge case
// when privileges must be elevated for changing sups before transitioning to
// the target identity
void map_uids_params(
    uids_map_t const* map,
    unsigned const num_params,
    uid_t const* normalized_params,
    uid_t* actual_params);
void map_gids_params(
    gids_map_t const* map,
    unsigned const num_params,
    uid_t const* normalized_params,
    gid_t* actual_params);

#endif /* PRIV2_GRAPH_H__ */

//##############################################################################
//                                   EOF
//##############################################################################
