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
// priv2_graph.c
//     Securely change process identity
//     Based on the work of Dan Tsafrir and David Wagner
//     Subsequent work by Mark Dittmer
//
// See also
//     http://code.google.com/p/change-process-identity
//     http://www.usenix.org/publications/login/2008-06/pdfs/tsafrir.pdf
//##############################################################################

//##############################################################################
// Include
//##############################################################################
#include "priv2_graph.h"

#include "debug.h"
#include "priv.h"
#include "priv2.h"
#include "priv2_generated.h"

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>

//##############################################################################
// Macros
//##############################################################################
#define NON_NEGATIVE_OR_FALSE(num) if (num < 0) { return false; }
#define NON_NULL_OR_FALSE(ptr) if (ptr == NULL) { return false; }
#define NON_NULL_OR_EINVAL(ptr) if (ptr == NULL) { errno = EINVAL; return -1; }
#define TRUE_OR_EINVAL(cond) if (!cond) { errno = EINVAL; return -1; }
#define TRUE_OR_EPERM(cond) if (!cond) { errno = EPERM; return -1; }
#define ZERO_OR_RTN_ERROR(num) if (num != 0) { return -1; }


//##############################################################################
// Internal data types
//##############################################################################
typedef struct uids_pair {
    uids_t prev;
    uids_t next;
} uids_pair_t;

typedef struct uids_normalization_data {
    uids_pair_t pair;
    uids_map_t map;
} uids_norm_data_t;

typedef struct gids_normalization_data {
    uids_pair_t pair; // Normalized ids are uid_t, even if actual ids are gid_t
    gids_map_t map;
} gids_norm_data_t;


//##############################################################################
// Internal prototypes
//##############################################################################
static bool can_set_suid_for_identity_change(
    uids_t const* current_uids, // Normalized previous (current) uids
    uids_t const* new_uids);    // Normalized target (next) uids

#define CAN_SET_IDS_FROM_GRAPH_DECL(id_name)            \
    static bool can_set_ ## id_name ## _from_graph_(    \
        uids_t const* first_ids,                        \
        uids_t const* second_ids)
CAN_SET_IDS_FROM_GRAPH_DECL(uids);
CAN_SET_IDS_FROM_GRAPH_DECL(gids);

#define CAN_SET_IDS_FROM_GRAPH_RECURSIVE_DECL(id_name)          \
    static bool can_set_ ## id_name ## _from_graph_recursive(   \
        uid_t const* predecessor_list,                          \
        int const first_idx,                                    \
        int const next_idx)
CAN_SET_IDS_FROM_GRAPH_RECURSIVE_DECL(uids);
CAN_SET_IDS_FROM_GRAPH_RECURSIVE_DECL(gids);

#define SET_IDS_FROM_GRAPH_RECURSIVE_DECL(id_name)              \
    static int set_ ## id_name ## _from_graph_recursive(        \
        norm_pcred_pair_t const* p,                             \
        uid_t const* predecessor_list,                          \
        int const first_idx,                                    \
        int const next_idx)
SET_IDS_FROM_GRAPH_RECURSIVE_DECL(uids);
SET_IDS_FROM_GRAPH_RECURSIVE_DECL(gids);

static uids_norm_data_t const normalize_uids(pcred_pair_t const* p);
static gids_norm_data_t const normalize_gids(
    uids_t const* normalized_current_uids,
    uids_t const* normalized_target_uids,
    pcred_pair_t const* p);

// NOTE: plural xids below is a misnomer; one id is normalized
static uid_t normalize_single_uids(uids_map_t* map, uid_t const id);
static uid_t normalize_single_gids(
    gids_map_t* map,
    gid_t id);

static int get_ids_idx(uids_t const* uids);
static uids_t const get_ids_from_array(uid_t const* arr);

#define SET_IDS_FROM_IDX_DECL(id_name) \
    static int set_ ## id_name ## _from_idx(    \
        norm_pcred_pair_t const* p,             \
        unsigned const current_idx,             \
        unsigned const next_idx)
SET_IDS_FROM_IDX_DECL(uids);
SET_IDS_FROM_IDX_DECL(gids);

#define EXECUTE_ID_FUNCTION_FROM_ARRAY_DECL(id_name) \
    static int execute_ ## id_name ## _function_from_array( \
        norm_pcred_pair_t const* p, \
        uid_t const* fn_array)
EXECUTE_ID_FUNCTION_FROM_ARRAY_DECL(uids);
EXECUTE_ID_FUNCTION_FROM_ARRAY_DECL(gids);

#ifdef DEBUG

#define print_uids_map(lvl, map) if (lvl <= DEBUG_LEVEL) {      \
        print_uids_map_(map);                                   \
    }
#define print_gids_map(lvl, map) if (lvl <= DEBUG_LEVEL) {      \
        print_gids_map_(map);                                   \
    }

static void print_uids_map_(uids_map_t const* map);
static void print_gids_map_(gids_map_t const* map);

#else

#define print_uids_map(...) do {} while(0)
#define print_gids_map(...) do {} while(0)

#endif


//##############################################################################
// Exported functions
//##############################################################################
norm_pcred_pair_t const get_normalized_pcred_pair(pcred_pair_t const* p) {
    norm_pcred_pair_t new_p;

    new_p.prev.actual.uids = p->prev.uids;
    new_p.next.actual.uids = p->next.uids;

    new_p.prev.actual.gids = p->prev.gids;
    new_p.next.actual.gids = p->next.gids;

    new_p.prev.sups = p->prev.sups;
    new_p.next.sups = p->next.sups;

    uids_norm_data_t const uids_norm_data = normalize_uids(p);
    new_p.prev.normalized.uids = uids_norm_data.pair.prev;
    new_p.next.normalized.uids = uids_norm_data.pair.next;
    new_p.uids_map = uids_norm_data.map;

    gids_norm_data_t const gids_norm_data = normalize_gids(
        &new_p.prev.normalized.uids,
        &new_p.prev.normalized.uids,
        p);
    new_p.prev.normalized.gids = gids_norm_data.pair.prev;
    new_p.next.normalized.gids = gids_norm_data.pair.next;
    new_p.gids_map = gids_norm_data.map;

    return new_p;
}

bool suid_privilege_is_attainable(
    uids_t const current_uids, // Normalized previous (current) uids
    uids_t const new_uids) {   // Normalized target (next) uids
    // HACK: Store a cache of size one so that when this is invoked multiple
    // times on the same pair, the cached value is returned. This works because
    // this value may be needed multiple times on the same pair pre-state-change
    // checks (and no other pairs are checked in the meantime).
    static bool cached = false;
    static uids_t cached_cu;
    static uids_t cached_nu;
    static bool cached_rtn = false;

    uids_t const* const c1 = &current_uids;
    uids_t const* const c2 = &cached_cu;
    uids_t const* const n1 = &new_uids;
    uids_t const* const n2 = &cached_nu;
    if (cached &&
        c1->r == c2->r && c1->e == c2->e && c1->s == c2->s &&
        n1->r == n2->r && n1->e == n2->e && n1->s == n2->s) {
        return cached_rtn;
    }

    cached_cu = current_uids;
    cached_nu = new_uids;
    cached_rtn = can_set_suid_for_identity_change(&current_uids, &new_uids);
    cached = true;

    return cached_rtn;
}

uids_t const get_suid_for_identity_change(norm_pcred_pair_t const* p) {
    uids_t const* current_uids = &p->prev.normalized.uids;
    uids_t const* new_uids = &p->next.normalized.uids;
    uid_t const** eps = effective_privileged_states;
    unsigned i;
    for (i = 0; eps[i] != NULL; ++i) {
        uids_t priv_uids = get_ids_from_array(eps[i]);
        if (can_set_uids_from_graph_(current_uids, &priv_uids) &&
            can_set_uids_from_graph_(&priv_uids, new_uids)) {
            return priv_uids;
        }
    }

    // This code should be unreachable: check can_set_suid_for_identity_change()
    // first!
    assert(false);

    // This is not a currect return, but this code should be unreachable
    return *current_uids;
}

#define CAN_SET_IDS_FROM_GRAPH_DEFN(id_name)                            \
    bool can_set_ ## id_name ## _from_graph(                            \
        norm_pcred_pair_t const* p) {                                   \
        return can_set_ ## id_name ## _from_graph_(                     \
            &p->prev.normalized. id_name ,                              \
            &p->next.normalized. id_name);                              \
    }                                                                   \
    static bool can_set_ ## id_name ## _from_graph_(                    \
        uids_t const* first_ids,                                        \
        uids_t const* second_ids) {                                     \
        int const first_idx = get_ids_idx(first_ids);                   \
        int const second_idx = get_ids_idx(second_ids);                 \
                                                                        \
        bool rtn;                                                       \
        if (first_idx < 0 || second_idx < 0) {                          \
            rtn = false;                                                \
        } else {                                                        \
            uid_t const* predecessor_list = predecessor_matrix[first_idx]; \
            rtn = can_set_ ## id_name ## _from_graph_recursive(         \
                predecessor_list,                                       \
                first_idx,                                              \
                second_idx);                                            \
        }                                                               \
        DPN(3, "Can set " #id_name " return %d; normalized values: <%d, %d, %d> to <%d, %d, %d>", \
            rtn,                                                        \
            first_ids->r,                                               \
            first_ids->e,                                               \
            first_ids->s,                                               \
            second_ids->r,                                              \
            second_ids->e,                                              \
            second_ids->s);                                             \
        return rtn;                                                     \
    }

    /*     int prev_idx = first_idx, next_idx;                             \ */
    /*     while (next_idx != second_idx) {                                \ */
    /*         next_idx = predecessor_list[prev_idx];                      \ */
    /*         if (next_idx == first_idx || next_idx == prev_idx) {        \ */
    /*             return false;                                           \ */
    /*         }                                                           \ */
    /*         prev_idx = next_idx;                                        \ */
    /*     }                                                               \ */
    /*     return true;                                                    \ */
    /* } */
CAN_SET_IDS_FROM_GRAPH_DEFN(uids);
CAN_SET_IDS_FROM_GRAPH_DEFN(gids);

#define CAN_SET_IDS_FROM_GRAPH_RECURSIVE_DEFN(id_name)          \
    static bool can_set_ ## id_name ## _from_graph_recursive(   \
        uid_t const* predecessor_list,                          \
        int const first_idx,                                    \
        int const next_idx) {                                   \
        if(next_idx < 0) {                                      \
            return false;                                       \
        }                                                       \
        if (next_idx == first_idx) {                            \
            return true;                                        \
        }                                                       \
        int const prev_idx = predecessor_list[next_idx];        \
        if (prev_idx == next_idx) {                             \
            return false;                                       \
        }                                                       \
        return can_set_ ## id_name ## _from_graph_recursive(    \
            predecessor_list,                                   \
            first_idx,                                          \
            prev_idx);                                          \
    }
CAN_SET_IDS_FROM_GRAPH_RECURSIVE_DEFN(uids);
CAN_SET_IDS_FROM_GRAPH_RECURSIVE_DEFN(gids);

#define SET_IDS_FROM_GRAPH_DEFN(id_name)                                \
    int set_ ## id_name ## _from_graph(                                 \
        norm_pcred_pair_t const* p) {                                   \
        uids_t const* current_ids = &p->prev.normalized. id_name ;      \
        uids_t const* new_ids = &p->next.normalized. id_name ;          \
                                                                        \
        int const current_idx = get_ids_idx(current_ids);               \
        int const new_idx = get_ids_idx(new_ids);                       \
                                                                        \
        DD(if(!(current_idx >= 0 && new_idx >= 0)) {                    \
                DPN(3, "Set " #id_name " failed; normalized values: <%d, %d, %d> to <%d, %d, %d>", \
                    p->prev.normalized. id_name .r,                     \
                    p->prev.normalized. id_name .e,                     \
                    p->prev.normalized. id_name .s,                     \
                    p->next.normalized. id_name .r,                     \
                    p->next.normalized. id_name .e,                     \
                    p->next.normalized. id_name .s);                    \
            });                                                         \
        assert(current_idx >= 0 && new_idx >= 0);                       \
                                                                        \
        DPN(3, "Initial normalized: %d, %d, %d :: idx %d",              \
            current_ids->r,                                             \
            current_ids->e,                                             \
            current_ids->s,                                             \
            current_idx);                                               \
        DPN(3, "Target normalized:  %d, %d, %d :: idx %d",              \
            new_ids->r,                                                 \
            new_ids->e,                                                 \
            new_ids->s,                                                 \
            new_idx);                                                   \
                                                                        \
        uid_t const* predecessor_list =                                 \
            predecessor_matrix[current_idx];                            \
        assert(set_ ## id_name ## _from_graph_recursive(                \
                   p,                                                   \
                   predecessor_list,                                    \
                   current_idx,                                         \
                   new_idx) == 0);                                      \
                                                                        \
        return 0;                                                       \
    }
SET_IDS_FROM_GRAPH_DEFN(uids);
SET_IDS_FROM_GRAPH_DEFN(gids);

#define SET_IDS_FROM_GRAPH_RECURSIVE_DEFN(id_name)              \
    static int set_ ## id_name ## _from_graph_recursive(        \
        norm_pcred_pair_t const* p,                             \
        uid_t const* predecessor_list,                          \
        int const first_idx,                                    \
        int const next_idx) {                                   \
        assert(next_idx >= 0);                                  \
        if (next_idx == first_idx) {                            \
            return 0;                                           \
        }                                                       \
        int const prev_idx = predecessor_list[next_idx];        \
        DPN(3, "Current actual uids: <%d, %d, %d>",             \
            p->prev.actual.uids.r,                              \
            p->prev.actual.uids.e,                              \
            p->prev.actual.uids.s);                             \
        DPN(3, "Current normalized uids: <%d, %d, %d>",         \
            p->prev.normalized.uids.r,                          \
            p->prev.normalized.uids.e,                          \
            p->prev.normalized.uids.s);                         \
        DPN(3, "Current actual gids: <%d, %d, %d>",              \
            p->prev.actual.gids.r,                              \
            p->prev.actual.gids.e,                              \
            p->prev.actual.gids.s);                             \
        DPN(3, "Current normalized gids: <%d, %d, %d>",          \
            p->prev.normalized.gids.r,                          \
            p->prev.normalized.gids.e,                          \
            p->prev.normalized.gids.s);                         \
        DPN(3, "Target actual uids: <%d, %d, %d>",             \
            p->next.actual.uids.r,                              \
            p->next.actual.uids.e,                              \
            p->next.actual.uids.s);                             \
        DPN(3, "Target normalized uids: <%d, %d, %d>",         \
            p->next.normalized.uids.r,                          \
            p->next.normalized.uids.e,                          \
            p->next.normalized.uids.s);                         \
        DPN(3, "Target actual gids: <%d, %d, %d>",              \
            p->next.actual.gids.r,                              \
            p->next.actual.gids.e,                              \
            p->next.actual.gids.s);                             \
        DPN(3, "Target normalized gids: <%d, %d, %d>",          \
            p->next.normalized.gids.r,                          \
            p->next.normalized.gids.e,                          \
            p->next.normalized.gids.s);                         \
        print_ ## id_name ## _map(3, &p-> id_name ## _map);     \
        assert(prev_idx != next_idx);                           \
        assert(set_ ## id_name ## _from_graph_recursive(        \
                   p,                                           \
                   predecessor_list,                            \
                   first_idx,                                   \
                   prev_idx) == 0);                             \
        assert(set_ ## id_name ## _from_idx(                    \
                   p,                                           \
                   prev_idx,                                    \
                   next_idx) == 0);                             \
                                                                \
        return 0;                                               \
    }
SET_IDS_FROM_GRAPH_RECURSIVE_DEFN(uids);
SET_IDS_FROM_GRAPH_RECURSIVE_DEFN(gids);

// Use recursion to stack up predecessors, then make function calls
// to successors

/* static int set_uids_from_graph_recursive( */
/*     norm_pcred_pair_t const* p, */
/*     uid_t const* predecessor_list, */
/*     int const first_idx, */
/*     int const next_idx) { */
/*     assert(next_idx >= 0); */
/*     if (next_idx == first_idx) { */
/*         return 0; */
/*     } */
/*     int const prev_idx = predecessor_list[next_idx]; */

/*     // Assert correctness because set_uids_from_graph() should have been called */
/*     // first */
/*     assert(prev_idx != next_idx); */
/*     assert(set_uids_from_graph_recursive( */
/*                p, */
/*                predecessor_list, */
/*                first_idx, */
/*                prev_idx) == 0); */
/*     assert(set_uids_from_idx( */
/*                p, */
/*                prev_idx, */
/*                next_idx) == 0); */

/*     return 0; */
/* } */

/* static int set_gids_from_graph_recursive( */
/*     norm_pcred_pair_t const* p, */
/*     uid_t const* predecessor_list, */
/*     int const first_idx, */
/*     int const next_idx) { */
/*     assert(next_idx >= 0); */
/*     if (next_idx == first_idx) { */
/*         return 0; */
/*     } */
/*     int const prev_idx = predecessor_list[next_idx]; */

/*     // We cannot assert correctness because there is no guarantee that the uid */
/*     // graph can be used as a gid graph (with 0 normalized to an unprivileged */
/*     // id) */
/*     if (!(prev_idx != next_idx) || */
/*         !(set_gids_from_graph_recursive( */
/*               p, */
/*               predecessor_list, */
/*               first_idx, */
/*               prev_idx) == 0) || */
/*         !(set_gids_from_idx( */
/*               p, */
/*               prev_idx, */
/*               next_idx) == 0)) { */
/*         return -1; */
/*     } */

/*     return 0; */
/* } */



//##############################################################################
// Internal functions
//##############################################################################
static bool can_set_suid_for_identity_change(
    uids_t const* current_uids, // Normalized previous (current) uids
    uids_t const* new_uids) {   // Normalized target (next) uids
    uid_t const** eps = effective_privileged_states;
    unsigned i;
    for (i = 0; eps[i] != NULL; ++i) {
        uids_t priv_uids = get_ids_from_array(eps[i]);
        if (can_set_uids_from_graph_(current_uids, &priv_uids) &&
            can_set_uids_from_graph_(&priv_uids, new_uids)) {
            return true;
        }
    }
    return false;
}

#define NEW_IDS_NORM_DEFN(id_name, id_type)                     \
    static id_name ## _map_t new_ ## id_name ## _map() {        \
        id_name ## _map_t m;                                    \
        m.counter = 0;                                          \
        m.ids[0] = m.ids[1] = m.ids[2] = (id_type)(-1);         \
        return m;                                               \
    }
NEW_IDS_NORM_DEFN(uids, uid_t);
NEW_IDS_NORM_DEFN(gids, gid_t);

/* #define NORMALIZE_IDS_DEFN(id_name)                                     \ */
/*     static id_name ## _norm_data_t const normalize_ ## id_name (        \ */
/*         pcred_pair_t const* p) {                                        \ */
/*         id_name ## _map_t map = new_ ## id_name ## _map();              \ */
/*                                                                         \ */
/*         uids_pair_t new_ids;                                            \ */
/*                                                                         \ */
/*         new_ids.prev.r = normalize_single_ ## id_name (                 \ */
/*             &map, p->prev. id_name .r);                                 \ */
/*         new_ids.prev.e = normalize_single_ ## id_name (                 \ */
/*             &map, p->prev. id_name .e);                                 \ */
/*         new_ids.prev.s = normalize_single_ ## id_name (                 \ */
/*             &map, p->prev. id_name .s);                                 \ */
/*                                                                         \ */
/*         new_ids.next.r = normalize_single_ ## id_name (                 \ */
/*             &map, p->next. id_name .r);                                 \ */
/*         new_ids.next.e = normalize_single_ ## id_name (                 \ */
/*             &map, p->next. id_name .e);                                 \ */
/*         new_ids.next.s = normalize_single_ ## id_name (                 \ */
/*             &map, p->next. id_name .s);                                 \ */
/*                                                                         \ */
/*         id_name ## _norm_data_t norm;                                   \ */
/*         norm.pair = new_ids;                                            \ */
/*         norm.map = map;                                                 \ */
/*                                                                         \ */
/*         return norm;                                                    \ */
/*     } */
/* NORMALIZE_IDS_DEFN(uids); */
/* NORMALIZE_IDS_DEFN(gids); */

static uids_norm_data_t const normalize_uids(
    pcred_pair_t const* p) {
    uids_map_t map = new_uids_map();

    uids_pair_t new_ids;

    new_ids.prev.r = normalize_single_uids(
        &map, p->prev.uids.r);
    new_ids.prev.e = normalize_single_uids(
        &map, p->prev.uids.e);
    new_ids.prev.s = normalize_single_uids(
        &map, p->prev.uids.s);

    new_ids.next.r = normalize_single_uids(
        &map, p->next.uids.r);
    new_ids.next.e = normalize_single_uids(
        &map, p->next.uids.e);
    new_ids.next.s = normalize_single_uids(
        &map, p->next.uids.s);

    uids_norm_data_t norm;
    norm.pair = new_ids;
    norm.map = map;

    return norm;
}

static gids_norm_data_t const normalize_gids(
    uids_t const* normalized_current_uids,
    uids_t const* normalized_target_uids,
    pcred_pair_t const* p) {
    gids_map_t map = new_gids_map();

    uids_pair_t new_ids;
    bool const force_privileged = suid_privilege_is_attainable(
        *normalized_current_uids,
        *normalized_target_uids);

    new_ids.prev.r = normalize_single_gids(
        &map, p->prev.gids.r);

    // HACK: Normalize previous egid to 0 when state is, in fact, privileged
    new_ids.prev.e = force_privileged ? (gid_t)(0) :
        normalize_single_gids(&map, p->prev.gids.e);

    new_ids.prev.s = normalize_single_gids(
        &map, p->prev.gids.s);

    new_ids.next.r = normalize_single_gids(
        &map, p->next.gids.r);
    new_ids.next.e = normalize_single_gids(
        &map, p->next.gids.e);
    new_ids.next.s = normalize_single_gids(
        &map, p->next.gids.s);

    gids_norm_data_t norm;
    norm.pair = new_ids;
    norm.map = map;

    return norm;
}

/* #define NORMALIZE_ID_DEFN(id_name, id_type)                     \ */
/*     static uid_t normalize_single_ ## id_name (                 \ */
/*         id_name ## _map_t* map,                                 \ */
/*         id_type id) {                                           \ */
/*         static const id_type id_neg_one = (id_type)(-1);        \ */
/*         static const uid_t uid_neg_one = (uid_t)(-1);           \ */
/*         if (id == id_neg_one) {                                 \ */
/*             return uid_neg_one;                                 \ */
/*         }                                                       \ */
/*         if (id == 0) {                                          \ */
/*             return (uid_t)(0);                                  \ */
/*         }                                                       \ */
/*         unsigned ids_idx;                                       \ */
/*         for (ids_idx = 0; ids_idx < map->counter; ++ids_idx) {  \ */
/*             if (map->ids[ids_idx] == id) {                      \ */
/*                 return ids_idx + 1;                             \ */
/*             }                                                   \ */
/*         }                                                       \ */
/*         map->ids[map->counter] = id;                            \ */
/*         map->counter = map->counter + 1;                        \ */
/*         assert(map->counter < 3);                               \ */
/*         return (uid_t)(map->counter);                           \ */
/*     } */
/* NORMALIZE_ID_DEFN(uids, uid_t); */
/* NORMALIZE_ID_DEFN(gids, gid_t); */

static uid_t normalize_single_uids(uids_map_t* map, uid_t const id) {
    static const uid_t id_neg_one = (uid_t)(-1);
    static const uid_t uid_neg_one = (uid_t)(-1);
    if (id == id_neg_one) {
        return uid_neg_one;
    }
    if (id == 0) {
        return (uid_t)(0);
    }
    unsigned ids_idx;
    for (ids_idx = 0; ids_idx < map->counter; ++ids_idx) {
        if (map->ids[ids_idx] == id) {
            return ids_idx + 1;
        }
    }
    map->ids[map->counter] = id;
    map->counter = map->counter + 1;
    assert(map->counter < ID_MAP_SIZE);
    return (uid_t)(map->counter);
}

static uid_t normalize_single_gids(
    gids_map_t* map,
    gid_t const id) {
    static const gid_t id_neg_one = (gid_t)(-1);
    static const uid_t uid_neg_one = (uid_t)(-1);
    if (id == id_neg_one) {
        return uid_neg_one;
    }
    // NOTE: Unlike in normalize_single_uids(), gid=0 gets normalized to a
    // non-zero value because the group-0 is like any other group; only
    // the super-USER has special meaning for changing identities. The potential
    // super-user case is handled before a normalize_single_gids() call is
    // considered.
    unsigned ids_idx;
    for (ids_idx = 0; ids_idx < map->counter; ++ids_idx) {
        if (map->ids[ids_idx] == id) {
            return ids_idx + 1;
        }
    }
    map->ids[map->counter] = id;
    map->counter = map->counter + 1;
    assert(map->counter < 3);
    return (uid_t)(map->counter);
}

static int get_ids_idx(uids_t const* uids) {
    // Some platforms don't support states containing -1;
    // if this is the case don't bother querying the graph
    if (!NEG_ONE_IS_SUPPORTED &&
        uids->r == (uid_t)(-1) && uids->e == (uid_t)(-1) &&
        uids->s == (uid_t)(-1)) {
        return -1;
    }

    int state_idx = state_idx_lookup(uids->r, uids->e, uids->s);
    DD(if (state_idx >= 0) {
            DPN(3, "State index lookup: <%d, %d, %d>", uids->r, uids->e, uids->s);
        } else {
            DPN(3, "State index lookup: no such state: <%d, %d, %d>", uids->r, uids->e, uids->s);
        });

    return state_idx;
}

static uids_t const get_ids_from_array(uid_t const* arr) {
    uids_t uids;
    uids.r = arr[0];
    uids.e = arr[1];
    uids.s = arr[2];
    return uids;
}

#define SET_IDS_FROM_IDX_DEFN(id_name)                          \
    static int set_ ## id_name ## _from_idx(                    \
        norm_pcred_pair_t const* p,                             \
        unsigned const current_idx,                             \
        unsigned const next_idx) {                              \
        uid_t const** function_list =                           \
            adjacency_matrix[current_idx][next_idx];            \
        DPN(3, "Current idx: %d", current_idx);                 \
        DPN(3, "Next idx: %d\n", next_idx);                     \
        NON_NULL_OR_EINVAL(function_list);                      \
        NON_NULL_OR_EINVAL(function_list[0]);                   \
        return execute_ ## id_name ## _function_from_array(     \
            p,                                                  \
            &function_list[0][0]);                              \
    }
SET_IDS_FROM_IDX_DEFN(uids)
SET_IDS_FROM_IDX_DEFN(gids)

/* #define EXECUTE_ID_FUNCTION_FROM_ARRAY_DEFN(id_name, id_type)   \ */
/*     static int execute_ ## id_name ## _function_from_array(     \ */
/*         norm_pcred_pair_t const* p,                             \ */
/*         uid_t const* fn_array) {                                \ */
/*         uid_t const function_id = fn_array[0];                  \ */
/*         unsigned const num_params = get_num_function_params(    \ */
/*             function_id);                                       \ */
/*         uid_t const* normalized_params = &fn_array[3];          \ */
/*         DPN(3, "Function ID: %d\n", function_id);               \ */
/*         DPSN(3, "Normalized params: ");                         \ */
/*         unsigned i;                                             \ */
/*         for (i = 0; i < num_params; ++i) {                      \ */
/*             DPSN(3, "%d ", normalized_params[i]);               \ */
/*         }                                                       \ */
/*         DPSN(3, "\n");                                          \ */
/*         print_ ## id_name ## _map(3, &p-> id_name ## _map);     \ */
/*         id_type actual_params[MAX_SYSCALL_PARAMS];              \ */
/*         map_ ## id_name ## _params(                             \ */
/*             &p-> id_name ## _map,                               \ */
/*             num_params,                                         \ */
/*             normalized_params,                                  \ */
/*             actual_params);                                     \ */
/*         DPSN(3, "Actual params: ");                             \ */
/*         for (i = 0; i < num_params; ++i) {                      \ */
/*             DPSN(3, "%d ", actual_params[i]);                   \ */
/*         }                                                       \ */
/*         DPSN(3, "\n");                                          \ */
/*         return execute_ ## id_name ## _function(                \ */
/*             function_id,                                        \ */
/*             actual_params);                                     \ */
/*     } */
/* EXECUTE_ID_FUNCTION_FROM_ARRAY_DEFN(uids, uid_t); */
/* EXECUTE_ID_FUNCTION_FROM_ARRAY_DEFN(gids, gid_t); */

// Function call in list:
// [0] = function id, [1] = expected return, [2] = expected error, [params]
static int execute_uids_function_from_array(
    norm_pcred_pair_t const* p,
    uid_t const* fn_array) {
    uid_t const function_id = fn_array[0];
    unsigned const num_params = get_num_function_params(
        function_id);
    uid_t const* normalized_params = &fn_array[3];
    DPN(3, "Function ID: %d\n", function_id);
    DPSN(3, "Normalized params: ");
    unsigned i;
    for (i = 0; i < num_params; ++i) {
        DPSN(3, "%d ", normalized_params[i]);
    }
    DPSN(3, "\n");
    print_uids_map(3, &p-> uids_map);
    uid_t actual_params[MAX_SYSCALL_PARAMS];
    map_uids_params(
        &p-> uids_map,
        num_params,
        normalized_params,
        actual_params);
    DPSN(3, "Actual params: ");
    for (i = 0; i < num_params; ++i) {
        DPSN(3, "%d ", actual_params[i]);
    }
    DPSN(3, "\n");
    return execute_uids_function(
        function_id,
        actual_params);
}

static int execute_gids_function_from_array(
    norm_pcred_pair_t const* p,
    uid_t const* fn_array) {
    uid_t const function_id = fn_array[0];
    unsigned const num_params = get_num_function_params(
        function_id);
    uid_t const* normalized_params = &fn_array[3];
    DPN(3, "Function ID: %d\n", function_id);
    DPSN(3, "Normalized params: ");
    unsigned i;
    for (i = 0; i < num_params; ++i) {
        DPSN(3, "%d ", normalized_params[i]);
    }
    DPSN(3, "\n");
    print_gids_map(3, &p-> gids_map);
    gid_t actual_params[MAX_SYSCALL_PARAMS];
    map_gids_params(
        &p-> gids_map,
        num_params,
        normalized_params,
        actual_params);
    DPSN(3, "Actual params: ");
    for (i = 0; i < num_params; ++i) {
        DPSN(3, "%d ", actual_params[i]);
    }
    DPSN(3, "\n");
    return execute_gids_function(
        function_id,
        actual_params);
}

/* #define MAP_ID_PARAMS_DEFN(id_name, id_type, id_struct_type)    \ */
/*     void map_ ## id_name ## _params(                            \ */
/*         id_name ## _map_t const* map,                           \ */
/*         unsigned const num_params,                              \ */
/*         uid_t const* normalized_params,                         \ */
/*         id_type* actual_params) {                               \ */
/*         uid_t const* ids = map->ids;                            \ */
/*         unsigned i;                                             \ */
/*         for (i = 0; i < num_params; ++i) {                      \ */
/*             if (normalized_params[i] == 0) {                    \ */
/*                 actual_params[i] = 0;                           \ */
/*                 continue;                                       \ */
/*             }                                                   \ */
/*             if (normalized_params[i] == (uid_t)(-1)) {          \ */
/*                 actual_params[i] = (id_type)(-1);               \ */
/*                 continue;                                       \ */
/*             }                                                   \ */
/*             unsigned j;                                         \ */
/*             for (j = 0; j < ID_MAP_SIZE; ++j) {                 \ */
/*                 if (normalized_params[i] == ids[j]) {           \ */
/*                     actual_params[i] = (id_type)(j + 1);        \ */
/*                 }                                               \ */
/*             }                                                   \ */
/*         }                                                       \ */
/*     } */
/* MAP_ID_PARAMS_DEFN(uids, uid_t, uids_t); */
/* MAP_ID_PARAMS_DEFN(gids, gid_t, gids_t); */

void map_uids_params(
    uids_map_t const* map,
    unsigned const num_params,
    uid_t const* normalized_params,
    uid_t* actual_params) {
    uid_t const* ids = map->ids;
    unsigned i;
    for (i = 0; i < num_params; ++i) {
        if (normalized_params[i] == 0) {
            actual_params[i] = 0;
            continue;
        }
        if (normalized_params[i] == (uid_t)(-1)) {
            actual_params[i] = (uid_t)(-1);
            continue;
        }
        int const map_idx = normalized_params[i] - 1;
        assert(map_idx < ID_MAP_SIZE && ids[map_idx] > 0);
        actual_params[i] = ids[map_idx];
    }
}

void map_gids_params(
    gids_map_t const* map,
    unsigned const num_params,
    uid_t const* normalized_params,
    gid_t* actual_params) {
    uid_t const* ids = map->ids;
    unsigned i;
    for (i = 0; i < num_params; ++i) {
        if (normalized_params[i] == (uid_t)(-1)) {
            actual_params[i] = (gid_t)(-1);
            continue;
        }
        // NOTE: Do not "pass through" gid=0 as is done with uids
        /* if (euid == 0 && normalized_params[i] == (uid_t)(0)) { */
        /*     actual_params[i] = (gid_t)(0); */
        /*     continue; */
        /* } */
        int const map_idx = normalized_params[i] - 1;
        // NOTE: >=0 below is, again, because gid=0 gets mapped
        assert(map_idx < ID_MAP_SIZE && ids[map_idx] >= 0);
        actual_params[i] = ids[map_idx];
    }
}

#ifdef DEBUG
static void print_uids_map_(uids_map_t const* map) {
    DPS("UID map: {");
    unsigned i;
    for (i = 0; i < map->counter; ++i) {
        DPS("%d => %d", map->ids[i], (i + 1));
        if ((i + 1) < map->counter) {
            DPS(", ");
        }
    }
    DP("}");
}
static void print_gids_map_(gids_map_t const* map) {
    DPS("GID map: {");
    unsigned i;
    for (i = 0; i < map->counter; ++i) {
        DPS("%d => %d", map->ids[i], (i + 1));
        if ((i + 1) < map->counter) {
            DPS(", ");
        }
    }
    DP("]");
}
#endif
