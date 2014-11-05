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

#include "Fork.h"
#include "Graph.h"

#include <set>
#include <queue>
#include <vector>

namespace boost { namespace serialization { class access; } }

template<
    typename VertexProperty,
    typename EdgeProperty,
    typename VertexGeneratorType,
    typename EdgeGeneratorType>
class ExploreCall;
template<
    typename VertexProperty,
    typename EdgeProperty,
    typename VertexGeneratorType,
    typename EdgeGeneratorType>
class ExploreState;

template<
    typename VertexProperty,
    typename EdgeProperty,
    typename VertexGeneratorType,
    typename EdgeGeneratorType>
class ExplorePriv2Call;

template<
    typename VertexProperty,
    typename EdgeProperty,
    typename VertexGeneratorType,
    typename EdgeGeneratorType>
class GraphExplorer {
    friend class ExploreCall<
        VertexProperty,
        EdgeProperty,
        VertexGeneratorType,
        EdgeGeneratorType>;
    friend class ExploreState<
        VertexProperty,
        EdgeProperty,
        VertexGeneratorType,
        EdgeGeneratorType>;

    friend class ExplorePriv2Call<
        VertexProperty,
        EdgeProperty,
        VertexGeneratorType,
        EdgeGeneratorType>;

public:
    typedef SetuidStateGraph<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType> Graph;
    typedef typename Graph::PathStep PathStep;
    typedef std::set<PathStep> PathStepSet;
    typedef typename Graph::Path Path;
    typedef ExploreCall<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType> ExploreCallType;
    typedef ExploreState<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType> ExploreStateType;
    typedef Fork<ExploreCallType> ForkExploreCall;
    typedef Fork<ExploreStateType> ForkExploreState;
    typedef std::vector<ForkExploreCall> ForkExploreCalls;
    typedef std::vector<ForkExploreState> ForkExploreStates;

    GraphExplorer(
        Graph _g,
        EdgeGeneratorType const& edgeGenerator,
        typename EdgeGeneratorType::VertexInputCollection const& genInput1,
        typename EdgeGeneratorType::EdgeInputCollection const& genInput2) :
        g(_g),
        edges(edgeGenerator.generateAll(genInput1, genInput2)),
        numStateForks(0) {}

    virtual ~GraphExplorer() {}

    virtual void exploreAll();

    Graph const& getGraph() const { return g; }

protected:
    Graph g;
    typename EdgeGeneratorType::OutputCollection edges;

    // Fork management
    unsigned numStateForks;
    static unsigned const forkFailureThreshold = 1;
    static unsigned const stateForkLimit = 25;
    static unsigned const failureSleepTime = 1;

    void bufferForkStates(
        std::set<VertexProperty>& vertexSet,
        std::queue<VertexProperty>& vertexQueue,
        ForkExploreStates& readBuffer,
        ForkExploreStates& writeBuffer);

    unsigned dispatchForkStates(
        std::queue<VertexProperty>& vertexQueue,
        ForkExploreStates& forkStates);

    virtual bool canJumpToVertex(VertexProperty const&) const = 0;

    virtual void jumpToVertex(VertexProperty const&) = 0;

    void followPath(Path const& path);

    virtual EdgeProperty exploreEdge(typename EdgeGeneratorType::OutputItem const&) = 0;
};

template<
    typename VertexProperty,
    typename EdgeProperty,
    typename VertexGeneratorType,
    typename EdgeGeneratorType>
class ExploreCall {
public:
    typedef GraphExplorer<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType> Explorer;
    typedef typename Explorer::PathStep PathStep;
    typedef PathStep Rtn;
    typedef typename EdgeGeneratorType::OutputItem Edge;

    struct Param {
        friend class boost::serialization::access;

        Param(VertexProperty const& _v, Edge const& _e) :
            v(_v), e(_e) {}

        VertexProperty const& v;
        Edge const& e;

    private:
        template<class Archive>
        void serialize(Archive& ar, unsigned int const version) {
            ar & v;
            ar & e;
        }
    };

    ExploreCall(Explorer& _e) : e(_e) {}
    ExploreCall(const ExploreCall& es) : e(es.e) {}
    ExploreCall& operator=(ExploreCall const& es) {
        this->e = es.e;
        return *this;
    }

    FORK_FUNCTOR_OPERATOR;

private:
    Explorer& e;
};

template<
    typename VertexProperty,
    typename EdgeProperty,
    typename VertexGeneratorType,
    typename EdgeGeneratorType>
class ExploreState {
public:
    typedef GraphExplorer<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType> Explorer;
    typedef typename Explorer::PathStep PathStep;
    typedef ExploreCall<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType> ExploreCallType;
    typedef Fork<ExploreCallType> ForkExploreCall;

    struct Edge {
        friend class boost::serialization::access;

        Edge() : vertex(), pathStep() {}
        Edge(VertexProperty _vertex, PathStep _pathStep) :
            vertex(_vertex), pathStep(_pathStep) {}

        VertexProperty vertex;
        PathStep pathStep;

        bool operator<(Edge const& e) const {
            return (vertex < e.vertex ||
                    (vertex == e.vertex && pathStep < e.pathStep));
        }

    private:
        template<class Archive>
        void serialize(Archive& ar, unsigned int const version) {
            ar & vertex;
            ar & pathStep;
        }
    };

    typedef std::set<Edge> EdgeSet;
    typedef EdgeSet Rtn;
    typedef VertexProperty Param;
    typedef std::vector< Fork<ExploreCall<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType> > > ForkExploreCalls;

    ExploreState(Explorer& _e) : e(_e) {}
    ExploreState(const ExploreState& es) : e(es.e) {}
    ExploreState& operator=(ExploreState const& es) {
        this->e = es.e;
        return *this;
    }

    FORK_FUNCTOR_OPERATOR;

private:
    Explorer& e;
};

// Priv2 functor: Used to peek at priv2 return values from a given starting
// state

template<
    typename VertexProperty,
    typename EdgeProperty,
    typename VertexGeneratorType,
    typename EdgeGeneratorType>
class ExplorePriv2Call {
public:
    typedef GraphExplorer<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType> Explorer;

    struct Param {
        friend class boost::serialization::access;

        Param(VertexProperty const& _v, Priv2Call const& _call) :
            vertex(_v), call(_call) {}

        VertexProperty const& vertex;
        Priv2Call const& call;

    private:
        template<class Archive>
        void serialize(Archive& ar, unsigned int const version) {
            ar & vertex;
            ar & call;
        }
    };

    struct Rtn {
        friend class boost::serialization::access;

        Rtn() : fnRtn(), nextState() {}

        Rtn(
            Priv2FunctionReturn const& _fnRtn,
            Priv2State const& _nextState) :
            fnRtn(_fnRtn),
            nextState(_nextState) {}

        Priv2FunctionReturn fnRtn;
        Priv2State nextState;

    private:
        template<class Archive>
        void serialize(Archive& ar, unsigned int const version) {
            ar & fnRtn;
            ar & nextState;
        }
    };

    ExplorePriv2Call(Explorer& _e) : e(_e) {}
    ExplorePriv2Call(const ExplorePriv2Call& ec) : e(ec.e) {}
    ExplorePriv2Call& operator=(ExplorePriv2Call const& ec) {
        this->e = ec.e;
        return *this;
    }

    FORK_FUNCTOR_OPERATOR;

private:
    Explorer& e;
};

// TODO: "Impl" here is a bit of a misnomer; the nested include contains
// whole classes (mostly for specialization) as well as implementations
// of functions defined in this file. Code-finding would be made easier by
// reorganizing this.
#include "GraphExplorerImpl.h"
