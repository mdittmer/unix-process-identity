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

#include "Platform.h"

#include <sys/types.h>
#include <vector>
#include <string>
#include <iostream>

template<
    typename VertexProperty,
    typename EdgeProperty,
    typename VertexGeneratorType,
    typename EdgeGeneratorType>
class SetuidStateGraph;

namespace boost {
    namespace serialization {
        class access;
    }
    namespace archive {
        class text_oarchive;
        class text_iarchive;
    }
}

typedef uid_t UID;
typedef int SetuidFunctionParam;

// TODO: This is now a misnomer because it encapsulates multiple types of
// functions (setuid-type being just one)
enum SetuidFunction {
    SetuidInvalid = -1,
    Setuid,
    Seteuid,
    Setreuid,
    Setresuid,
    SetuidFunctionEnd,
    DropPrivPerm,
    DropPrivTemp,
    RestorePriv,
    PrivFunctionEnd,
};
std::ostream& operator<<(std::ostream& os, SetuidFunction const& sf);

struct SetuidState {
    friend class boost::serialization::access;

    // TODO: These should be private, but some code requires "auto-construction"
    // The current fix is dangerous because it looks like a valid state
    SetuidState() : ruid(0), euid(0), svuid(0) {}

    SetuidState(
        UID _ruid,
        UID _euid,
        UID _svuid) :
        ruid(_ruid),
        euid(_euid),
        svuid(_svuid) {}

    UID ruid;
    UID euid;
    UID svuid;

    static SetuidState get();
    // static SetuidState parse(std::string const& str, int pid);

    bool operator<(SetuidState const& s) const {
        return ((ruid < s.ruid) ||
                (ruid == s.ruid && euid < s.euid) ||
                (ruid == s.ruid && euid == s.euid && svuid < s.svuid));
    }
    bool operator==(SetuidState const& s) const {
        return (ruid == s.ruid && euid == s.euid && svuid == s.svuid);
    }

private:
    template<class Archive>
    void serialize(Archive& ar, unsigned int const version) {
        ar & ruid;
        ar & euid;
        ar & svuid;
    }
};
std::ostream& operator<<(std::ostream& os, SetuidState const& ss);

struct SetuidFunctionReturn {
    template<
        typename VertexProperty,
        typename EdgeProperty,
        typename VertexGeneratorType,
        typename EdgeGeneratorType>
    friend class SetuidStateGraph;
    friend class boost::serialization::access;

    int value;
    int errNumber;
    std::string errDescription;

    SetuidFunctionReturn(SetuidFunctionReturn const& r) :
        value(r.value),
        errNumber(r.errNumber),
        errDescription(r.errDescription) {}

    // static SetuidFunctionReturn parse(std::string const& str);

    bool operator<(SetuidFunctionReturn const& sfr) const {
        return ((value < sfr.value) ||
                (value == sfr.value && errNumber < sfr.errNumber));
    }

    bool operator==(SetuidFunctionReturn const& sfr) const {
        return (value == sfr.value && errNumber == sfr.errNumber);
    }

    // TODO: These should be private, but some code requires "auto-construction"
    SetuidFunctionReturn() :
        value(-1),
        errNumber(-1),
        errDescription("Function return object uninitialized") {}
    SetuidFunctionReturn(int _value, int _errNumber, std::string _errDescription) :
        value(_value),
        errNumber(_errNumber),
        errDescription(_errDescription) {}

private:
    template<class Archive>
    void serialize(Archive& ar, unsigned int const version) {
        ar & value;
        ar & errNumber;
        ar & errDescription;
    }
};
std::ostream& operator<<(std::ostream& os, SetuidFunctionReturn const& sfr);


typedef std::vector<SetuidFunctionParam> SetuidFunctionParams;
std::ostream& operator<<(std::ostream& os, SetuidFunctionParams const& sfp);

struct SetuidFunctionCall {
    friend class boost::serialization::access;

    // TODO: These should be private, but some code requires "auto-construction"
    SetuidFunctionCall() : function(SetuidInvalid), params(), rtn(), weight(0) {}

    SetuidFunctionCall(
        SetuidFunction _function,
        SetuidFunctionParams _params,
        SetuidFunctionReturn _rtn) :
        function(_function),
        params(_params),
        rtn(_rtn),
        weight(1) {}

    SetuidFunction function;
    SetuidFunctionParams params;
    SetuidFunctionReturn rtn;
    // TODO: This should be const, but that breaks some container magic
    unsigned weight;

    bool operator<(SetuidFunctionCall const& sfc) const {
        return ((static_cast<int>(function) < static_cast<int>(sfc.function)) ||
                (static_cast<int>(function) == static_cast<int>(sfc.function) &&
                 params < sfc.params) ||
                (static_cast<int>(function) == static_cast<int>(sfc.function) &&
                 params == sfc.params && rtn < sfc.rtn) ||
                (static_cast<int>(function) == static_cast<int>(sfc.function) &&
                 params == sfc.params && rtn == sfc.rtn &&
                 weight < sfc.weight));
    }
    bool operator==(SetuidFunctionCall const& sfc) const {
        return (static_cast<int>(function) == static_cast<int>(sfc.function) &&
                 params == sfc.params && rtn == sfc.rtn &&
                 weight == sfc.weight);
    }

private:
    template<class Archive>
    void serialize(Archive& ar, unsigned int const version) {
        ar & function;
        ar & params;
        ar & rtn;
    }
};
std::ostream& operator<<(std::ostream& os, SetuidFunctionCall const& sf);
