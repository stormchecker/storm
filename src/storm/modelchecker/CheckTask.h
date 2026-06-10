#ifndef STORM_MODELCHECKER_CHECKTASK_H_
#define STORM_MODELCHECKER_CHECKTASK_H_

#include <boost/optional.hpp>
#include <memory>

#include "storm/logic/Formulas.h"
#include "storm/utility/constants.h"

#include "storm/adapters/IntervalForward.h"
#include "storm/logic/ComparisonType.h"
#include "storm/logic/PlayerCoalition.h"
#include "storm/modelchecker/hints/ModelCheckerHint.h"
#include "storm/solver/UncertaintyResolutionMode.h"

#include "storm/exceptions/InvalidOperationException.h"

namespace storm {
namespace logic {
class Formula;
}

namespace modelchecker {

enum class CheckType { Probabilities, Rewards };

/*
 * This class is used to customize the checking process of a formula.
 */
template<typename FormulaType = storm::logic::Formula, typename ValueType = double>
class CheckTask {
   public:
    template<typename OtherFormulaType, typename OtherValueType>
    friend class CheckTask;

    /*!
     * Creates a task object with the default options for the given formula.
     */
    CheckTask(FormulaType const& formula, bool onlyInitialStatesRelevant = false, UncertaintyResolutionMode = UncertaintyResolutionMode::Unset)
        : formula(formula), hint(new ModelCheckerHint()) {
        this->onlyInitialStatesRelevant = onlyInitialStatesRelevant;
        this->produceSchedulers = false;
        this->qualitative = false;
        this->uncertaintyResolutionMode = UncertaintyResolutionMode::Unset;

        updateOperatorInformation();
    }

    /*!
     * Copies the check task from the source while replacing the formula with the new one. In particular, this
     * changes the formula type of the check task object.
     */
    template<typename NewFormulaType>
    CheckTask<NewFormulaType, ValueType> substituteFormula(NewFormulaType const& newFormula) const {
        CheckTask<NewFormulaType, ValueType> result(newFormula, this->optimizationDirection, this->playerCoalition, this->rewardModel,
                                                    this->onlyInitialStatesRelevant, this->bound, this->qualitative, this->produceSchedulers, this->hint,
                                                    this->uncertaintyResolutionMode);
        result.updateOperatorInformation();
        return result;
    }

    /*!
     * If the currently specified formula is an OperatorFormula, this method updates the information that is given in the Operator formula.
     * Calling this method has no effect if the provided formula is not an operator formula.
     */
    void updateOperatorInformation() {
        storm::logic::Formula const* formulaForOperatorInformation = &formula.get();
        if (formulaForOperatorInformation->isCvarFormula()) {
            formulaForOperatorInformation = &formulaForOperatorInformation->asCvarFormula().getSubformula();
        }

        if (formulaForOperatorInformation->isOperatorFormula()) {
            storm::logic::OperatorFormula const& operatorFormula = formulaForOperatorInformation->asOperatorFormula();
            if (operatorFormula.hasBound()) {
                this->bound = operatorFormula.getBound();
            }

            if (operatorFormula.hasOptimalityType()) {
                this->optimizationDirection = operatorFormula.getOptimalityType();
            } else if (operatorFormula.hasBound()) {
                this->optimizationDirection = operatorFormula.getComparisonType() == storm::logic::ComparisonType::Less ||
                                                      operatorFormula.getComparisonType() == storm::logic::ComparisonType::LessEqual
                                                  ? OptimizationDirection::Maximize
                                                  : OptimizationDirection::Minimize;
            }

            if (formulaForOperatorInformation->isProbabilityOperatorFormula()) {
                storm::logic::ProbabilityOperatorFormula const& probabilityOperatorFormula = formulaForOperatorInformation->asProbabilityOperatorFormula();

                if (probabilityOperatorFormula.hasBound()) {
                    if (storm::utility::isZero(probabilityOperatorFormula.template getThresholdAs<ValueType>()) ||
                        storm::utility::isOne(probabilityOperatorFormula.template getThresholdAs<ValueType>())) {
                        this->qualitative = true;
                    }
                }
            } else if (formulaForOperatorInformation->isRewardOperatorFormula()) {
                storm::logic::RewardOperatorFormula const& rewardOperatorFormula = formulaForOperatorInformation->asRewardOperatorFormula();
                this->rewardModel = rewardOperatorFormula.getOptionalRewardModelName();

                if (rewardOperatorFormula.hasBound()) {
                    if (storm::utility::isZero(rewardOperatorFormula.template getThresholdAs<ValueType>())) {
                        this->qualitative = true;
                    }
                }
            }
        }
    }

    /*!
     * Negate the optimization direction and the bound threshold, if those exist.
     * Suitable for switching from Pmin[phi] to 1-Pmax[!psi] computations.
     */
    CheckTask negate() const {
        CheckTask result(*this);
        if (isOptimizationDirectionSet()) {
            // switch from min to max and vice-versa
            result.setOptimizationDirection(invert(getOptimizationDirection()));
        }

        if (isBoundSet()) {
            // invert bound comparison type (retain strictness),
            // convert threshold to 1- threshold
            result.bound = storm::logic::Bound(storm::logic::invertPreserveStrictness(getBound().comparisonType), 1 - getBound().threshold);
        }

        return result;
    }

    /*!
     * Copies the check task from the source while replacing the considered ValueType the new one. In particular, this
     * changes the formula type of the check task object.
     */
    template<typename NewValueType>
    CheckTask<FormulaType, NewValueType> convertValueType() const {
        return CheckTask<FormulaType, NewValueType>(this->formula, this->optimizationDirection, this->playerCoalition, this->rewardModel,
                                                    this->onlyInitialStatesRelevant, this->bound, this->qualitative, this->produceSchedulers, this->hint,
                                                    this->uncertaintyResolutionMode);
    }

    /*!
     * Retrieves the formula from this task.
     */
    FormulaType const& getFormula() const {
        return formula.get();
    }

    /*!
     * Retrieves whether an optimization direction was set.
     */
    bool isOptimizationDirectionSet() const {
        return static_cast<bool>(optimizationDirection);
    }

    /*!
     * Retrieves the optimization direction (if set).
     */
    storm::OptimizationDirection const& getOptimizationDirection() const {
        return optimizationDirection.get();
    }

    /*!
     * Sets the optimization direction.
     */
    void setOptimizationDirection(storm::OptimizationDirection const& dir) {
        optimizationDirection = dir;
    }

    /*!
     * Retrieves whether a player coalition was set.
     */
    bool isPlayerCoalitionSet() const {
        return static_cast<bool>(playerCoalition);
    }

    /*!
     * Retrieves the player coalition (if set).
     */
    storm::logic::PlayerCoalition const& getPlayerCoalition() const {
        return playerCoalition.get();
    }

    /*!
     * Sets the player coalition.
     */
    CheckTask<FormulaType, ValueType>& setPlayerCoalition(storm::logic::PlayerCoalition const& coalition) {
        playerCoalition = coalition;
        return *this;
    }

    /*!
     * Retrieves whether a reward model was set.
     */
    bool isRewardModelSet() const {
        return static_cast<bool>(rewardModel);
    }

    /*!
     * Retrieves the reward model over which to perform the checking (if set).
     */
    std::string const& getRewardModel() const {
        return rewardModel.get();
    }

    /*!
     * Retrieves whether only the initial states are relevant in the computation.
     */
    bool isOnlyInitialStatesRelevantSet() const {
        return onlyInitialStatesRelevant;
    }

    /*!
     * Sets whether only initial states are relevant.
     */
    CheckTask<FormulaType, ValueType>& setOnlyInitialStatesRelevant(bool value = true) {
        this->onlyInitialStatesRelevant = value;
        return *this;
    }

    /*!
     * Retrieves whether there is a bound with which the values for the states will be compared.
     */
    bool isBoundSet() const {
        return static_cast<bool>(bound);
    }

    /*!
     * Retrieves the value of the bound (if set).
     */
    ValueType getBoundThreshold() const {
        STORM_LOG_THROW(!bound.get().threshold.containsVariables(), storm::exceptions::InvalidOperationException,
                        "Cannot evaluate threshold '" << bound.get().threshold << "' as it contains undefined constants.");
        return bound.get().template evaluateThresholdAs<ValueType>();
    }

    /*!
     * Retrieves the comparison type of the bound (if set).
     */
    storm::logic::ComparisonType const& getBoundComparisonType() const {
        return bound.get().comparisonType;
    }

    /*!
     * Retrieves the bound (if set).
     */
    storm::logic::Bound const& getBound() const {
        return bound.get();
    }

    /*!
     * Retrieves the bound
     */
    boost::optional<storm::logic::Bound> const& getOptionalBound() const {
        return bound;
    }

    /*!
     * Retrieves whether the computation only needs to be performed qualitatively, because the values will only
     * be compared to 0/1.
     */
    bool isQualitativeSet() const {
        return qualitative;
    }

    /*!
     * sets whether the computation only needs to be performed qualitatively, because the values will only
     * be compared to 0/1.
     */
    void setQualitative(bool value) {
        qualitative = value;
    }

    /*!
     * Sets whether to produce schedulers (if supported).
     */
    void setProduceSchedulers(bool produceSchedulers = true) {
        this->produceSchedulers = produceSchedulers;
    }

    /*!
     * Retrieves whether scheduler(s) are to be produced (if supported).
     */
    bool isProduceSchedulersSet() const {
        return produceSchedulers;
    }

    /*!
     * sets a hint that might contain information that speeds up the modelchecking process (if supported by the model checker)
     */
    void setHint(std::shared_ptr<ModelCheckerHint> const& hint) {
        this->hint = hint;
    }

    /*!
     * Retrieves a hint that might contain information that speeds up the modelchecking process (if supported by the model checker)
     */
    ModelCheckerHint const& getHint() const {
        return *hint;
    }

    ModelCheckerHint& getHint() {
        return *hint;
    }

    /*!
     * Retrieves the mode which decides how the uncertainty will be resolved.
     */
    UncertaintyResolutionMode getUncertaintyResolutionMode() const {
        return uncertaintyResolutionMode;
    }

    /*!
     * Sets the mode which decides how the uncertainty will be resolved.
     */
    void setUncertaintyResolutionMode(UncertaintyResolutionMode uncertaintyResolutionMode) {
        this->uncertaintyResolutionMode = uncertaintyResolutionMode;
    }

    /*!
     * Returns whether the mode, which decides how the uncertainty will be resolved, is set.
     */
    bool isUncertaintyResolutionModeSet() const {
        return isSet(this->uncertaintyResolutionMode);
    }

    /*!
     * Conversion operator that strips the type of the formula.
     */
    operator CheckTask<storm::logic::Formula, ValueType>() const {
        return this->template substituteFormula<storm::logic::Formula>(this->getFormula());
    }

   private:
    /*!
     * Creates a task object with the given options.
     *
     * @param formula The formula to attach to the task.
     * @param optimizationDirection If set, the probabilities will be minimized/maximized.
     * @param playerCoalition If set, the given coalitions of players will be assumed.
     * @param rewardModelName If given, the checking has to be done wrt. to this reward model.
     * @param onlyInitialStatesRelevant If set to true, the model checker may decide to only compute the values
     * for the initial states.
     * @param bound The bound with which the states will be compared.
     * @param qualitative A flag specifying whether the property needs to be checked qualitatively, i.e. compared
     * with bounds 0/1.
     * @param produceSchedulers If supported by the model checker and the model formalism, schedulers to achieve
     * a value will be produced if this flag is set.
     */
    CheckTask(std::reference_wrapper<FormulaType const> const& formula, boost::optional<storm::OptimizationDirection> const& optimizationDirection,
              boost::optional<storm::logic::PlayerCoalition> playerCoalition, boost::optional<std::string> const& rewardModel, bool onlyInitialStatesRelevant,
              boost::optional<storm::logic::Bound> const& bound, bool qualitative, bool produceSchedulers, std::shared_ptr<ModelCheckerHint> const& hint,
              UncertaintyResolutionMode uncertaintyResolutionMode)
        : formula(formula),
          optimizationDirection(optimizationDirection),
          playerCoalition(playerCoalition),
          rewardModel(rewardModel),
          onlyInitialStatesRelevant(onlyInitialStatesRelevant),
          bound(bound),
          qualitative(qualitative),
          produceSchedulers(produceSchedulers),
          hint(hint),
          uncertaintyResolutionMode(uncertaintyResolutionMode) {
        // Intentionally left empty.
    }

    // The formula that is to be checked.
    std::reference_wrapper<FormulaType const> formula;

    // If set, the probabilities will be minimized/maximized.
    boost::optional<storm::OptimizationDirection> optimizationDirection;

    // If set, the given coalitions of players will be assumed.
    boost::optional<storm::logic::PlayerCoalition> playerCoalition;

    // If set, the reward property has to be interpreted over this model.
    boost::optional<std::string> rewardModel;

    // If set to true, the model checker may decide to only compute the values for the initial states.
    bool onlyInitialStatesRelevant;

    // The bound with which the states will be compared.
    boost::optional<storm::logic::Bound> bound;

    // A flag specifying whether the property needs to be checked qualitatively, i.e. compared with bounds 0/1.
    bool qualitative;

    // If supported by the model checker and the model formalism, schedulers to achieve a value will be produced
    // if this flag is set.
    bool produceSchedulers;

    // A hint that might contain information that speeds up the modelchecking process (if supported by the model checker)
    std::shared_ptr<ModelCheckerHint> hint;

    // Whether uncertainty should be resolved be minimizing, maximizing, acting robust or cooperative.
    UncertaintyResolutionMode uncertaintyResolutionMode;
};

}  // namespace modelchecker
}  // namespace storm

#endif /* STORM_MODELCHECKER_CHECKTASK_H_ */
