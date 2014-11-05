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

#include "GraphVisitor.h"

#include <boost/graph/breadth_first_search.hpp>

#include <map>
#include <set>
#include <utility>

BASIC_VISITOR_NAME(Vertex, UIDAccumulator) {
    VERTEX_VISITOR_BODY;

public:
    typedef std::set<UID> UIDs;

    UIDAccumulatorVisitor(UIDs& _uids) : uids(_uids) {}
    virtual ~UIDAccumulatorVisitor() {}

private:
    UIDs& uids;
};

BASIC_VISITOR_NAME(Vertex, VertexAccumulator) {
    VERTEX_VISITOR_BODY;

public:
    typedef std::set<typename Graph::VertexPropertyType> Vertices;

    VertexAccumulatorVisitor(Vertices& _vertices) : vertices(_vertices) {}
    virtual ~VertexAccumulatorVisitor() {}

private:
    Vertices& vertices;
};

BASIC_VISITOR_NAME(Edge, EdgeAccumulator) {
    EDGE_VISITOR_BODY;

public:
    typedef std::set<typename Graph::EdgePropertyType> Edges;

    EdgeAccumulatorVisitor(Edges& _edges) : edges(_edges) {}
    virtual ~EdgeAccumulatorVisitor() {}

private:
    Edges& edges;
};

BASIC_VISITOR_NAME(Vertex, PrivJumpAccumulator) {
    VERTEX_VISITOR_BODY;

public:
    typedef std::map< typename Graph::VertexPropertyType, typename Graph::VertexPropertyType > Jumps;

    PrivJumpAccumulatorVisitor(Graph const& _g, Jumps& _jumps) :
        g(_g),
        jumps(_jumps) {}
    virtual ~PrivJumpAccumulatorVisitor() {}

private:
    Graph const& g;
    Jumps& jumps;
};

BASIC_VISITOR_NAME(Edge, EdgeMapAccumulator) {
    EDGE_VISITOR_BODY;

public:
    typedef std::pair<SetuidState, SetuidState> StatePair;
    typedef std::set<typename Graph::EdgePropertyType> EdgeSet;
    typedef std::map<StatePair, EdgeSet> EdgeMap;

    EdgeMapAccumulatorVisitor(EdgeMap& _edgeMap) : edgeMap(_edgeMap) {}
    virtual ~EdgeMapAccumulatorVisitor() {}

private:
    EdgeMap& edgeMap;
};

template<typename Graph>
class VertexEdgeAccumulator {
public:
    typedef std::set<UID> UIDs;
    typedef std::set<typename Graph::VertexPropertyType> Vertices;
    typedef std::set<typename Graph::EdgePropertyType> Edges;
    typedef std::map< typename Graph::VertexPropertyType, typename Graph::VertexPropertyType > Jumps;

    UIDs generateUIDs(
        Graph const& graph,
        typename Graph::Vertex start) const {
        UIDs uids;
        boost::breadth_first_search(
            graph.getGraph(),
            start,
            boost::visitor(UIDAccumulatorVisitor<Graph>(uids)));
        return uids;
    }
    Vertices generateVertices(
        Graph const& graph,
        typename Graph::Vertex start) const {
        Vertices vertices;
        boost::breadth_first_search(
            graph.getGraph(),
            start,
            boost::visitor(VertexAccumulatorVisitor<Graph>(vertices)));
        return vertices;
    }
    Jumps generatePrivJumps(
        Graph const& graph,
        typename Graph::Vertex start) const {
        Jumps jumps;
        boost::breadth_first_search(
            graph.getGraph(),
            start,
            boost::visitor(PrivJumpAccumulatorVisitor<Graph>(graph, jumps)));
        return jumps;
    }
    Edges generateEdges(
        Graph const& graph,
        typename Graph::Vertex const& start) const {
        Edges edges;
        boost::breadth_first_search(
            graph.getGraph(),
            start,
            boost::visitor(EdgeAccumulatorVisitor<Graph>(edges)));
        return edges;
    }
};

template<typename Graph>
class EdgeMapAccumulator {
public:
    typedef std::pair<SetuidState, SetuidState> StatePair;
    typedef std::set<typename Graph::EdgePropertyType> EdgeSet;
    typedef std::map<StatePair, EdgeSet> EdgeMap;

    EdgeMap generateEdges(
        Graph const& graph,
        typename Graph::Vertex const& start) const {
        EdgeMap em;
        boost::breadth_first_search(
            graph.getGraph(),
            start,
            boost::visitor(EdgeMapAccumulatorVisitor<Graph>(em)));
        return em;
    }
};

#include "VisitorAccumulatorImpl.h"
