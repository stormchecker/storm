#pragma once

#include <exception>
#include <sstream>

// Exceptions must be catchable across shared library boundaries even with hidden visibility.
#define STORM_EXCEPTION_EXPORT_ATTRIBUTE __attribute__((visibility("default")))

/*!
 * Macro to generate descendant exception classes. As all classes are nearly the same, this makes changing common
 * features much easier.
 */
#define STORM_NEW_EXCEPTION(exception_name)                                        \
    class STORM_EXCEPTION_EXPORT_ATTRIBUTE exception_name : public BaseException { \
       public:                                                                     \
        exception_name() : BaseException() {}                                      \
        exception_name(char const* cstr) : BaseException(cstr) {}                  \
        exception_name(exception_name const& cp) : BaseException(cp) {}            \
        ~exception_name() throw() {}                                               \
        virtual std::string type() const override {                                \
            return #exception_name;                                                \
        }                                                                          \
        template<typename T>                                                       \
        exception_name& operator<<(T const& var) {                                 \
            this->stream << var;                                                   \
            return *this;                                                          \
        }                                                                          \
    };
