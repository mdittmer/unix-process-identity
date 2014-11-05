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
#include "GraphExplorer.h"
#include "GraphVisitor.h"
#include "Platform.h"
#include "Priv2State.h"
#include "SetuidState.h"

#include <boost/graph/breadth_first_search.hpp>

#include <exception>
#include <map>
#include <string>

typedef std::map<UID, UID> UIDMap;

template<typename Graph>
class Normalizer :
    public SetuidStateEdgeVisitor< Graph > {
public:
    typedef typename Graph::Vertex Vertex;
    typedef typename Graph::VertexPropertyType VertexProperty;
    typedef typename Graph::EdgePropertyType EdgeProperty;

    static UIDMap generateUIDMap(UIDSet const& uidSet);

    static UIDSet generateUIDSet(UIDMap const& uidMap);

    static UID mapUID(UIDMap const& uidMap, UID uid);

    static SetuidState mapState(UIDMap const& uidMap, SetuidState const& s);

    static SetuidFunctionCall mapFunctionCall(
        UIDMap const& uidMap,
        SetuidFunctionCall const& oldSFC);

    Normalizer(Graph& _g, UIDMap const& _uidMap) :
        uidMap(_uidMap),
        g(_g) {}

    virtual void visitEdge(
        EdgeProperty const& fc,
        VertexProperty const& s1,
        VertexProperty const& s2);

private:
    UIDMap const uidMap;
    Graph& g;
};

EDGE_VISITOR_CLASS(GeneralSanity)
EDGE_VISITOR_CLASS(SetuidSanity)
EDGE_VISITOR_CLASS(SeteuidSanity)
EDGE_VISITOR_CLASS(SetreuidSanity)
EDGE_VISITOR_CLASS(SetresuidSanity)
EDGE_VISITOR_CLASS(PrivSanity)
EDGE_VISITOR_CLASS(DropPrivPermSanity)
EDGE_VISITOR_CLASS(DropPrivTempSanity)
EDGE_VISITOR_CLASS(RestorePrivSanity)

template<typename Graph>
class StartStateVisitor : public SetuidStateVertexVisitor< Graph > {
public:
    typedef typename Graph::Vertex Vertex;
    typedef typename Graph::VertexPropertyType VertexProperty;

    StartStateVisitor(Graph const& _g) : g(_g) {}
    virtual ~StartStateVisitor() {}

    void visitVertex(VertexProperty const& v);

private:
    Graph const& g;
};

// We want to know whether from any state of the form:
//   <x, y, z>; x = 0 or z = 0, y != 0
// We can jump to <a, 0, c> and back again to <x, y, z>
template<typename Graph>
class SomewhatReversibleVisitor : public SetuidStateVertexVisitor< Graph > {
public:
    typedef typename Graph::Vertex Vertex;
    typedef typename Graph::VertexPropertyType VertexProperty;

    SomewhatReversibleVisitor(Graph const& _g) : g(_g) {}
    virtual ~SomewhatReversibleVisitor() {}

    void visitVertex(VertexProperty const& v);

private:
    Graph const& g;
};

template<typename Graph>
class Priv2SanityVisitor : public SetuidStateVertexVisitor< Graph > {
public:
    typedef typename Graph::Vertex Vertex;
    typedef typename Graph::VertexPropertyType VertexProperty;
    typedef typename Priv2EdgeGenerator::OutputCollection CallSet;
    typedef IndividualCallExplorer<
        typename Graph::VertexPropertyType,
        typename Graph::EdgePropertyType,
        typename Graph::VertexGeneratorTypename,
        typename Graph::EdgeGeneratorTypename,
        ExplorePriv2Call<
            typename Graph::VertexPropertyType,
            typename Graph::EdgePropertyType,
            typename Graph::VertexGeneratorTypename,
            typename Graph::EdgeGeneratorTypename> > Explorer;

    Priv2SanityVisitor(Graph const& _g, CallSet const& _cs) :
        g(_g),
        callSet(_cs),
        explorer(_g) {}
    virtual ~Priv2SanityVisitor() {}

    void visitVertex(VertexProperty const& v);

private:
    Graph const& g;
    CallSet const& callSet;
    Explorer explorer;
};

#include "GraphVerificationImpl.h"

#include "APFunctorImpl.h"

#define APF_VISITOR_CLASS(_class_kind, _function_kind, _function_str)   \
    BASIC_VISITOR_NAME(Edge, _class_kind) {                             \
    public:                                                             \
        BASIC_VISITOR_TYPEDEFS;                                         \
                                                                        \
        _class_kind ## Visitor(Graph const& graph) :                    \
            apf(_class_kind< Graph >()) {}                              \
                                                                        \
        virtual void visitEdge(                                         \
            EdgeProperty const& e,                                      \
            VertexProperty const& v1,                                   \
            VertexProperty const& v2) {                                 \
            if (e.function == _function_kind) {                         \
                E_CONFIRM(                                              \
                    apf(e, v1, v2),                                     \
                    "Expected appropriate privileges functor: \""       \
                    _function_str "\" to return true");                 \
            }                                                           \
        }                                                               \
    private:                                                            \
        _class_kind< Graph > const apf;                                 \
    };

APF_VISITOR_CLASS(SetuidTautology, Setuid, "setuid tautology")
APF_VISITOR_CLASS(SeteuidTautology, Seteuid, "seteuid tautology")
APF_VISITOR_CLASS(SetreuidTautology, Setreuid, "setreuid tautology")
APF_VISITOR_CLASS(SetreuidCleanTautology, Setreuid, "setreuid clean tautology")
APF_VISITOR_CLASS(SetresuidTautology, Setresuid, "setresuid tautology")
APF_VISITOR_CLASS(SetreuidForDropPrivPerm, Setresuid, "setreuid for dropprivperm")
APF_VISITOR_CLASS(SetuidRootAP, Setuid, "setuid: root has appropriate privileges")
APF_VISITOR_CLASS(SetuidNonRootNAP, Setuid, "setuid: non-root does not have appropriate privileges")
APF_VISITOR_CLASS(SeteuidRootAP, Seteuid, "seteuid: root has appropriate privileges")
APF_VISITOR_CLASS(SeteuidNonRootNAP, Seteuid, "seteuid: non-root does not have appropriate privileges")
