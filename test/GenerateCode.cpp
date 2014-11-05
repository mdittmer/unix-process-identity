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
#include "GraphWriter.h"
#include "ModuleCodeGen.h"
#include "Platform.h"
#include "SetuidState.h"
#include "Util.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

typedef SetuidState VP;
typedef SetuidFunctionCall EP;
typedef VertexGenerator<UID, SetuidState> VG;
typedef SetuidEdgeGenerator EG;

typedef SetuidStateGraph<VP, EP, VG, EG> Graph;

int main(int argc, char* argv[]) {
    std::vector<std::string> names;
    UIDSet uids;

    if (argc < 2) {
        std::cerr
            << "ERROR: Must have at least one argument: archive-file-basename"
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
        ModuleGenerator<Graph> generator(graph);

        std::stringstream sourceName;
        sourceName << *it;
        sourceName << ".c";
        std::ofstream sourceStream(
            sourceName.str().c_str(),
            std::ofstream::out);
        generator.streamSource(sourceStream);

        std::stringstream headerName;
        headerName << *it;
        headerName << ".h";
        std::ofstream headerStream(
            headerName.str().c_str(),
            std::ofstream::out);
        generator.streamHeader(headerStream);
    }

    return 0;
}