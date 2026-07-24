#pragma once

#include "storm/modelchecker/results/QualitativeCheckResult.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdType.h"

namespace storm {
namespace modelchecker {
template<storm::dd::DdType Type>
class SymbolicQualitativeCheckResult : public QualitativeCheckResult {
   public:
    SymbolicQualitativeCheckResult() = default;
    SymbolicQualitativeCheckResult(storm::dd::Bdd<Type> const& reachableStates, storm::dd::Bdd<Type> const& truthValues);
    SymbolicQualitativeCheckResult(storm::dd::Bdd<Type> const& reachableStates, storm::dd::Bdd<Type> const& states, storm::dd::Bdd<Type> const& truthValues);

    SymbolicQualitativeCheckResult(SymbolicQualitativeCheckResult const& other) = default;
    SymbolicQualitativeCheckResult& operator=(SymbolicQualitativeCheckResult const& other) = default;
    SymbolicQualitativeCheckResult(SymbolicQualitativeCheckResult&& other) = default;
    SymbolicQualitativeCheckResult& operator=(SymbolicQualitativeCheckResult&& other) = default;

    virtual std::unique_ptr<CheckResult> clone() const override;

    virtual bool isSymbolic() const override;
    virtual bool isResultForAllStates() const override;

    virtual bool isSymbolicQualitativeCheckResult() const override;

    virtual QualitativeCheckResult& operator&=(QualitativeCheckResult const& other) override;
    virtual QualitativeCheckResult& operator|=(QualitativeCheckResult const& other) override;
    virtual void complement() override;

    virtual bool existsTrue() const override;
    virtual bool forallTrue() const override;
    virtual uint64_t count() const override;

    storm::dd::Bdd<Type> const& getTruthValuesVector() const;
    storm::dd::Bdd<Type> const& getStates() const;
    storm::dd::Bdd<Type> const& getReachableStates() const;

    virtual std::ostream& writeToStream(std::ostream& out) const override;

    virtual void filter(QualitativeCheckResult const& filter) override;

   private:
    // The set of all reachable states.
    storm::dd::Bdd<Type> reachableStates;

    // The set of states for which this check result contains values.
    storm::dd::Bdd<Type> states;

    // The values of the qualitative check result.
    storm::dd::Bdd<Type> truthValues;
};
}  // namespace modelchecker
}  // namespace storm
