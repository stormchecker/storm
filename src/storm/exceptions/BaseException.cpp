#include "storm/exceptions/BaseException.h"

namespace storm {
namespace exceptions {
BaseException::BaseException() : exception() {
    // Intentionally left empty.
}

BaseException::BaseException(BaseException const& other) : exception(other), stream(other.stream.str()) {
    // Intentionally left empty.
}

BaseException::BaseException(char const* cstr) {
    stream << cstr;
}

BaseException::~BaseException() {
    // Intentionally left empty.
}

const char* BaseException::what() const noexcept {
    errorString = this->type() + ": " + this->stream.str();
    if (!this->additionalInfo().empty()) {
        errorString += " " + this->additionalInfo();
    }
    return errorString.c_str();
}

std::string BaseException::type() const {
    return "BaseException";
}

std::string BaseException::additionalInfo() const {
    return "";
}
}  // namespace exceptions
}  // namespace storm
