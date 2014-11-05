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
#include "Priv2State.h"
#include "SetuidState.h"
#include "Graph.h"
#include "GraphVerification.h"
#include "GraphReader.h"
#include "Util.h"
#include "VisitorAccumulator.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>

#include <iostream>
#include <fstream>

typedef SetuidState VP;
typedef SetuidFunctionCall EP;
typedef VertexGenerator<UID, SetuidState> VG;
typedef SetuidEdgeGenerator EG;

typedef SetuidStateGraph<VP, EP, VG, EG> Graph;

#define VISITOR(_name) _name ## Visitor<Graph>

template<typename Visitor>
static void visitGraph(
    Graph const& graph,
    Graph::Vertex const& start) {
        boost::breadth_first_search(
            graph.getGraph(),
            start,
            boost::visitor(Visitor(graph)));
}

template<typename Visitor>
static void visitGraph2(
    Graph const& graph,
    Graph::Vertex const& start,
    Priv2CallSet const& calls) {
        boost::breadth_first_search(
            graph.getGraph(),
            start,
            boost::visitor(Visitor(graph, calls)));
}

int main(int argc, char* argv[]) {
    std::string name;
    UIDSet uids;

    if (argc < 3) {
        std::cerr << "ERROR: Must have at least two argument: archive-file-basename uid1 [uid2 ...]"
                  << std::endl;
        return -1;
    }

    name = argv[1];
    for (int i = 2; i < argc; ++i) {
        uids.insert(stoi(argv[i], NULL, 10));
    }

    ArchiveReader<Graph> reader;
    Graph const graph = reader.read(name);
    Graph::Vertex const& start =
        graph.getVertex(graph.getStart());

    // Generate all priv2 calls based on UID set (don't bother with any
    // "extra params")
    Priv2CallSet calls = Priv2EdgeGenerator().generateAll(
        uids,
        Priv2EdgeGenerator::EdgeInputCollection());

    std::cerr << std::endl << " :: Verifying \"" << name << "\""
              << std::endl;

    visitGraph< VISITOR(GeneralSanity) >(graph, start);
    visitGraph2< VISITOR(Priv2Sanity) >(graph, start, calls);

    return 0;
}
