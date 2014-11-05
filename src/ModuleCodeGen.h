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

#include "Assertions.h"
#include "CodeGen.h"
#include "Graph.h"
#include "SetuidState.h"
#include "VisitorAccumulator.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <utility>
#include <vector>

template<typename Graph>
class ModuleGenerator {
public:
    typedef typename Graph::VertexPropertyType Vertex;
    typedef typename Graph::EdgePropertyType Edge;
    typedef typename Graph::VertexGeneratorTypename VertexGeneratorType;
    typedef typename Graph::EdgeGeneratorTypename EdgeGeneratorType;
    typedef std::set<Vertex> VertexSet;
    typedef std::set<Edge> EdgeSet;
    typedef std::vector<Vertex> VertexList;
    typedef std::vector<Edge> EdgeList;

    typedef std::map<SetuidState, StateCodeGenerator> StateMap;
    typedef std::map<SetuidFunctionCall, CallCodeGenerator> CallMap;

    typedef std::pair<SetuidState, SetuidState> StatePair;
    typedef std::map<StatePair, EdgeSet> EdgeMap;

    typedef NullTerminatedArrayConstGenerator<UID*> StateListGenerator;
    typedef NullTerminatedArrayConstGenerator<UID*> CallListGenerator;
    typedef std::map<StatePair, CallListGenerator> CallSetMap;
    typedef ArrayConstGenerator<SymbolGenerator> PrivJumpGenerator;

    ModuleGenerator(Graph const& _g) :
        g(_g),
        ng(),
        intType("int const"),
        unsignedType("unsigned const"),
        unsignedArrayType(unsignedType.getArrayType()),
        uidType("uid_t const"),
        uidPtrType(uidType.getPtrType()),
        uidPtrPtrType(uidPtrType.getPtrType()),
        uidPtrGenType(uidPtrType.name()),
        uidPtrPtrGenType(uidPtrPtrType.name()),
        uidPtrGenArrayType(uidPtrGenType.getArrayType()),
        uidPtrPtrGenArrayType(uidPtrPtrGenType.getArrayType()),
        adjacencyCode(NULL),
        predecessorCode(NULL),
        effectivePrivStatesCode(NULL),
        privJumpCode(NULL) {

        generateNames();

        generateStates();
        generateCalls();
        generateCallLists();
        generateAdjMatrix();
        generatePredecessors();
        generatePrivJumps();
    }

    virtual ~ModuleGenerator() {
        delete adjacencyCode;
        delete predecessorCode;
        delete effectivePrivStatesCode;
        delete privJumpCode;
    }

    std::ostream& streamHeader(std::ostream& os) const {
        os << "#ifdef __linux__" << std::endl
           << "#ifndef _GNU_SOURCE" << std::endl
           << "#define _GNU_SOURCE" << std::endl
           << "#endif" << std::endl
           << "#endif" << std::endl;

        os << std::endl << "/** Include **/" << std::endl << std::endl;

        os << "#include <sys/types.h>" << std::endl;

        os << std::endl << "/** Constants **/" << std::endl << std::endl;

        // TODO: A proper generator for this should be written

        VertexEdgeAccumulator<Graph> accumulator;
        UIDSet const uSet = accumulator.generateUIDs(
            g,
            g.getVertex(g.getStart()));
        unsigned numNormalUnprivilegedUIDs = 0;
        bool negOneIsSupported = false;
        for (UIDSet::const_iterator it = uSet.begin(), ie = uSet.end();
             it != ie; ++it) {
            if (*it != static_cast<UID>(-1) &&
                *it != static_cast<UID>(0)) {
                ++numNormalUnprivilegedUIDs;
            }
            if (*it == static_cast<UID>(-1)) {
                negOneIsSupported = true;
            }
        }
        os << "#define MAX_NORMALIZED_IDS "
           << numNormalUnprivilegedUIDs << std::endl
           << "#define NEG_ONE_IS_SUPPORTED "
           << negOneIsSupported << std::endl;

        os << std::endl << "/** Globals **/" << std::endl << std::endl;

        ASSERT(adjacencyCode);
        os << adjacencyCode->defn();
        ASSERT(predecessorCode);
        os << predecessorCode->defn();
        ASSERT(effectivePrivStatesCode);
        os << effectivePrivStatesCode->defn();
        ASSERT(privJumpCode);
        os << privJumpCode->defn();

        os << std::endl << "/** Functions **/" << std::endl << std::endl;

        // TODO: A proper generator for this should be written (this code is
        // copied from stream*() functions below)

        std::string const& ut = uidType.name();
        os << "int " << stateIdxLookupName << "(" << ut << " ruid, " << ut
           << " euid, " << ut << " svuid);" << std::endl;

        os << "unsigned const " << getNumFunctionParamsName << "(" << ut
           << " call);" << std::endl;

        os << "int " << executeFunctionName << "(" << ut << "  call, "
           << ut << "* params);" << std::endl;

        os << "int " << executeGroupFunctionName << "(" << ut << "  call, "
           << ut << "* params);" << std::endl;

        return os;
    }

    std::ostream& streamSource(std::ostream& os) const {
        os << std::endl << "/** Include **/" << std::endl << std::endl;

        os << "#include \"priv2_generated.h\"" << std::endl << std::endl;
        os << "#include \"Platform.h\" /* Lives with CPP code */" << std::endl
           << std::endl;
        os << "#include <assert.h>" << std::endl;
        os << "#include <errno.h>" << std::endl;
        os << "#include <stdbool.h>" << std::endl;
        os << "#include <stddef.h>" << std::endl;
        os << "#include <sys/types.h>" << std::endl;
        os << "#include <unistd.h>" << std::endl;

        os << std::endl << "/** States **/" << std::endl << std::endl;

        for (typename VertexList::const_iterator it = states.begin(),
                 ie = states.end(); it != ie; ++it) {
            StateMap::const_iterator stateCodePair = stateCode.find(*it);
            ASSERT(stateCodePair != stateCode.end());
            os << stateCodePair->second;
        }

        os << std::endl << "/** Function calls **/" << std::endl << std::endl;

        for (typename EdgeList::const_iterator it = calls.begin(),
                 ie = calls.end(); it != ie; ++it) {
            CallMap::const_iterator callCodePair = callCode.find(*it);
            ASSERT(callCodePair != callCode.end());
            os << callCodePair->second;
        }

        os << std::endl << "/** Function call sets **/" << std::endl
           << std::endl;

        for (typename VertexList::const_iterator it1 = states.begin(),
                 ie1 = states.end(); it1 != ie1; ++it1) {
            for (typename VertexList::const_iterator it2 = states.begin(),
                     ie2 = states.end(); it2 != ie2; ++it2) {
                StatePair const pair = std::make_pair(*it1, *it2);
                CallSetMap::const_iterator pairAndGenerator =
                    callSetCode.find(pair);
                if (pairAndGenerator != callSetCode.end()) {
                    os << pairAndGenerator->second;
                }
            }
        }

        os << std::endl << "/** Adjacency matrix **/" << std::endl << std::endl;

        ASSERT(adjacencyCode);
        os << *adjacencyCode;

        os << std::endl << "/** Predecessor matrix **/" << std::endl
           << std::endl;

        ASSERT(predecessorCode);
        os << *predecessorCode;

        os << std::endl << "/** Effective privileged states **/" << std::endl
           << std::endl;

        ASSERT(effectivePrivStatesCode);
        os << *effectivePrivStatesCode;

        os << std::endl << "/** Priv jump list **/" << std::endl
           << std::endl;

        ASSERT(privJumpCode);
        os << *privJumpCode;

        // Lookup state array index
        streamStateIdxLookup(os);
        // Lookup number of parameters based on function ID
        streamGetNumFunctionParams(os);
        // Setuid-like function execution
        streamExecuteFunction(os, false);
        // Setgid-like function execution
        streamExecuteFunction(os, true);

        return os;
    }

    friend std::ostream& operator<<(
        std::ostream& os,
        ModuleGenerator<Graph> const& mg) {
        os << std::endl << "/*** Header ***/" << std::endl << std::endl;
        mg.streamHeader(os);
        os << std::endl << "/*** Source ***/" << std::endl << std::endl;
        mg.streamSource(os);
        return os;
    }

private:
    Graph const& g;
    NameGenerator ng;

    TypeGenerator<int> intType;
    TypeGenerator<unsigned> unsignedType;
    TypeGenerator< InlineArrayConstGenerator<unsigned> > unsignedArrayType;
    TypeGenerator<UID> uidType;
    TypeGenerator<UID*> uidPtrType;
    TypeGenerator<UID**> uidPtrPtrType;
    TypeGenerator<SymbolGenerator> uidPtrGenType;
    TypeGenerator<SymbolGenerator> uidPtrPtrGenType;
    TypeGenerator< InlineArrayConstGenerator<SymbolGenerator> > uidPtrGenArrayType;
    TypeGenerator< InlineArrayConstGenerator<SymbolGenerator> > uidPtrPtrGenArrayType;

    std::string stateIdxLookupName;
    std::string getNumFunctionParamsName;
    std::string executeFunctionName;
    std::string executeGroupFunctionName;

    VertexList states;
    EdgeList calls;
    StateMap stateCode;
    CallMap callCode;
    CallSetMap callSetCode;
    AdjacencyMatrixGenerator* adjacencyCode;
    PredecessorMatrixGenerator* predecessorCode;
    StateListGenerator* effectivePrivStatesCode;
    PrivJumpGenerator* privJumpCode;

    void generateNames() {
        {
            NameList nameParts;
            nameParts.push_back("state");
            nameParts.push_back("idx");
            nameParts.push_back("lookup");
            stateIdxLookupName = ng.generate(nameParts);
        }
        {
            NameList nameParts;
            nameParts.push_back("get");
            nameParts.push_back("num");
            nameParts.push_back("function");
            nameParts.push_back("params");
            getNumFunctionParamsName = ng.generate(nameParts);
        }
        {
            NameList nameParts;
            nameParts.push_back("execute");
            nameParts.push_back("uids");
            nameParts.push_back("function");
            executeFunctionName = ng.generate(nameParts);
        }
        {
            NameList nameParts;
            nameParts.push_back("execute");
            nameParts.push_back("gids");
            nameParts.push_back("function");
            executeGroupFunctionName = ng.generate(nameParts);
        }
    }

    void generateStates() {
        VertexEdgeAccumulator<Graph> accumulator;
        VertexSet const vSet = accumulator.generateVertices(
            g,
            g.getVertex(g.getStart()));
        StateListGenerator::ItemList effectivePrivilegedStates;
        for (typename VertexSet::const_iterator it = vSet.begin(),
                 ie = vSet.end(); it != ie; ++it) {
            states.push_back(*it);
            stateCode.insert(std::make_pair(
                                 *it,
                                 StateCodeGenerator(ng, uidType, *it)));

            if (it->euid == 0) {
                StateMap::iterator scIt = stateCode.find(*it);
                ASSERT(scIt != stateCode.end());
                effectivePrivilegedStates.push_back(scIt->second);
            }
        }
        std::sort(states.begin(), states.end());

        NameList effectiveName;
        effectiveName.push_back("effective");
        effectiveName.push_back("privileged");
        effectiveName.push_back("states");
        effectivePrivStatesCode = new StateListGenerator(
            ng,
            effectiveName,
            uidPtrType,
            effectivePrivilegedStates);
    }

    void generateCalls() {
        VertexEdgeAccumulator<Graph> accumulator;
        EdgeSet const eSet = accumulator.generateEdges(
            g,
            g.getVertex(g.getStart()));
        for (typename EdgeSet::const_iterator it = eSet.begin(),
                 ie = eSet.end(); it != ie; ++it) {
            calls.push_back(*it);
            callCode.insert(std::make_pair(
                                *it,
                                CallCodeGenerator(ng, uidType, *it)));
        }
        std::sort(calls.begin(), calls.end());
    }

    void generateCallLists() {
        EdgeMapAccumulator<Graph> accumulator;
        EdgeMap const eSetMap = accumulator.generateEdges(
            g,
            g.getVertex(g.getStart()));
        for (typename EdgeMap::const_iterator it = eSetMap.begin(),
                 ie = eSetMap.end(); it != ie; ++it) {
            StatePair const& states = it->first;
            EdgeSet const& calls = it->second;
            ASSERT(callSetCode.find(states) == callSetCode.end());
            // Accumulate items for insertion into callSetCode
            CallListGenerator::ItemList items;
            for (typename EdgeSet::const_iterator it2 = calls.begin(),
                     ie2 = calls.end(); it2 != ie2; ++it2) {
                // Lookup call code in callCode map by edge
                // (i.e., by SetuidFunctionCall)
                CallMap::const_iterator pairAndGenerator = callCode.find(*it2);
                ASSERT(pairAndGenerator != callCode.end());
                CallCodeGenerator const& ccg = pairAndGenerator->second;
                items.push_back(ccg);
            }

            NameList nameParts(
                StateCodeGenerator::generateNameParts(states.first));
            nameParts.push_back("to");
            NameList secondParts(
                StateCodeGenerator::generateNameParts(states.second));
            nameParts.insert(
                nameParts.end(),
                secondParts.begin(),
                secondParts.end());

            callSetCode.insert(
                std::make_pair(
                    states,
                    CallListGenerator(
                        ng,
                        nameParts,
                        uidPtrType,
                        items)));
        }
    }

    void generateAdjMatrix() {
        // unsigned const dimSize = states.size();
        AdjacencyMatrixGenerator::ItemList items;
        for (typename VertexList::const_iterator it1 = states.begin(),
                 ie1 = states.end(); it1 != ie1; ++it1) {
            AdjacencyMatrixGenerator::ItemList::value_type::ItemList innerList;
            for (typename VertexList::const_iterator it2 = states.begin(),
                     ie2 = states.end(); it2 != ie2; ++it2) {
                StatePair const pair = std::make_pair(*it1, *it2);
                CallSetMap::const_iterator pairAndGenerator =
                    callSetCode.find(pair);
                if (pairAndGenerator == callSetCode.end()) {
                    innerList.push_back(SymbolGenerator::nullGenerator);
                } else {
                    innerList.push_back(pairAndGenerator->second);
                }
            }
            items.push_back(
                AdjacencyMatrixGenerator::ItemList::value_type(
                    uidPtrPtrGenType,
                    innerList));
        }
        adjacencyCode = new AdjacencyMatrixGenerator(ng, uidPtrPtrGenArrayType, items);
    }

    void generatePredecessors() {
        unsigned const dimSize = states.size();
        PredecessorMatrixGenerator::ItemList items;
        for (typename VertexList::const_iterator it1 = states.begin(),
                 ie1 = states.end(); it1 != ie1; ++it1) {
            PredecessorMatrixGenerator::Item::ItemList innerList;
            Graph const newGraph(g, *it1);

            for (typename VertexList::const_iterator it2 = states.begin(),
                     ie2 = states.end(); it2 != ie2; ++it2) {
                SetuidState const& predecessor = newGraph.getPredecessor(*it2);
                int predecessorIdx = getStateIdx(predecessor);
                ASSERT(predecessorIdx >= 0 &&
                       static_cast<unsigned>(predecessorIdx) < dimSize);
                innerList.push_back(predecessorIdx);
            }
            items.push_back(
                InlineArrayConstGenerator<unsigned>(
                    unsignedType,
                    innerList
                    ));
        }
        predecessorCode = new PredecessorMatrixGenerator(
            ng,
            unsignedArrayType,
            items);
    }

    void generatePrivJumps() {
        // Generate jumps
        VertexEdgeAccumulator<Graph> accumulator;
        typename VertexEdgeAccumulator<Graph>::Jumps jMap =
            accumulator.generatePrivJumps(
                g,
                g.getVertex(g.getStart()));

        PrivJumpGenerator::ItemList items;
        for (typename VertexList::const_iterator it = states.begin(),
                 ie = states.end(); it != ie; ++it) {
            typename VertexEdgeAccumulator<Graph>::Jumps::const_iterator
                jIt = jMap.find(*it);
            if (jIt == jMap.end()) {
                items.push_back(SymbolGenerator::nullGenerator);
            } else {
                SetuidState const& privState = jIt->second;
                StateMap::const_iterator stateGenIt = stateCode.find(privState);
                ASSERT(stateGenIt != stateCode.end());
                items.push_back(stateGenIt->second);
            }
        }

        NameList nameParts;
        nameParts.push_back("priv");
        nameParts.push_back("jumps");
        privJumpCode =
            new PrivJumpGenerator(ng, nameParts, uidPtrGenType, items);
    }

    std::ostream& streamStateIdxLookup(std::ostream& os) const {
        std::string const& ut = uidType.name();
        os << "int " << stateIdxLookupName << "("
           << ut << " ruid, "
           << ut << " euid, "
           << ut << " svuid) {" << std::endl;

        VertexEdgeAccumulator<Graph> accumulator;
        UIDSet const uSet = accumulator.generateUIDs(
            g,
            g.getVertex(g.getStart()));
        UIDSet::const_iterator ie = uSet.end();

        os << "switch (ruid) {" << std::endl
           << "default:" << std::endl
           // << "if (ruid == (uid_t)(-1)) {" << std::endl
           // << "return -1; /* Platform doesn't support -1, but caller might "
           // << "attempt to look it up */" << std::endl
           // << "}" << std::endl
           << "assert(false && \"Invalid ruid\");" << std::endl;
        for (UIDSet::const_iterator rIt = uSet.begin(); rIt != ie; ++rIt) {
            os << "case " << *rIt << ":" << std::endl
               << "switch (euid) {" << std::endl
               << "default:" << std::endl
               // << "if (euid == (uid_t)(-1)) {" << std::endl
               // << "return -1; /* Platform doesn't support -1, but caller might "
               // << "attempt to look it up */" << std::endl
               // << "}" << std::endl
               << "assert(0 && \"Invalid euid\");" << std::endl
               << "break;" << std::endl;
            for (UIDSet::const_iterator eIt = uSet.begin(); eIt != ie; ++eIt) {
                os << "case " << *eIt << ":" << std::endl
                   << "switch (svuid) {"
                   << std::endl
                   << "default:" << std::endl
                   // << "if (svuid == (uid_t)(-1)) {" << std::endl
                   // << "return -1; /* Platform doesn't support -1, but caller "
                   // << "might attempt to look it up */" << std::endl
                   // << "}" << std::endl
                   << "assert(0 && \"Invalid svuid\");" << std::endl
                   << "break;" << std::endl;
                for (UIDSet::const_iterator svIt = uSet.begin();
                     svIt != ie; ++svIt) {
                    os << "case " << *svIt << ":" << std::endl
                       << "return " << getStateIdx(
                           SetuidState(*rIt, *eIt, *svIt)) << ";" << std::endl;
                }
                os << "}" << std::endl;
            }
            os << "}" << std::endl;
        }
        os << "}" << std::endl
           << "}" << std::endl;
        return os;
    }

    int getStateIdx(SetuidState const& state) const {
        typename VertexList::const_iterator it = std::find(
            states.begin(),
            states.end(),
            state);
        if (it == states.end()) {
            return -1;
        } else {
            ASSERT(it - states.begin() >= 0);
            return it - states.begin();
        }
    }

    std::ostream& streamGetNumFunctionParams(std::ostream& os) const {
        std::string const& ut = uidType.name();
        std::string const& functionName = getNumFunctionParamsName;
        std::string const errStr =
            "Invalid function ID on number-of-parameters lookup";

        os << "unsigned const " << functionName << "(" << ut << " call) {"
           << std::endl
           << "switch (call) {"
           << std::endl
           << "default:" << std::endl
           << "assert(false && \"" << errStr << "\");" << std::endl
           << "errno = EINVAL;" << std::endl
           << "return -1;" << std::endl
           << "break;" << std::endl;
        for (int i = static_cast<int>(Setuid);
             i < static_cast<int>(SetuidFunctionEnd); ++i) {

            SetuidFunction fn = static_cast<SetuidFunction>(i);
            os << "case " << i << ": /* " << fn << " */" << std::endl;
            switch (fn) {
            default:
                ASSERT(false);
                break;
            case Setuid:
            case Seteuid:
                os << "return 1;" << std::endl;
                break;
            case Setreuid:
                os << "return 2;"
                   << std::endl;
                break;
            case Setresuid:
                os << "#if HAS_SETRESUID" << std::endl
                   << "return 3;"
                   << std::endl
                   << "#else" << std::endl
                   << "assert(false && \"Invalid function call\");" << std::endl
                   << "errno = EINVAL;" << std::endl
                   << "return -1;" << std::endl
                   << "#endif" << std::endl;
                break;
            }
            os << "break;" << std::endl;
        }
        os << "}" << std::endl
           << "}" << std::endl;
        return os;
    }

    std::ostream& streamExecuteFunction(
        std::ostream& os,
        bool groupFunction) const {

        std::string const& ut = uidType.name();
        std::string const& functionName = groupFunction ?
            executeGroupFunctionName :
            executeFunctionName;
        std::string const errStr = groupFunction ?
            "Invalid group function call" :
            "Invalid function call";

        os << "int " << functionName << "(" << ut << " call, "
           << ut << "* params) {" << std::endl
           << "switch (call) {"
           << std::endl
           << "default:" << std::endl
           << "assert(false && \"" << errStr << "\");" << std::endl
           << "errno = EINVAL;" << std::endl
           << "return -1;" << std::endl
           << "break;" << std::endl;
        for (int i = static_cast<int>(Setuid);
             i < static_cast<int>(SetuidFunctionEnd); ++i) {

            SetuidFunction fn = static_cast<SetuidFunction>(i);
            std::stringstream nameSS;
            if (groupFunction) {
                nameSS << getGroupFunctionName(fn);
            } else {
                nameSS << fn;
            }
            std::string const name = nameSS.str();

            os << "case " << i << ": /* " << name << " */" << std::endl;
            switch (fn) {
            default:
                ASSERT(false);
                break;
            case Setuid:
            case Seteuid:
                os << "return " << name << "(params[0]);" << std::endl;
                break;
            case Setreuid:
                os << "return " << name << "(params[0], params[1]);"
                   << std::endl;
                break;
            case Setresuid:
                os << "#if HAS_SETRESUID" << std::endl
                   << "return " << name << "(params[0], params[1], params[2]);"
                   << std::endl
                   << "#else" << std::endl
                   << "assert(false && \"Invalid function call\");" << std::endl
                   << "errno = EINVAL;" << std::endl
                   << "return -1;" << std::endl
                   << "#endif" << std::endl;
                break;
            }
            os << "break;" << std::endl;
        }
        os << "}" << std::endl
           << "}" << std::endl;
        return os;
    }

    std::string const getGroupFunctionName(SetuidFunction const& sf) const {
        std::stringstream ss;
        ss << sf;
        std::string const str = ss.str();
        unsigned const length = str.length();
        unsigned const size = length + 1;
        char* cstr = new char[size];
        std::strcpy(cstr, str.c_str());
        ASSERT(cstr[length - 3] == 'u');
        cstr[length - 3] = 'g';
        return std::string(cstr);
    }
};
