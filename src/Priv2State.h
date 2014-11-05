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

// TODO: This file contains a mix of state and graph datatypes. Some of them
// should probably be abstracted into general *State *Param *Rtn *Call types
// to reduce code size and maintain generality for generators and explorers

#include "SetuidState.h"

#include "Assertions.h"

extern "C" {
#include "priv.h"
#include "priv2.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
}

#include <algorithm>
#include <cstring>
#include <set>
#include <vector>

template<
    typename VertexProperty,
    typename EdgeProperty,
    typename VertexGeneratorType,
    typename EdgeGeneratorType>
class Priv2StateGraph;

namespace boost {
    namespace serialization {
        class access;
    }
    namespace archive {
        class text_oarchive;
        class text_iarchive;
    }
}

typedef gid_t GID;

struct SetgidState {
    friend class boost::serialization::access;

    // TODO: These should be private, but some code requires "auto-construction"
    // The current fix is dangerous because it looks like a valid state
    SetgidState() : rgid(0), egid(0), svgid(0) {}

    SetgidState(
        GID _rgid,
        GID _egid,
        GID _svgid) :
        rgid(_rgid),
        egid(_egid),
        svgid(_svgid) {}

    GID rgid;
    GID egid;
    GID svgid;

    static SetgidState get();
    // static SetgidState parse(std::string const& str, int pid);

    bool operator<(SetgidState const& s) const {
        return ((rgid < s.rgid) ||
                (rgid == s.rgid && egid < s.egid) ||
                (rgid == s.rgid && egid == s.egid && svgid < s.svgid));
    }
    bool operator==(SetgidState const& s) const {
        return (rgid == s.rgid && egid == s.egid && svgid == s.svgid);
    }

private:
    template<class Archive>
    void serialize(Archive& ar, unsigned int const version) {
        ar & rgid;
        ar & egid;
        ar & svgid;
    }
};
std::ostream& operator<<(std::ostream& os, SetgidState const& ss);

// TODO: No need to inline everything here...
struct SupGroups {
    friend class boost::serialization::access;

    typedef std::vector<GID> GroupList;
    GroupList list;

    SupGroups() : list(), canonical(false) {
        canonicalize();
    }

    SupGroups(GroupList const& l) : list(l), canonical(false) {
        canonicalize();
    }

    virtual ~SupGroups() {}

    unsigned size() const { return list.size(); }

    static SupGroups get() {
        sups_t s;
        get_sups(&s);
        SupGroups sg;
        for (int i = 0; i < s.size; ++i) {
            sg.list.push_back(s.list[i]);
        }
        free(s.list);
        sg.canonicalize();
        return sg;
    }

    sups_t toSupsT() const {
        sups_t s;
        // Use malloc because get_sups() uses malloc; caller takes ownership of
        // s.list pointer
        s.list = static_cast<gid_t*>(malloc(sizeof(GID) * size()));
        for (unsigned i = 0; i < size(); ++i) {
            s.list[i] = list.at(i);
        }
        s.size = size();
        return s;
    }

    static SupGroups fromSupsT(sups_t const& s) {
        SupGroups sg;
        unsigned const size = s.size;
        for (unsigned i; i < size; ++i) {
            sg.list.push_back(s.list[i]);
        }
        sg.canonicalize();
        return sg;
    }

    bool operator<(SupGroups const& sg) const;
    bool operator==(SupGroups const& sg) const;

private:
    void canonicalize() {
        if (!canonical) {
            std::sort(list.begin(), list.end());
            std::unique(list.begin(), list.end());
        }
        canonical = true;
    }

    bool canonical;

    template<class Archive>
    void serialize(Archive& ar, unsigned int const version) {
        ar & list;
    }
};
std::ostream& operator<<(std::ostream& os, SupGroups const& sg);

struct Priv2State {
    friend class boost::serialization::access;

    // TODO: These should be private, but some code requires "auto-construction"
    // The current fix is dangerous because it looks like a valid state
    Priv2State() : uState(), gState(), supGroups() {}

    Priv2State(
        SetuidState u,
        SetgidState g,
        SupGroups   sg) :
        uState(u),
        gState(g),
        supGroups(sg) {}

    SetuidState uState;
    SetgidState gState;
    SupGroups   supGroups;

    static Priv2State get();

    bool operator<(Priv2State const& s) const {
        return (uState < s.uState || gState < s.gState ||
                supGroups < s.supGroups);
    }
    bool operator==(Priv2State const& s) const {
        return (uState == s.uState && gState == s.gState &&
                supGroups == s.supGroups);
    }

private:
    template<class Archive>
    void serialize(Archive& ar, unsigned int const version) {
        ar & uState;
        ar & gState;
        ar & supGroups;
    }
};
std::ostream& operator<<(std::ostream& os, Priv2State const& s);

// Priv2 state is a unified process identification state
typedef Priv2State IDState;

struct Priv2FunctionParam {
    // Copied from ucred_t
    UID uid;
    GID gid;
    SupGroups supGroups;

    // TODO: This should be private, but it is necessary for union-like
    // param type construction
    Priv2FunctionParam() :
        uid(0), gid(0), supGroups() {}

    Priv2FunctionParam(UID const& u, GID const& g, SupGroups const& sg) :
        uid(u), gid(g), supGroups(sg) {}

    ucred_t toUcredT() const {
        ucred_t u;
        u.uid = uid;
        u.gid = gid;
        // Caller will take ownership of u.sups.list pointer
        u.sups = supGroups.toSupsT();
        return u;
    }

    static Priv2FunctionParam fromUcredT(ucred_t const& u) {
        return Priv2FunctionParam(u.uid, u.gid, SupGroups::fromSupsT(u.sups));
    }

    bool operator<(Priv2FunctionParam const& p) const;
    bool operator==(Priv2FunctionParam const& p) const;
};

enum Priv2Function {
    Priv2Invalid = -1,
    AssumeIDPerm,
    AssumeIDTemp,
    Priv2FunctionEnd
};
std::ostream& operator<<(std::ostream& os, Priv2Function const& p2f);

enum IDFunctionType {
    InvalidFunctionType = -1,
    SetuidFunctionType,
    Priv2FunctionType
};

typedef union IDFunctionUnion {
    SetuidFunction setuid;
    Priv2Function priv2;
} IDFunctionU;

struct IDFunction {
    friend class boost::serialization::access;

    IDFunctionU fn;
    IDFunctionType ty;

private:
    template<class Archive>
    void serialize(Archive& ar, unsigned int const version) {
        ar & fn;
        ar & ty;
    }
};
std::ostream& operator<<(std::ostream& os, IDFunction const& idf);

struct IDFunctionParams {
    friend class boost::serialization::access;

    IDFunctionParams() :
        setuidParams(), priv2Param(), ty(InvalidFunctionType) {}

    IDFunctionParams(SetuidFunctionParams const& sp) :
        setuidParams(sp), priv2Param(), ty(SetuidFunctionType) {}
    IDFunctionParams(Priv2FunctionParam const& p2p) :
        setuidParams(), priv2Param(p2p), ty(Priv2FunctionType) {}

    SetuidFunctionParams setuidParams;
    Priv2FunctionParam priv2Param;

    IDFunctionType ty;

private:
    template<class Archive>
    void serialize(Archive& ar, unsigned int const version) {
        ar & setuidParams;
        ar & priv2Param;
        ar & ty;
    }
};
std::ostream& operator<<(std::ostream& os, IDFunctionParams const& idfp);

typedef SetuidFunctionReturn Priv2FunctionReturn;
typedef SetuidFunctionReturn IDFunctionReturn;

typedef std::vector<Priv2FunctionParam> Priv2FunctionParams;
std::ostream& operator<<(std::ostream& os, Priv2FunctionParams const& sfp);

struct Priv2Call {
    friend class boost::serialization::access;

    // Default call is an "invalid call" value
    Priv2Call() : function(Priv2Invalid), params() {}
    Priv2Call(Priv2Function const& _function, Priv2FunctionParams const& _params) :
        function(_function), params(_params) {}

    Priv2Function function;
    Priv2FunctionParams params;

    bool operator<(Priv2Call const& c) const {
        return ((static_cast<int>(function) < static_cast<int>(c.function)) ||
                (function == c.function && params < c.params));
    }

    bool operator==(Priv2Call const& c) const {
        return (function == c.function && params == c.params);
    }

    Priv2FunctionReturn execute() const {
        ASSERT(params.size() == 1);
        ucred_t param = params.at(0).toUcredT();
        int success = -1;
        switch (function) {
        default:
            ASSERT(false);
            success = -1;
            break;
        case AssumeIDPerm:
            success = change_identity_permanently(&param);
            break;
        case AssumeIDTemp:
            success = change_identity_temporarily(&param);
            break;
        }
        free(param.sups.list);
        Priv2FunctionReturn rtn = success == 0 ?
            Priv2FunctionReturn(success, 0, "") :
            Priv2FunctionReturn(success, errno, std::strerror(errno));
        return rtn;
    }

private:
    template<class Archive>
    void serialize(Archive& ar, unsigned int const version) {
        ar & function;
        ar & params;
    }
};
std::ostream& operator<<(std::ostream& os, Priv2Call const& p2c);
typedef std::set<Priv2Call> Priv2CallSet;

struct Priv2FunctionCall {
    friend class boost::serialization::access;

    // TODO: These should be private, but some code requires "auto-construction"
    Priv2FunctionCall() : function(Priv2Invalid), params(), rtn(), weight(0) {}

    Priv2FunctionCall(
        Priv2Function _function,
        Priv2FunctionParams _params,
        Priv2FunctionReturn _rtn) :
        function(_function),
        params(_params),
        rtn(_rtn),
        weight(1) {}

    Priv2Function function;
    Priv2FunctionParams params;
    Priv2FunctionReturn rtn;
    // TODO: This should be const, but that breaks some container magic
    unsigned weight;

    bool operator<(Priv2FunctionCall const& p2fc) const {
        return ((static_cast<int>(function) < static_cast<int>(p2fc.function)) ||
                (static_cast<int>(function) == static_cast<int>(p2fc.function) &&
                 params < p2fc.params) ||
                (static_cast<int>(function) == static_cast<int>(p2fc.function) &&
                 params == p2fc.params && rtn < p2fc.rtn) ||
                (static_cast<int>(function) == static_cast<int>(p2fc.function) &&
                 params == p2fc.params && rtn == p2fc.rtn &&
                 weight < p2fc.weight));
    }
    bool operator==(Priv2FunctionCall const& p2fc) const {
        return (static_cast<int>(function) == static_cast<int>(p2fc.function) &&
                 params == p2fc.params && rtn == p2fc.rtn &&
                 weight == p2fc.weight);
    }

private:
    template<class Archive>
    void serialize(Archive& ar, unsigned int const version) {
        ar & function;
        ar & params;
        ar & rtn;
    }
};
std::ostream& operator<<(std::ostream& os, Priv2FunctionCall const& p2fc);

struct IDFunctionCall {
    friend class boost::serialization::access;

    IDFunctionCall() :
        setuidCall(), priv2Call(), ty(InvalidFunctionType) {}

    IDFunctionCall(SetuidFunctionCall const& sc) :
        setuidCall(sc), priv2Call(), ty(SetuidFunctionType) {}
    IDFunctionCall(Priv2FunctionCall const& p2c) :
        setuidCall(), priv2Call(p2c), ty(Priv2FunctionType) {}

    SetuidFunctionCall setuidCall;
    Priv2FunctionCall priv2Call;

    IDFunctionType ty;

private:
    template<class Archive>
    void serialize(Archive& ar, unsigned int const version) {
        ar & setuidCall;
        ar & priv2Call;
        ar & ty;
    }
};
