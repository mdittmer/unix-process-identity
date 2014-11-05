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
#include "Platform.h"
// #include "Priv2State.h"
#include "PrivWrapper.h"
#include "SetuidState.h"

#include <errno.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <queue>
#include <set>

template<typename VertexProperty, typename EdgeProperty, typename VertexGeneratorType, typename EdgeGeneratorType>
void GraphExplorer<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::exploreAll() {
    std::set<VertexProperty> vertexSet;
    std::queue<VertexProperty> vertexQueue;

    vertexSet.insert(g.getStart());
    vertexQueue.push(g.getStart());

    // Exhaust all found states
    ForkExploreStates forkStates1, forkStates2;
    dispatchForkStates(vertexQueue, forkStates1);
    while (!vertexQueue.empty() ||    // More states ready to dispatch OR
           forkStates1.size() != 0 || // More states ready to read (1) OR
           forkStates2.size() != 0) { // More states ready to read (2)
        // Read from states dispatched to 1, dispatch new states to 2
        bufferForkStates(vertexSet, vertexQueue, forkStates1, forkStates2);
        // All states form 1 read; clear it out
        forkStates1 = ForkExploreStates();
        // Read from states dispatched to 2, dispatch new states to 1
        bufferForkStates(vertexSet, vertexQueue, forkStates2, forkStates1);
        // All states form 2 read; clear it out
        forkStates2 = ForkExploreStates();
    }
}

template<typename VertexProperty, typename EdgeProperty, typename VertexGeneratorType, typename EdgeGeneratorType>
void GraphExplorer<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::bufferForkStates(
    std::set<VertexProperty>& vertexSet,
    std::queue<VertexProperty>& vertexQueue,
    ForkExploreStates& readBuffer,
    ForkExploreStates& writeBuffer) {
    // Read from existing forked states from readBuffer
    for (typename ForkExploreStates::iterator fesIt = readBuffer.begin(),
             fesIe = readBuffer.end(); fesIt != fesIe; ++fesIt) {

        typename ExploreStateType::Rtn edgeSet = fesIt->read();
        --numStateForks;

        // Construct each edge found
        for (typename ExploreStateType::Rtn::const_iterator
                 esrIt = edgeSet.begin(), esrIe = edgeSet.end();
             esrIt != esrIe; ++esrIt) {
            VertexProperty const& v1 = esrIt->vertex;
            EdgeProperty const& e = esrIt->pathStep.edge;
            VertexProperty const& v2 = esrIt->pathStep.nextVertex;
            g.addEdge(v1, v2, e);
            // If edge leads to a new state, enqueue it
            if (vertexSet.find(v2) == vertexSet.end()) {
                vertexSet.insert(v2);
                vertexQueue.push(v2);
            }
        }

        // Double-buffer new states to writeBuffer (up to static class limits)
        dispatchForkStates(vertexQueue, writeBuffer);
    }
}

template<typename VertexProperty, typename EdgeProperty, typename VertexGeneratorType, typename EdgeGeneratorType>
unsigned GraphExplorer<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::dispatchForkStates(
    std::queue<VertexProperty>& vertexQueue, ForkExploreStates& forkStates) {
    unsigned newlyDispatched = 0;
    unsigned failures = 0;
    while (!vertexQueue.empty() &&
           failures < forkFailureThreshold &&
           numStateForks < stateForkLimit) {
        VertexProperty state = vertexQueue.front();
        forkStates.push_back(ForkExploreState(ExploreStateType(*this)));
        if (forkStates.back().run(state) != -1) {
            // Only remove element from vertex queue if the fork succeeded
            vertexQueue.pop();
            ++numStateForks;
            ++newlyDispatched;
        } else {
            // If the fork failed, ditch the Fork object; we'll try again
            forkStates.pop_back();
            ++failures;
        }
    }
    if (failures >= forkFailureThreshold) {
        std::cerr << "Fork failure threshold reached; still trying..."
                  << std::endl;
        sleep(failureSleepTime);
    }

    if (forkStates.size() >= stateForkLimit) {
        std::cerr << "State fork limit reached; still trying..."
                  << std::endl;
    }
    return newlyDispatched;
}

template<typename VertexProperty, typename EdgeProperty, typename VertexGeneratorType, typename EdgeGeneratorType>
typename ExploreCall<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::Rtn
ExploreCall<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::operator() (
    Param const& param) {
    VertexProperty const& startVertex = param.v;
    Edge const& edge = param.e;
    typename Explorer::PathStep newStep;

#if HAS_SETRESUID

    if (e.canJumpToVertex(startVertex)) {
        e.jumpToVertex(startVertex);
    } else {
        e.followPath(e.g.getPath(startVertex));
    }

#else

    e.followPath(e.g.getPath(startVertex));

#endif

    ASSERT(VertexProperty::get() == startVertex);
    EdgeProperty newEdge = e.exploreEdge(edge);
    VertexProperty nextVertex = VertexProperty::get();
    newStep = PathStep(newEdge, nextVertex);
    return newStep;
}

template<typename VertexProperty, typename EdgeProperty, typename VertexGeneratorType, typename EdgeGeneratorType>
typename ExploreState<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::Rtn
ExploreState<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::operator() (
    Param const& param) {
    VertexProperty state = param;
    Rtn rtn;
    ForkExploreCalls exploreCalls;

    for (CallSet::iterator it = e.edges.begin(), ie = e.edges.end();
         it != ie; ++it) {
        exploreCalls.push_back(ForkExploreCall(ExploreCallType(e)));
        exploreCalls.back().run(typename ExploreCallType::Param(state, *it));
        // Do individual call explorations for a state in series;
        // parallelization is managed at the state dispatch level
        exploreCalls.back().wait();
    }
    for (typename ForkExploreCalls::iterator it = exploreCalls.begin(),
             ie = exploreCalls.end(); it != ie; ++it) {
        PathStep const& ps = it->read();
        rtn.insert(Edge(state, ps));
    }
    return rtn;
}

template<typename VertexProperty, typename EdgeProperty, typename VertexGeneratorType, typename EdgeGeneratorType>
void GraphExplorer<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::followPath(
    Path const& path) {
    for (typename Path::const_iterator it = path.begin(), ie = path.end();
         it != ie; ++it) {
        EdgeProperty const& e = it->edge;
        VertexProperty const& v = it->nextVertex;

        exploreEdge(
            typename EdgeGeneratorType::OutputItem(e.function, e.params));

        ASSERT(VertexProperty::get() == v);
    }
}

template<typename EdgeProperty, typename VertexGeneratorType, typename EdgeGeneratorType>
class SetuidStateGraphExplorer :
    public GraphExplorer<SetuidState, EdgeProperty, VertexGeneratorType, EdgeGeneratorType> {
public:
    typedef SetuidStateGraph<SetuidState, EdgeProperty, VertexGeneratorType, EdgeGeneratorType> Graph;
    typedef GraphExplorer<SetuidState, EdgeProperty, VertexGeneratorType, EdgeGeneratorType> Super;

    SetuidStateGraphExplorer(
        Graph _g,
        EdgeGeneratorType const& edgeGenerator,
        typename EdgeGeneratorType::VertexInputCollection const& genInput1,
        typename EdgeGeneratorType::EdgeInputCollection const& genInput2) :
        Super(_g, edgeGenerator, genInput1, genInput2) {}

    virtual ~SetuidStateGraphExplorer() {}

protected:
    virtual bool canJumpToVertex(SetuidState const& ss) const {
        return (HAS_SETRESUID) && !isPartiallyHiddenState(ss);
    }

    static bool isPartiallyHiddenState(SetuidState const& ss) {
        static UID const special = 0 - 1;
        return (ss.ruid == special || ss.euid == special || ss.svuid == special);
    }

    virtual void jumpToVertex(SetuidState const& ss) {
        ASSERT(canJumpToVertex(ss));
        SetuidFunctionParams sfp;
        sfp.push_back(ss.ruid);
        sfp.push_back(ss.euid);
        sfp.push_back(ss.svuid);

        this->exploreEdge(typename EdgeGeneratorType::OutputItem(Setresuid, sfp));
    }
};

template<typename VertexGeneratorType, typename EdgeGeneratorType>
class SetuidStateCallGraphExplorer :
    public SetuidStateGraphExplorer<SetuidFunctionCall, VertexGeneratorType, EdgeGeneratorType> {
public:
    typedef SetuidStateGraph<SetuidState, SetuidFunctionCall, VertexGeneratorType, EdgeGeneratorType> Graph;
    typedef SetuidStateGraphExplorer<SetuidFunctionCall, VertexGeneratorType, EdgeGeneratorType> Super;

    SetuidStateCallGraphExplorer(
        Graph _g,
        EdgeGeneratorType const& edgeGenerator,
        typename EdgeGeneratorType::VertexInputCollection const& genInput1,
        typename EdgeGeneratorType::EdgeInputCollection const& genInput2) :
        Super(_g, edgeGenerator, genInput1, genInput2) {}

    virtual ~SetuidStateCallGraphExplorer() {}

protected:
    SetuidFunctionCall exploreEdge(
        typename EdgeGeneratorType::OutputItem const& call) {
        int success;
        switch (call.function) {
        default:
            ASSERT(false);
            success = false;
            break;
        case Setuid:
            ASSERT(call.params.size() == 1);
            success = setuid(call.params.at(0));
            break;
        case Seteuid:
            ASSERT(call.params.size() == 1);
            success = seteuid(call.params.at(0));
            break;
        case Setreuid:
            ASSERT(call.params.size() == 2);
            success = setreuid(call.params.at(0), call.params.at(1));
            break;

#if HAS_SETRESUID

        case Setresuid:
            ASSERT(call.params.size() == 3);
            success = setresuid(call.params.at(0), call.params.at(1), call.params.at(2));
            break;

#endif

        case DropPrivPerm:
            ASSERT(call.params.size() == 1);
            success = PrivWrapper::dropPrivPerm(call.params.at(0));
            break;
        case DropPrivTemp:
            ASSERT(call.params.size() == 1);
            success = PrivWrapper::dropPrivTemp(call.params.at(0));
            break;
        case RestorePriv:
            ASSERT(call.params.size() == 1);
            success = PrivWrapper::restorePriv(call.params.at(0));
            break;
        }
        SetuidFunctionReturn rtn = success == 0 ?
            SetuidFunctionReturn(success, 0, "") :
            SetuidFunctionReturn(success, errno, std::strerror(errno));
        return SetuidFunctionCall(call.function, call.params, rtn);
    }
};

// Below: Used to peek at priv2 return values from a given starting state

// TODO: These type acrobatics occur because explorers contain functions that
// need to be invoked by their forking functors; we could avoid inheriting from
// "explorers" here, and just do some other fork-based "single-call-runner" if
// things like followPath() and [can]JumpToState() were abstracted out into some
// kind of "PathWalker" class instead of being tied up with the state
// exploration logic

template<
    typename VertexProperty,
    typename EdgeProperty,
    typename VertexGeneratorType,
    typename EdgeGeneratorType,
    typename FunctorType>
class IndividualCallExplorer : public SetuidStateCallGraphExplorer<VertexGeneratorType, EdgeGeneratorType> {
public:
    typedef SetuidStateGraph<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType> Graph;
    typedef SetuidStateCallGraphExplorer<VertexGeneratorType, EdgeGeneratorType> Super;
    typedef Fork<FunctorType> Functor;
    typedef typename FunctorType::Param Param;
    typedef typename FunctorType::Rtn Rtn;

    IndividualCallExplorer(Graph _g) :
        Super(
            _g,
            EdgeGeneratorType(),
            typename EdgeGeneratorType::VertexInputCollection(),
            typename EdgeGeneratorType::EdgeInputCollection()) {}
    virtual ~IndividualCallExplorer() {}

    // Individual call explorers don't work like this
    virtual void exploreAll() { ASSERT(false); }

    // TODO: This interface is slow; we should keep run and read decoupled and
    // possibly manage pipe/fork failures in a more nuanced way
    virtual Rtn exploreOne(
        Param const& param) {
        Functor* functor = new Functor(*this);
        while (functor->run(param) == -1) {
            std::cerr << "Individual call explorer: pipe/fork failed. "
                      << "Retrying..." << std::endl;
            sleep(1);
        }
        Rtn rtn = functor->read();
        functorQueue.push(functor);
        while (functorQueue.size() >= forkLimit) {
            Functor* f = functorQueue.front();
            f->wait();
            functorQueue.pop();
            delete f;
        }
        return rtn;
    }

private:
    std::queue<Functor*> functorQueue;
    static unsigned const forkLimit = 8;
};

template<typename VertexProperty, typename EdgeProperty, typename VertexGeneratorType, typename EdgeGeneratorType>
typename ExplorePriv2Call<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::Rtn
ExplorePriv2Call<VertexProperty, EdgeProperty, VertexGeneratorType, EdgeGeneratorType>::operator() (
    Param const& param) {
    VertexProperty const& startVertex = param.vertex;
    Priv2Call const& call = param.call;

#if HAS_SETRESUID

    if (e.canJumpToVertex(startVertex)) {
        e.jumpToVertex(startVertex);
    } else {
        e.followPath(e.g.getPath(startVertex));
    }

#else

    e.followPath(e.g.getPath(startVertex));

#endif

    ASSERT(VertexProperty::get() == startVertex);
    Priv2FunctionReturn fnRtn = call.execute();
    Priv2State nextState = Priv2State::get();
    Rtn rtn(fnRtn, nextState);
    return rtn;
}

// TODO: It appears that this thorough degree of checking is not feasible
// without running individual gid+sups graph state generation for each
// uid state in series. The memory requirements are too great

// template<typename VertexGeneratorType, typename EdgeGeneratorType>
// class Priv2StateCallGraphExplorer :
//     public SetuidStateGraphExplorer<Priv2FunctionCall, VertexGeneratorType, EdgeGeneratorType> {
// public:
//     typedef SetuidStateGraph<Priv2State, Priv2FunctionCall, VertexGeneratorType, EdgeGeneratorType> Graph;
//     typedef SetuidStateGraphExplorer<Priv2FunctionCall, VertexGeneratorType, EdgeGeneratorType> Super;

//     Priv2StateCallGraphExplorer(
//         Graph _g,
//         EdgeGeneratorType const& edgeGenerator,
//         typename EdgeGeneratorType::VertexInputCollection const& genInput1,
//         typename EdgeGeneratorType::EdgeInputCollection const& genInput2) :
//         Super(_g, edgeGenerator, genInput1, genInput2) {}

//     virtual ~Priv2StateCallGraphExplorer() {}

// protected:
//     Priv2FunctionCall exploreEdge(
//         typename EdgeGeneratorType::OutputItem const& call) {
//         int success;
//         switch (call.function) {
//         default:
//             ASSERT(false);
//             success = false;
//             break;
//         case AssumeIDPerm:
//             success = assume_identity_permanently(&call.params);
//             break;
//         case AssumeIDTemp:
//             success = assume_identity_temporarily(&call.params);
//             break;
//         }
//         Priv2FunctionReturn rtn = success == 0 ?
//             Priv2FunctionReturn(success, 0, "") :
//             Priv2FunctionReturn(success, errno, std::strerror(errno));
//         return Priv2FunctionCall(call.function, call.params, rtn);
//     }
// };

// template<typename VertexGeneratorType, typename EdgeGeneratorType>
// class IDStateCallGraphExplorer :
//     public SetuidStateGraphExplorer<IDFunctionCall, VertexGeneratorType, EdgeGeneratorType> {
// public:
//     // TODO: Is this right?
//     // Graph contains SetuidState; Priv2State (i.e., IDState) objects are
//     // constructed and verified in place
//     typedef SetuidStateGraph<SetuidState, IDFunctionCall, VertexGeneratorType, EdgeGeneratorType> Graph;
//     typedef SetuidStateGraphExplorer<IDFunctionCall, VertexGeneratorType, EdgeGeneratorType> Super;

//     IDStateCallGraphExplorer(
//         Graph _g,
//         EdgeGeneratorType const& edgeGenerator,
//         typename EdgeGeneratorType::VertexInputCollection const& genInput1,
//         typename EdgeGeneratorType::EdgeInputCollection const& genInput2) :
//         Super(_g, edgeGenerator, genInput1, genInput2) {}

//     virtual ~IDStateCallGraphExplorer() {}

// protected:
//     Priv2FunctionCall exploreEdge(
//         typename EdgeGeneratorType::OutputItem const& call) {
//         int success;
//         switch (call.function) {
//         default:
//             ASSERT(false);
//             success = false;
//             break;
//         case AssumeIDPerm:
//             success = assume_identity_permanently(&call.params);
//             break;
//         case AssumeIDTemp:
//             success = assume_identity_temporarily(&call.params);
//             break;
//         }
//         Priv2FunctionReturn rtn = success == 0 ?
//             Priv2FunctionReturn(success, 0, "") :
//             Priv2FunctionReturn(success, errno, std::strerror(errno));
//         return Priv2FunctionCall(call.function, call.params, rtn);
//     }
// };
