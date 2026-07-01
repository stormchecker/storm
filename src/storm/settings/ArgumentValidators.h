#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <boost/optional.hpp>

namespace storm {
namespace settings {

template<typename ValueType>
class ArgumentValidator {
   public:
    virtual ~ArgumentValidator() = default;

    /*!
     * Checks whether the argument passes the validation.
     */
    virtual bool isValid(ValueType const& value) = 0;

    /*!
     * Retrieves a string representation of the valid values.
     */
    virtual std::string toString() const = 0;
};

template<typename ValueType>
class RangeArgumentValidator : public ArgumentValidator<ValueType> {
   public:
    RangeArgumentValidator(boost::optional<ValueType> const& lower, boost::optional<ValueType> const& upper, bool lowerIncluded, bool upperIncluded);

    virtual bool isValid(ValueType const& value) override;
    virtual std::string toString() const override;

   private:
    boost::optional<ValueType> lower;
    boost::optional<ValueType> upper;
    bool lowerIncluded;
    bool upperIncluded;
};

class FileValidator : public ArgumentValidator<std::string> {
   public:
    enum class Mode { Exists, Writable };

    FileValidator(Mode mode);

    virtual bool isValid(std::string const& value) override;
    virtual std::string toString() const override;

   private:
    Mode mode;
};

class MultipleChoiceValidator : public ArgumentValidator<std::string> {
   public:
    MultipleChoiceValidator(std::vector<std::string> const& legalValues);

    virtual bool isValid(std::string const& value) override;
    virtual std::string toString() const override;

   private:
    std::vector<std::string> legalValues;
};

class ArgumentValidatorFactory {
   public:
    static std::shared_ptr<ArgumentValidator<int64_t>> createIntegerRangeValidatorExcluding(int_fast64_t lowerBound, int_fast64_t upperBound);
    static std::shared_ptr<ArgumentValidator<uint64_t>> createUnsignedRangeValidatorExcluding(uint64_t lowerBound, uint64_t upperBound);
    static std::shared_ptr<ArgumentValidator<uint64_t>> createUnsignedRangeValidatorIncluding(uint64_t lowerBound, uint64_t upperBound);
    static std::shared_ptr<ArgumentValidator<double>> createDoubleRangeValidatorExcluding(double lowerBound, double upperBound);

    static std::shared_ptr<ArgumentValidator<double>> createDoubleRangeValidatorIncluding(double lowerBound, double upperBound);

    static std::shared_ptr<ArgumentValidator<int64_t>> createIntegerGreaterValidator(int_fast64_t lowerBound);
    static std::shared_ptr<ArgumentValidator<uint64_t>> createUnsignedGreaterValidator(uint64_t lowerBound);
    static std::shared_ptr<ArgumentValidator<double>> createDoubleGreaterValidator(double lowerBound);

    static std::shared_ptr<ArgumentValidator<int64_t>> createIntegerGreaterEqualValidator(int_fast64_t lowerBound);
    static std::shared_ptr<ArgumentValidator<uint64_t>> createUnsignedGreaterEqualValidator(uint64_t lowerBound);
    static std::shared_ptr<ArgumentValidator<double>> createDoubleGreaterEqualValidator(double lowerBound);

    static std::shared_ptr<ArgumentValidator<std::string>> createExistingFileValidator();
    static std::shared_ptr<ArgumentValidator<std::string>> createWritableFileValidator();

    static std::shared_ptr<ArgumentValidator<std::string>> createMultipleChoiceValidator(std::vector<std::string> const& choices);

   private:
    template<typename ValueType>
    static std::shared_ptr<ArgumentValidator<ValueType>> createRangeValidatorExcluding(ValueType lowerBound, ValueType upperBound);

    template<typename ValueType>
    static std::shared_ptr<ArgumentValidator<ValueType>> createRangeValidatorIncluding(ValueType lowerBound, ValueType upperBound);

    template<typename ValueType>
    static std::shared_ptr<ArgumentValidator<ValueType>> createGreaterValidator(ValueType lowerBound, bool equalAllowed);
};

}  // namespace settings
}  // namespace storm
