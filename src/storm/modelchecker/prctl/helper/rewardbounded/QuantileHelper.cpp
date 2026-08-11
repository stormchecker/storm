#include "storm/modelchecker/prctl/helper/rewardbounded/QuantileHelper.h"

#include <boost/optional.hpp>
#include <memory>
#include <set>
#include <vector>

#include "storm/environment/solver/MinMaxSolverEnvironment.h"

#include "storm/modelchecker/prctl/helper/rewardbounded/MultiDimensionalRewardUnfolding.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/utility/vector.h"

#include "storm/logic/BoundedUntilFormula.h"
#include "storm/logic/ProbabilityOperatorFormula.h"

#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm {
namespace modelchecker {
namespace helper {
namespace rewardbounded {

template<typename ModelType>
QuantileHelper<ModelType>::QuantileHelper(ModelType const& model, storm::logic::QuantileFormula const& quantileFormula)
    : model(model), quantileFormula(quantileFormula) {
    // Do all kinds of sanity check.
    std::set<storm::expressions::Variable> quantileVariables;
    for (auto const& quantileVariable : quantileFormula.getBoundVariables()) {
        STORM_LOG_THROW(quantileVariables.count(quantileVariable) == 0, storm::exceptions::NotSupportedException,
                        "Quantile formula considers the same bound variable twice.");
        quantileVariables.insert(quantileVariable);
    }
    STORM_LOG_THROW(quantileFormula.getSubformula().isProbabilityOperatorFormula(), storm::exceptions::NotSupportedException,
                    "Quantile formula needs probability operator inside. The formula " << quantileFormula << " is not supported.");
    auto const& probOpFormula = quantileFormula.getSubformula().asProbabilityOperatorFormula();
    STORM_LOG_THROW(probOpFormula.hasBound(), storm::exceptions::InvalidOperationException,
                    "Probability operator inside quantile formula needs to have a bound.");
    STORM_LOG_THROW(!model.isNondeterministicModel() || probOpFormula.hasOptimalityType(), storm::exceptions::InvalidOperationException,
                    "Probability operator inside quantile formula needs to have an optimality type.");
    STORM_LOG_WARN_COND(probOpFormula.getBound().comparisonType == storm::logic::ComparisonType::Greater ||
                            probOpFormula.getBound().comparisonType == storm::logic::ComparisonType::LessEqual,
                        "Probability operator inside quantile formula needs to have bound > or <=. The specified comparison type might lead to "
                        "non-termination.");  // This has to do with letting bound variables approach infinity, e.g.,  Pr>0.7 [F "goal"] holds iff Pr>0.7 [F<=B
                                              // "goal"] holds for some B.
    STORM_LOG_THROW(probOpFormula.getSubformula().isBoundedUntilFormula(), storm::exceptions::NotSupportedException,
                    "Quantile formula needs bounded until probability operator formula as subformula. The formula " << quantileFormula << " is not supported.");
    auto const& boundedUntilFormula = probOpFormula.getSubformula().asBoundedUntilFormula();
    std::set<storm::expressions::Variable> boundVariables;
    for (uint64_t dim = 0; dim < boundedUntilFormula.getDimension(); ++dim) {
        storm::expressions::Expression boundExpression;
        if (boundedUntilFormula.hasUpperBound(dim)) {
            STORM_LOG_THROW(!boundedUntilFormula.hasLowerBound(dim), storm::exceptions::NotSupportedException,
                            "Interval bounds are not supported within quantile formulas.");
            STORM_LOG_THROW(!boundedUntilFormula.isUpperBoundStrict(dim), storm::exceptions::NotSupportedException,
                            "Only non-strict upper reward bounds are supported for quantiles.");
            boundExpression = boundedUntilFormula.getUpperBound(dim);
        } else if (boundedUntilFormula.hasLowerBound(dim)) {
            STORM_LOG_THROW(!boundedUntilFormula.isLowerBoundStrict(dim), storm::exceptions::NotSupportedException,
                            "Only non-strict lower reward bounds are supported for quantiles.");
            boundExpression = boundedUntilFormula.getLowerBound(dim);
        }
        if (boundExpression.isInitialized() && boundExpression.containsVariables()) {
            STORM_LOG_THROW(
                boundExpression.isVariable(), storm::exceptions::NotSupportedException,
                "Non-trivial bound expressions such as '" << boundExpression << "' are not supported. Either specify a constant or a quantile variable.");
            storm::expressions::Variable const& boundVariable = boundExpression.getBaseExpression().asVariableExpression().getVariable();
            STORM_LOG_THROW(boundVariables.count(boundVariable) == 0, storm::exceptions::NotSupportedException,
                            "Variable " << boundExpression << " occurs at multiple reward bounds.");
            boundVariables.insert(boundVariable);
            STORM_LOG_THROW(quantileVariables.count(boundVariable) == 1, storm::exceptions::NotSupportedException,
                            "The formula contains undefined constant '" << boundExpression << "'.");
        }
    }
}

enum class BoundTransformation { None, GreaterZero, GreaterEqualZero, LessEqualZero };
std::shared_ptr<storm::logic::ProbabilityOperatorFormula> transformBoundedUntilOperator(storm::logic::ProbabilityOperatorFormula const& boundedUntilOperator,
                                                                                        std::vector<BoundTransformation> const& transformations,
                                                                                        bool complementQuery = false) {
    auto const& origBoundedUntil = boundedUntilOperator.getSubformula().asBoundedUntilFormula();
    STORM_LOG_ASSERT(transformations.size() == origBoundedUntil.getDimension(),
                     "Tried to replace the bound of a dimension that is higher than the number of dimensions of the formula.");
    std::vector<std::shared_ptr<storm::logic::Formula const>> leftSubformulas, rightSubformulas;
    std::vector<boost::optional<storm::logic::TimeBound>> lowerBounds, upperBounds;
    std::vector<storm::logic::TimeBoundReference> timeBoundReferences;

    for (uint64_t dim = 0; dim < origBoundedUntil.getDimension(); ++dim) {
        if (origBoundedUntil.hasMultiDimensionalSubformulas()) {
            leftSubformulas.push_back(origBoundedUntil.getLeftSubformula(dim).asSharedPointer());
            rightSubformulas.push_back(origBoundedUntil.getRightSubformula(dim).asSharedPointer());
        }
        timeBoundReferences.push_back(origBoundedUntil.getTimeBoundReference(dim));
        if (transformations[dim] == BoundTransformation::None) {
            if (origBoundedUntil.hasLowerBound(dim)) {
                lowerBounds.push_back(storm::logic::TimeBound(origBoundedUntil.isLowerBoundStrict(dim), origBoundedUntil.getLowerBound(dim)));
            } else {
                lowerBounds.push_back(boost::none);
            }
            if (origBoundedUntil.hasUpperBound(dim)) {
                upperBounds.push_back(storm::logic::TimeBound(origBoundedUntil.isUpperBoundStrict(dim), origBoundedUntil.getUpperBound(dim)));
            } else {
                upperBounds.push_back(boost::none);
            }
        } else {
            // We need a zero expression in all other cases
            storm::expressions::Expression zero;
            if (origBoundedUntil.hasLowerBound(dim)) {
                zero = origBoundedUntil.getLowerBound(dim).getManager().rational(0.0);
            } else {
                STORM_LOG_THROW(origBoundedUntil.hasUpperBound(dim), storm::exceptions::InvalidOperationException,
                                "The given bounded until formula has no cost-bound for one dimension.");
                zero = origBoundedUntil.getUpperBound(dim).getManager().rational(0.0);
            }
            if (transformations[dim] == BoundTransformation::LessEqualZero) {
                lowerBounds.push_back(boost::none);
                upperBounds.push_back(storm::logic::TimeBound(false, zero));
            } else {
                STORM_LOG_ASSERT(transformations[dim] == BoundTransformation::GreaterZero || transformations[dim] == BoundTransformation::GreaterEqualZero,
                                 "Unhandled bound transformation.");
                lowerBounds.push_back(storm::logic::TimeBound(transformations[dim] == BoundTransformation::GreaterZero, zero));
                upperBounds.push_back(boost::none);
            }
        }
    }
    std::shared_ptr<storm::logic::Formula> newBoundedUntil;
    if (origBoundedUntil.hasMultiDimensionalSubformulas()) {
        newBoundedUntil = std::make_shared<storm::logic::BoundedUntilFormula>(leftSubformulas, rightSubformulas, lowerBounds, upperBounds, timeBoundReferences);
    } else {
        newBoundedUntil = std::make_shared<storm::logic::BoundedUntilFormula>(origBoundedUntil.getLeftSubformula().asSharedPointer(),
                                                                              origBoundedUntil.getRightSubformula().asSharedPointer(), lowerBounds, upperBounds,
                                                                              timeBoundReferences);
    }
    storm::logic::OperatorInformation newOpInfo(boundedUntilOperator.getOperatorInformation().optimalityType, boundedUntilOperator.getBound());
    if (complementQuery) {
        newOpInfo.bound->comparisonType = storm::logic::invert(newOpInfo.bound->comparisonType);
    }
    return std::make_shared<storm::logic::ProbabilityOperatorFormula>(newBoundedUntil, newOpInfo);
}

/// Increases the precision of solver results
void increasePrecision(storm::Environment& env) {
    STORM_LOG_DEBUG("Increasing precision of underlying solver.");
    auto factor = storm::utility::convertNumber<storm::RationalNumber, std::string>("0.1");
    env.solver().setLinearEquationSolverPrecision(
        static_cast<storm::RationalNumber>(env.solver().getPrecisionOfLinearEquationSolver(env.solver().getLinearEquationSolverType()).first.get() * factor));
    env.solver().minMax().setPrecision(env.solver().minMax().getPrecision() * factor);
}

/*!
 * Computes a lower / upper bound on the actual result of a sound minmax or linear equation solver
 *
 */
template<typename ValueType>
std::pair<ValueType, ValueType> getLowerUpperBound(storm::Environment const& env, ValueType const& factor, ValueType const& value, bool minMax = true) {
    ValueType prec;
    bool relative;
    if (minMax) {
        prec = storm::utility::convertNumber<ValueType>(env.solver().minMax().getPrecision());
        relative = env.solver().minMax().getRelativeTerminationCriterion();
    } else {
        prec =
            storm::utility::convertNumber<ValueType>(env.solver().getPrecisionOfLinearEquationSolver(env.solver().getLinearEquationSolverType()).first.get());
        relative = env.solver().getPrecisionOfLinearEquationSolver(env.solver().getLinearEquationSolverType()).second.get();
    }
    prec *= factor;
    if (relative) {
        ValueType one = storm::utility::one<ValueType>();
        ValueType lower = value * (one / (prec + one));
        ValueType upper = value * (one + prec / (prec + one));
        return std::make_pair(lower, upper);
    } else {
        return std::pair<ValueType, ValueType>(value - prec, value + prec);
    }
}

template<typename ModelType>
uint64_t QuantileHelper<ModelType>::getDimension() const {
    return quantileFormula.getSubformula().asProbabilityOperatorFormula().getSubformula().asBoundedUntilFormula().getDimension();
}

template<typename ModelType>
storm::storage::BitVector QuantileHelper<ModelType>::getOpenDimensions() const {
    auto const& boundedUntil = quantileFormula.getSubformula().asProbabilityOperatorFormula().getSubformula().asBoundedUntilFormula();
    storm::storage::BitVector res(getDimension(), false);
    for (uint64_t dim = 0; dim < getDimension(); ++dim) {
        auto const& bound = boundedUntil.hasLowerBound(dim) ? boundedUntil.getLowerBound(dim) : boundedUntil.getUpperBound(dim);
        if (bound.containsVariables()) {
            res.set(dim, true);
        }
    }
    return res;
}

template<typename ModelType>
storm::expressions::Variable const& QuantileHelper<ModelType>::getVariableForDimension(uint64_t const& dim) const {
    auto const& boundedUntil = quantileFormula.getSubformula().asProbabilityOperatorFormula().getSubformula().asBoundedUntilFormula();
    return (boundedUntil.hasLowerBound(dim) ? boundedUntil.getLowerBound(dim) : boundedUntil.getUpperBound(dim))
        .getBaseExpression()
        .asVariableExpression()
        .getVariable();
}

template<typename ModelType>
std::vector<std::vector<typename ModelType::ValueType>> QuantileHelper<ModelType>::computeQuantile(Environment const& env) {
    numCheckedEpochs = 0;
    numPrecisionRefinements = 0;
    swEpochAnalysis.reset();
    swExploration.reset();
    cachedSubQueryResults.clear();

    std::vector<std::vector<ValueType>> result;
    Environment envCpy = env;  // It might be necessary to increase the precision during the computation
    // Call the internal recursive function
    auto internalResult = computeQuantile(envCpy, getOpenDimensions(), false);

    // Translate the result by applying the scaling factors and permutation.
    std::vector<uint64_t> permutation;
    for (auto const& v : quantileFormula.getBoundVariables()) {
        uint64_t openDim = 0;
        for (auto dim : getOpenDimensions()) {
            if (getVariableForDimension(dim) == v) {
                permutation.push_back(openDim);
                break;
            }
            ++openDim;
        }
    }
    STORM_LOG_ASSERT(permutation.size() == getOpenDimensions().getNumberOfSetBits(), "Permutation size mismatch.");
    for (auto const& costLimits : internalResult.first.getGenerator()) {
        std::vector<ValueType> resultPoint;
        for (auto const& dim : permutation) {
            CostLimit const& cl = costLimits[dim];
            resultPoint.push_back(cl.isInfinity() ? storm::utility::infinity<ValueType>()
                                                  : storm::utility::convertNumber<ValueType>(cl.get()) * internalResult.second[dim]);
        }
        result.push_back(resultPoint);
    }
    if (storm::settings::getModule<storm::settings::modules::CoreSettings>().isShowStatisticsSet()) {
        std::cout << "Number of checked epochs: " << numCheckedEpochs << '\n';
        std::cout << "Number of required precision refinements: " << numPrecisionRefinements << '\n';
        std::cout << "Time for epoch exploration: " << swExploration << " seconds.\n";
        std::cout << "\tTime for epoch model analysis: " << swEpochAnalysis << " seconds.\n";
    }
    return result;
}

template<typename ModelType>
std::pair<CostLimitClosure, std::vector<typename QuantileHelper<ModelType>::ValueType>> QuantileHelper<ModelType>::computeQuantile(
    Environment& env, storm::storage::BitVector const& consideredDimensions, bool complementaryQuery) {
    STORM_LOG_ASSERT(consideredDimensions.isSubsetOf(getOpenDimensions()),
                     "Considered dimensions for a quantile query should be a subset of the set of dimensions without a fixed bound.");

    storm::storage::BitVector cacheKey = consideredDimensions;
    cacheKey.resize(cacheKey.size() + 1, complementaryQuery);
    auto cacheIt = cachedSubQueryResults.find(cacheKey);
    if (cacheIt != cachedSubQueryResults.end()) {
        return cacheIt->second;
    }

    auto boundedUntilOp = transformBoundedUntilOperator(quantileFormula.getSubformula().asProbabilityOperatorFormula(),
                                                        std::vector<BoundTransformation>(getDimension(), BoundTransformation::None), complementaryQuery);
    std::set<storm::expressions::Variable> infinityVariables;
    storm::storage::BitVector lowerBoundedDimensions(getDimension());
    storm::storage::BitVector downwardClosedDimensions(getDimension());
    bool hasLowerValueBound = storm::logic::isLowerBound(boundedUntilOp->getComparisonType());
    for (auto d : getOpenDimensions()) {
        if (consideredDimensions.get(d)) {
            bool hasLowerCostBound = boundedUntilOp->getSubformula().asBoundedUntilFormula().hasLowerBound(d);
            lowerBoundedDimensions.set(d, hasLowerCostBound);
            downwardClosedDimensions.set(d, hasLowerCostBound == hasLowerValueBound);
        } else {
            infinityVariables.insert(getVariableForDimension(d));
        }
    }
    downwardClosedDimensions = downwardClosedDimensions % consideredDimensions;
    CostLimitClosure satCostLimits(downwardClosedDimensions), unsatCostLimits(~downwardClosedDimensions);

    // Initialize the (un)sat cost limits to guarantee termination
    bool onlyUpperCostBounds = lowerBoundedDimensions.empty();
    bool onlyLowerCostBounds = lowerBoundedDimensions == consideredDimensions;
    if (onlyUpperCostBounds || onlyLowerCostBounds) {
        for (auto k : consideredDimensions) {
            storm::storage::BitVector subQueryDimensions = consideredDimensions;
            subQueryDimensions.set(k, false);
            bool subQueryComplement = complementaryQuery != ((onlyUpperCostBounds && hasLowerValueBound) || (onlyLowerCostBounds && !hasLowerValueBound));
            auto subQueryResult = computeQuantile(env, subQueryDimensions, subQueryComplement);
            for (auto const& subQueryCostLimit : subQueryResult.first.getGenerator()) {
                CostLimits initPoint;
                uint64_t i = 0;
                for (auto dim : consideredDimensions) {
                    if (dim == k) {
                        initPoint.push_back(CostLimit::infinity());
                    } else {
                        initPoint.push_back(subQueryCostLimit[i]);
                        ++i;
                    }
                }
                if (subQueryComplement == complementaryQuery) {
                    satCostLimits.insert(initPoint);
                } else {
                    unsatCostLimits.insert(initPoint);
                }
            }
        }
    } else {
        STORM_LOG_WARN("Quantile formula considers mixtures of upper and lower reward-bounds. Termination is not guaranteed.");
    }

    // Loop until the goal precision is reached.
    STORM_LOG_DEBUG("Computing quantile for dimensions: " << consideredDimensions);
    while (true) {
        // initialize reward unfolding and data that will be needed for each epoch
        MultiDimensionalRewardUnfolding<ValueType, true> rewardUnfolding(model, boundedUntilOp, infinityVariables);
        if (computeQuantile(env, consideredDimensions, *boundedUntilOp, lowerBoundedDimensions, satCostLimits, unsatCostLimits, rewardUnfolding)) {
            std::vector<ValueType> scalingFactors;
            for (auto dim : consideredDimensions) {
                scalingFactors.push_back(rewardUnfolding.getDimension(dim).scalingFactor);
            }
            std::pair<CostLimitClosure, std::vector<ValueType>> result(satCostLimits, scalingFactors);
            cachedSubQueryResults.emplace(cacheKey, result);
            return result;
        }
        STORM_LOG_WARN("Restarting quantile computation after " << swExploration << " seconds due to insufficient precision.");
        ++numPrecisionRefinements;
        increasePrecision(env);
    }
}

bool getNextCandidateCostLimit(CostLimit const& candidateCostLimitSum, CostLimits& current) {
    if (current.size() == 0) {
        return false;
    }
    uint64_t iSum = current.front().get();
    if (iSum == candidateCostLimitSum.get()) {
        return false;
    }
    for (uint64_t i = 1; i < current.size(); ++i) {
        iSum += current[i].get();
        if (iSum == candidateCostLimitSum.get()) {
            ++current[i - 1].get();
            uint64_t newVal = current[i].get() - 1;
            current[i].get() = 0;
            current.back().get() = newVal;
            return true;
        }
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException,
                    "The entries of the current cost limit candidate do not sum up to the current candidate sum.");
    return false;
}

bool translateEpochToCostLimits(EpochManager::Epoch const& epoch, EpochManager::Epoch const& startEpoch, storm::storage::BitVector const& consideredDimensions,
                                storm::storage::BitVector const& lowerBoundedDimensions, EpochManager const& epochManager, CostLimits& epochAsCostLimits) {
    for (uint64_t dim = 0; dim < consideredDimensions.size(); ++dim) {
        if (consideredDimensions.get(dim)) {
            if (lowerBoundedDimensions.get(dim)) {
                if (epochManager.isBottomDimension(epoch, dim)) {
                    epochAsCostLimits.push_back(CostLimit(0));
                } else {
                    epochAsCostLimits.push_back(CostLimit(epochManager.getDimensionOfEpoch(epoch, dim) + 1));
                }
            } else {
                if (epochManager.isBottomDimension(epoch, dim)) {
                    return false;
                } else {
                    epochAsCostLimits.push_back(CostLimit(epochManager.getDimensionOfEpoch(epoch, dim)));
                }
            }
        } else {
            if (epochManager.isBottomDimension(epoch, dim)) {
                if (!epochManager.isBottomDimension(startEpoch, dim)) {
                    return false;
                }
            } else if (epochManager.getDimensionOfEpoch(epoch, dim) != epochManager.getDimensionOfEpoch(startEpoch, dim)) {
                return false;
            }
        }
    }
    return true;
}

template<typename ModelType>
bool QuantileHelper<ModelType>::computeQuantile(Environment& env, storm::storage::BitVector const& consideredDimensions,
                                                storm::logic::ProbabilityOperatorFormula const& boundedUntilOperator,
                                                storm::storage::BitVector const& lowerBoundedDimensions, CostLimitClosure& satCostLimits,
                                                CostLimitClosure& unsatCostLimits, MultiDimensionalRewardUnfolding<ValueType, true>& rewardUnfolding) {
    auto lowerBound = rewardUnfolding.getLowerObjectiveBound();
    auto upperBound = rewardUnfolding.getUpperObjectiveBound();
    std::vector<ValueType> x, b;
    std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> minMaxSolver;  // Needed for MDP
    std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> linEqSolver;         // Needed for DTMC
    if (!model.isNondeterministicModel()) {
        rewardUnfolding.setEquationSystemFormatForEpochModel(storm::solver::GeneralLinearEquationSolverFactory<ValueType>().getEquationProblemFormat(env));
    }

    swExploration.start();
    bool progress = true;
    for (CostLimit candidateCostLimitSum(0); progress; ++candidateCostLimitSum.get()) {
        CostLimits currentCandidate(satCostLimits.dimension(), CostLimit(0));
        if (!currentCandidate.empty()) {
            currentCandidate.back() = candidateCostLimitSum;
        }
        // We can still have progress if one of the closures is empty and the other is not full.
        // This ensures that we do not terminate too early in case that the (un)satCostLimits are initially non-empty.
        progress = (satCostLimits.empty() && !unsatCostLimits.full()) || (unsatCostLimits.empty() && !satCostLimits.full());
        do {
            if (!satCostLimits.contains(currentCandidate) && !unsatCostLimits.contains(currentCandidate)) {
                progress = true;
                // Transform candidate cost limits to an appropriate start epoch
                auto startEpoch = rewardUnfolding.getStartEpoch(true);
                auto costLimitIt = currentCandidate.begin();
                for (auto dim : consideredDimensions) {
                    if (lowerBoundedDimensions.get(dim)) {
                        if (costLimitIt->get() > 0) {
                            rewardUnfolding.getEpochManager().setDimensionOfEpoch(startEpoch, dim, costLimitIt->get() - 1);
                        } else {
                            rewardUnfolding.getEpochManager().setBottomDimension(startEpoch, dim);
                        }
                    } else {
                        rewardUnfolding.getEpochManager().setDimensionOfEpoch(startEpoch, dim, costLimitIt->get());
                    }
                    ++costLimitIt;
                }
                STORM_LOG_DEBUG("Checking start epoch " << rewardUnfolding.getEpochManager().toString(startEpoch) << ".");
                auto epochSequence = rewardUnfolding.getEpochComputationOrder(startEpoch, true);
                for (auto const& epoch : epochSequence) {
                    ++numCheckedEpochs;
                    swEpochAnalysis.start();
                    auto& epochModel = rewardUnfolding.setCurrentEpoch(epoch);
                    if (model.isNondeterministicModel()) {
                        rewardUnfolding.setSolutionForCurrentEpoch(
                            epochModel.analyzeSingleObjective(env, boundedUntilOperator.getOptimalityType(), x, b, minMaxSolver, lowerBound, upperBound));
                    } else {
                        rewardUnfolding.setSolutionForCurrentEpoch(epochModel.analyzeSingleObjective(env, x, b, linEqSolver, lowerBound, upperBound));
                    }
                    swEpochAnalysis.stop();

                    CostLimits epochAsCostLimits;
                    if (translateEpochToCostLimits(epoch, startEpoch, consideredDimensions, lowerBoundedDimensions, rewardUnfolding.getEpochManager(),
                                                   epochAsCostLimits)) {
                        ValueType currValue = rewardUnfolding.getInitialStateResult(epoch);
                        bool propertySatisfied;
                        if (env.solver().isForceSoundness()) {
                            ValueType sumOfEpochDimensions =
                                storm::utility::convertNumber<ValueType>(rewardUnfolding.getEpochManager().getSumOfDimensions(epoch) + 1);
                            auto lowerUpperValue = getLowerUpperBound(env, sumOfEpochDimensions, currValue);
                            propertySatisfied = boundedUntilOperator.getBound().isSatisfied(lowerUpperValue.first);
                            if (propertySatisfied != boundedUntilOperator.getBound().isSatisfied(lowerUpperValue.second)) {
                                // unclear result due to insufficient precision.
                                swExploration.stop();
                                return false;
                            }
                        } else {
                            propertySatisfied = boundedUntilOperator.getBound().isSatisfied(currValue);
                        }
                        if (propertySatisfied) {
                            satCostLimits.insert(epochAsCostLimits);
                        } else {
                            unsatCostLimits.insert(epochAsCostLimits);
                        }
                    }
                }
            }
        } while (getNextCandidateCostLimit(candidateCostLimitSum, currentCandidate));
        if (!progress) {
            progress = !CostLimitClosure::unionFull(satCostLimits, unsatCostLimits);
        }
    }
    swExploration.stop();
    return true;
}

template class QuantileHelper<storm::models::sparse::Mdp<double>>;
template class QuantileHelper<storm::models::sparse::Mdp<storm::RationalNumber>>;
template class QuantileHelper<storm::models::sparse::Dtmc<double>>;
template class QuantileHelper<storm::models::sparse::Dtmc<storm::RationalNumber>>;

}  // namespace rewardbounded
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
