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
#include "GraphReader.h"
#include "GraphVerification.h"
#include "GraphWriter.h"
#include "Platform.h"
#include "SetuidState.h"
#include "Util.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>

#include <fstream>

using namespace boost;

typedef SetuidState VP;
typedef SetuidFunctionCall EP;
typedef VertexGenerator<UID, SetuidState> VG;
typedef SetuidEdgeGenerator EG;

typedef SetuidStateGraph<VP, EP, VG, EG> Graph;
typedef Normalizer<Graph> NormalizerType;

template<typename Visitor>
static Graph bfsTransform(
    Graph& graph,
    Graph::Vertex const& startVertex,
    UIDSet const& uidSet) {
    Graph::VertexPropertyType const& startState =
        get(vertex_bundle, graph.getGraph(), startVertex);
    UIDMap mapping = NormalizerType::generateUIDMap(uidSet);
    UIDSet newUIDSet = NormalizerType::generateUIDSet(mapping);
    SetuidState newStartState = NormalizerType::mapState(mapping, startState);
    Graph rtn(VertexGenerator<UID, SetuidState>(), newUIDSet, newStartState);
    breadth_first_search(
        graph.getGraph(),
        startVertex,
        visitor(Visitor(rtn, mapping)));
    return rtn;
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

    std::string basename(argv[1]);

    for (int i = 2; i < argc; ++i) {
        uids.insert(stoi(argv[i], NULL, 10));
    }
    ParamSet extraParams;
    extraParams.insert(-1); // Don't-care value

    GraphName inName(basename, uids, extraParams);
    Graph graph = ArchiveReader<Graph>().read(inName);

    Graph::Vertex const& startVertex =
        graph.getVertex(graph.getStart());
    Graph newGraph =
        bfsTransform<NormalizerType>(graph, startVertex, uids);

    GraphName outName(basename + "__Normalized", uids, extraParams);
    std::cout << "Writing archive..." << std::endl;
    ArchiveWriter<Graph>().write(newGraph, outName);
    std::cout << "Writing graphviz..." << std::endl;
    DotWriter<Graph>().write(newGraph, outName);

    return 0;
}
