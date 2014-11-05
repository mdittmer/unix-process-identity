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
#include "GraphVerification.h"
#include "GraphReader.h"
#include "Util.h"

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

int main(int argc, char* argv[]) {
    std::vector<std::string> names;
    UIDSet uids;

    if (argc < 2) {
        std::cerr << "ERROR: Must have at least one argument: archive-file-basename"
                  << std::endl;
        return -1;
    }

    for (int i = 1; i < argc; ++i) {
        names.push_back(argv[i]);
    }

    ArchiveReader<Graph> reader;
    for (std::vector<std::string>::const_iterator it = names.begin(),
             ie = names.end(); it != ie; ++it) {
        Graph const graph = reader.read(*it);
        Graph::Vertex const& start =
            graph.getVertex(graph.getStart());

        std::cerr << std::endl << " :: Verifying \"" << *it << "\""
                  << std::endl;

        // Clean tests; other than FreeBSD's out-of-date setuid(), these tests
        // should report no errors
        visitGraph< VISITOR(SetuidTautology) >(graph, start);
        visitGraph< VISITOR(SeteuidTautology) >(graph, start);
        visitGraph< VISITOR(SetreuidCleanTautology) >(graph, start);
        visitGraph< VISITOR(SetresuidTautology) >(graph, start);

        // Debug tests: These are used to expose irregularities with respect
        // to "normal behaviour"

        // Test actual setreuid() standard (there are many failures)
        // visitGraph< VISITOR(SetreuidTautology) >(graph, start);

        // visitGraph< VISITOR(SetreuidForDropPrivPerm) >(graph, start);

        // Test: setuid(): euid=0 does not imply NOT appropriate privileges
        // (this should always pass)
        // visitGraph< VISITOR(SetuidRootAP) >(graph, start);

        // Test: setuid(): euid!=0 does not imply appropriate privileges (this
        // will fail in cases where appropriate privileges is more complicated)
        // visitGraph< VISITOR(SetuidNonRootNAP) >(graph, start);

        // visitGraph< VISITOR(SeteuidRootAP) >(graph, start);
        // visitGraph< VISITOR(SeteuidNonRootNAP) >(graph, start);
    }

    return 0;
}
