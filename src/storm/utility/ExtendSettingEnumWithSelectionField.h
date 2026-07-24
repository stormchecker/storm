#pragma once

#include <cassert>
#include <string>

#define ExtendEnumsWithSelectionField(NAME, ...)                 \
    enum class NAME : int { __VA_ARGS__ };                       \
    enum class NAME##Selection : int{__VA_ARGS__, FROMSETTINGS}; \
    std::string toString(NAME);                                  \
    inline NAME convert(NAME##Selection e) {                     \
        assert(e != NAME##Selection::FROMSETTINGS);              \
        return static_cast<NAME>(e);                             \
    }                                                            \
    inline std::string toString(NAME##Selection e) {             \
        if (e == NAME##Selection::FROMSETTINGS) {                \
            return "[from settings]";                            \
        } else {                                                 \
            return toString(convert(e));                         \
        }                                                        \
    }
