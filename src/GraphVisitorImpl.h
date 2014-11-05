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

#include <boost/graph/adjacency_list.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <utility>

char const* GraphVisitorError::what() const throw() {
    return msg.c_str();
}

template<typename Graph>
VertexVisitorError<Graph>::VertexVisitorError(
    std::string _desc,
    VertexProperty _v) :
    desc(_desc), v(_v) {
    std::stringstream ss;
    ss << desc << std::endl;
    ss << "  " << v << std::endl;
    msg = ss.str();
}

template<typename Graph>
EdgeVisitorError<Graph>::EdgeVisitorError(
    std::string _desc,
    EdgeProperty _e,
    VertexProperty _v1,
    VertexProperty _v2) :
    desc(_desc), e(_e), v1(_v1), v2(_v2) {
    std::stringstream ss;
    ss << desc << std::endl;
    ss << "  " << e << std::endl;
    ss << "  " << v1 << std::endl;
    ss << "  " << v2 << std::endl;
    msg = ss.str();
}

template<typename Graph>
template<typename EdgeType, typename GraphType>
void SetuidStateEdgeVisitor<Graph>::examine_edge(
    EdgeType e,
    GraphType& g) {
    Vertex v1 = source(e, g), v2 = target(e, g);
    try {
        visitEdge(g[e], g[v1], g[v2]);
    } catch (EdgeVisitorError<Graph>& gve) {
        std::cerr << "Graph visitor error:" << std::endl;

        std::string empty("");

        ASSERT(empty != gve.what());

        std::cerr << gve.what() << std::endl;
    }
}

template<typename Graph>
template<typename VertexType, typename GraphType>
void SetuidStateVertexVisitor<Graph>::examine_vertex(
    VertexType v,
    GraphType& g) {
    try {
        visitVertex(g[v]);
    } catch (VertexVisitorError<Graph>& gve) {
        std::cerr << "Graph visitor error:" << std::endl;
        ASSERT(std::string("") != gve.what());
        std::cerr << gve.what() << std::endl;
    }
}
