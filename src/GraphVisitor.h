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

#include <boost/graph/breadth_first_search.hpp>

class GraphVisitorError : public std::exception {
public:
    virtual ~GraphVisitorError() throw() {}

    virtual char const* what() const throw();

protected:
    std::string msg;
};

template<typename Graph>
class VertexVisitorError : public GraphVisitorError {
public:
    typedef typename Graph::VertexPropertyType VertexProperty;

    VertexVisitorError(
        std::string _desc,
        VertexProperty _v);

    virtual ~VertexVisitorError() throw() {}

    std::string const desc;
    VertexProperty const v;
};

template<typename Graph>
class EdgeVisitorError : public GraphVisitorError {
public:
    typedef typename Graph::VertexPropertyType VertexProperty;
    typedef typename Graph::EdgePropertyType EdgeProperty;

    EdgeVisitorError(
        std::string _desc,
        EdgeProperty _e,
        VertexProperty _v1,
        VertexProperty _v2);

    virtual ~EdgeVisitorError() throw() {}

    std::string const desc;
    EdgeProperty const e;
    VertexProperty const v1;
    VertexProperty const v2;
};

template<typename Graph>
class SetuidStateEdgeVisitor : public boost::default_bfs_visitor {
public:
    typedef typename Graph::Vertex Vertex;
    typedef typename Graph::VertexPropertyType VertexProperty;
    typedef typename Graph::EdgePropertyType EdgeProperty;

    virtual ~SetuidStateEdgeVisitor() {}

    template<typename EdgeType, typename GraphType>
    void examine_edge(EdgeType e, GraphType& g);

    virtual void visitEdge(
        EdgeProperty const& e,
        VertexProperty const& v1,
        VertexProperty const& v2) = 0;
};

template<typename Graph>
class SetuidStateVertexVisitor : public boost::default_bfs_visitor {
public:
    typedef typename Graph::Vertex Vertex;
    typedef typename Graph::VertexPropertyType VertexProperty;
    typedef typename Graph::EdgePropertyType EdgeProperty;

    virtual ~SetuidStateVertexVisitor() {}

    template<typename VertexType, typename GraphType>
    void examine_vertex(VertexType v, GraphType& g);

    virtual void visitVertex(VertexProperty const& v) = 0;
};

#define BASIC_VISITOR_NAME(_type, _class_kind)          \
    template<typename Graph>                            \
    class _class_kind ## Visitor :                      \
        public SetuidState ## _type ## Visitor< Graph >

#define BASIC_VISITOR_TYPEDEFS                                          \
    typedef typename Graph::Vertex Vertex;                              \
    typedef typename Graph::VertexPropertyType VertexProperty;          \
    typedef typename Graph::EdgePropertyType EdgeProperty;

#define EDGE_VISITOR_METHOD                     \
    virtual void visitEdge(                     \
        EdgeProperty const& fc,                 \
        VertexProperty const& s1,               \
        VertexProperty const& s2);

#define EDGE_VISITOR_BODY                       \
    public:                                     \
    BASIC_VISITOR_TYPEDEFS                      \
    EDGE_VISITOR_METHOD

#define EDGE_VISITOR_CLASS(_class_kind)         \
    BASIC_VISITOR_NAME(Edge, _class_kind) {     \
        EDGE_VISITOR_BODY;                      \
                                                \
    public:                                     \
        _class_kind ## Visitor(Graph const&) {} \
        virtual ~_class_kind ## Visitor() {}    \
    };

#define VERTEX_VISITOR_METHOD                           \
    virtual void visitVertex(VertexProperty const& s);  \

#define VERTEX_VISITOR_BODY                     \
    public:                                     \
    BASIC_VISITOR_TYPEDEFS                      \
    VERTEX_VISITOR_METHOD

#define VERTEX_VISITOR_CLASS(_class_kind)       \
    BASIC_VISITOR_NAME(Vertex, _class_kind) {   \
        VERTEX_VISITOR_BODY;                    \
                                                \
    public:                                     \
        _class_kind ## Visitor(Graph const&) {} \
        virtual ~_class_kind ## Visitor() {}    \
    };

#include "GraphVisitorImpl.h"
