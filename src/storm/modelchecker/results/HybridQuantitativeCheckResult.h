#ifndef STORM_MODELCHECKER_HYBRIDQUANTITATIVECHECKRESULT_H_
#define STORM_MODELCHECKER_HYBRIDQUANTITATIVECHECKRESULT_H_

#include "storm/modelchecker/results/QuantitativeCheckResult.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/dd/Odd.h"

namespace storm {
namespace modelchecker {
template<storm::dd::DdType Type, typename ValueType = double>
class HybridQuantitativeCheckResult : public QuantitativeCheckResult<ValueType> {
   public:
    HybridQuantitativeCheckResult() = default;
    HybridQuantitativeCheckResult(storm::dd::Bdd<Type> const& reachableStates, storm::dd::Bdd<Type> const& symbolicStates,
                                  storm::dd::Add<Type, ValueType> const& symbolicValues, storm::dd::Bdd<Type> const& explicitStates, storm::dd::Odd const& odd,
                                  std::vector<ValueType> const& explicitValues);

    HybridQuantitativeCheckResult(HybridQuantitativeCheckResult const& other) = default;
    HybridQuantitativeCheckResult& operator=(HybridQuantitativeCheckResult const& other) = default;
    HybridQuantitativeCheckResult(HybridQuantitativeCheckResult&& other) = default;
    HybridQuantitativeCheckResult& operator=(HybridQuantitativeCheckResult&& other) = default;

    virtual std::unique_ptr<CheckResult> clone() const override;

    virtual std::unique_ptr<CheckResult> compareAgainstBound(storm::logic::ComparisonType comparisonType, ValueType const& bound) const override;

    std::unique_ptr<CheckResult> toExplicitQuantitativeCheckResult() const;

    virtual bool isHybrid() const override;
    virtual bool isResultForAllStates() const override;

    virtual bool isHybridQuantitativeCheckResult() const override;

    storm::dd::Bdd<Type> const& getSymbolicStates() const;

    storm::dd::Add<Type, ValueType> const& getSymbolicValueVector() const;

    storm::dd::Bdd<Type> const& getExplicitStates() const;

    storm::dd::Odd const& getOdd() const;

    std::vector<ValueType> const& getExplicitValueVector() const;

    virtual std::ostream& writeToStream(std::ostream& out) const override;

    virtual void filter(QualitativeCheckResult const& filter) override;

    virtual ValueType getMin() const override;

    virtual ValueType getMax() const override;

    virtual ValueType sum() const override;

    virtual ValueType average() const override;

    virtual void oneMinus() override;

   private:
    bool hasValueType(std::type_info const& t) const override {
        return t == typeid(ValueType);
    }

    // The set of all reachable states.
    storm::dd::Bdd<Type> reachableStates;

    // The set of all states whose result is stored symbolically.
    storm::dd::Bdd<Type> symbolicStates;

    // The symbolic value vector.
    storm::dd::Add<Type, ValueType> symbolicValues;

    // The set of all states whose result is stored explicitly.
    storm::dd::Bdd<Type> explicitStates;

    // The ODD that enables translation of the explicit values to a symbolic format.
    storm::dd::Odd odd;

    // The explicit value vector.
    std::vector<ValueType> explicitValues;
};
}  // namespace modelchecker
}  // namespace storm

#endif /* STORM_MODELCHECKER_HYBRIDQUANTITATIVECHECKRESULT_H_ */
