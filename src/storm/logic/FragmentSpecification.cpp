#include "storm/logic/FragmentSpecification.h"

#include <iostream>
#include "storm/logic/RewardAccumulation.h"

namespace storm {
namespace logic {

FragmentSpecification propositional() {
    FragmentSpecification propositional;

    propositional.setBooleanLiteralFormulasAllowed(true);
    propositional.setBinaryBooleanStateFormulasAllowed(true);
    propositional.setUnaryBooleanStateFormulasAllowed(true);
    propositional.setAtomicExpressionFormulasAllowed(true);
    propositional.setAtomicLabelFormulasAllowed(true);

    return propositional;
}

FragmentSpecification reachability() {
    FragmentSpecification reachability = propositional();

    reachability.setProbabilityOperatorsAllowed(true);
    reachability.setUntilFormulasAllowed(true);
    reachability.setReachabilityProbabilityFormulasAllowed(true);
    reachability.setOperatorAtTopLevelRequired(true);
    reachability.setNestedOperatorsAllowed(false);

    return reachability;
}

FragmentSpecification pctl() {
    FragmentSpecification pctl = propositional();

    pctl.setProbabilityOperatorsAllowed(true);
    pctl.setGloballyFormulasAllowed(true);
    pctl.setReachabilityProbabilityFormulasAllowed(true);
    pctl.setNextFormulasAllowed(true);
    pctl.setUntilFormulasAllowed(true);
    pctl.setBoundedUntilFormulasAllowed(true);
    pctl.setStepBoundedUntilFormulasAllowed(true);
    pctl.setTimeBoundedUntilFormulasAllowed(true);

    return pctl;
}

FragmentSpecification pctlstar() {
    FragmentSpecification pctlstar = pctl();

    pctlstar.setBinaryBooleanPathFormulasAllowed(true);
    pctlstar.setUnaryBooleanPathFormulasAllowed(true);
    pctlstar.setNestedOperatorsAllowed(true);
    pctlstar.setNestedPathFormulasAllowed(true);
    pctlstar.setHOAPathFormulasAllowed(true);

    return pctlstar;
}

FragmentSpecification flatPctl() {
    FragmentSpecification flatPctl = pctl();

    flatPctl.setNestedOperatorsAllowed(false);

    return flatPctl;
}

FragmentSpecification rpatl() {
    FragmentSpecification rpatl = propositional();

    // TODO: Only allow OperatorFormulas when they are inside of a GameFormula?
    // TODO: Require that operator formulas are required at the top level of a GameFormula?
    rpatl.setGameFormulasAllowed(true);
    rpatl.setRewardOperatorsAllowed(true);
    rpatl.setLongRunAverageRewardFormulasAllowed(true);
    rpatl.setLongRunAverageOperatorsAllowed(true);

    return rpatl;
}

FragmentSpecification prctl() {
    FragmentSpecification prctl = pctl();

    prctl.setRewardOperatorsAllowed(true);
    prctl.setCumulativeRewardFormulasAllowed(true);
    prctl.setInstantaneousFormulasAllowed(true);
    prctl.setReachabilityRewardFormulasAllowed(true);
    prctl.setLongRunAverageOperatorsAllowed(true);
    prctl.setStepBoundedCumulativeRewardFormulasAllowed(true);
    prctl.setTimeBoundedCumulativeRewardFormulasAllowed(true);

    return prctl;
}

FragmentSpecification prctlstar() {
    FragmentSpecification prctlstar = pctlstar();

    prctlstar.setRewardOperatorsAllowed(true);
    prctlstar.setCumulativeRewardFormulasAllowed(true);
    prctlstar.setInstantaneousFormulasAllowed(true);
    prctlstar.setReachabilityRewardFormulasAllowed(true);
    prctlstar.setLongRunAverageOperatorsAllowed(true);
    prctlstar.setStepBoundedCumulativeRewardFormulasAllowed(true);
    prctlstar.setTimeBoundedCumulativeRewardFormulasAllowed(true);

    return prctlstar;
}

FragmentSpecification csl() {
    FragmentSpecification csl = pctl();

    csl.setTimeBoundedUntilFormulasAllowed(true);

    return csl;
}

FragmentSpecification cslstar() {
    FragmentSpecification cslstar = csl();

    cslstar.setBinaryBooleanPathFormulasAllowed(true);
    cslstar.setUnaryBooleanPathFormulasAllowed(true);
    cslstar.setNestedOperatorsAllowed(true);
    cslstar.setNestedPathFormulasAllowed(true);
    cslstar.setHOAPathFormulasAllowed(true);

    return cslstar;
}

FragmentSpecification csrl() {
    FragmentSpecification csrl = csl();

    csrl.setRewardOperatorsAllowed(true);
    csrl.setCumulativeRewardFormulasAllowed(true);
    csrl.setInstantaneousFormulasAllowed(true);
    csrl.setReachabilityRewardFormulasAllowed(true);
    csrl.setLongRunAverageOperatorsAllowed(true);
    csrl.setTimeBoundedCumulativeRewardFormulasAllowed(true);

    return csrl;
}

FragmentSpecification csrlstar() {
    FragmentSpecification csrlstar = cslstar();

    csrlstar.setRewardOperatorsAllowed(true);
    csrlstar.setCumulativeRewardFormulasAllowed(true);
    csrlstar.setInstantaneousFormulasAllowed(true);
    csrlstar.setReachabilityRewardFormulasAllowed(true);
    csrlstar.setLongRunAverageOperatorsAllowed(true);
    csrlstar.setTimeBoundedCumulativeRewardFormulasAllowed(true);

    return csrlstar;
}

FragmentSpecification multiObjective() {
    FragmentSpecification multiObjective = propositional();

    multiObjective.setMultiObjectiveFormulasAllowed(true);
    multiObjective.setMultiObjectiveFormulaAtTopLevelRequired(true);
    multiObjective.setNestedMultiObjectiveFormulasAllowed(false);
    multiObjective.setOperatorsAtTopLevelOfMultiObjectiveFormulasRequired(true);
    multiObjective.setNestedOperatorsInsideMultiObjectiveFormulasAllowed(false);
    multiObjective.setProbabilityOperatorsAllowed(true);
    multiObjective.setUntilFormulasAllowed(true);
    multiObjective.setGloballyFormulasAllowed(true);
    multiObjective.setReachabilityProbabilityFormulasAllowed(true);
    multiObjective.setRewardOperatorsAllowed(true);
    multiObjective.setReachabilityRewardFormulasAllowed(true);
    multiObjective.setTotalRewardFormulasAllowed(true);
    multiObjective.setBoundedUntilFormulasAllowed(true);
    multiObjective.setStepBoundedUntilFormulasAllowed(true);
    multiObjective.setTimeBoundedUntilFormulasAllowed(true);
    multiObjective.setLongRunAverageOperatorsAllowed(true);
    multiObjective.setLongRunAverageRewardFormulasAllowed(true);

    return multiObjective;
}

FragmentSpecification lexObjective() {
    FragmentSpecification lexObjective = propositional();

    lexObjective.setMultiObjectiveFormulasAllowed(true);
    lexObjective.setMultiObjectiveFormulaAtTopLevelRequired(true);
    lexObjective.setNestedMultiObjectiveFormulasAllowed(false);
    lexObjective.setOperatorsAtTopLevelOfMultiObjectiveFormulasRequired(true);
    lexObjective.setNestedOperatorsInsideMultiObjectiveFormulasAllowed(false);
    lexObjective.setProbabilityOperatorsAllowed(true);
    lexObjective.setUntilFormulasAllowed(true);
    lexObjective.setGloballyFormulasAllowed(true);
    lexObjective.setReachabilityProbabilityFormulasAllowed(true);
    lexObjective.setRewardOperatorsAllowed(true);
    lexObjective.setReachabilityRewardFormulasAllowed(true);
    lexObjective.setTotalRewardFormulasAllowed(true);
    lexObjective.setBoundedUntilFormulasAllowed(true);
    lexObjective.setStepBoundedUntilFormulasAllowed(true);
    lexObjective.setTimeBoundedUntilFormulasAllowed(true);
    lexObjective.setLongRunAverageOperatorsAllowed(true);
    lexObjective.setLongRunAverageRewardFormulasAllowed(true);

    lexObjective.setCumulativeRewardFormulasAllowed(true);
    lexObjective.setInstantaneousFormulasAllowed(true);
    lexObjective.setStepBoundedCumulativeRewardFormulasAllowed(true);
    lexObjective.setTimeBoundedCumulativeRewardFormulasAllowed(true);
    lexObjective.setBinaryBooleanPathFormulasAllowed(true);
    lexObjective.setUnaryBooleanPathFormulasAllowed(true);
    lexObjective.setNestedOperatorsAllowed(true);
    lexObjective.setNestedPathFormulasAllowed(true);
    lexObjective.setHOAPathFormulasAllowed(true);
    lexObjective.setProbabilityOperatorsAllowed(true);
    lexObjective.setGloballyFormulasAllowed(true);
    lexObjective.setReachabilityProbabilityFormulasAllowed(true);
    lexObjective.setNextFormulasAllowed(true);
    lexObjective.setUntilFormulasAllowed(true);
    lexObjective.setBoundedUntilFormulasAllowed(true);
    lexObjective.setStepBoundedUntilFormulasAllowed(true);
    lexObjective.setTimeBoundedUntilFormulasAllowed(true);

    return lexObjective;
}

FragmentSpecification quantiles() {
    FragmentSpecification quantiles = propositional();

    quantiles.setQuantileFormulasAllowed(true);
    quantiles.setQuantileFormulaAtTopLevelRequired(true);
    quantiles.setProbabilityOperatorsAllowed(true);
    quantiles.setRewardOperatorsAllowed(true);
    quantiles.setBoundedUntilFormulasAllowed(true);
    quantiles.setStepBoundedUntilFormulasAllowed(true);
    quantiles.setTimeBoundedUntilFormulasAllowed(true);
    quantiles.setRewardBoundedUntilFormulasAllowed(true);
    quantiles.setStepBoundedCumulativeRewardFormulasAllowed(true);
    quantiles.setTimeBoundedCumulativeRewardFormulasAllowed(true);
    quantiles.setRewardBoundedCumulativeRewardFormulasAllowed(true);
    quantiles.setCumulativeRewardFormulasAllowed(true);
    quantiles.setMultiDimensionalBoundedUntilFormulasAllowed(true);
    quantiles.setMultiDimensionalCumulativeRewardFormulasAllowed(true);

    return quantiles;
}

FragmentSpecification::FragmentSpecification() {
    probabilityOperator = false;
    rewardOperator = false;
    expectedTimeOperator = false;
    longRunAverageOperator = false;

    multiObjectiveFormula = false;
    quantileFormula = false;

    globallyFormula = false;
    reachabilityProbabilityFormula = false;
    nextFormula = false;
    untilFormula = false;
    boundedUntilFormula = false;
    hoaPathFormula = false;

    atomicExpressionFormula = false;
    atomicLabelFormula = false;
    booleanLiteralFormula = false;
    unaryBooleanStateFormula = false;
    binaryBooleanStateFormula = false;
    unaryBooleanPathFormula = false;
    binaryBooleanPathFormula = false;

    cumulativeRewardFormula = false;
    instantaneousRewardFormula = false;
    reachabilityRewardFormula = false;
    longRunAverageRewardFormula = false;
    totalRewardFormula = false;

    conditionalProbabilityFormula = false;
    conditionalRewardFormula = false;

    reachabilityTimeFormula = false;

    gameFormula = false;

    nestedOperators = true;
    nestedPathFormulas = false;
    nestedMultiObjectiveFormulas = false;
    nestedOperatorsInsideMultiObjectiveFormulas = false;
    onlyEventuallyFormuluasInConditionalFormulas = true;
    stepBoundedUntilFormulas = false;
    timeBoundedUntilFormulas = false;
    rewardBoundedUntilFormulas = false;
    multiDimensionalBoundedUntilFormulas = false;
    stepBoundedCumulativeRewardFormulas = false;
    timeBoundedCumulativeRewardFormulas = false;
    rewardBoundedCumulativeRewardFormulas = false;
    multiDimensionalCumulativeRewardFormulas = false;

    qualitativeOperatorResults = true;
    quantitativeOperatorResults = true;

    operatorAtTopLevelRequired = false;
    multiObjectiveFormulaAtTopLevelRequired = false;
    operatorsAtTopLevelOfMultiObjectiveFormulasRequired = false;
    quantileFormulaAtTopLevelRequired = false;

    rewardAccumulation = false;

    discountedCumulativeRewardFormula = false;
    discountedTotalRewardFormula = false;
}

FragmentSpecification FragmentSpecification::copy() const {
    return FragmentSpecification(*this);
}

bool FragmentSpecification::areProbabilityOperatorsAllowed() const {
    return probabilityOperator;
}

FragmentSpecification& FragmentSpecification::setProbabilityOperatorsAllowed(bool newValue) {
    this->probabilityOperator = newValue;
    return *this;
}

bool FragmentSpecification::areRewardOperatorsAllowed() const {
    return rewardOperator;
}

FragmentSpecification& FragmentSpecification::setRewardOperatorsAllowed(bool newValue) {
    this->rewardOperator = newValue;
    return *this;
}

bool FragmentSpecification::areTimeOperatorsAllowed() const {
    return expectedTimeOperator;
}

FragmentSpecification& FragmentSpecification::setTimeOperatorsAllowed(bool newValue) {
    this->expectedTimeOperator = newValue;
    return *this;
}

bool FragmentSpecification::areLongRunAverageOperatorsAllowed() const {
    return longRunAverageOperator;
}

FragmentSpecification& FragmentSpecification::setLongRunAverageOperatorsAllowed(bool newValue) {
    this->longRunAverageOperator = newValue;
    return *this;
}

bool FragmentSpecification::areMultiObjectiveFormulasAllowed() const {
    return multiObjectiveFormula;
}

FragmentSpecification& FragmentSpecification::setMultiObjectiveFormulasAllowed(bool newValue) {
    this->multiObjectiveFormula = newValue;
    return *this;
}

bool FragmentSpecification::areQuantileFormulasAllowed() const {
    return quantileFormula;
}

FragmentSpecification& FragmentSpecification::setQuantileFormulasAllowed(bool newValue) {
    this->quantileFormula = newValue;
    return *this;
}

bool FragmentSpecification::areGloballyFormulasAllowed() const {
    return globallyFormula;
}

FragmentSpecification& FragmentSpecification::setGloballyFormulasAllowed(bool newValue) {
    this->globallyFormula = newValue;
    return *this;
}

bool FragmentSpecification::areReachabilityProbabilityFormulasAllowed() const {
    return reachabilityProbabilityFormula;
}

FragmentSpecification& FragmentSpecification::setReachabilityProbabilityFormulasAllowed(bool newValue) {
    this->reachabilityProbabilityFormula = newValue;
    return *this;
}

bool FragmentSpecification::areNextFormulasAllowed() const {
    return nextFormula;
}

FragmentSpecification& FragmentSpecification::setNextFormulasAllowed(bool newValue) {
    this->nextFormula = newValue;
    return *this;
}

bool FragmentSpecification::areUntilFormulasAllowed() const {
    return untilFormula;
}

FragmentSpecification& FragmentSpecification::setUntilFormulasAllowed(bool newValue) {
    this->untilFormula = newValue;
    return *this;
}

bool FragmentSpecification::areBoundedUntilFormulasAllowed() const {
    return boundedUntilFormula;
}

FragmentSpecification& FragmentSpecification::setBoundedUntilFormulasAllowed(bool newValue) {
    this->boundedUntilFormula = newValue;
    return *this;
}

bool FragmentSpecification::areHOAPathFormulasAllowed() const {
    return hoaPathFormula;
}

FragmentSpecification& FragmentSpecification::setHOAPathFormulasAllowed(bool newValue) {
    this->hoaPathFormula = newValue;
    return *this;
}

bool FragmentSpecification::areAtomicExpressionFormulasAllowed() const {
    return atomicExpressionFormula;
}

FragmentSpecification& FragmentSpecification::setAtomicExpressionFormulasAllowed(bool newValue) {
    this->atomicExpressionFormula = newValue;
    return *this;
}

bool FragmentSpecification::areAtomicLabelFormulasAllowed() const {
    return atomicLabelFormula;
}

FragmentSpecification& FragmentSpecification::setAtomicLabelFormulasAllowed(bool newValue) {
    this->atomicLabelFormula = newValue;
    return *this;
}

bool FragmentSpecification::areBooleanLiteralFormulasAllowed() const {
    return booleanLiteralFormula;
}

FragmentSpecification& FragmentSpecification::setBooleanLiteralFormulasAllowed(bool newValue) {
    this->booleanLiteralFormula = newValue;
    return *this;
}

bool FragmentSpecification::areUnaryBooleanStateFormulasAllowed() const {
    return unaryBooleanStateFormula;
}

FragmentSpecification& FragmentSpecification::setUnaryBooleanStateFormulasAllowed(bool newValue) {
    this->unaryBooleanStateFormula = newValue;
    return *this;
}

bool FragmentSpecification::areUnaryBooleanPathFormulasAllowed() const {
    return unaryBooleanPathFormula;
}

FragmentSpecification& FragmentSpecification::setUnaryBooleanPathFormulasAllowed(bool newValue) {
    this->unaryBooleanPathFormula = newValue;
    return *this;
}

bool FragmentSpecification::areBinaryBooleanStateFormulasAllowed() const {
    return binaryBooleanStateFormula;
}

FragmentSpecification& FragmentSpecification::setBinaryBooleanStateFormulasAllowed(bool newValue) {
    this->binaryBooleanStateFormula = newValue;
    return *this;
}

bool FragmentSpecification::areBinaryBooleanPathFormulasAllowed() const {
    return binaryBooleanPathFormula;
}

FragmentSpecification& FragmentSpecification::setBinaryBooleanPathFormulasAllowed(bool newValue) {
    this->binaryBooleanPathFormula = newValue;
    return *this;
}

bool FragmentSpecification::areCumulativeRewardFormulasAllowed() const {
    return cumulativeRewardFormula;
}

FragmentSpecification& FragmentSpecification::setCumulativeRewardFormulasAllowed(bool newValue) {
    this->cumulativeRewardFormula = newValue;
    return *this;
}

bool FragmentSpecification::areInstantaneousRewardFormulasAllowed() const {
    return instantaneousRewardFormula;
}

FragmentSpecification& FragmentSpecification::setInstantaneousFormulasAllowed(bool newValue) {
    this->instantaneousRewardFormula = newValue;
    return *this;
}

bool FragmentSpecification::areReachabilityRewardFormulasAllowed() const {
    return reachabilityRewardFormula;
}

FragmentSpecification& FragmentSpecification::setReachabilityRewardFormulasAllowed(bool newValue) {
    this->reachabilityRewardFormula = newValue;
    return *this;
}

bool FragmentSpecification::areLongRunAverageRewardFormulasAllowed() const {
    return longRunAverageRewardFormula;
}

FragmentSpecification& FragmentSpecification::setLongRunAverageRewardFormulasAllowed(bool newValue) {
    this->longRunAverageRewardFormula = newValue;
    return *this;
}

bool FragmentSpecification::areTotalRewardFormulasAllowed() const {
    return totalRewardFormula;
}

FragmentSpecification& FragmentSpecification::setTotalRewardFormulasAllowed(bool newValue) {
    this->totalRewardFormula = newValue;
    return *this;
}

bool FragmentSpecification::areConditionalProbabilityFormulasAllowed() const {
    return conditionalProbabilityFormula;
}

FragmentSpecification& FragmentSpecification::setConditionalProbabilityFormulasAllowed(bool newValue) {
    this->conditionalProbabilityFormula = newValue;
    return *this;
}

bool FragmentSpecification::areConditionalRewardFormulasFormulasAllowed() const {
    return conditionalRewardFormula;
}

FragmentSpecification& FragmentSpecification::setConditionalRewardFormulasAllowed(bool newValue) {
    this->conditionalRewardFormula = newValue;
    return *this;
}

bool FragmentSpecification::areReachbilityTimeFormulasAllowed() const {
    return reachabilityTimeFormula;
}

FragmentSpecification& FragmentSpecification::setReachbilityTimeFormulasAllowed(bool newValue) {
    this->reachabilityTimeFormula = newValue;
    return *this;
}

bool FragmentSpecification::areNestedOperatorsAllowed() const {
    return this->nestedOperators;
}

FragmentSpecification& FragmentSpecification::setNestedOperatorsAllowed(bool newValue) {
    this->nestedOperators = newValue;
    return *this;
}

bool FragmentSpecification::areNestedPathFormulasAllowed() const {
    return this->nestedPathFormulas;
}

FragmentSpecification& FragmentSpecification::setNestedPathFormulasAllowed(bool newValue) {
    this->nestedPathFormulas = newValue;
    return *this;
}

bool FragmentSpecification::areNestedMultiObjectiveFormulasAllowed() const {
    return this->nestedMultiObjectiveFormulas;
}

FragmentSpecification& FragmentSpecification::setNestedMultiObjectiveFormulasAllowed(bool newValue) {
    this->nestedMultiObjectiveFormulas = newValue;
    return *this;
}

bool FragmentSpecification::areNestedOperatorsInsideMultiObjectiveFormulasAllowed() const {
    return this->nestedOperatorsInsideMultiObjectiveFormulas;
}

FragmentSpecification& FragmentSpecification::setNestedOperatorsInsideMultiObjectiveFormulasAllowed(bool newValue) {
    this->nestedOperatorsInsideMultiObjectiveFormulas = newValue;
    return *this;
}

bool FragmentSpecification::areOnlyEventuallyFormuluasInConditionalFormulasAllowed() const {
    return this->onlyEventuallyFormuluasInConditionalFormulas;
}

FragmentSpecification& FragmentSpecification::setOnlyEventuallyFormuluasInConditionalFormulasAllowed(bool newValue) {
    this->onlyEventuallyFormuluasInConditionalFormulas = newValue;
    return *this;
}

bool FragmentSpecification::areStepBoundedUntilFormulasAllowed() const {
    return this->stepBoundedUntilFormulas;
}

FragmentSpecification& FragmentSpecification::setStepBoundedUntilFormulasAllowed(bool newValue) {
    this->stepBoundedUntilFormulas = newValue;
    return *this;
}

bool FragmentSpecification::areTimeBoundedUntilFormulasAllowed() const {
    return this->timeBoundedUntilFormulas;
}

FragmentSpecification& FragmentSpecification::setTimeBoundedUntilFormulasAllowed(bool newValue) {
    this->timeBoundedUntilFormulas = newValue;
    return *this;
}

bool FragmentSpecification::areRewardBoundedUntilFormulasAllowed() const {
    return this->rewardBoundedUntilFormulas;
}

FragmentSpecification& FragmentSpecification::setRewardBoundedUntilFormulasAllowed(bool newValue) {
    this->rewardBoundedUntilFormulas = newValue;
    return *this;
}

bool FragmentSpecification::areMultiDimensionalBoundedUntilFormulasAllowed() const {
    return this->multiDimensionalBoundedUntilFormulas;
}

FragmentSpecification& FragmentSpecification::setMultiDimensionalBoundedUntilFormulasAllowed(bool newValue) {
    this->multiDimensionalBoundedUntilFormulas = newValue;
    return *this;
}

bool FragmentSpecification::areStepBoundedCumulativeRewardFormulasAllowed() const {
    return this->stepBoundedCumulativeRewardFormulas;
}

FragmentSpecification& FragmentSpecification::setStepBoundedCumulativeRewardFormulasAllowed(bool newValue) {
    this->stepBoundedCumulativeRewardFormulas = newValue;
    return *this;
}

bool FragmentSpecification::areTimeBoundedCumulativeRewardFormulasAllowed() const {
    return this->timeBoundedCumulativeRewardFormulas;
}

FragmentSpecification& FragmentSpecification::setTimeBoundedCumulativeRewardFormulasAllowed(bool newValue) {
    this->timeBoundedCumulativeRewardFormulas = newValue;
    return *this;
}

bool FragmentSpecification::areRewardBoundedCumulativeRewardFormulasAllowed() const {
    return this->rewardBoundedCumulativeRewardFormulas;
}

FragmentSpecification& FragmentSpecification::setRewardBoundedCumulativeRewardFormulasAllowed(bool newValue) {
    this->rewardBoundedCumulativeRewardFormulas = newValue;
    return *this;
}

bool FragmentSpecification::areMultiDimensionalCumulativeRewardFormulasAllowed() const {
    return this->multiDimensionalCumulativeRewardFormulas;
}

FragmentSpecification& FragmentSpecification::setMultiDimensionalCumulativeRewardFormulasAllowed(bool newValue) {
    this->multiDimensionalCumulativeRewardFormulas = newValue;
    return *this;
}

FragmentSpecification& FragmentSpecification::setOperatorsAllowed(bool newValue) {
    this->setProbabilityOperatorsAllowed(newValue);
    this->setRewardOperatorsAllowed(newValue);
    this->setLongRunAverageOperatorsAllowed(newValue);
    this->setTimeOperatorsAllowed(newValue);
    return *this;
}

FragmentSpecification& FragmentSpecification::setTimeAllowed(bool newValue) {
    this->setTimeOperatorsAllowed(newValue);
    this->setReachbilityTimeFormulasAllowed(newValue);
    return *this;
}

FragmentSpecification& FragmentSpecification::setLongRunAverageProbabilitiesAllowed(bool newValue) {
    this->setLongRunAverageOperatorsAllowed(newValue);
    return *this;
}

bool FragmentSpecification::areQuantitativeOperatorResultsAllowed() const {
    return this->quantitativeOperatorResults;
}

FragmentSpecification& FragmentSpecification::setQuantitativeOperatorResultsAllowed(bool newValue) {
    this->quantitativeOperatorResults = newValue;
    return *this;
}

bool FragmentSpecification::areQualitativeOperatorResultsAllowed() const {
    return this->qualitativeOperatorResults;
}

FragmentSpecification& FragmentSpecification::setQualitativeOperatorResultsAllowed(bool newValue) {
    this->qualitativeOperatorResults = newValue;
    return *this;
}

bool FragmentSpecification::isOperatorAtTopLevelRequired() const {
    return operatorAtTopLevelRequired;
}

FragmentSpecification& FragmentSpecification::setOperatorAtTopLevelRequired(bool newValue) {
    operatorAtTopLevelRequired = newValue;
    return *this;
}

bool FragmentSpecification::isMultiObjectiveFormulaAtTopLevelRequired() const {
    return multiObjectiveFormulaAtTopLevelRequired;
}

FragmentSpecification& FragmentSpecification::setMultiObjectiveFormulaAtTopLevelRequired(bool newValue) {
    multiObjectiveFormulaAtTopLevelRequired = newValue;
    return *this;
}

bool FragmentSpecification::areOperatorsAtTopLevelOfMultiObjectiveFormulasRequired() const {
    return operatorsAtTopLevelOfMultiObjectiveFormulasRequired;
}

FragmentSpecification& FragmentSpecification::setOperatorsAtTopLevelOfMultiObjectiveFormulasRequired(bool newValue) {
    operatorsAtTopLevelOfMultiObjectiveFormulasRequired = newValue;
    return *this;
}

bool FragmentSpecification::isQuantileFormulaAtTopLevelRequired() const {
    return quantileFormulaAtTopLevelRequired;
}

FragmentSpecification& FragmentSpecification::setQuantileFormulaAtTopLevelRequired(bool newValue) {
    quantileFormulaAtTopLevelRequired = newValue;
    return *this;
}

bool FragmentSpecification::isRewardAccumulationAllowed() const {
    return rewardAccumulation;
}

FragmentSpecification& FragmentSpecification::setRewardAccumulationAllowed(bool newValue) {
    rewardAccumulation = newValue;
    return *this;
}

bool FragmentSpecification::areGameFormulasAllowed() const {
    return gameFormula;
}

FragmentSpecification& FragmentSpecification::setGameFormulasAllowed(bool newValue) {
    gameFormula = newValue;
    return *this;
}

bool FragmentSpecification::areDiscountedCumulativeRewardFormulasAllowed() const {
    return discountedCumulativeRewardFormula;
}

FragmentSpecification& FragmentSpecification::setDiscountedCumulativeRewardFormulasAllowed(bool newValue) {
    this->discountedCumulativeRewardFormula = newValue;
    return *this;
}

bool FragmentSpecification::areDiscountedTotalRewardFormulasAllowed() const {
    return discountedTotalRewardFormula;
}

FragmentSpecification& FragmentSpecification::setDiscountedTotalRewardFormulasAllowed(bool newValue) {
    this->discountedTotalRewardFormula = newValue;
    return *this;
}

}  // namespace logic
}  // namespace storm
