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
#include "SetuidState.h"

#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

typedef std::vector<std::string> NameList;
typedef std::set<std::string> NameSet;

class NameGenerator {
public:
    NameGenerator() : names() {}
    NameGenerator(NameSet const& _names) : names(_names) {}

    std::string generate(NameList const& nameParts = NameList()) {
        std::stringstream ss;
        for (NameList::const_iterator it = nameParts.begin(),
                 ie = nameParts.end(); it != ie; ++it) {
            ss << *it;
            if (it + 1 != ie) {
                ss << "_";
            }
        }
        while (names.find(ss.str()) != names.end()) {
            ss << "_";
        }
        std::string rtn = ss.str();
        names.insert(rtn);
        return rtn;
    }

    std::string generate(std::string const& singleName) {
        return generate(NameList(1, singleName));
    }

private:
    NameSet names;
};

class CodeGenerator {
public:
    virtual ~CodeGenerator() {}
};

template<typename T>
std::ostream& operator<<(
    std::ostream& os,
    std::vector<T> const& l) {
    os << "{" << std::endl;
    for (typename std::vector<T>::const_iterator
             it = l.begin(),ie = l.end(); it != ie; ++it) {
        os << *it;
        if (it + 1 != ie) {
            os << ",";
        }
        os << std::endl;
    }
    os << "}" << std::endl;
    return os;
}

template<typename T>
class InlineNamedArrayConstGenerator;

template<typename T>
class InlineArrayConstGenerator;

template<typename CPPT>
class TypeGenerator : public CodeGenerator {
public:
    typedef CPPT CPPType;

    TypeGenerator(std::string const& _typeName) : typeName(_typeName) {}
    virtual ~TypeGenerator() {}

    std::string const& name() const { return typeName; }

    TypeGenerator<CPPT*> getPtrType() {
        std::stringstream ss;
        ss << name() << "*";
        return TypeGenerator<CPPT*>(ss.str());
    }

    // TODO: This is a hack; it only works because TypeGenerators don't store
    // array-related information such as "I'm an array", or "I'm an array of
    // size X"
    TypeGenerator< InlineArrayConstGenerator<CPPT> > getArrayType() {
        return TypeGenerator< InlineArrayConstGenerator<CPPT> >(name());
    }

    friend std::ostream& operator<<(
        std::ostream& os,
        TypeGenerator const& tg) {
        os << tg.name();
        return os;
    }

private:
    std::string typeName;
};

class DefaultTypeGenerator {
public:
    static TypeGenerator<int> const defaultTypeGenerator;
};

class SymbolGenerator : public CodeGenerator {
public:
    SymbolGenerator(NameGenerator& ng) : nameStr(ng.generate()) {}
    SymbolGenerator(NameGenerator& ng, NameList const& nameParts) :
        nameStr(ng.generate(nameParts)) {}
    virtual ~SymbolGenerator() {}

    std::string const& name() const { return nameStr; }

    friend std::ostream& operator<<(
        std::ostream& os,
        SymbolGenerator const& sg) {
        os << sg.name();
        return os;
    }

    static SymbolGenerator const nullGenerator;

private:
    SymbolGenerator() : nameStr("NULL") {}

    std::string nameStr;
};

template<typename CPPT>
class TypedefGenerator : public TypeGenerator<CPPT> {
public:
    typedef CPPT CPPType;

    TypedefGenerator(
        NameGenerator& ng,
        std::string const& _baseType,
        std::string const& _typeName) :
        TypeGenerator<CPPType>(ng.generate(_typeName)),
        baseType(_baseType) {}
    virtual ~TypedefGenerator() {}

    std::string const& base() const { return baseType; }

    friend std::ostream& operator<<(
        std::ostream& os,
        TypedefGenerator const& tdg) {
        os << "typedef " << tdg.base() << " " << tdg.name() << ";" << std::endl;
        return os;
    }

private:
    std::string const baseType;
};

// Dimensional generators can generate n-dimensional data structures
template<typename T>
class DimensionalGenerator : public CodeGenerator {
public:
    typedef T Item;
};

// Dimension (not dimension) generators generate the [N][M]... part of
// dimensional type specifiers
template<typename T>
class DimensionGenerator {
public:
    typedef T Item;

    DimensionGenerator(unsigned _size, Item const* _firstItem) :
        size(_size),
        firstItem(_firstItem) {}

    friend std::ostream& operator<<(
        std::ostream& os,
        DimensionGenerator const& dg) {
        os << "[" << dg.size << "]";
        return os;
    }
private:
    unsigned size;
    Item const* firstItem;
};

// Template pattern for dimension generators requires specialization for each
// dimensional generator that uses them
#define DIMENSION_GENERATOR_TEMPLATED_SPECIALIZATION_DEFN(NestedType)   \
    template<typename _T>                                               \
    class DimensionGenerator< NestedType<_T> > {                        \
    public:                                                             \
    typedef NestedType<_T> Item;                                        \
                                                                        \
    DimensionGenerator(unsigned _size, Item const* _firstItem) :        \
        size(_size),                                                    \
        firstItem(_firstItem) {}                                        \
                                                                        \
    friend std::ostream& operator<<(                                    \
        std::ostream& os,                                               \
        DimensionGenerator const& dg) {                                 \
        os << "[" << dg.size << "]";                                    \
        if (dg.firstItem) {                                             \
            os << dg.firstItem->dimGen();                               \
        } else {                                                        \
            os << "[0]";                                                \
        }                                                               \
        return os;                                                      \
    }                                                                   \
                                                                        \
    private:                                                            \
    unsigned size;                                                      \
    Item const* firstItem;                                              \
    };
#define DIMENSION_GENERATOR_SPECIALIZATION_DEFN(NestedType)             \
    template<>                                                          \
    class DimensionGenerator< NestedType > {                            \
    public:                                                             \
    typedef NestedType Item;                                            \
                                                                        \
    DimensionGenerator(unsigned _size, Item const* _firstItem) :        \
        size(_size),                                                    \
        firstItem(_firstItem) {}                                        \
                                                                        \
    friend std::ostream& operator<<(                                    \
        std::ostream& os,                                               \
        DimensionGenerator const& dg) {                                 \
        os << "[" << dg.size << "]";                                    \
        if (dg.firstItem) {                                             \
            os << dg.firstItem->dimGen();                               \
        } else {                                                        \
            os << "[0]";                                                \
        }                                                               \
        return os;                                                      \
    }                                                                   \
                                                                        \
    private:                                                            \
    unsigned size;                                                      \
    Item const* firstItem;                                              \
    };

template<typename T>
class InlineNamedArrayConstGenerator : public DimensionalGenerator<T> {
public:
    typedef std::pair<std::string, T> Item;
    typedef std::vector<Item> ItemList;

    // Default constructor allows for storage in vector
    InlineNamedArrayConstGenerator() :
        tg(DefaultTypeGenerator::defaultTypeGenerator),
        dg(0, NULL),
        items() {}

    InlineNamedArrayConstGenerator(
        TypeGenerator<T> const& _tg,
        ItemList const& _items) :
        tg(_tg),
        items(_items),
        dg(items.size(), items.size() > 0 ? &items.at(0).second : NULL) {}

    InlineNamedArrayConstGenerator(
        NameList const& nameParts,
        TypeGenerator<T> const& _tg,
        ItemList const& _items) :
        tg(_tg),
        items(_items),
        dg(items.size(), items.size() > 0 ? &items.at(0).second : NULL) {}

    virtual ~InlineNamedArrayConstGenerator() {}

    DimensionGenerator<T> const& dimGen() const { return dg; }

    friend std::ostream& operator<<(
        std::ostream& os,
        InlineNamedArrayConstGenerator const& inacg) {
        return inacg.streamInlineNamedArrayConstGenerator(os);
    }

protected:
    TypeGenerator<T> tg;
    ItemList items;
    DimensionGenerator<T> dg;

    std::ostream& streamInlineNamedArrayConstGenerator(std::ostream& os) const {
        os << "{" << std::endl;
        for (typename ItemList::const_iterator it = items.begin(),
                 ie = items.end(); it != ie; ++it) {
            os << "(" << tg.name() << ")(" << it->second << ")";
            if (it + 1 != ie) {
                os << ",";
            }
            os << " /* " << it->first << " */" << std::endl;
        }
        os << "}";
        return os;
    }
};
DIMENSION_GENERATOR_TEMPLATED_SPECIALIZATION_DEFN(InlineNamedArrayConstGenerator);

template<typename T>
class NamedArrayConstGenerator :
    public SymbolGenerator,
    public InlineNamedArrayConstGenerator<T> {
public:
    typedef std::pair<std::string, T> Item;
    typedef std::vector<Item> ItemList;
    NamedArrayConstGenerator(
        NameGenerator& ng,
        TypeGenerator<T> const& _tg,
        ItemList const& _items) :
        SymbolGenerator(ng),
        InlineNamedArrayConstGenerator<T>(_tg, _items) {}
    NamedArrayConstGenerator(
        NameGenerator& ng,
        NameList const& nameParts,
        TypeGenerator<T> const& _tg,
        ItemList const& _items) :
        SymbolGenerator(ng, nameParts),
        InlineNamedArrayConstGenerator<T>(nameParts, _tg, _items) {}
    virtual ~NamedArrayConstGenerator() {}

    std::string const idxName(unsigned idx) const {
        std::stringstream ss;
        ss << name() << "[" << idx << "]";
        return ss.str();
    }

    std::string const defn() const {
        std::stringstream ss;
        ss << "extern " << defnName() << ";" << std::endl;
        return ss.str();
    }

    friend std::ostream& operator<<(
        std::ostream& os,
        NamedArrayConstGenerator const& nacg) {
        os << nacg.defnName() << " =" << std::endl;
        nacg.streamInlineNamedArrayConstGenerator(os);
        os << ";" << std::endl;
        return os;
    }

private:
    std::string defnName() const {
        std::stringstream ss;
        ss << this->tg.name() << " " << name() << this->dimGen();
        return ss.str();
    }
};

template<typename T>
class InlineArrayConstGenerator : DimensionalGenerator<T> {
public:
    typedef std::vector<T> ItemList;

    // Default constructor required for storing object in classes
    InlineArrayConstGenerator() :
        tg(DefaultTypeGenerator::defaultTypeGenerator),
        items(),
        dg(0, NULL) {}

    InlineArrayConstGenerator(
        TypeGenerator<T> const& _tg,
        ItemList const& _items) :
        tg(_tg),
        items(_items),
        dg(items.size(), items.size() > 0 ? &items.at(0) : NULL) {}

    virtual ~InlineArrayConstGenerator() {}

    DimensionGenerator<T> const& dimGen() const { return dg; }

    friend std::ostream& operator<<(
        std::ostream& os,
        InlineArrayConstGenerator const& iacg) {
        return iacg.streamInlineArrayConstGenerator(os);
    }

protected:
    TypeGenerator<T> tg;
private:
    ItemList items;
protected:
    DimensionGenerator<T> dg;

    std::ostream& streamInlineArrayConstGenerator(std::ostream& os) const {
        os << items;
        return os;
    }
};
DIMENSION_GENERATOR_TEMPLATED_SPECIALIZATION_DEFN(InlineArrayConstGenerator);

template<typename T>
class ArrayConstGenerator :
    public SymbolGenerator,
    public InlineArrayConstGenerator<T> {
public:
    typedef T Item;
    typedef std::vector<Item> ItemList;

    ArrayConstGenerator(
        NameGenerator& ng,
        TypeGenerator<T> const& _tg,
        ItemList const& _items) :
        SymbolGenerator(ng),
        InlineArrayConstGenerator<T>(_tg, _items) {}
    ArrayConstGenerator(
        NameGenerator& ng,
        NameList const& nameParts,
        TypeGenerator<T> const& _tg,
        ItemList const& _items) :
        SymbolGenerator(ng, nameParts),
        InlineArrayConstGenerator<T>(_tg, _items) {}
    virtual ~ArrayConstGenerator() {}

    std::string const idxName(unsigned idx) const {
        std::stringstream ss;
        ss << name() << "[" << idx << "]";
        return ss.str();
    }

    std::string const defn() const {
        std::stringstream ss;
        ss << "extern " << defnName() << ";" << std::endl;
        return ss.str();
    }

    friend std::ostream& operator<<(
        std::ostream& os,
        ArrayConstGenerator const& acg) {
        os << acg.defnName() << " =" << std::endl;
        acg.streamInlineArrayConstGenerator(os);
        os << ";" << std::endl;
        return os;
    }

private:
    std::string defnName() const {
        std::stringstream ss;
        ss << this->tg.name() << " " << name() << this->dimGen();
        return ss.str();
    }
};

class AdjacencyMatrixGenerator :
    public ArrayConstGenerator< InlineArrayConstGenerator<SymbolGenerator> > {
public:
    typedef InlineArrayConstGenerator<SymbolGenerator> Item;
    typedef std::vector<Item> ItemList;

    AdjacencyMatrixGenerator(
        NameGenerator& ng,
        TypeGenerator<Item> const& _tg,
        ItemList const& _items) :
        ArrayConstGenerator< InlineArrayConstGenerator<SymbolGenerator> >(
            ng,
            generateNameParts(),
            _tg,
            _items) {}
    virtual ~AdjacencyMatrixGenerator() {}

private:
    NameList generateNameParts() {
        NameList l;
        l.push_back("adjacency");
        l.push_back("matrix");
        return l;
    }
};

class PredecessorMatrixGenerator :
    public ArrayConstGenerator< InlineArrayConstGenerator<unsigned> > {
public:
    typedef InlineArrayConstGenerator<unsigned> Item;
    typedef std::vector<Item> ItemList;

    PredecessorMatrixGenerator(
        NameGenerator& ng,
        TypeGenerator<Item> const& _tg,
        ItemList const& _items) :
        ArrayConstGenerator< InlineArrayConstGenerator<unsigned> >(
            ng,
            generateNameParts(),
            _tg,
            _items) {}
    virtual ~PredecessorMatrixGenerator() {}

private:
    NameList generateNameParts() {
        NameList l;
        l.push_back("predecessor");
        l.push_back("matrix");
        return l;
    }
};

class PrivJumpsGenerator :
    public ArrayConstGenerator<SymbolGenerator> {
public:
    typedef SymbolGenerator Item;
    typedef std::vector<Item> ItemList;

    PrivJumpsGenerator(
        NameGenerator& ng,
        TypeGenerator<Item> const& _tg,
        ItemList const& _items) :
        ArrayConstGenerator<SymbolGenerator>(
            ng,
            generateNameParts(),
            _tg,
            _items) {}
    virtual ~PrivJumpsGenerator() {}

private:
    NameList generateNameParts() {
        NameList l;
        l.push_back("priv");
        l.push_back("jumps");
        return l;
    }
};

template<typename T>
class NullTerminatedArrayConstGenerator : public SymbolGenerator {
public:
    typedef SymbolGenerator Item;
    typedef std::vector<Item> ItemList;

    NullTerminatedArrayConstGenerator(
        NameGenerator& ng,
        TypeGenerator<T> const& _tg,
        ItemList const& _items) :
        SymbolGenerator(ng),
        tg(_tg),
        items(_items) {}
    NullTerminatedArrayConstGenerator(
        NameGenerator& ng,
        NameList const& nameParts,
        TypeGenerator<T> const& _tg,
        ItemList const& _items) :
        SymbolGenerator(ng, nameParts),
        tg(_tg),
        items(_items) {}
    virtual ~NullTerminatedArrayConstGenerator() {}

    std::string const idxName(unsigned idx) const {
        std::stringstream ss;
        ss << this->name() << "[" << idx << "]";
        return ss.str();
    }

    std::string const defn() const {
        std::stringstream ss;
        ss << "extern " << defnName() << ";" << std::endl;
        return ss.str();
    }

    friend std::ostream& operator<<(
        std::ostream& os,
        NullTerminatedArrayConstGenerator const& ntacg) {
        os << ntacg.defnName() << " =" << std::endl;
        os << "{" << std::endl;
        for (typename ItemList::const_iterator it = ntacg.items.begin(),
                 ie = ntacg.items.end(); it != ie; ++it) {
            os << it->name() << "," << std::endl;
        }
        os << "NULL" << std::endl;
        os << "};" << std::endl;
        return os;
    }

private:
    TypeGenerator<T> const& tg;
    ItemList items;

    std::string defnName() const {
        unsigned const size = items.size() + 1;
        std::stringstream ss;
        ss << tg.name() << " " << name() << "[" << size << "]";
        return ss.str();
    }
};

class StateCodeGenerator : public NamedArrayConstGenerator<UID> {
public:
    // For generate*() functions:
    template<typename Graph>
    friend class ModuleGenerator;

    typedef std::pair<std::string, UID> Item;
    typedef std::vector<Item> ItemList;

    StateCodeGenerator(
        NameGenerator& _ng,
        TypeGenerator<UID> const& _tg,
        SetuidState const& _state) :
        NamedArrayConstGenerator<UID>(
            _ng,
            generateNameParts(_state),
            _tg,
            generateItems(_state)) {}
    virtual ~StateCodeGenerator() {}

    std::string const ruid() const { return idxName(0); }

    std::string const euid() const { return idxName(1); }

    std::string const svuid() const { return idxName(2); }

private:
    static NameList generateNameParts(SetuidState const& state) {
        NameList l;
        std::stringstream r, e, sv;
        r << static_cast<unsigned>(state.ruid);
        e << static_cast<unsigned>(state.euid);
        sv << static_cast<unsigned>(state.svuid);
        l.push_back("state");
        l.push_back(r.str());
        l.push_back(e.str());
        l.push_back(sv.str());
        return l;
    }

    static ItemList generateItems(SetuidState const& state) {
        ItemList l;
        l.push_back(std::make_pair("ruid", state.ruid));
        l.push_back(std::make_pair("euid", state.euid));
        l.push_back(std::make_pair("svuid", state.svuid));
        return l;
    }
};

class InlineCallCodeGenerator : public InlineNamedArrayConstGenerator<UID> {
public:
    // For generate*() functions:
    template<typename Graph>
    friend class ModuleGenerator;
    friend class CallCodeGenerator;

    typedef std::pair<std::string, UID> Item;
    typedef std::vector<Item> ItemList;

    InlineCallCodeGenerator(
        TypeGenerator<UID> const& _tg,
        SetuidFunctionCall const& _call) :
        InlineNamedArrayConstGenerator<UID>(
            _tg,
            generateItems(_call)) {}
    virtual ~InlineCallCodeGenerator() {}

private:
    static ItemList generateItems(SetuidFunctionCall const& call) {
        static std::string const param = "param";
        ItemList l;
        l.push_back(std::make_pair("function", call.function));
        l.push_back(std::make_pair("rtn", call.rtn.value));
        l.push_back(std::make_pair("err", call.rtn.errNumber));
        unsigned count = 0;
        for (SetuidFunctionParams::const_iterator it = call.params.begin(),
                 ie = call.params.end(); it != ie; ++it) {
            std::stringstream ss;
            ss << param;
            ss << static_cast<unsigned>(count);
            l.push_back(std::make_pair(ss.str(), *it));
            ++count;
        }
        return l;
    }
};
DIMENSION_GENERATOR_SPECIALIZATION_DEFN(InlineCallCodeGenerator);

class CallCodeGenerator : public NamedArrayConstGenerator<UID> {
public:
    // For generate*() functions:
    template<typename Graph>
    friend class ModuleGenerator;

    typedef std::pair<std::string, UID> Item;
    typedef std::vector<Item> ItemList;

    CallCodeGenerator(
        NameGenerator& _ng,
        TypeGenerator<UID> const& _tg,
        SetuidFunctionCall const& _call) :
        NamedArrayConstGenerator<UID>(
            _ng,
            generateNameParts(_call),
            _tg,
            InlineCallCodeGenerator::generateItems(_call)) {}
    virtual ~CallCodeGenerator() {}

    std::string const function() const { return idxName(0); }

    std::string const rtn() const { return idxName(1); }

    std::string const err() const { return idxName(2); }

    // First param: 0
    std::string const param(unsigned idx) const { return idxName(3 + idx); }

private:
    static NameList generateNameParts(SetuidFunctionCall const& call) {
        NameList l;
        std::stringstream fn, rtn, err;
        l.push_back("call");
        fn << static_cast<unsigned>(call.function);
        l.push_back(fn.str());
        rtn << static_cast<unsigned>(call.rtn.value);
        l.push_back(rtn.str());
        err << static_cast<unsigned>(call.rtn.errNumber);
        l.push_back(err.str());
        for (SetuidFunctionParams::const_iterator it = call.params.begin(),
                 ie = call.params.end(); it != ie; ++it) {
            std::stringstream ss;
            ss << static_cast<unsigned>(*it);
            l.push_back(ss.str());
        }
        return l;
    }
};
