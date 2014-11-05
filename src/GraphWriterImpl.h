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


#include "Graph.h"
#include "GraphName.h"

#include <boost/archive/text_oarchive.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>

#include <fstream>

template<typename Graph>
void ArchiveWriter<Graph>::write(
    Graph const& graph,
    GraphName const& name) const {
    std::ofstream ofsa(
        std::string(name.getName() + ".archive").c_str(),
        std::ofstream::out);
    {
        boost::archive::text_oarchive oa(ofsa);
        oa << graph;
    }
}

template<typename Graph>
void DotWriter<Graph>::write(
    Graph const& graph,
    GraphName const& name) const {
    std::ofstream ofsg(
        std::string(name.getName() + ".dot").c_str(),
        std::ofstream::out);
    typename Graph::Graph const& boostGraph = graph.getGraph();
    LabelWriter<typename Graph::Graph> lwriter(boostGraph);
    GraphWriter gwriter;
    boost::write_graphviz(ofsg, boostGraph, lwriter, lwriter, gwriter);
}

#ifndef U2P
#define U2P(_uid) static_cast<SetuidFunctionParam>(_uid)
#endif
#ifndef P2U
#define P2U(_param) static_cast<UID>(_param)
#endif
#ifndef CP
#define CP(_uid, _param) ((_uid) == P2U(_param))
#endif

template<typename Graph>
void CSVWriterVisitor<Graph>::visitEdge(
    EdgeProperty const& e,
    VertexProperty const& v1,
    VertexProperty const& v2) {
    static unsigned const maxParams = 3;
    static int const missingValue = -99;
    unsigned count;
    os << U2P(v1.ruid) << "," << U2P(v1.euid) << "," << U2P(v1.svuid) << ",\""
       << e.function << "\",";
    count = 0;
    for (SetuidFunctionParams::const_iterator it = e.params.begin(),
             ie = e.params.end(); it != ie; ++it) {
        os << *it << ",";
        ++count;
    }
    for (; count < maxParams; ++count) {
        os << missingValue << ",";
    }
    os << e.rtn.value << "," << e.rtn.errNumber << ",";
    os << U2P(v2.ruid) << "," << U2P(v2.euid) << "," << U2P(v2.svuid) << ",";
    count = 0;
    os << CP(v1.ruid, 0) << ","
       << CP(v1.euid, 0) << ","
       << CP(v1.svuid, 0) << ",";
    for (SetuidFunctionParams::const_iterator it = e.params.begin(),
             ie = e.params.end(); it != ie; ++it) {
        os << CP(v1.ruid, *it) << ","
           << CP(v1.euid, *it) << ","
           << CP(v1.svuid, *it) << ",";
        ++count;
    }
    for (; count < maxParams; ++count) {
        os << missingValue << "," << missingValue << "," << missingValue << ",";
    }
    os << std::endl;
}

template<typename Graph>
void CSVWriterVisitor<Graph>::writeHeader() {
    os << "\"PreRuid\",\"PreEuid\",\"PreSvuid\",\"FunctionName\",\"Param1\","
       << "\"Param2\",\"Param3\",\"RtnValue\",\"RtnError\",\"PostRuid\","
       << "\"PostEuid\",\"PostSvuid\",\"RuidIsRoot\",\"EuidIsRoot\","
       << "\"SvuidIsRoot\",\"RuidIsParam1\",\"EuidIsParam1\",\"SvuidIsParam1\","
       << "\"RuidIsParam2\",\"EuidIsParam2\",\"SvuidIsParam2\","
       << "\"RuidIsParam3\",\"EuidIsParam3\",\"SvuidIsParam3\"," << std::endl;
}

template<typename Graph>
void CSVWriter<Graph>::write(
    Graph const& graph,
    std::string const& name) const {
    std::ofstream ofs(
        std::string(name + ".csv").c_str(),
        std::ofstream::out);
    boost::breadth_first_search(
        graph.getGraph(),
        graph.getVertex(graph.getStart()),
        boost::visitor(
            CSVWriterVisitor<Graph>(
                graph,
                ofs)));
}
