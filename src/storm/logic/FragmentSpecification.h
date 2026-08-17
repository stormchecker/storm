#pragma once

#include <map>
#include <string>

namespace storm {
namespace logic {

class RewardAccumulation;

class FragmentSpecification {
   public:
    FragmentSpecification();
    FragmentSpecification(FragmentSpecification const& other) = default;
    FragmentSpecification(FragmentSpecification&& other) = default;
    FragmentSpecification& operator=(FragmentSpecification const& other) = default;
    FragmentSpecification& operator=(FragmentSpecification&& other) = default;

    FragmentSpecification copy() const;

    bool areProbabilityOperatorsAllowed() const;
    FragmentSpecification& setProbabilityOperatorsAllowed(bool newValue);

    bool areRewardOperatorsAllowed() const;
    FragmentSpecification& setRewardOperatorsAllowed(bool newValue);

    bool areTimeOperatorsAllowed() const;
    FragmentSpecification& setTimeOperatorsAllowed(bool newValue);

    bool areLongRunAverageOperatorsAllowed() const;
    FragmentSpecification& setLongRunAverageOperatorsAllowed(bool newValue);

    bool areMultiObjectiveFormulasAllowed() const;
    FragmentSpecification& setMultiObjectiveFormulasAllowed(bool newValue);

    bool areQuantileFormulasAllowed() const;
    FragmentSpecification& setQuantileFormulasAllowed(bool newValue);

    bool areGloballyFormulasAllowed() const;
    FragmentSpecification& setGloballyFormulasAllowed(bool newValue);

    bool areReachabilityProbabilityFormulasAllowed() const;
    FragmentSpecification& setReachabilityProbabilityFormulasAllowed(bool newValue);

    bool areNextFormulasAllowed() const;
    FragmentSpecification& setNextFormulasAllowed(bool newValue);

    bool areUntilFormulasAllowed() const;
    FragmentSpecification& setUntilFormulasAllowed(bool newValue);

    bool areBoundedUntilFormulasAllowed() const;
    FragmentSpecification& setBoundedUntilFormulasAllowed(bool newValue);

    bool areHOAPathFormulasAllowed() const;
    FragmentSpecification& setHOAPathFormulasAllowed(bool newValue);

    bool areAtomicExpressionFormulasAllowed() const;
    FragmentSpecification& setAtomicExpressionFormulasAllowed(bool newValue);

    bool areAtomicLabelFormulasAllowed() const;
    FragmentSpecification& setAtomicLabelFormulasAllowed(bool newValue);

    bool areBooleanLiteralFormulasAllowed() const;
    FragmentSpecification& setBooleanLiteralFormulasAllowed(bool newValue);

    bool areUnaryBooleanStateFormulasAllowed() const;
    FragmentSpecification& setUnaryBooleanStateFormulasAllowed(bool newValue);

    bool areUnaryBooleanPathFormulasAllowed() const;
    FragmentSpecification& setUnaryBooleanPathFormulasAllowed(bool newValue);

    bool areBinaryBooleanStateFormulasAllowed() const;
    FragmentSpecification& setBinaryBooleanStateFormulasAllowed(bool newValue);

    bool areBinaryBooleanPathFormulasAllowed() const;
    FragmentSpecification& setBinaryBooleanPathFormulasAllowed(bool newValue);

    bool areCumulativeRewardFormulasAllowed() const;
    FragmentSpecification& setCumulativeRewardFormulasAllowed(bool newValue);

    bool areInstantaneousRewardFormulasAllowed() const;
    FragmentSpecification& setInstantaneousFormulasAllowed(bool newValue);

    bool areReachabilityRewardFormulasAllowed() const;
    FragmentSpecification& setReachabilityRewardFormulasAllowed(bool newValue);

    bool areLongRunAverageRewardFormulasAllowed() const;
    FragmentSpecification& setLongRunAverageRewardFormulasAllowed(bool newValue);

    bool areTotalRewardFormulasAllowed() const;
    FragmentSpecification& setTotalRewardFormulasAllowed(bool newValue);

    bool areConditionalProbabilityFormulasAllowed() const;
    FragmentSpecification& setConditionalProbabilityFormulasAllowed(bool newValue);

    bool areConditionalRewardFormulasFormulasAllowed() const;
    FragmentSpecification& setConditionalRewardFormulasAllowed(bool newValue);

    bool areReachbilityTimeFormulasAllowed() const;
    FragmentSpecification& setReachbilityTimeFormulasAllowed(bool newValue);

    bool areNestedOperatorsAllowed() const;
    FragmentSpecification& setNestedOperatorsAllowed(bool newValue);

    bool areNestedPathFormulasAllowed() const;
    FragmentSpecification& setNestedPathFormulasAllowed(bool newValue);

    bool areNestedMultiObjectiveFormulasAllowed() const;
    FragmentSpecification& setNestedMultiObjectiveFormulasAllowed(bool newValue);

    bool areNestedOperatorsInsideMultiObjectiveFormulasAllowed() const;
    FragmentSpecification& setNestedOperatorsInsideMultiObjectiveFormulasAllowed(bool newValue);

    bool areOnlyEventuallyFormuluasInConditionalFormulasAllowed() const;
    FragmentSpecification& setOnlyEventuallyFormuluasInConditionalFormulasAllowed(bool newValue);

    bool areStepBoundedUntilFormulasAllowed() const;
    FragmentSpecification& setStepBoundedUntilFormulasAllowed(bool newValue);

    bool areTimeBoundedUntilFormulasAllowed() const;
    FragmentSpecification& setTimeBoundedUntilFormulasAllowed(bool newValue);

    bool areRewardBoundedUntilFormulasAllowed() const;
    FragmentSpecification& setRewardBoundedUntilFormulasAllowed(bool newValue);

    bool areMultiDimensionalBoundedUntilFormulasAllowed() const;
    FragmentSpecification& setMultiDimensionalBoundedUntilFormulasAllowed(bool newValue);

    bool areStepBoundedCumulativeRewardFormulasAllowed() const;
    FragmentSpecification& setStepBoundedCumulativeRewardFormulasAllowed(bool newValue);

    bool areTimeBoundedCumulativeRewardFormulasAllowed() const;
    FragmentSpecification& setTimeBoundedCumulativeRewardFormulasAllowed(bool newValue);

    bool areRewardBoundedCumulativeRewardFormulasAllowed() const;
    FragmentSpecification& setRewardBoundedCumulativeRewardFormulasAllowed(bool newValue);

    bool areMultiDimensionalCumulativeRewardFormulasAllowed() const;
    FragmentSpecification& setMultiDimensionalCumulativeRewardFormulasAllowed(bool newValue);

    bool areQuantitativeOperatorResultsAllowed() const;
    FragmentSpecification& setQuantitativeOperatorResultsAllowed(bool newValue);

    bool areQualitativeOperatorResultsAllowed() const;
    FragmentSpecification& setQualitativeOperatorResultsAllowed(bool newValue);

    bool isOperatorAtTopLevelRequired() const;
    FragmentSpecification& setOperatorAtTopLevelRequired(bool newValue);

    bool isMultiObjectiveFormulaAtTopLevelRequired() const;
    FragmentSpecification& setMultiObjectiveFormulaAtTopLevelRequired(bool newValue);

    bool areOperatorsAtTopLevelOfMultiObjectiveFormulasRequired() const;
    FragmentSpecification& setOperatorsAtTopLevelOfMultiObjectiveFormulasRequired(bool newValue);

    bool isQuantileFormulaAtTopLevelRequired() const;
    FragmentSpecification& setQuantileFormulaAtTopLevelRequired(bool newValue);

    bool isRewardAccumulationAllowed() const;
    FragmentSpecification& setRewardAccumulationAllowed(bool newValue);

    bool areGameFormulasAllowed() const;
    FragmentSpecification& setGameFormulasAllowed(bool newValue);

    bool areDiscountedCumulativeRewardFormulasAllowed() const;
    FragmentSpecification& setDiscountedCumulativeRewardFormulasAllowed(bool newValue);

    bool areDiscountedTotalRewardFormulasAllowed() const;
    FragmentSpecification& setDiscountedTotalRewardFormulasAllowed(bool newValue);

    FragmentSpecification& setOperatorsAllowed(bool newValue);
    FragmentSpecification& setTimeAllowed(bool newValue);
    FragmentSpecification& setLongRunAverageProbabilitiesAllowed(bool newValue);

   private:
    // Flags that indicate whether it is legal to see such a formula.
    bool probabilityOperator;
    bool rewardOperator;
    bool expectedTimeOperator;
    bool longRunAverageOperator;

    bool multiObjectiveFormula;
    bool quantileFormula;

    bool globallyFormula;
    bool reachabilityProbabilityFormula;
    bool nextFormula;
    bool untilFormula;
    bool boundedUntilFormula;
    bool hoaPathFormula;

    bool atomicExpressionFormula;
    bool atomicLabelFormula;
    bool booleanLiteralFormula;
    bool unaryBooleanStateFormula;
    bool binaryBooleanStateFormula;
    bool unaryBooleanPathFormula;
    bool binaryBooleanPathFormula;

    bool cumulativeRewardFormula;
    bool instantaneousRewardFormula;
    bool reachabilityRewardFormula;
    bool longRunAverageRewardFormula;
    bool totalRewardFormula;

    bool conditionalProbabilityFormula;
    bool conditionalRewardFormula;

    bool reachabilityTimeFormula;

    bool gameFormula;

    // Members that indicate certain restrictions.
    bool nestedOperators;
    bool nestedPathFormulas;
    bool nestedMultiObjectiveFormulas;
    bool nestedOperatorsInsideMultiObjectiveFormulas;
    bool onlyEventuallyFormuluasInConditionalFormulas;
    bool stepBoundedUntilFormulas;
    bool timeBoundedUntilFormulas;
    bool rewardBoundedUntilFormulas;
    bool multiDimensionalBoundedUntilFormulas;
    bool stepBoundedCumulativeRewardFormulas;
    bool timeBoundedCumulativeRewardFormulas;
    bool rewardBoundedCumulativeRewardFormulas;
    bool multiDimensionalCumulativeRewardFormulas;
    bool quantitativeOperatorResults;
    bool qualitativeOperatorResults;
    bool operatorAtTopLevelRequired;
    bool multiObjectiveFormulaAtTopLevelRequired;
    bool quantileFormulaAtTopLevelRequired;
    bool operatorsAtTopLevelOfMultiObjectiveFormulasRequired;

    bool rewardAccumulation;

    bool discountedCumulativeRewardFormula;
    bool discountedTotalRewardFormula;
};

// Propositional.
FragmentSpecification propositional();

// Just reachability properties.
FragmentSpecification reachability();

// Regular PCTL.
FragmentSpecification pctl();

// Flat PCTL.
FragmentSpecification flatPctl();

// rPATL for SMGs
FragmentSpecification rpatl();
// PCTL*
FragmentSpecification pctlstar();

// PCTL + cumulative, instantaneous, reachability and long-run rewards.
FragmentSpecification prctl();

// PCTL* + cumulative, instantaneous, reachability and long-run rewards.
FragmentSpecification prctlstar();

// Regular CSL.
FragmentSpecification csl();

// CSL*, i.e., CSL with LTL path formulas.
FragmentSpecification cslstar();

// CSL + cumulative, instantaneous, reachability and long-run rewards.
FragmentSpecification csrl();

// CSL* + cumulative, instantaneous, reachability and long-run rewards.
FragmentSpecification csrlstar();

// Multi-Objective formulas.
FragmentSpecification multiObjective();

// Lex-Objective formulas
FragmentSpecification lexObjective();

// Quantile formulas.
FragmentSpecification quantiles();

}  // namespace logic
}  // namespace storm
