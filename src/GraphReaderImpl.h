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

#include <boost/archive/text_iarchive.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

#include <fstream>

template<typename Graph>
Graph ArchiveReader<Graph>::read(std::string const& name) const {
    std::ifstream ifsa((name + ".archive").c_str(), std::ofstream::in);
    Graph graph;
    {
        boost::archive::text_iarchive ia(ifsa);
        ia >> graph;
    }
    return graph;
}

template<typename Graph>
Graph ArchiveReader<Graph>::read(GraphName const& name) const {
    return read(name.getName());
}
