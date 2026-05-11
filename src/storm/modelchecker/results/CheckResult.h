#ifndef STORM_MODELCHECKER_CHECKRESULT_H_
#define STORM_MODELCHECKER_CHECKRESULT_H_

#include <iosfwd>
#include <memory>

#include "storm/logic/ComparisonType.h"
#include "storm/storage/dd/DdType.h"

namespace storm {
namespace modelchecker {
// Forward-declare the existing subclasses.
class QualitativeCheckResult;
template<typename ValueType>
class QuantitativeCheckResult;
template<typename ValueType>
class ExplicitQualitativeCheckResult;

template<typename ValueType>
class ExplicitQuantitativeCheckResult;

template<typename ValueType>
class ExplicitParetoCurveCheckResult;

template<typename ValueType>
class LexicographicCheckResult;

template<storm::dd::DdType Type>
class SymbolicQualitativeCheckResult;

template<storm::dd::DdType Type, typename ValueType>
class SymbolicQuantitativeCheckResult;

template<storm::dd::DdType Type, typename ValueType>
class HybridQuantitativeCheckResult;

template<storm::dd::DdType Type, typename ValueType>
class SymbolicParetoCurveCheckResult;

// The base class of all check results.
class CheckResult {
   public:
    virtual ~CheckResult() = default;

    virtual std::unique_ptr<CheckResult> clone() const = 0;

    /*!
     * Filters the current result wrt. to the filter, i.e. only keeps the entries that are selected by the filter.
     * This means that the filter must be a qualitative result of proper type (symbolic/explicit).
     *
     * @param filter A qualitative result that serves as a filter.
     */
    virtual void filter(QualitativeCheckResult const& filter) = 0;

    // Methods to retrieve the actual type of the check result.
    virtual bool isExplicit() const;
    virtual bool isSymbolic() const;
    virtual bool isHybrid() const;
    virtual bool isQuantitative() const;
    virtual bool isQualitative() const;
    virtual bool isParetoCurveCheckResult() const;
    virtual bool isLexicographicCheckResult() const;
    virtual bool isExplicitQualitativeCheckResult() const;
    virtual bool isExplicitQuantitativeCheckResult() const;
    virtual bool isExplicitParetoCurveCheckResult() const;
    virtual bool isSymbolicQualitativeCheckResult() const;
    virtual bool isSymbolicQuantitativeCheckResult() const;
    virtual bool isSymbolicParetoCurveCheckResult() const;
    virtual bool isHybridQuantitativeCheckResult() const;
    virtual bool isResultForAllStates() const;

    QualitativeCheckResult& asQualitativeCheckResult();
    QualitativeCheckResult const& asQualitativeCheckResult() const;

    template<typename ValueType>
    QuantitativeCheckResult<ValueType>& asQuantitativeCheckResult();

    template<typename ValueType>
    QuantitativeCheckResult<ValueType> const& asQuantitativeCheckResult() const;

    template<typename ValueType>
    ExplicitQualitativeCheckResult<ValueType>& asExplicitQualitativeCheckResult();

    template<typename ValueType>
    ExplicitQualitativeCheckResult<ValueType> const& asExplicitQualitativeCheckResult() const;

    template<typename ValueType>
    ExplicitQuantitativeCheckResult<ValueType>& asExplicitQuantitativeCheckResult();

    template<typename ValueType>
    ExplicitQuantitativeCheckResult<ValueType> const& asExplicitQuantitativeCheckResult() const;

    template<typename ValueType>
    ExplicitParetoCurveCheckResult<ValueType>& asExplicitParetoCurveCheckResult();

    template<typename ValueType>
    ExplicitParetoCurveCheckResult<ValueType> const& asExplicitParetoCurveCheckResult() const;

    template<typename ValueType>
    LexicographicCheckResult<ValueType>& asLexicographicCheckResult();

    template<typename ValueType>
    LexicographicCheckResult<ValueType> const& asLexicographicCheckResult() const;

    template<storm::dd::DdType Type>
    SymbolicQualitativeCheckResult<Type>& asSymbolicQualitativeCheckResult();

    template<storm::dd::DdType Type>
    SymbolicQualitativeCheckResult<Type> const& asSymbolicQualitativeCheckResult() const;

    template<storm::dd::DdType Type, typename ValueType>
    SymbolicQuantitativeCheckResult<Type, ValueType>& asSymbolicQuantitativeCheckResult();

    template<storm::dd::DdType Type, typename ValueType>
    SymbolicQuantitativeCheckResult<Type, ValueType> const& asSymbolicQuantitativeCheckResult() const;

    template<storm::dd::DdType Type, typename ValueType>
    HybridQuantitativeCheckResult<Type, ValueType>& asHybridQuantitativeCheckResult();

    template<storm::dd::DdType Type, typename ValueType>
    HybridQuantitativeCheckResult<Type, ValueType> const& asHybridQuantitativeCheckResult() const;

    template<storm::dd::DdType Type, typename ValueType>
    SymbolicParetoCurveCheckResult<Type, ValueType>& asSymbolicParetoCurveCheckResult();

    template<storm::dd::DdType Type, typename ValueType>
    SymbolicParetoCurveCheckResult<Type, ValueType> const& asSymbolicParetoCurveCheckResult() const;

    virtual bool hasScheduler() const;

    /*!
     * Checks whether the ValueType of the result matches the given type.
     * @tparam T type to match
     * @return
     */
    template<typename T>
    bool hasValueType() const {
        return hasValueType(typeid(T));
    }

    virtual std::ostream& writeToStream(std::ostream& out) const = 0;

   private:
    virtual bool hasValueType(std::type_info const& t) const;
};

std::ostream& operator<<(std::ostream& out, CheckResult const& checkResult);
}  // namespace modelchecker
}  // namespace storm

#endif /* STORM_MODELCHECKER_CHECKRESULT_H_ */
