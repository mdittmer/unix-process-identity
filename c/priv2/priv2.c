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
// priv2.c
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
#ifdef __linux__
#define _GNU_SOURCE
#define __USE_GNU
#include <sys/capability.h>
#endif

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "debug.h"
#include "priv.h"
#include "priv2.h"
#include "priv2_generated.h"
#include "priv2_graph.h"

// TODO: This currently lives with the C++ code
#include "Platform.h"

//##############################################################################
// Internal data structures
//##############################################################################
typedef enum change_identity_status_enum {
    ci_invalid = -1,
    ci_success = 0,
    ci_invalid_values,
    ci_setuid_failed,
    ci_setgid_failed,
    ci_setuid_setgid_failed
} ci_status_e;

typedef enum groups_timing_enum {
    groups_first,  // Change groups first (in "current state")
    groups_last,   // Change groups last  (in "target state" )
    groups_elevate // Temporarily elevate privileges to change groups
} groups_timing_e;

//##############################################################################
// Macros
//##############################################################################
#define TRUE_OR_INVALID_VALUES(t) if (!(t)) { return ci_invalid_values; }
#define TRUE_OR_SETUID_FAILED(t) if (!(t)) { return ci_setuid_failed; }
#define TRUE_OR_SETGID_FAILED(t) if (!(t)) { return ci_setgid_failed; }

//##############################################################################
// Internal prototypes
//##############################################################################

// Memory clean-up for pair
static void delete_pair(pcred_pair_t const* p);
// Fill out prev with current state; force euid=0 when force_privileged (used by
// platform-specific credential systems)
static pcred_pair_t new_pair(bool const force_privileged);
// Attempt to fill out a pair; fail if all candidate pairs are unreachable
static ci_status_e const fill_pair(
    ucred_t const* u,
    bool const is_permanent,
    bool const force_privileged,
    pcred_pair_t* p);
// Construct pair and set next to permanent pcred_t based on u
static pcred_pair_t const get_pair_permanent(
    ucred_t const* u,
    bool const force_privileged);
// Construct pairs where next is a valid temporary pcred_t based on u
static id_pairs_t const get_pairs_temporary(
    ucred_t const* u,
    bool const force_privileged);
// As above, but set next = prev, and leave non-const; used for manipulating
// next based on prev
static pcred_pair_t get_pair_current(bool const force_privileged);

// Change identities
static int change_identity(ucred_t const* uc, bool is_permanent);

#if IS_LINUX

static int change_identity_linux(ucred_t const* uc, bool is_permanent);

// Capability-specific stuff:

static bool can_set_cap_setuid();
static int set_cap_setuid();
static int clear_cap_setuid();

static bool can_set_cap_setgid();
static int set_cap_setgid();
static int clear_cap_setgid();

#endif

#if IS_SOLARIS
static int change_identity_solaris(ucred_t const* uc, bool is_permanent);
#endif

static ci_status_e change_identity_basic(ucred_t const* uc, bool is_permanent);

// Handle edge case: privileges must be elevated before setting supplementary
// groups list, then transitioning to target state
// NOTE: Input is orriginal pair: <current state, target state>
static void change_identity_elevate_for_sups(norm_pcred_pair_t const* np);

// Sanity uid and gid
static bool ids_are_sane(ucred_t const* ucred);
// Sanity check supplementary groups list
static bool sups_are_sane(sups_t const* sups);
// Test whether it is necessary to change supplementary groups
static bool sups_change_needed(norm_pcred_pair_t const* p);
// Test whether it is necessary to change main groups (i.e., <rgid, egid, sgid>)
static bool groups_change_needed(norm_pcred_pair_t const* p);
// Test whether it is possible to achieve supplementary group change
static bool sups_are_possible(norm_pcred_pair_t const* p);
// Test whether it is possible to achieve group change (i.e., desired change to
// <rgid, egid, sgid>)
static bool groups_are_possible(norm_pcred_pair_t const* p);
// Check for "don't change" supplementary groups list
static bool sups_is_dont_change(sups_t const* sups);
// Test whether it would be possible to change supplementary groups
/* static bool suid_privilege_is_attainable(norm_pcred_pair_t const* p); */
// Test whether it is possible to change supplementary groups in the current
// state
static bool suid_privilege_is_effective(uids_t const* current_uids);


//##############################################################################
// Higher level algorithm
//##############################################################################

static int change_identity(ucred_t const* uc, bool is_permanent) {
#if IS_LINUX
    return change_identity_linux(uc, is_permanent);
#elif IS_SOLARIS
    return change_identity_solaris(uc, is_permanent);
#else
    ci_status_e status =
        change_identity_basic(uc, is_permanent);
    if (status == ci_invalid_values) {
        errno = EINVAL;
        return -1;
    } else if (status != ci_success) {
        errno = EPERM;
        return -1;
    } else {
        return 0;
    }
#endif
}

static ci_status_e change_identity_basic(ucred_t const* uc, bool is_permanent) {
    // Sanity check for ids and sups
    if (!ids_are_sane(uc) ||
        !sups_are_sane(&uc->sups)) {
        return ci_invalid_values;
    }

    // Construct state pair: <current state, target state>
    pcred_pair_t p;
    ci_status_e status = fill_pair(uc, is_permanent, false, &p);

    // Back out immediately if transition is impossible
    if (status != ci_success) {
        delete_pair(&p);
        return status;
    }

    norm_pcred_pair_t const np = get_normalized_pcred_pair(&p);

    // Determine when to change groups: first, last, or elevate-before-change
    bool const must_change_sups = sups_change_needed(&np);
    bool const must_change_groups = groups_change_needed(&np);
    bool const current_priv = suid_privilege_is_effective(&p.prev.uids);
    bool const target_priv = suid_privilege_is_effective(&p.next.uids);
    bool const no_priv = !suid_privilege_is_attainable(
        np.prev.normalized.uids,
        np.next.normalized.uids);
    groups_timing_e const groups_timing =
        (no_priv || current_priv) ?
        groups_first :
        (target_priv ?
         groups_last :
         groups_elevate);

    switch (groups_timing) {
    default:
        assert(false);
        break;
    case groups_first:
        DPN(3, "Changing identity (basic): groups_first");
        break;
    case groups_last:
        DPN(3, "Changing identity (basic): groups_last");
        break;
    case groups_elevate:
        DPN(3, "Changing identity (basic): groups_elevate");
        break;
    }

    if (groups_timing == groups_elevate) {
        // The whole process is managed differently if we need to elevate
        // privileges to set sups before continuing to the final state
        change_identity_elevate_for_sups(&np);
    } else {

        if (groups_timing == groups_first) {
            // First change (triggered by no_priv or current_priv):

            if (must_change_groups) {
                // TODO: We do not currently have a graph of groups so there is
                // no guarantee that the group functions will work; however, by
                // doing them first when no_priv, we avoid the need for a
                // rollback if they fail
                if (set_gids_from_graph(&np) != 0) {
                    return ci_setgid_failed;
                }
            }

            if (must_change_sups) {
                assert(set_sups(&np.next.sups) == 0);
            }
        }

        assert(set_uids_from_graph(&np) == 0);

        if (groups_timing == groups_last) {
            // Last change (triggered by target_priv and not no_priv or
            // current_priv):
            if (must_change_sups) {
                assert(set_sups(&np.next.sups) == 0);
            }
            if (must_change_groups) {
                assert(set_gids_from_graph(&np) == 0);
            }
        }
    }

    delete_pair(&p);
    return ci_success;
}

static void change_identity_elevate_for_sups(norm_pcred_pair_t const* np) {
    // Elevate privileges to change sups, then transition to target identity

    // Get the uids of the intermediate privileged state we will use for
    // setting sups
    uids_t const priv_norm_uids = get_suid_for_identity_change(np);

    // HACK: Recycle code for denormalizing parameter lists to denormalize
    // the real, effective, and saved uids for our intermediate privileged
    // state
    uid_t const normalized_priv_uids[3] = {
        priv_norm_uids.r,
        priv_norm_uids.e,
        priv_norm_uids.s
    };
    uid_t actual_priv_uids[3];
    map_uids_params(
        &np->uids_map,
        3,
        normalized_priv_uids,
        actual_priv_uids);

    // Construct pair for first jump: current state -> privileged state
    pcred_pair_t p1 = get_pair_current(false);
    p1.next.uids.r = actual_priv_uids[0];
    p1.next.uids.e = actual_priv_uids[1];
    p1.next.uids.s = actual_priv_uids[2];
    norm_pcred_pair_t const np1 = get_normalized_pcred_pair(&p1);

    // Change effective user to privileged value
    assert(set_uids_from_graph(&np1) == 0);
    delete_pair(&p1);

    // Construct pair for second jump:
    // new current (privileged) state -> target state
    pcred_pair_t p2 = get_pair_current(false);
    p2.next.uids = np->next.actual.uids;
    p2.next.gids = np->next.actual.gids;
    p2.next.sups = np->next.sups;
    norm_pcred_pair_t const np2 = get_normalized_pcred_pair(&p2);

    assert(
        set_sups(&np2.next.sups) == 0 &&  // Change sups to target value
        set_gids_from_graph(&np2) == 0 && // Change groups to target value
        set_uids_from_graph(&np2) == 0);  // Change user to target value
    delete_pair(&p2);
}


#if IS_LINUX
static int change_identity_linux(ucred_t const* uc, bool is_permanent) {
    // Attempt "basic" algorithm first
    ci_status_e status =
        change_identity_basic(uc, is_permanent);
    if (status == ci_invalid_values) {
        // There are no false positives for cross-platform EINVAL detection;
        // return error conditoin immediately
        errno = EINVAL;
        return -1;
    } else if (status == ci_success) {
        return 0;
    }

    // If basic algorithm fails, attempt to use capabilities

    if (status == ci_setuid_failed || status == ci_setuid_setgid_failed) {
        if (!can_set_cap_setuid()) {
            errno = EPERM;
            return -1;
        } else {
            assert(set_cap_setuid() == 0);
        }
    }
    if (status == ci_setgid_failed || status == ci_setuid_setgid_failed) {
        if (!can_set_cap_setgid()) {
            errno = EPERM;
            return -1;
        } else {
            assert(set_cap_setgid() == 0);
        }
    }

    // Construct state pair: <current state, target state> with spoofed
    // current-effective-uid = super-user (via "true" in arg list below); this
    // allows us to use an accurate state graph with above-elevated privileges
    pcred_pair_t p;
    assert(fill_pair(uc, is_permanent, true, &p) == ci_success);

    norm_pcred_pair_t const np = get_normalized_pcred_pair(&p);

    assert(
        set_sups(&np.next.sups) == 0 &&  // Change sups to target value
        set_gids_from_graph(&np) == 0 && // Change groups to target value
        set_uids_from_graph(&np) == 0);  // Change user to target value

    // Revoke added capabilities so long as effective user is not super-user
    if (np.next.actual.uids.e != 0) {
        if (status == ci_setuid_failed || status == ci_setuid_setgid_failed) {
            assert(clear_cap_setuid() == 0);
        }
        if (status == ci_setgid_failed || status == ci_setuid_setgid_failed) {
            assert(clear_cap_setgid() == 0);
        }
    }

    return 0;
}

#define CAN_SET_CAP_DEFN(basic_name, cap_name)                          \
    static bool can_set_cap_ ## basic_name () {                         \
        cap_value_t const cap = cap_name;                               \
        cap_flag_value_t flag_value;                                    \
        cap_t caps = cap_get_proc();                                    \
                                                                        \
        if (cap_get_flag(caps, cap, CAP_PERMITTED, &flag_value) != 0) { \
            return false;                                               \
        } else {                                                        \
            return flag_value == CAP_SET;                               \
        }                                                               \
    }
CAN_SET_CAP_DEFN(setuid, CAP_SETUID);
CAN_SET_CAP_DEFN(setgid, CAP_SETGID);

#define MODIFY_CAP_DEFN(mod_name, basic_name, cap_name, cap_value)      \
    static int mod_name ## _cap_ ## basic_name () {                     \
        cap_value_t const cap = cap_name;                               \
        cap_flag_value_t const flag_value = cap_value;                  \
        cap_t caps = cap_get_proc();                                    \
                                                                        \
        return cap_set_flag(caps, CAP_EFFECTIVE, 1, &cap, flag_value);  \
    }
MODIFY_CAP_DEFN(set, setuid, CAP_SETUID, CAP_SET);
MODIFY_CAP_DEFN(clear, setuid, CAP_SETUID, CAP_CLEAR);
MODIFY_CAP_DEFN(set, setgid, CAP_SETGID, CAP_SET);
MODIFY_CAP_DEFN(clear, setgid, CAP_SETGID, CAP_CLEAR);
#endif

#if IS_SOLARIS
static ci_status_e change_identity_solaris(
    ucred_t const* uc,
    bool is_permanent) {
    ci_status_e status =
        change_identity_basic(uc, is_permanent);

    // TODO: Implement: try temporarily adding privileges

    // Temporary measure:
    if (status == ci_invalid_values) {
        errno = EINVAL;
        return -1;
    } else if (status != ci_success) {
        errno = EPERM;
        return -1;
    } else {
        return 0;
    }
}
#endif

int change_identity_permanently(ucred_t const* uc) {
    /* print_ucred("Changing identity permanently", uc); */
    /* pcred_t pcred; */
    /* get_pcred(&pcred); */
    /* print_pcred("Changing identity permanently", &pcred); */
    /* free(pcred.sups.list); */

    return change_identity(uc, true);
}

int change_identity_temporarily(ucred_t const* uc) {
    /* print_ucred("Changing identity temporarily", uc); */
    /* pcred_t pcred; */
    /* get_pcred(&pcred); */
    /* print_pcred("Changing identity temporarily", &pcred); */
    /* free(pcred.sups.list); */

    return change_identity(uc, false);
}


//##############################################################################
// Process data construction
//##############################################################################
static pcred_pair_t new_pair(bool const force_privileged) {
    pcred_pair_t p;
    // malloc()s p.prev.sups.list
    get_pcred(&p.prev);

    if (force_privileged) {
        // HACK: Spoof effective uid to be super-user; this fools the id state
        // graph into finding paths that are consistent with platform-specific
        // elevated privilege models (e.g., Linux capabilities, Solaris
        // privileges)
        p.prev.uids.e = 0;
    }

    return p;
}

static void delete_pair(pcred_pair_t const* p) {
    // free malloc()ed data from previous get_pcred() call
    free(p->prev.sups.list);
    // Note: p->next.sups.list is owned by caller (copied from ucred_t input)
}

static ci_status_e const fill_pair(
    ucred_t const* u,
    bool const is_permanent,
    bool const force_privileged,
    pcred_pair_t* p) {

    // Only one candidate pair for permanent change; check feasibility, return
    if (is_permanent) {
        *p = get_pair_permanent(u, force_privileged);

        norm_pcred_pair_t const np = get_normalized_pcred_pair(p);

        if (!sups_are_possible(&np) ||
            !groups_are_possible(&np)) {
            if (!can_set_uids_from_graph(&np)) {
                return ci_setuid_setgid_failed;
            } else {
                return ci_setgid_failed;
            }
        } else {
            if (!can_set_uids_from_graph(&np)) {
                return ci_setuid_failed;
            } else {
                return ci_success;
            }
        }
    }

    // Multiple candidate pairs for temporary change; need to "do our best" to
    // find a good one

    // Get candidate uids and gids
    id_pairs_t pairs = get_pairs_temporary(u, force_privileged);
    // prev and next.sups is always the same; we are looking for reachable uids
    // and gids
    p->prev = pairs.prev;
    p->next.sups = pairs.sups;

    // Try all candidate next IDs. Return once we find a reachable one. If none
    // are reachable, return the "most likely to succeed" IDs, based on:
    // missing gid privileged > missing uid privilege > missing both privileges
    ci_status_e status = ci_invalid;
    int uid_idx = -1, gid_idx = -1;
    unsigned i;
    for (i = 0; i < ID_PAIR_COLLECTION_SIZE; ++i) {
        unsigned j;
        for (j = 0; j < ID_PAIR_COLLECTION_SIZE; ++j) {
            p->next.uids = pairs.uids[i];
            p->next.gids = pairs.gids[j];
            norm_pcred_pair_t const np = get_normalized_pcred_pair(p);
            if (!can_set_uids_from_graph(&np)) {
                if (!sups_are_possible(&np) ||
                    !groups_are_possible(&np)) {
                    // Better than nothing:
                    if (status == ci_invalid) {
                        uid_idx = i;
                        gid_idx = j;
                        status = ci_setuid_setgid_failed;
                    }
                } else {
                    // Better than failing on both setuid and setgid:
                    if (status == ci_invalid ||
                        status == ci_setuid_setgid_failed) {
                        uid_idx = i;
                        gid_idx = j;
                        status = ci_setuid_failed;
                    }
                }
            } else if (!sups_are_possible(&np) ||
                       !groups_are_possible(&np)) {
                // Better than any other error condition:
                // We assume that getting setgid privileges is more difficult
                // than getting setuid privileges
                if (status == ci_invalid ||
                    status == ci_setuid_setgid_failed ||
                    status == ci_setuid_failed) {
                    uid_idx = i;
                    gid_idx = j;
                    status = ci_setgid_failed;
                }
            } else {
                // Best! No failures!
                uid_idx = i;
                gid_idx = j;
                status = ci_success;
                break;
            }
        }
        // Break out of both loops is an error-free pair has been found
        if (status == ci_success) {
            break;
        }
    }

    // Sanity check
    assert(uid_idx >= 0 && gid_idx >= 0);

    // Copy best uids and gids into place
    p->next.uids = pairs.uids[uid_idx];
    p->next.gids = pairs.gids[gid_idx];

    DPN(3, "Fill pair temporary status: %d", (int)(status));

    return status;
}

static pcred_pair_t const get_pair_permanent(
    ucred_t const* u,
    bool const force_privileged) {
    pcred_pair_t p = new_pair(force_privileged);
    p.next.uids.r = p.next.uids.e = p.next.uids.s = u->uid;
    p.next.gids.r = p.next.gids.e = p.next.gids.s = u->gid;
    p.next.sups = u->sups;
    return p;
}

static id_pairs_t const get_pairs_temporary(
    ucred_t const* u,
    bool const force_privileged) {
    pcred_pair_t p = new_pair(force_privileged);
    id_pairs_t pairs;

    pairs.prev = p.prev;
    pairs.sups = u->sups;

    // Effective ID must be u's ids
    unsigned i;
    for (i = 0; i < ID_PAIR_COLLECTION_SIZE; ++i) {
        pairs.uids[i].e = u->uid;
        pairs.gids[i].e = u->gid;
    }

    // Prefer leaving real ID unchanged and using saved ID to store previous
    // effective ID
    pairs.uids[0].r = p.prev.uids.r;
    pairs.gids[0].r = p.prev.gids.r;
    pairs.uids[0].s = p.prev.uids.e;
    pairs.gids[0].s = p.prev.gids.e;

    pairs.uids[1].r = p.prev.uids.e;
    pairs.gids[1].r = p.prev.gids.e;
    pairs.uids[1].s = p.prev.uids.e;
    pairs.gids[1].s = p.prev.gids.e;

    pairs.uids[2].r = p.prev.uids.s;
    pairs.gids[2].r = p.prev.gids.s;
    pairs.uids[2].s = p.prev.uids.e;
    pairs.gids[2].s = p.prev.gids.e;

    pairs.uids[3].r = p.prev.uids.e;
    pairs.gids[3].r = p.prev.gids.e;
    pairs.uids[3].s = p.prev.uids.r;
    pairs.gids[3].s = p.prev.gids.r;

    pairs.uids[4].r = p.prev.uids.e;
    pairs.gids[4].r = p.prev.gids.e;
    pairs.uids[4].s = p.prev.uids.s;
    pairs.gids[4].s = p.prev.gids.s;

    return pairs;
}

static pcred_pair_t get_pair_current(bool const force_privileged) {
    pcred_pair_t p = new_pair(force_privileged);
    p.next = p.prev;
    return p;
}


//##############################################################################
// Group operations
//##############################################################################
static bool sups_are_possible(norm_pcred_pair_t const* p
    ) {
    return (!sups_change_needed(p) ||
            suid_privilege_is_effective(&p->prev.normalized.uids) ||
            suid_privilege_is_attainable(
                p->prev.normalized.uids,
                p->next.normalized.uids));
}

static bool groups_are_possible(norm_pcred_pair_t const* p) {
    return (!groups_change_needed(p) ||
            suid_privilege_is_effective(&p->prev.normalized.uids) ||
            // TODO: Line of code below depends on our assumption:
            // "correct_uid_graph_without_special_treatment_for_root = correct_gid_graph"
            can_set_gids_from_graph(p) ||
            suid_privilege_is_attainable(
                p->prev.normalized.uids,
                p->next.normalized.uids));
}

static bool sups_change_needed(norm_pcred_pair_t const* p) {
    return !(sups_is_dont_change(&p->next.sups) ||
             eql_sups(&p->prev.sups, &p->next.sups));
}

static bool groups_change_needed(norm_pcred_pair_t const* p) {
    // TODO: Is it safer to use actual or normalized gids here?
    return (
        p->prev.actual.gids.r != p->next.actual.gids.r ||
        p->prev.actual.gids.e != p->next.actual.gids.e ||
        p->prev.actual.gids.s != p->next.actual.gids.s);
}

static bool ids_are_sane(ucred_t const* ucred) {
    return (NEG_ONE_IS_SUPPORTED ||
            (ucred->uid != (uid_t)(-1) && ucred->gid != (gid_t)(-1)));
}

static bool sups_are_sane(sups_t const* sups) {
    if (sups->size < 0) {
        return false;
    }

    // Copied from priv.c's ucred_is_sane():

    long   nm  = sysconf(_SC_NGROUPS_MAX);
    long   i, n = sups->size;
    gid_t const* arr = sups->list;

    // Make sure supplementary groups are sorted ascending with no duplicates
    for(i = 1; i < n; i++) {
	if(arr[i - 1] >= arr[i]) {
	    return false;
        }
    }

    // The size of the supplementary gropus array should be reasonable
    return (nm >= 0 && nm >= n && n >= 0);
}

static bool sups_is_dont_change(sups_t const* sups) {
    return (!sups->size && !sups->list); // List size is 0 and list is NULL
}

/* static bool suid_privilege_is_attainable(norm_pcred_pair_t const* p) { */
/*     // HACK: Store a cache of size one so that when this is invoked multiple */
/*     // times on the same pair, the cached value is returned. This works because */
/*     // this value may be needed multiple times on the same pair pre-state-change */
/*     // checks (and no other pairs are checked in the meantime). */
/*     static norm_pcred_pair_t const* cached_p = NULL; */
/*     static bool cached_rtn = false; */

/*     if (p == cached_p) { */
/*         return cached_rtn; */
/*     } */

/*     cached_p = p; */
/*     cached_rtn = can_set_suid_for_identity_change(p); */

/*     return cached_rtn; */
/* } */

static bool suid_privilege_is_effective(uids_t const* current_uids) {
    return (current_uids->e == 0);
}

//##############################################################################
//                                   EOF
//##############################################################################
