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

#include "Graph.h"
#include "GraphName.h"
#include "GraphVisitor.h"

#include <iostream>

template<typename Name>
struct LabelWriter {
    LabelWriter(Name _name) : name(_name) {}
    template<typename VertexOrEdge>
    void operator() (std::ostream& os, VertexOrEdge const& ve) {
        os << "[label=\"" << name[ve] << "\"]";
    }

private:
    Name name;
};

struct GraphWriter {
    void operator() (std::ostream& os) const {
        os << "graph [size=10,ratio=0.3]" << std::endl;
    }
};

template<typename Graph>
struct ArchiveWriter {
    void write(Graph const& graph, GraphName const& name) const;
};

template<typename Graph>
struct DotWriter {
    void write(Graph const& graph, GraphName const& name) const;
};

BASIC_VISITOR_NAME(Edge, CSVWriter) {
public:
    BASIC_VISITOR_TYPEDEFS;
    EDGE_VISITOR_METHOD;

    CSVWriterVisitor(Graph const& _g, std::ostream& _os) :
        g(_g), os(_os) { writeHeader(); }

private:
    Graph const& g;
    std::ostream& os;

    void writeHeader();
};

template<typename Graph>
struct CSVWriter {
    typedef typename Graph::VertexGeneratorTypename VertexGeneratorType;
    typedef typename Graph::EdgeGeneratorTypename EdgeGeneratorType;

    void write(Graph const& graph, std::string const& name) const;
};

#include "GraphWriterImpl.h"
