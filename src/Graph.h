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

#include "SetuidState.h"

#include <map>
#include <set>
#include <utility>
#include <vector>

#include <boost/graph/adjacency_list.hpp>

namespace boost { namespace serialization { class access; } }

typedef std::set<UID> UIDSet;
typedef std::set<SetuidFunctionParam> ParamSet;
typedef std::set<SetuidState> StateSet;

struct Call {
    friend class boost::serialization::access;

    // Default call is an "invalid call" value
    Call() : function(SetuidInvalid), params() {}
    Call(SetuidFunction const& _function, SetuidFunctionParams const& _params) :
        function(_function), params(_params) {}

    SetuidFunction function;
    SetuidFunctionParams params;

    bool operator<(Call const& c) const {
        return ((static_cast<int>(function) < static_cast<int>(c.function)) ||
                (function == c.function && params < c.params));
    }

    bool operator==(Call const& c) const {
        return (function == c.function && params == c.params);
    }

private:
    template<class Archive>
    void serialize(Archive& ar, unsigned int const version) {
        ar & function;
        ar & params;
    }
};
typedef std::set<Call> CallSet;

template<typename InputType, typename OutputType>
struct VertexGenerator {
    typedef InputType InputItem;
    typedef OutputType OutputItem;
    typedef std::set<InputItem> InputCollection;
    typedef std::set<OutputItem> OutputCollection;

    virtual OutputCollection generateAll(InputCollection const&) const;
};

template<typename VertexInputType, typename EdgeInputType, typename OutputType>
struct EdgeGenerator {
    typedef VertexInputType VertexInputItem;
    typedef EdgeInputType EdgeInputItem;
    typedef OutputType OutputItem;

    typedef std::set<VertexInputItem> VertexInputCollection;
    typedef std::set<EdgeInputItem> EdgeInputCollection;
    typedef std::set<OutputItem> OutputCollection;

    virtual ~EdgeGenerator() {}

    virtual OutputCollection generateAll(
        VertexInputCollection const&,
        EdgeInputCollection const&) const = 0;

protected:
    virtual EdgeInputCollection unifyInputs(
        VertexInputCollection const&,
        EdgeInputCollection const&) const;
};

// class SetuidStateGenerator {
// public:
//     virtual StateSet generateAll(UIDSet const& uids) const;
// };

// class SetuidCallGenerator {
// public:
//     virtual CallSet generateAll(
//         UIDSet const& users,
//         ParamSet const& extraParams) const;
// };

// TODO: This is now a misnomer, state other than uids can be stored here;
// should be renamed to something like "StateGraph" or "ProcessStateGraph"
template<
    typename VertexProperty,
    typename EdgeProperty,
    typename VertexGeneratorType,
    typename EdgeGeneratorType>
class SetuidStateGraph {
public:
    friend class boost::serialization::access;

    typedef VertexProperty VertexPropertyType;
    typedef EdgeProperty EdgePropertyType;
    typedef VertexGeneratorType VertexGeneratorTypename;
    typedef EdgeGeneratorType EdgeGeneratorTypename;

    typedef boost::adjacency_list<
        boost::multisetS,
        boost::vecS,
        boost::directedS,
        VertexProperty,
        EdgeProperty> Graph;

    typedef typename boost::graph_traits<Graph>::vertex_descriptor Vertex;
    typedef typename boost::graph_traits<Graph>::edge_descriptor Edge;
    typedef typename Graph::out_edge_iterator EdgeIterator;
    typedef std::pair<EdgeIterator, EdgeIterator> EdgeIteratorPair;
    typedef std::vector<Vertex> PredecessorList;
    typedef int Distance;
    typedef std::vector<Distance> DistanceList;

    struct PathStep {
        friend class boost::serialization::access;

        PathStep() : edge(), nextVertex() {}
        PathStep(EdgeProperty const& _edge, VertexProperty const& _nextVertex) :
            edge(_edge), nextVertex(_nextVertex) {}

        EdgeProperty edge;
        VertexProperty nextVertex;

        bool operator<(PathStep const& ps) const {
            return (edge < ps.edge ||
                    (edge == ps.edge && nextVertex < ps.nextVertex));
        }

    private:
    template<class Archive>
    void serialize(Archive& ar, unsigned int const version) {
        ar & edge;
        ar & nextVertex;
    }
    };

    typedef std::list<PathStep> Path;
    typedef std::map<VertexProperty, Vertex> VertexPropertyMap;

    SetuidStateGraph() {}

    SetuidStateGraph(
        VertexGeneratorType const& generator,
        typename VertexGeneratorType::InputCollection const& ic,
        VertexProperty const& _start);

    SetuidStateGraph(SetuidStateGraph const& ssg, VertexProperty const& _start);

    Path getPath(VertexProperty const&) const;
    void addEdge(
        VertexProperty const& v1,
        VertexProperty const& v2,
        EdgeProperty const& e);

    Vertex const& getVertex(VertexProperty const& vp) const;
    EdgeIteratorPair const getEdges(
        VertexProperty const& s1,
        VertexProperty const& s2) const;

    VertexProperty const& getPredecessor(VertexProperty const& v) const;

    VertexProperty const& getStart() const { return start; }

    Graph const& getGraph() const { return g; }

private:
    Graph g;
    VertexProperty start;
    VertexPropertyMap vPropMap;
    PredecessorList pred;
    DistanceList dist;

    void computeShortestPaths();

    template<class Archive>
    void serialize(Archive& ar, unsigned int const version) {
        ar & g;
        ar & start;
        ar & pred;
        ar & dist;
        ar & vPropMap;
    }
};

#include "GraphImpl.h"
