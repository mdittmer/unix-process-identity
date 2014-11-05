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

#include <utility>

template<typename Graph>
void UIDAccumulatorVisitor<Graph>::visitVertex(VertexProperty const& v) {
    if (uids.find(v.ruid) == uids.end()) {
        uids.insert(v.ruid);
    }
    if (uids.find(v.euid) == uids.end()) {
        uids.insert(v.euid);
    }
    if (uids.find(v.svuid) == uids.end()) {
        uids.insert(v.svuid);
    }
}

template<typename Graph>
void VertexAccumulatorVisitor<Graph>::visitVertex(VertexProperty const& v) {
    if (vertices.find(v) == vertices.end()) {
        vertices.insert(v);
    }
}

template<typename Graph>
void EdgeAccumulatorVisitor<Graph>::visitEdge(
    EdgeProperty const& e,
    VertexProperty const& v1,
    VertexProperty const& v2) {
    if (edges.find(e) == edges.end()) {
        edges.insert(e);
    }
}

template<typename Graph>
void PrivJumpAccumulatorVisitor<Graph>::visitVertex(VertexProperty const& v) {
    Vertex _v = g.getVertex(v);
    typename Graph::Graph const& boostGraph = g.getGraph();
    // Only interested in <x, y, z>: y != 0 and (x = 0 or z = 0)
    if (!(v.euid != 0 && (v.ruid == 0 || v.svuid == 0))) {
        return;
    }
    // Store jumps to <a, 0, c>
    std::vector<typename Graph::Edge> candidateJumps;
    std::pair<
        typename Graph::Graph::out_edge_iterator,
        typename Graph::Graph::out_edge_iterator>
        i = boost::out_edges(_v, boostGraph);
    // Find jumps to <a, 0, c>
    for (; i.first != i.second; ++i.first) {
        Vertex vCandidate = target(*i.first, boostGraph);
        VertexProperty const& vpCandidate = boostGraph[vCandidate];
        if (vpCandidate.euid == 0) {
            candidateJumps.push_back(*i.first);
        }
    }
    // Find jumps from <a, 0, c> back to original <x, y, z>
    bool foundJumpBack = false;
    for (typename std::vector<typename Graph::Edge>::iterator
             it = candidateJumps.begin(), ie = candidateJumps.end();
         it != ie; ++it) {
        i = boost::out_edges(target(*it, boostGraph), boostGraph);
        for (; i.first != i.second; ++i.first) {
            if (target(*i.first, boostGraph) == _v) {
                // Store mapping from <x, y, z> to  <a, 0, c>; hints at what
                // system calls to make to temporarily elevate privilege
                jumps.insert(std::make_pair(
                                 v,
                                 boostGraph[ source(*i.first, boostGraph) ] ));
                foundJumpBack = true;
                break;
            }
        }
        if (foundJumpBack) {
            break;
        }
    }
    ASSERT(foundJumpBack);
}

template<typename Graph>
void EdgeMapAccumulatorVisitor<Graph>::visitEdge(
    EdgeProperty const& e,
    VertexProperty const& v1,
    VertexProperty const& v2) {
    StatePair pair(v1, v2);
    typename EdgeMap::iterator mapIt = edgeMap.find(pair);
    if (mapIt == edgeMap.end()) {
        edgeMap.insert(std::make_pair(pair, EdgeSet()));
        edgeMap.find(pair)->second.insert(e);
    } else if (mapIt->second.find(e) == mapIt->second.end()) {
        mapIt->second.insert(e);
    }
}
