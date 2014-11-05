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

#include "Assertions.h"
#include "Graph.h"
#include "Platform.h"
#include "Priv2State.h"
#include "SetuidState.h"

#include <boost/graph/breadth_first_search.hpp>

extern "C" {
#include "priv.h"

#include <errno.h>
#include <stdlib.h>
}

#include <algorithm>
#include <map>
#include <utility>
#include <vector>


#define SOMEWHAT_PRIVILEGED(s) s.ruid == 0 || s.euid == 0 || s.svuid == 0
#define INVALID_UID static_cast<UID>(-1)
#define HAS_INVALID_UID(s)                      \
    (s.ruid == INVALID_UID ||                   \
     s.euid == INVALID_UID ||                   \
     s.svuid == INVALID_UID)

// TODO: This error framework could be improved; throwing an error immediately
// prevents subsequent checks from being performed on a vertex or edge

// Run in context where Graph and v are appropriately defined
#define V_CONFIRM(cond, desc) if (!(cond)) {            \
        throw VertexVisitorError<Graph>(desc, v);       \
    }

// Run in context where Graph, e, v1, and v2 are appropriately defined
#define E_CONFIRM(cond, desc) if (!(cond)) {                    \
        throw EdgeVisitorError<Graph>(desc, e, v1, v2);        \
    }

template<typename Graph>
UIDMap Normalizer<Graph>::generateUIDMap(
    UIDSet const& uidSet) {
    UIDMap uidMap;
    std::vector<int> uidVect;
    for (UIDSet::iterator it = uidSet.begin(), ie = uidSet.end();
         it != ie; ++it) {
        uidVect.push_back(static_cast<int>(*it));
    }
    std::sort(uidVect.begin(), uidVect.end());
    int counter = 1;
    for (std::vector<int>::iterator it = uidVect.begin(), ie = uidVect.end();
         it != ie; ++it) {
        if (*it <= 0) {
            uidMap.insert(std::make_pair(
                              static_cast<UID>(*it),
                              static_cast<UID>(*it)));
        } else {
            uidMap.insert(std::make_pair(static_cast<UID>(*it), counter));
            ++counter;
        }
    }
    return uidMap;
}

template<typename Graph>
UIDSet Normalizer<Graph>::generateUIDSet(
    UIDMap const& uidMap) {
    UIDSet newSet;
    for (UIDMap::const_iterator it = uidMap.begin(), ie = uidMap.end();
         it != ie; ++it) {
        newSet.insert(it->second);
    }
    return newSet;
}

template<typename Graph>
UID Normalizer<Graph>::mapUID(
    UIDMap const& uidMap,
    UID uid) {
    UIDMap::const_iterator mapping = uidMap.find(uid);
    ASSERT(mapping != uidMap.end());
    return mapping->second;
}

template<typename Graph>
SetuidState Normalizer<Graph>::mapState(
    UIDMap const& uidMap,
    SetuidState const& s) {
    return SetuidState(
        mapUID(uidMap, s.ruid),
        mapUID(uidMap, s.euid),
        mapUID(uidMap, s.svuid));
}

template<typename Graph>
SetuidFunctionCall Normalizer<Graph>::mapFunctionCall(
    UIDMap const& uidMap,
    SetuidFunctionCall const& oldSFC) {
    SetuidFunctionParams newParams;
    for (SetuidFunctionParams::const_iterator it = oldSFC.params.begin(),
             ie = oldSFC.params.end(); it != ie; ++it) {
        newParams.push_back(mapUID(uidMap, *it));
    }
    return SetuidFunctionCall(oldSFC.function, newParams, oldSFC.rtn);
}

template<typename Graph>
void Normalizer<Graph>::visitEdge(
    EdgeProperty const& e,
    VertexProperty const& v1,
    VertexProperty const& v2) {
    g.addEdge(
        mapState(uidMap, v1),
        mapState(uidMap, v2),
        mapFunctionCall(uidMap, e)
        );
}

template<typename Graph>
void GeneralSanityVisitor<Graph>::visitEdge(
    EdgeProperty const& e,
    VertexProperty const& v1,
    VertexProperty const& v2) {
    switch (e.function) {
    default:
        E_CONFIRM(false, "Invalid setuid function type");
        break;
    case Setuid:
        E_CONFIRM(e.params.size() == 1,
                  "Expected 1 parameter for setuid() call");
        break;
    case Seteuid:
        E_CONFIRM(e.params.size() == 1,
                  "Expected 1 parameter for seteuid() call");
        break;
    case Setreuid:
        E_CONFIRM(e.params.size() == 2,
                  "Expected 2 parameters for setreuid() call");
        break;

#if HAS_SETRESUID

    case Setresuid:
        E_CONFIRM(e.params.size() == 3,
                  "Expected 3 parameters for setresuid() call");
        break;

#endif

    case DropPrivPerm:
        E_CONFIRM(e.params.size() == 1,
                  "Expected 1 parameter for dropprivperm() call");
        break;
    case DropPrivTemp:
        E_CONFIRM(e.params.size() == 1,
                  "Expected 1 parameter for dropprivtemp() call");
        break;
    case RestorePriv:
        E_CONFIRM(e.params.size() == 1,
                  "Expected 1 parameter for restorepriv() call");
        break;
    }
    E_CONFIRM(e.rtn.value == 0 || e.rtn.value == -1,
              "Expected function return of 0 or -1");
    E_CONFIRM(e.rtn.value == -1 || e.rtn.errNumber == 0,
              "Expected no error value for function return of 0");
    E_CONFIRM(e.rtn.value == 0 ||
              e.rtn.errNumber == EPERM ||
              e.rtn.errNumber == EINVAL,
              "Expected no error codes other than EPERM and EINVAL");
    E_CONFIRM(!(HAS_INVALID_UID(v1)) &&
              !(HAS_INVALID_UID(v2)),
              "Expected no edges to/from states containing UID -1");
    E_CONFIRM(
        (   (SOMEWHAT_PRIVILEGED(v1))   ) ||
        ( ! (SOMEWHAT_PRIVILEGED(v2))   ),
        "Expected no transitions from unprivileged states to somewhat privileged states");
    E_CONFIRM(e.rtn.value == 0 ||
              (v1.ruid == v2.ruid && v1.euid == v2.euid &&
               v1.svuid == v2.svuid),
              "Expected no change in UID values on failed function call");
    E_CONFIRM(v1.euid != 0 || e.rtn.value == 0 ||
              (e.rtn.value == -1 && e.rtn.errNumber == EINVAL),
              "Expected root to only fail function call with EINVAL error code");
}

template<typename Graph>
void SetuidSanityVisitor<Graph>::visitEdge(
    EdgeProperty const& e,
    VertexProperty const& v1,
    VertexProperty const& v2) {
    if (e.function == Setuid) {
        E_CONFIRM(e.rtn.value == -1 ||
                  v2.euid == static_cast<UID>(e.params.at(0)),
                  "Expected successful setuid() to change euid appropriately");
    }
}

template<typename Graph>
void SeteuidSanityVisitor<Graph>::visitEdge(
    EdgeProperty const& e,
    VertexProperty const& v1,
    VertexProperty const& v2) {
    if (e.function == Seteuid) {
        E_CONFIRM(e.rtn.value == -1 ||
                  v2.euid == static_cast<UID>(e.params.at(0)),
                  "Expected successful seteuid() to change euid appropriately");
        E_CONFIRM(v1.ruid == v2.ruid && v1.svuid == v2.svuid,
                  "Expected ruid and svuid to remain unchanged on seteuid() call");
    }
}

template<typename Graph>
void SetreuidSanityVisitor<Graph>::visitEdge(
    EdgeProperty const& e,
    VertexProperty const& v1,
    VertexProperty const& v2) {
    if (e.function == Setreuid) {
        E_CONFIRM(e.rtn.value == -1 ||
                  (e.params[0] == -1 && v1.ruid == v2.ruid) ||
                  (e.params[0] != -1 &&
                   v2.ruid == static_cast<UID>(e.params[0])),
                  "Expected ruid to be set appropriately on setreuid() call");
        E_CONFIRM(e.rtn.value == -1 ||
                  (e.params[1] == -1 && v1.euid == v2.euid) ||
                  (e.params[1] != -1 &&
                   v2.euid == static_cast<UID>(e.params[1])),
                  "Expected euid to be set appropriately on setreuid() call");
        E_CONFIRM(v1.euid != 0 || e.rtn.value == 0,
                  "Expected root to always succeed on setreuid() call");
    }
}

template<typename Graph>
void SetresuidSanityVisitor<Graph>::visitEdge(
    EdgeProperty const& e,
    VertexProperty const& v1,
    VertexProperty const& v2) {
    if (e.function == Setresuid) {
        E_CONFIRM(e.rtn.value == -1 ||
                  (e.params[0] == -1 && v1.ruid == v2.ruid) ||
                  (e.params[0] != -1 &&
                   v2.ruid == static_cast<UID>(e.params[0])),
                  "Expected ruid to be set appropriately on setresuid() call");
        E_CONFIRM(e.rtn.value == -1 ||
                  (e.params[1] == -1 && v1.euid == v2.euid) ||
                  (e.params[1] != -1 &&
                   v2.euid == static_cast<UID>(e.params[1])),
                  "Expected euid to be set appropriately on setresuid() call");
        E_CONFIRM(e.rtn.value == -1 ||
                  (e.params[2] == -1 && v1.svuid == v2.svuid) ||
                  (e.params[2] != -1 &&
                   v2.svuid == static_cast<UID>(e.params[2])),
                  "Expected svuid to be set appropriately on setresuid() call");
        E_CONFIRM(v1.euid != 0 || e.rtn.value == 0,
                  "Expected root to always succeed on setresuid() call");
    }
}

template<typename Graph>
void PrivSanityVisitor<Graph>::visitEdge(
    EdgeProperty const& e,
    VertexProperty const& v1,
    VertexProperty const& v2) {
    if (e.function == DropPrivPerm ||
        e.function == DropPrivTemp ||
        e.function == RestorePriv) {
        E_CONFIRM(e.rtn.value != 0 || v2.euid == static_cast<UID>(e.params.at(0)),
                  "Expected successful priv function to set euid to param value");
    }
}

template<typename Graph>
void DropPrivPermSanityVisitor<Graph>::visitEdge(
    EdgeProperty const& e,
    VertexProperty const& v1,
    VertexProperty const& v2) {
    if (e.function == DropPrivPerm) {
        E_CONFIRM(e.rtn.value != 0 ||
                  (v2.ruid != 0 && v2.euid != 0 && v2.svuid != 0),
                  "Expected successful dropprivperm to eliminate root uid");
    }
}

template<typename Graph>
void DropPrivTempSanityVisitor<Graph>::visitEdge(
    EdgeProperty const& e,
    VertexProperty const& v1,
    VertexProperty const& v2) {
    if (e.function == DropPrivTemp) {
        E_CONFIRM(e.rtn.value != 0 ||
                  (v2.euid == static_cast<UID>(e.params.at(0))),
                  "Expected successful dropprivtemp to correctly set euid");
    }
}

template<typename Graph>
void RestorePrivSanityVisitor<Graph>::visitEdge(
    EdgeProperty const& e,
    VertexProperty const& v1,
    VertexProperty const& v2) {
    if (e.function == RestorePriv) {
        E_CONFIRM(e.rtn.value != 0 ||
                  (v2.euid == static_cast<UID>(e.params.at(0))),
                  "Expected successful restorepriv to correctly set euid");
        // TODO: Confirm that no UIDs got lost in the shuffle
    }
}

template<typename Graph>
void StartStateVisitor<Graph>::visitVertex(VertexProperty const& v) {
    typename Graph::Vertex _v = g.getVertex(v);
    typename Graph::Graph const& boostGraph = g.getGraph();
    // Only interested in start states:
    // <a, a, a> or <a, b, b> -- (i.e., euid == svuid)
    if (!(v.euid == v.svuid)) {
        return;
    }
    // Determine whether there are out-edges from this state
    bool outEdgesExist = false;
    std::pair<
        typename Graph::Graph::out_edge_iterator,
        typename Graph::Graph::out_edge_iterator>
        i = boost::out_edges(_v, boostGraph);
    for (; i.first != i.second; ++i.first) {
        outEdgesExist = true;
        break;
    }
    V_CONFIRM(outEdgesExist,
              "Expected to find out edges from every possible start state");
}

template<typename Graph>
void SomewhatReversibleVisitor<Graph>::visitVertex(VertexProperty const& v) {
    typename Graph::Vertex _v = g.getVertex(v);
    typename Graph::Graph const& boostGraph = g.getGraph();
    // Only interested in <x, y, z>: y != 0 and (x = 0 or z = 0)
    if (!(v.euid != 0 && (v.ruid == 0 || v.svuid == 0))) {
        return;
    }
    // Store jumps to <a, 0, c>
    std::vector<typename Graph::Vertex> candidateJumps;
    std::pair<
        typename Graph::Graph::out_edge_iterator,
        typename Graph::Graph::out_edge_iterator>
        i = boost::out_edges(_v, boostGraph);
    // Find jumps to <a, 0, c>
    for (; i.first != i.second; ++i.first) {
        typename Graph::Vertex vCandidate = target(*i.first, boostGraph);
        VertexProperty const& vpCandidate = boostGraph[vCandidate];
        if (vpCandidate.euid == 0) {
            candidateJumps.push_back(vCandidate);
        }
    }
    // Find jumps from <a, 0, c> back to original <x, y, z>
    bool foundJumpBack = false;
    for (typename std::vector<typename Graph::Vertex>::iterator
             it = candidateJumps.begin(), ie = candidateJumps.end();
         it != ie; ++it) {
        i = boost::out_edges(*it, boostGraph);
        for (; i.first != i.second; ++i.first) {
            if (target(*i.first, boostGraph) == _v) {
                foundJumpBack = true;
                break;
            }
        }
        if (foundJumpBack) {
            break;
        }
    }
    V_CONFIRM(foundJumpBack,
              "Expected to find reversible privilege escalation");
}

// TODO: This should be integrated into the error framework, rather than writing
// directly to std::cerr
#include <iostream>
#define P2_CONFIRM(cond, desc)                                  \
    if (!(cond)) {                                              \
        std::cerr << "Priv2 verification error:" << std::endl   \
                  << desc << std::endl                          \
                  << "  " << *it << std::endl                   \
                  << "    " << fnRtn << std::endl               \
                  << "  " << v << std::endl                     \
                  << priv2State << std::endl;                   \
            }

template<typename Graph>
void Priv2SanityVisitor<Graph>::visitVertex(VertexProperty const& v) {
    for (CallSet::const_iterator it = callSet.begin(), ie = callSet.end();
         it != ie; ++it) {
        typename Explorer::Rtn rtnAndNewState =
            explorer.exploreOne(typename Explorer::Param(v, *it));
        Priv2FunctionReturn const& fnRtn = rtnAndNewState.fnRtn;
        Priv2State const& priv2State = rtnAndNewState.nextState;
        SetuidState const& uState = priv2State.uState;
        SetgidState const& gState = priv2State.gState;
        SupGroups const& supGroups = priv2State.supGroups;

        ASSERT(it->params.size() == 1);
        Priv2FunctionParam const& param = it->params.at(0);

        if (v.euid == 0) {
            P2_CONFIRM(
                fnRtn.value == 0 ||
                (fnRtn.value == -1 && fnRtn.errNumber == EINVAL &&
                 (param.uid == static_cast<UID>(-1) ||
                  param.gid == static_cast<GID>(-1))),
                "Expected root to succeed or to fail due to id=-1 (but for no other reason)");
        }

        if (param.uid != static_cast<UID>(-1) &&
            param.gid != static_cast<GID>(-1) &&
            (v.ruid == param.uid ||
             v.euid == param.uid ||
             v.svuid == param.uid)//  &&
            // (v.rgid == param.gid ||
            //  v.egid == param.gid ||
            //  v.svgid == param.gid)
            ) {
            P2_CONFIRM(
                fnRtn.value == 0,
                "Expected attempt to assume identity with existing uids to succeed");
        }

        switch (it->function) {
        default:
            ASSERT(false);
            break;
        case AssumeIDPerm:
            if (fnRtn.value == 0) {
                P2_CONFIRM(
                    uState.ruid == param.uid &&
                    uState.euid == param.uid &&
                    uState.svuid == param.uid,
                    "Expected uids to all change correctly on change_identity_permanently success");
                P2_CONFIRM(
                    gState.rgid == param.gid &&
                    gState.egid == param.gid &&
                    gState.svgid == param.gid,
                    "Expected gids to all change correctly on change_identity_permanently success");

                {
                    sups_t param_sups = param.supGroups.toSupsT();
                    sups_t new_sups = supGroups.toSupsT();
                    if (!eql_sups(&new_sups, &param_sups)) {
                        print_sups("Param sups", &param_sups);
                        print_sups("New sups", &new_sups);
                    }
                    P2_CONFIRM(
                        eql_sups(&new_sups, &param_sups),
                        "Expected supplementary groups to change correctly on change_identity_permanently success");
                    free(param_sups.list);
                    free(new_sups.list);
                }
            } else {
                P2_CONFIRM(
                    fnRtn.value == -1,
                    "Expected negative-one return value on change_identity_permanently failure");
            }
            break;
        case AssumeIDTemp:
            if (fnRtn.value == 0) {
                P2_CONFIRM(
                    uState.euid == param.uid &&
                    (uState.ruid == v.euid || uState.svuid == v.euid),
                    "Expected uids to all change correctly on change_identity_temporarily success");

                // TODO: Completely testing gid change success condition on
                // AssumeIDTemp does not seem to be (meaningfully) possible
                // using a purely uids graph. We only verify egid.
                P2_CONFIRM(
                    gState.egid == param.gid,
                    "Expected egid to change correctly on change_identity_temporarily success");

                {
                    sups_t param_sups = param.supGroups.toSupsT();
                    sups_t new_sups = supGroups.toSupsT();
                    if (!eql_sups(&new_sups, &param_sups)) {
                        print_sups("Param sups", &param_sups);
                        print_sups("New sups", &new_sups);
                    }
                    P2_CONFIRM(
                        eql_sups(&new_sups, &param_sups),
                        "Expected supplementary groups to change correctly on change_identity_temporarily success");
                    free(param_sups.list);
                    free(new_sups.list);
                }
            } else {
                P2_CONFIRM(
                    fnRtn.value == -1,
                    "Expected negative-one return value on change_identity_temporarily failure");
            }
            break;
        }

        // TODO: Testing group change failures condition does not seem to be
        // (meaningfully) possible using a purely uids graph. We checks uids
        // only.

        // Failure details (either success or no uid changes):
        P2_CONFIRM(
            fnRtn.value == 0 ||
            (uState.ruid == v.ruid &&
             uState.euid == v.euid &&
             uState.svuid == v.svuid),
            "Expected uids not to change on change_identity_* failure");
    }
}
