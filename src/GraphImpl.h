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
#include "Priv2State.h"
#include "SetuidState.h"

#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>

// Vertex and Edge generator implementations

template<>
struct VertexGenerator<UID, SetuidState> {
    typedef UID InputItem;
    typedef SetuidState OutputItem;
    typedef std::set<UID> InputCollection;
    typedef std::set<SetuidState> OutputCollection;

    OutputCollection generateAll(InputCollection const& uids) const {
        StateSet ss;
        UIDSet::const_iterator ie = uids.end();
        for (UIDSet::const_iterator rIt = uids.begin(); rIt != ie; ++rIt) {
            for (UIDSet::const_iterator eIt = uids.begin(); eIt != ie; ++eIt) {
                for (UIDSet::const_iterator svIt = uids.begin();
                     svIt != ie; ++svIt) {
                    ss.insert(SetuidState(*rIt, *eIt, *svIt));
                }
            }
        }
        return ss;
    }
};

template<typename VertexInputType, typename EdgeInputType, typename OutputType>
typename EdgeGenerator<VertexInputType, EdgeInputType, OutputType>::EdgeInputCollection
EdgeGenerator<VertexInputType, EdgeInputType, OutputType>::unifyInputs(
    VertexInputCollection const& vic,
    EdgeInputCollection const& eic) const {
    EdgeInputCollection unified(eic);

    for (typename VertexInputCollection::const_iterator it = vic.begin(),
             ie = vic.end(); it != ie; ++it) {
        if (unified.find(
                static_cast<EdgeInputItem>(*it)) == unified.end()) {
            unified.insert(static_cast<EdgeInputItem>(*it));
        }
    }

    return unified;
}

struct SetuidEdgeGenerator :
    public EdgeGenerator<UID, SetuidFunctionParam, Call> {
    typedef UID VertexInputItem;
    typedef SetuidFunctionParam EdgeInputItem;
    typedef Call OutputItem;

    typedef std::set<VertexInputItem> VertexInputCollection;
    typedef std::set<EdgeInputItem> EdgeInputCollection;
    typedef std::set<OutputItem> OutputCollection;

    virtual ~SetuidEdgeGenerator() {}

    virtual OutputCollection generateAll(
        VertexInputCollection const& users,
        EdgeInputCollection const& extraParams) const {
        ParamSet params = unifyInputs(users, extraParams);
        CallSet cs;

        // Generate all valid setuid-type function calls
        ParamSet::const_iterator ie = params.end();
        for (int f = Setuid; f < SetuidFunctionEnd; ++f) {
            SetuidFunction sf = static_cast<SetuidFunction>(f);

            if (!HAS_SETRESUID && sf == Setresuid) {
                continue;
            }

            switch (sf) {
            default:
                ASSERT(false);
                break;
            case Setuid:
                for (ParamSet::const_iterator it1 = params.begin(); it1 != ie; ++it1) {
                    // The setuid() function does not support "don't-care";
                    // however, if -1 is to be treated as a UID, then it must be
                    // supported
                    if (*it1 != -1 ||
                        (*it1 == -1 && users.find(*it1) != users.end())) {
                        SetuidFunctionParams params;
                        params.push_back(*it1);
                        cs.insert(Call(sf, params));
                    }
                }
                break;
            case Seteuid:
                for (ParamSet::const_iterator it1 = params.begin(); it1 != ie; ++it1) {
                    // The seteuid() function does not support "don't-care";
                    // however, if -1 is to be treated as a UID, then it must be
                    // supported
                    if (*it1 != -1 ||
                        (*it1 == -1 && users.find(*it1) != users.end())) {
                        SetuidFunctionParams params;
                        params.push_back(*it1);
                        cs.insert(Call(sf, params));
                    }
                }
                break;
            case Setreuid:
                for (ParamSet::const_iterator it1 = params.begin(); it1 != ie; ++it1) {
                    for (ParamSet::const_iterator it2 = params.begin(); it2 != ie; ++it2) {
                        SetuidFunctionParams params;
                        params.push_back(*it1);
                        params.push_back(*it2);
                        cs.insert(Call(sf, params));
                    }
                }
                break;

#if HAS_SETRESUID

            case Setresuid:
                for (ParamSet::const_iterator it1 = params.begin(); it1 != ie; ++it1) {
                    for (ParamSet::const_iterator it2 = params.begin(); it2 != ie; ++it2) {
                        for (ParamSet::const_iterator it3 = params.begin(); it3 != ie; ++it3) {
                            SetuidFunctionParams params;
                            params.push_back(*it1);
                            params.push_back(*it2);
                            params.push_back(*it3);
                            cs.insert(Call(sf, params));
                        }
                    }
                }
                break;

#endif

            }
        }
        return cs;
    }
};

struct PrivEdgeGenerator :
    public EdgeGenerator<UID, SetuidFunctionParam, Call> {
    typedef UID VertexInputItem;
    typedef SetuidFunctionParam EdgeInputItem;
    typedef Call OutputItem;

    typedef std::set<VertexInputItem> VertexInputCollection;
    typedef std::set<EdgeInputItem> EdgeInputCollection;
    typedef std::set<OutputItem> OutputCollection;

    virtual ~PrivEdgeGenerator() {}

    virtual OutputCollection generateAll(
        VertexInputCollection const& users,
        EdgeInputCollection const& extraParams) const {
        ParamSet params(unifyInputs(users, extraParams));
        CallSet cs;

        // Generate all valid priv-type function calls
        ParamSet::const_iterator ie = params.end();
        for (int f = DropPrivPerm; f < PrivFunctionEnd; ++f) {
            SetuidFunction pf = static_cast<SetuidFunction>(f);

            switch (pf) {
            default:
                ASSERT(false);
                break;
            case DropPrivPerm:
            case DropPrivTemp:
            case RestorePriv:
                for (ParamSet::const_iterator it1 = params.begin(); it1 != ie; ++it1) {
                    // Priv functions do not support "don't-care";
                    // however, if -1 is to be treated as a UID, then it must be
                    // supported
                    if (*it1 != -1 ||
                        (*it1 == -1 && users.find(*it1) != users.end())) {
                        SetuidFunctionParams params;
                        params.push_back(*it1);
                        cs.insert(Call(pf, params));
                    }
                }
                break;
            }
        }
        return cs;
    }
};

struct SetuidPrivEdgeGenerator :
    public EdgeGenerator<UID, SetuidFunctionParam, Call> {
    typedef UID VertexInputItem;
    typedef SetuidFunctionParam EdgeInputItem;
    typedef Call OutputItem;

    typedef std::set<VertexInputItem> VertexInputCollection;
    typedef std::set<EdgeInputItem> EdgeInputCollection;
    typedef std::set<OutputItem> OutputCollection;

    virtual ~SetuidPrivEdgeGenerator() {}

    virtual OutputCollection generateAll(
        VertexInputCollection const& users,
        EdgeInputCollection const& extraParams) const {
        CallSet callSet,
            setuidCallSet(
                SetuidEdgeGenerator().generateAll(users, extraParams)),
            privCallSet(PrivEdgeGenerator().generateAll(users, extraParams));
        std::vector<Call> calls(setuidCallSet.size() + privCallSet.size()),
            setuidCalls, privCalls;

        std::copy(
            setuidCallSet.begin(),
            setuidCallSet.end(),
            std::back_inserter(setuidCalls));
        std::copy(
            privCallSet.begin(),
            privCallSet.end(),
            std::back_inserter(privCalls));

        std::sort(setuidCalls.begin(), setuidCalls.end());
        std::sort(privCalls.begin(), privCalls.end());
        std::set_union(
            setuidCalls.begin(),
            setuidCalls.end(),
            privCalls.begin(),
            privCalls.end(),
            calls.begin());

        // If the set intersection is non-empty, then there will be extra
        // elements containing an "invalid call" value that should be removed
        // before constructing the union std::set
        while (calls.size() > 0 && calls.back() == Call()) {
            calls.pop_back();
        }

        std::copy(
            calls.begin(),
            calls.end(),
            std::inserter(callSet, callSet.begin()));

        return callSet;
    }
};

struct Priv2EdgeGenerator :
    public EdgeGenerator<UID, SetuidFunctionParam, Priv2Call> {
    typedef UID VertexInputItem;
    typedef SetuidFunctionParam EdgeInputItem;
    typedef Priv2Call OutputItem;

    typedef std::set<VertexInputItem> VertexInputCollection;
    typedef std::set<EdgeInputItem> EdgeInputCollection;
    typedef std::set<OutputItem> OutputCollection;

    virtual ~Priv2EdgeGenerator() {}

    virtual OutputCollection generateAll(
        VertexInputCollection const& users,
        EdgeInputCollection const& extraParams) const {
        ParamSet params(unifyInputs(users, extraParams));
        Priv2CallSet cs;

        // TODO: Still need to implement testing supplementary groups; use
        // current value for now:
        // - gids: -1, 0, 1
        // - sups:
        //     empty sups group,
        //     current sups group (if not empty),
        //     current sups group + 1 more group
        std::vector<GID> gids;
        gids.push_back(static_cast<GID>(-1));
        gids.push_back(static_cast<GID>(0));
        gids.push_back(static_cast<GID>(1));
        SupGroups sg = SupGroups::get();
        std::vector<SupGroups> supGroups;
        if (sg.size() != 0) {
            supGroups.push_back(SupGroups());
        }
        supGroups.push_back(sg);
        GID extraGroup = 0;
        while (std::find(
                   sg.list.begin(),
                   sg.list.end(),
                   extraGroup) != sg.list.end()) {
            ++extraGroup;
        }
        sg.list.push_back(extraGroup);
        supGroups.push_back(sg);

        // Generate all valid priv2-type function calls
        for (int f = AssumeIDPerm; f < Priv2FunctionEnd; ++f) {
            Priv2Function pf = static_cast<Priv2Function>(f);

            switch (pf) {
            default:
                ASSERT(false);
                break;
            case AssumeIDPerm:
            case AssumeIDTemp:
                for (ParamSet::const_iterator
                         it1 = params.begin(), ie1 = params.end();
                     it1 != ie1; ++it1) {
                    for (std::vector<GID>::const_iterator
                             it2 = gids.begin(), ie2 = gids.end();
                         it2 != ie2; ++it2) {
                        for (std::vector<SupGroups>::const_iterator
                                 it3 = supGroups.begin(), ie3 = supGroups.end();
                             it3 != ie3; ++it3) {
                            Priv2FunctionParams params;
                            params.push_back(Priv2FunctionParam(
                                                 *it1,
                                                 *it2,
                                                 *it3));
                            cs.insert(Priv2Call(pf, params));
                        }
                    }
                }

                //     for (ParamSet::const_iterator it2 = params.begin();
                //          it2 != ie; ++it2) {

                //         // TODO: Still need to implement testing supplementary
                //         // groups; for now, just set uid+gid
                //         Priv2FunctionParams params;
                //         params.push_back(Priv2FunctionParam(
                //                              *it1,     // Generated uid
                //                              *it2,     // Generated gid
                //                              supGroups // Current sup-groups
                //                              ));
                //         cs.insert(Priv2Call(pf, params));
                //     }
                // }
                break;
            }
        }
        return cs;
    }
};

// Graph implementation

template<typename VertexProperty, typename EdgeProperty, typename VertexGeneratorType, typename EdgeGeneratorType>
SetuidStateGraph<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::SetuidStateGraph(
    VertexGeneratorType const& generator,
    typename VertexGeneratorType::InputCollection const& ic,
    VertexProperty const& _start) :
    start(_start) {
    typename VertexGeneratorType::OutputCollection vs =
        generator.generateAll(ic);
    ASSERT(vs.find(start) != vs.end());
    pred = PredecessorList(vs.size());
    dist = DistanceList(vs.size());
    for (typename VertexGeneratorType::OutputCollection::const_iterator it =
             vs.begin(), ie = vs.end(); it != ie; ++it) {
        vPropMap.insert(std::make_pair(*it, add_vertex(*it, g)));
    }
    computeShortestPaths();
}

template<typename VertexProperty, typename EdgeProperty, typename VertexGeneratorType, typename EdgeGeneratorType>
SetuidStateGraph<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::SetuidStateGraph(
    SetuidStateGraph<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType> const& ssg,
    VertexProperty const& _start) :
    g(ssg.g),
    start(_start),
    vPropMap(ssg.vPropMap) {
    unsigned const numVertices = boost::num_vertices(g);
    pred = PredecessorList(numVertices);
    dist = DistanceList(numVertices);
    computeShortestPaths();
}

template<typename VertexProperty, typename EdgeProperty, typename VertexGeneratorType, typename EdgeGeneratorType>
typename SetuidStateGraph<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::Path
SetuidStateGraph<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::getPath(
    VertexProperty const& sv) const {
    typename VertexPropertyMap::const_iterator vCurrentItr = vPropMap.find(sv);
    ASSERT(vCurrentItr != vPropMap.end());
    Path path;
    Vertex vCurrent = vCurrentItr->second, vPrevious;
    for (vPrevious = pred.at(get(boost::vertex_index, g, vCurrent));
         vCurrent != vPrevious;
         vPrevious = pred.at(get(boost::vertex_index, g, vCurrent))) {

        std::pair<
            typename Graph::out_edge_iterator,
            typename Graph::out_edge_iterator>
            edges = boost::edge_range(vPrevious, vCurrent, g);
        ASSERT(edges.first != edges.second);
        path.push_front(PathStep(
                            get(boost::edge_bundle, g, *edges.first),
                            get(boost::vertex_bundle, g, vCurrent)));

        vCurrent = vPrevious;
    }
    typename VertexPropertyMap::const_iterator startItr = vPropMap.find(start);
    ASSERT(vPropMap.find(start) != vPropMap.end() &&
           vCurrent == startItr->second);
    return path;
}

template<typename VertexProperty, typename EdgeProperty, typename VertexGeneratorType, typename EdgeGeneratorType>
void SetuidStateGraph<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::addEdge(
    VertexProperty const& v1,
    VertexProperty const& v2,
    EdgeProperty const& e) {
    typename VertexPropertyMap::const_iterator itr1 = vPropMap.find(v1);
    typename VertexPropertyMap::const_iterator itr2 = vPropMap.find(v2);
    ASSERT(itr1 != vPropMap.end() && itr2 != vPropMap.end());
    add_edge(itr1->second, itr2->second, e, g);
    computeShortestPaths();
}

template<typename VertexProperty, typename EdgeProperty, typename VertexGeneratorType, typename EdgeGeneratorType>
typename SetuidStateGraph<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::Vertex const&
SetuidStateGraph<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::getVertex(
    VertexProperty const& vp) const {
    typename VertexPropertyMap::const_iterator vpItr = vPropMap.find(vp);
    ASSERT(vpItr != vPropMap.end());
    return vpItr->second;
}

template<typename VertexProperty, typename EdgeProperty, typename VertexGeneratorType, typename EdgeGeneratorType>
typename SetuidStateGraph<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::EdgeIteratorPair const
SetuidStateGraph<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::getEdges(
    VertexProperty const& v1,
    VertexProperty const& v2) const {
    typename VertexPropertyMap::const_iterator itr1 = vPropMap.find(v1);
    typename VertexPropertyMap::const_iterator itr2 = vPropMap.find(v2);
    ASSERT(itr1 != vPropMap.end() && itr2 != vPropMap.end());
    return edge_range(itr1->second, itr2->second, g);
}

template<typename VertexProperty, typename EdgeProperty, typename VertexGeneratorType, typename EdgeGeneratorType>
VertexProperty const& SetuidStateGraph<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::getPredecessor(
    VertexProperty const& v) const {
    typename VertexPropertyMap::const_iterator vItr = vPropMap.find(v);
    ASSERT(vItr != vPropMap.end());
    Vertex vertex = vItr->second;
    Vertex predecessorVertex = pred.at(get(boost::vertex_index, g, vertex));
    VertexProperty const& predecessor = get(
        boost::vertex_bundle,
        g,
        predecessorVertex);
    return predecessor;
}

template<typename VertexProperty, typename EdgeProperty, typename VertexGeneratorType, typename EdgeGeneratorType>
void SetuidStateGraph<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::computeShortestPaths() {
    ASSERT(vPropMap.find(start) != vPropMap.end());
    boost::dijkstra_shortest_paths(
        g,
        vPropMap[start],
        weight_map(get(&EdgeProperty::weight, g)).
        predecessor_map(make_iterator_property_map(
                            pred.begin(), get(boost::vertex_index, g))).
        distance_map(make_iterator_property_map(
                         dist.begin(), get(boost::vertex_index, g))));
}
