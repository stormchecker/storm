#include "storm/settings/ArgumentValidators.h"

#include <boost/algorithm/string/join.hpp>

#include <sys/stat.h>

#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/io/file.h"
#include "storm/settings/Argument.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {

template<typename ValueType>
RangeArgumentValidator<ValueType>::RangeArgumentValidator(boost::optional<ValueType> const& lower, boost::optional<ValueType> const& upper, bool lowerIncluded,
                                                          bool upperIncluded)
    : lower(lower), upper(upper), lowerIncluded(lowerIncluded), upperIncluded(upperIncluded) {
    // Intentionally left empty.
}

template<typename ValueType>
bool RangeArgumentValidator<ValueType>::isValid(ValueType const& value) {
    bool result = true;
    if (lower) {
        if (lowerIncluded) {
            result &= value >= lower.get();
        } else {
            result &= value > lower.get();
        }
    }
    if (upper) {
        if (upperIncluded) {
            result &= value <= upper.get();
        } else {
            result &= value < upper.get();
        }
    }
    return result;
}

template<typename ValueType>
std::string RangeArgumentValidator<ValueType>::toString() const {
    std::stringstream stream;
    stream << "in ";
    if (lower) {
        if (lowerIncluded) {
            stream << "[";
        } else {
            stream << "(";
        }
        stream << lower.get();
    } else {
        stream << "(-inf";
    }
    stream << ", ";
    if (upper) {
        stream << upper.get();
        if (upperIncluded) {
            stream << "]";
        } else {
            stream << ")";
        }
    } else {
        stream << "+inf)";
    }

    return stream.str();
}

FileValidator::FileValidator(Mode mode) : mode(mode) {
    // Intentionally left empty.
}

bool FileValidator::isValid(std::string const& filename) {
    if (mode == Mode::Exists) {
        // First check existence as ifstream::good apparently also returns true for directories.
        struct stat info;
        stat(filename.c_str(), &info);
        STORM_LOG_THROW(info.st_mode & S_IFREG, storm::exceptions::IllegalArgumentValueException,
                        "Unable to read from non-existing file '" << filename << "'.");

        // Now that we know it's a file, we can check its readability.
        STORM_LOG_THROW(storm::io::fileExistsAndIsReadable(filename), storm::exceptions::IllegalArgumentValueException,
                        "Unable to read from file '" << filename << "'.");

        return true;
    } else if (mode == Mode::Writable) {
        std::ofstream filestream;
        storm::io::openFile(filename, filestream, false, true);
        STORM_LOG_THROW(filestream.is_open(), storm::exceptions::IllegalArgumentValueException, "Could not open file '" << filename << "' for writing.");
        storm::io::closeFile(filestream);
        std::remove(filename.c_str());
        return true;
    }
    return false;
}

std::string FileValidator::toString() const {
    if (mode == Mode::Exists) {
        return "existing file";
    } else {
        return "writable file";
    }
}

MultipleChoiceValidator::MultipleChoiceValidator(std::vector<std::string> const& legalValues) : legalValues(legalValues) {
    // Intentionally left empty.
}

bool MultipleChoiceValidator::isValid(std::string const& value) {
    for (auto const& legalValue : legalValues) {
        if (legalValue == value) {
            return true;
        }
    }
    return false;
}

std::string MultipleChoiceValidator::toString() const {
    return "in {" + boost::join(legalValues, ", ") + "}";
}

std::shared_ptr<ArgumentValidator<int64_t>> ArgumentValidatorFactory::createIntegerRangeValidatorExcluding(int_fast64_t lowerBound, int_fast64_t upperBound) {
    return createRangeValidatorExcluding<int64_t>(lowerBound, upperBound);
}

std::shared_ptr<ArgumentValidator<uint64_t>> ArgumentValidatorFactory::createUnsignedRangeValidatorExcluding(uint64_t lowerBound, uint64_t upperBound) {
    return createRangeValidatorExcluding<uint64_t>(lowerBound, upperBound);
}

std::shared_ptr<ArgumentValidator<uint64_t>> ArgumentValidatorFactory::createUnsignedRangeValidatorIncluding(uint64_t lowerBound, uint64_t upperBound) {
    return createRangeValidatorIncluding<uint64_t>(lowerBound, upperBound);
}

std::shared_ptr<ArgumentValidator<double>> ArgumentValidatorFactory::createDoubleRangeValidatorExcluding(double lowerBound, double upperBound) {
    return createRangeValidatorExcluding<double>(lowerBound, upperBound);
}

std::shared_ptr<ArgumentValidator<double>> ArgumentValidatorFactory::createDoubleRangeValidatorIncluding(double lowerBound, double upperBound) {
    return createRangeValidatorIncluding<double>(lowerBound, upperBound);
}

std::shared_ptr<ArgumentValidator<int64_t>> ArgumentValidatorFactory::createIntegerGreaterValidator(int_fast64_t lowerBound) {
    return createGreaterValidator<int64_t>(lowerBound, false);
}

std::shared_ptr<ArgumentValidator<uint64_t>> ArgumentValidatorFactory::createUnsignedGreaterValidator(uint64_t lowerBound) {
    return createGreaterValidator<uint64_t>(lowerBound, false);
}

std::shared_ptr<ArgumentValidator<double>> ArgumentValidatorFactory::createDoubleGreaterValidator(double lowerBound) {
    return createGreaterValidator<double>(lowerBound, false);
}

std::shared_ptr<ArgumentValidator<int64_t>> ArgumentValidatorFactory::createIntegerGreaterEqualValidator(int_fast64_t lowerBound) {
    return createGreaterValidator<int64_t>(lowerBound, true);
}

std::shared_ptr<ArgumentValidator<uint64_t>> ArgumentValidatorFactory::createUnsignedGreaterEqualValidator(uint64_t lowerBound) {
    return createGreaterValidator<uint64_t>(lowerBound, true);
}

std::shared_ptr<ArgumentValidator<double>> ArgumentValidatorFactory::createDoubleGreaterEqualValidator(double lowerBound) {
    return createGreaterValidator<double>(lowerBound, true);
}

std::shared_ptr<ArgumentValidator<std::string>> ArgumentValidatorFactory::createExistingFileValidator() {
    return std::make_unique<FileValidator>(FileValidator::Mode::Exists);
}

std::shared_ptr<ArgumentValidator<std::string>> ArgumentValidatorFactory::createWritableFileValidator() {
    return std::make_unique<FileValidator>(FileValidator::Mode::Writable);
}

std::shared_ptr<ArgumentValidator<std::string>> ArgumentValidatorFactory::createMultipleChoiceValidator(std::vector<std::string> const& choices) {
    return std::make_unique<MultipleChoiceValidator>(choices);
}

template<typename ValueType>
std::shared_ptr<ArgumentValidator<ValueType>> ArgumentValidatorFactory::createRangeValidatorExcluding(ValueType lowerBound, ValueType upperBound) {
    return std::make_unique<RangeArgumentValidator<ValueType>>(lowerBound, upperBound, false, false);
}

template<typename ValueType>
std::shared_ptr<ArgumentValidator<ValueType>> ArgumentValidatorFactory::createRangeValidatorIncluding(ValueType lowerBound, ValueType upperBound) {
    return std::make_unique<RangeArgumentValidator<ValueType>>(lowerBound, upperBound, true, true);
}

template<typename ValueType>
std::shared_ptr<ArgumentValidator<ValueType>> ArgumentValidatorFactory::createGreaterValidator(ValueType lowerBound, bool equalAllowed) {
    return std::make_unique<RangeArgumentValidator<ValueType>>(lowerBound, boost::none, equalAllowed, false);
}

}  // namespace settings
}  // namespace storm
