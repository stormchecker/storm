#include "storm/builder/BuilderOptions.h"

#include "storm/builder/TerminalStatesGetter.h"

#include "storm/logic/Formulas.h"
#include "storm/logic/LiftableTransitionRewardsVisitor.h"

#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace builder {

LabelOrExpression::LabelOrExpression(storm::expressions::Expression const& expression) : labelOrExpression(expression) {
    // Intentionally left empty.
}

LabelOrExpression::LabelOrExpression(std::string const& label) : labelOrExpression(label) {
    // Intentionally left empty.
}

bool LabelOrExpression::isLabel() const {
    return labelOrExpression.which() == 0;
}

std::string const& LabelOrExpression::getLabel() const {
    return boost::get<std::string>(labelOrExpression);
}

bool LabelOrExpression::isExpression() const {
    return labelOrExpression.which() == 1;
}

storm::expressions::Expression const& LabelOrExpression::getExpression() const {
    return boost::get<storm::expressions::Expression>(labelOrExpression);
}

BuilderOptions::BuilderOptions(bool buildAllRewardModels, bool buildAllLabels)
    : buildAllRewardModels(buildAllRewardModels),
      buildAllLabels(buildAllLabels),
      applyMaximalProgressAssumption(false),
      buildChoiceLabels(false),
      buildStateValuations(false),
      buildObservationValuations(false),
      buildChoiceOrigins(false),
      scaleAndLiftTransitionRewards(true),
      explorationChecks(false),
      inferObservationsFromActions(false),
      addOverlappingGuardsLabel(false),
      addOutOfBoundsState(false),
      reservedBitsForUnboundedVariables(32),
      showProgress(false),
      showProgressDelay(0),
      stochasticTolerance(0.0) {
    // Intentionally left empty.
}

BuilderOptions::BuilderOptions(storm::logic::Formula const& formula, storm::storage::SymbolicModelDescription const& modelDescription)
    : BuilderOptions({formula.asSharedPointer()}, modelDescription) {
    // Intentionally left empty.
}

BuilderOptions::BuilderOptions(std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas,
                               storm::storage::SymbolicModelDescription const& modelDescription)
    : BuilderOptions() {
    if (!formulas.empty()) {
        for (auto const& formula : formulas) {
            this->preserveFormula(*formula, modelDescription);
        }
        if (formulas.size() == 1) {
            this->setTerminalStatesFromFormula(*formulas.front());
        }
    }

    if (modelDescription.hasModel()) {
        this->setApplyMaximalProgressAssumption(modelDescription.getModelType() == storm::storage::SymbolicModelDescription::ModelType::MA);
        this->setBuildChoiceOrigins(modelDescription.getModelType() == storm::storage::SymbolicModelDescription::ModelType::POMDP);
        this->setBuildChoiceLabels(modelDescription.getModelType() == storm::storage::SymbolicModelDescription::ModelType::POMDP);
    }
}

void BuilderOptions::preserveFormula(storm::logic::Formula const& formula, storm::storage::SymbolicModelDescription const& modelDescription) {
    // If we already had terminal states, we need to erase them.
    if (hasTerminalStates()) {
        clearTerminalStates();
    }

    // Determine the reward models we need to build.
    std::set<std::string> referencedRewardModels = formula.getReferencedRewardModels();
    for (auto const& rewardModelName : referencedRewardModels) {
        rewardModelNames.emplace(rewardModelName);
    }

    // Extract all the labels used in the formula.
    std::vector<std::shared_ptr<storm::logic::AtomicLabelFormula const>> atomicLabelFormulas = formula.getAtomicLabelFormulas();
    for (auto const& formula : atomicLabelFormulas) {
        addLabel(formula->getLabel());
    }

    // Extract all the expressions used in the formula.
    std::vector<std::shared_ptr<storm::logic::AtomicExpressionFormula const>> atomicExpressionFormulas = formula.getAtomicExpressionFormulas();
    for (auto const& formula : atomicExpressionFormulas) {
        addLabel(formula->getExpression());
    }

    scaleAndLiftTransitionRewards =
        scaleAndLiftTransitionRewards && storm::logic::LiftableTransitionRewardsVisitor(modelDescription).areTransitionRewardsLiftable(formula);
}

void BuilderOptions::setTerminalStatesFromFormula(storm::logic::Formula const& formula) {
    getTerminalStatesFromFormula(
        formula, [this](storm::expressions::Expression const& expr, bool inverted) { this->addTerminalExpression(expr, inverted); },
        [this](std::string const& label, bool inverted) { this->addTerminalLabel(label, inverted); });
}

std::set<std::string> const& BuilderOptions::getRewardModelNames() const {
    return rewardModelNames;
}

std::set<std::string> const& BuilderOptions::getLabelNames() const {
    return labelNames;
}

std::vector<std::pair<std::string, storm::expressions::Expression>> const& BuilderOptions::getExpressionLabels() const {
    return expressionLabels;
}

std::vector<std::pair<LabelOrExpression, bool>> const& BuilderOptions::getTerminalStates() const {
    return terminalStates;
}

bool BuilderOptions::hasTerminalStates() const {
    return !terminalStates.empty();
}

void BuilderOptions::clearTerminalStates() {
    terminalStates.clear();
}

bool BuilderOptions::isApplyMaximalProgressAssumptionSet() const {
    return applyMaximalProgressAssumption;
}

bool BuilderOptions::isBuildChoiceLabelsSet() const {
    return buildChoiceLabels;
}

bool BuilderOptions::isBuildStateValuationsSet() const {
    return buildStateValuations;
}

bool BuilderOptions::isBuildObservationValuationsSet() const {
    return buildObservationValuations;
}

bool BuilderOptions::isBuildChoiceOriginsSet() const {
    return buildChoiceOrigins;
}

bool BuilderOptions::isBuildAllRewardModelsSet() const {
    return buildAllRewardModels;
}

bool BuilderOptions::isBuildAllLabelsSet() const {
    return buildAllLabels;
}

bool BuilderOptions::isInferObservationsFromActionsSet() const {
    return inferObservationsFromActions;
}

bool BuilderOptions::isScaleAndLiftTransitionRewardsSet() const {
    return scaleAndLiftTransitionRewards;
}

bool BuilderOptions::isAddOutOfBoundsStateSet() const {
    return addOutOfBoundsState;
}

uint64_t BuilderOptions::getReservedBitsForUnboundedVariables() const {
    return reservedBitsForUnboundedVariables;
}

bool BuilderOptions::isAddOverlappingGuardLabelSet() const {
    return addOverlappingGuardsLabel;
}

BuilderOptions& BuilderOptions::setBuildAllRewardModels(bool newValue) {
    buildAllRewardModels = newValue;
    return *this;
}

bool BuilderOptions::isExplorationChecksSet() const {
    return explorationChecks;
}

bool BuilderOptions::isShowProgressSet() const {
    return showProgress;
}

uint64_t BuilderOptions::getShowProgressDelay() const {
    return showProgressDelay;
}

double BuilderOptions::getStochasticTolerance() const {
    return stochasticTolerance;
}

BuilderOptions& BuilderOptions::setExplorationChecks(bool newValue) {
    explorationChecks = newValue;
    return *this;
}

BuilderOptions& BuilderOptions::addRewardModel(std::string const& rewardModelName) {
    STORM_LOG_THROW(!buildAllRewardModels, storm::exceptions::InvalidSettingsException, "Cannot add reward model, because all reward models are built anyway.");
    rewardModelNames.emplace(rewardModelName);
    return *this;
}

BuilderOptions& BuilderOptions::setBuildAllLabels(bool newValue) {
    buildAllLabels = newValue;
    return *this;
}

BuilderOptions& BuilderOptions::addLabel(storm::expressions::Expression const& expression) {
    std::stringstream stream;
    stream << expression;
    expressionLabels.emplace_back(stream.str(), expression);
    return *this;
}

BuilderOptions& BuilderOptions::addLabel(std::string const& labelName) {
    STORM_LOG_THROW(!buildAllLabels, storm::exceptions::InvalidSettingsException, "Cannot add label, because all labels are built anyway.");
    labelNames.insert(labelName);
    return *this;
}

BuilderOptions& BuilderOptions::addTerminalExpression(storm::expressions::Expression const& expression, bool value) {
    terminalStates.push_back(std::make_pair(LabelOrExpression(expression), value));
    return *this;
}

BuilderOptions& BuilderOptions::addTerminalLabel(std::string const& label, bool value) {
    terminalStates.push_back(std::make_pair(LabelOrExpression(label), value));
    return *this;
}

BuilderOptions& BuilderOptions::setApplyMaximalProgressAssumption(bool newValue) {
    applyMaximalProgressAssumption = newValue;
    return *this;
}

BuilderOptions& BuilderOptions::setBuildChoiceLabels(bool newValue) {
    buildChoiceLabels = newValue;
    return *this;
}

BuilderOptions& BuilderOptions::setBuildStateValuations(bool newValue) {
    buildStateValuations = newValue;
    return *this;
}

BuilderOptions& BuilderOptions::setBuildObservationValuations(bool newValue) {
    buildObservationValuations = newValue;
    return *this;
}

BuilderOptions& BuilderOptions::setBuildChoiceOrigins(bool newValue) {
    buildChoiceOrigins = newValue;
    return *this;
}

BuilderOptions& BuilderOptions::setScaleAndLiftTransitionRewards(bool newValue) {
    scaleAndLiftTransitionRewards = newValue;
    return *this;
}

BuilderOptions& BuilderOptions::setAddOutOfBoundsState(bool newValue) {
    addOutOfBoundsState = newValue;
    return *this;
}

BuilderOptions& BuilderOptions::setReservedBitsForUnboundedVariables(uint64_t newValue) {
    reservedBitsForUnboundedVariables = newValue;
    return *this;
}

BuilderOptions& BuilderOptions::setAddOverlappingGuardsLabel(bool newValue) {
    addOverlappingGuardsLabel = newValue;
    return *this;
}

BuilderOptions& BuilderOptions::setStochasticTolerance(double newValue) {
    stochasticTolerance = newValue;
    return *this;
}

BuilderOptions& BuilderOptions::setShowProgress(bool newValue) {
    showProgress = newValue;
    return *this;
}

BuilderOptions& BuilderOptions::setShowProgressDelay(uint64_t newValue) {
    showProgressDelay = newValue;
    return *this;
}

BuilderOptions& BuilderOptions::substituteExpressions(
    std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const& substitutionFunction) {
    for (auto& e : expressionLabels) {
        e.second = substitutionFunction(e.second);
    }

    for (auto& t : terminalStates) {
        if (t.first.isExpression()) {
            t.first = LabelOrExpression(substitutionFunction(t.first.getExpression()));
        }
    }
    return *this;
}

}  // namespace builder
}  // namespace storm
