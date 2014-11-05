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


#include "Platform.h"
#include "SetuidState.h"
#include "Graph.h"
#include "GraphExplorer.h"
#include "GraphVerification.h"
#include "GraphWriter.h"
#include "Util.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>

#include <fstream>

// typedef boost::default_bfs_visitor GraphVisitor;
// typedef std::vector<int> GraphVisitors;

typedef SetuidState VP;
typedef SetuidFunctionCall EP;
typedef VertexGenerator<UID, SetuidState> VG;
typedef SetuidPrivEdgeGenerator EG;

typedef SetuidStateGraph<VP, EP, VG, EG> Graph;
typedef SetuidStateCallGraphExplorer<VG, EG> Explorer;

template<typename Visitor>
static void visitGraph(
    Graph const& graph,
    Graph::Vertex const& start) {
    boost::breadth_first_search(
        graph.getGraph(),
        start,
        boost::visitor(Visitor()));
}

int main(int argc, char* argv[]) {
    UIDSet uids;

    if (argc < 3) {
        std::cerr << "ERROR: Must have at least two arguments: basename and a UID"
                  << std::endl;
        return -1;
    }

    try {
        stoi(argv[1], NULL, 10);
        std::cerr << "ERROR: First argument must not be an integer"
                  << std::endl;
        return -1;
    } catch (...) {}

    std::string basename(std::string(argv[1]) + "_mixed");

    if (basename.find("/") != std::string::npos) {
        std::cerr << "ERROR: Basename may not contain path separator"
                  << std::endl;
        return -1;
    }

    for (int i = 2; i < argc; ++i) {
        uids.insert(stoi(argv[i], NULL, 10));
    }
    ParamSet extraParams;
    extraParams.insert(-1); // Don't-care value
    SetuidState startState = SetuidState::get();
    Explorer explorer(
        Graph(VG(), uids, startState),
        EG(),
        uids,
        extraParams);

    explorer.exploreAll();

    Graph const& graph = explorer.getGraph();
    GraphName name(basename, uids, extraParams);
    ArchiveWriter<Graph>().write(graph, name);
    DotWriter<Graph>().write(graph, name);

    return 0;
}
