#include "storm-pomdp/transformer/ApplyFiniteSchedulerToPomdp.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/utility/vector.h"

namespace storm {
namespace transformer {
PomdpFscApplicationMode parsePomdpFscApplicationMode(std::string const& mode) {
    if (mode == "standard") {
        return PomdpFscApplicationMode::STANDARD;
    } else if (mode == "simple-linear") {
        return PomdpFscApplicationMode::SIMPLE_LINEAR;
    } else if (mode == "simple-linear-inverse") {
        return PomdpFscApplicationMode::SIMPLE_LINEAR_INVERSE;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Mode " << mode << " not known.");
    }
}

struct RationalFunctionConstructor {
    RationalFunctionConstructor(std::shared_ptr<RawPolynomialCache> const& cache) : cache(cache) {}

    storm::RationalFunction translate(storm::RationalFunctionVariable const& var) {
        storm::Polynomial pol(storm::RawPolynomial(var), cache);
        return storm::RationalFunction(pol);
    }

    std::shared_ptr<RawPolynomialCache> cache;
};

template<typename ValueType>
std::shared_ptr<RawPolynomialCache> getCache(storm::models::sparse::Pomdp<ValueType> const&) {
    return std::make_shared<RawPolynomialCache>();
}

template<>
std::shared_ptr<RawPolynomialCache> getCache(storm::models::sparse::Pomdp<storm::RationalFunction> const& model) {
    for (auto const& entry : model.getTransitionMatrix()) {
        if (!entry.getValue().isConstant()) {
            return entry.getValue().nominatorAsPolynomial().pCache();
        }
    }
    return std::make_shared<RawPolynomialCache>();
}

template<typename ValueType>
std::unordered_map<uint32_t, std::vector<storm::RationalFunction>> ApplyFiniteSchedulerToPomdp<ValueType>::getObservationChoiceWeights(
    PomdpFscApplicationMode applicationMode) const {
    std::unordered_map<uint32_t, std::vector<storm::RationalFunction>> res;
    RationalFunctionConstructor ratFuncConstructor(getCache(pomdp));

    for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
        auto observation = pomdp.getObservation(state);
        auto it = res.find(observation);
        if (it == res.end()) {
            std::vector<storm::RationalFunction> weights;
            storm::RationalFunction collected = storm::utility::one<storm::RationalFunction>();
            storm::RationalFunction lastWeight = storm::utility::one<storm::RationalFunction>();
            for (uint64_t a = 0; a < pomdp.getNumberOfChoices(state) - 1; ++a) {
                std::string varName = "p" + std::to_string(observation) + "_" + std::to_string(a);
                storm::RationalFunction var = ratFuncConstructor.translate(storm::createRFVariable(varName));
                if (applicationMode == PomdpFscApplicationMode::SIMPLE_LINEAR) {
                    weights.push_back(collected * var);
                    collected *= storm::utility::one<storm::RationalFunction>() - var;
                } else if (applicationMode == PomdpFscApplicationMode::SIMPLE_LINEAR_INVERSE) {
                    weights.push_back(collected * (storm::utility::one<storm::RationalFunction>() - var));
                    collected *= var;
                } else if (applicationMode == PomdpFscApplicationMode::STANDARD) {
                    weights.push_back(var);
                }
                lastWeight -= weights.back();
            }
            weights.push_back(lastWeight);
            res.emplace(observation, weights);
        }
        STORM_LOG_ASSERT(it == res.end() || it->second.size() == pomdp.getNumberOfChoices(state),
                         "Number of choices must be equal for every state with same number of actions.");
    }
    return res;
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> ApplyFiniteSchedulerToPomdp<ValueType>::transform(
    PomdpFscApplicationMode applicationMode) const {
    STORM_LOG_THROW(pomdp.isCanonic(), storm::exceptions::IllegalArgumentException, "POMDP needs to be canonic.");
    storm::storage::sparse::ModelComponents<storm::RationalFunction> modelComponents;

    uint64_t nrStates = pomdp.getNumberOfStates();
    std::unordered_map<uint32_t, std::vector<storm::RationalFunction>> observationChoiceWeights = getObservationChoiceWeights(applicationMode);
    storm::storage::SparseMatrixBuilder<storm::RationalFunction> smb(nrStates, nrStates, 0, true);

    for (uint64_t state = 0; state < nrStates; ++state) {
        auto const& weights = observationChoiceWeights.at(pomdp.getObservation(state));
        std::map<uint64_t, storm::RationalFunction> weightedTransitions;
        for (uint64_t action = 0; action < pomdp.getNumberOfChoices(state); ++action) {
            auto ratSum = storm::utility::zero<storm::RationalFunction>();
            uint64_t nrEntries = pomdp.getTransitionMatrix().getRow(state, action).getNumberOfEntries();
            uint64_t currEntry = 1;
            for (auto const& entry : pomdp.getTransitionMatrix().getRow(state, action)) {
                auto it = weightedTransitions.find(entry.getColumn());
                auto entryVal = storm::utility::convertNumber<storm::RationalFunction>(entry.getValue());
                ratSum += entryVal;
                if (currEntry == nrEntries && storm::utility::one<storm::RationalFunction>() - ratSum != storm::utility::zero<storm::RationalFunction>()) {
                    // In case there are numeric problems with the conversion, we simply add the lost mass to the last value
                    entryVal += (storm::utility::one<storm::RationalFunction>() - ratSum);
                }
                if (it == weightedTransitions.end()) {
                    weightedTransitions[entry.getColumn()] = entryVal * weights[action];
                } else {
                    it->second += entryVal * weights[action];
                }
                ++currEntry;
            }
        }
        for (auto const& entry : weightedTransitions) {
            smb.addNextValue(state, entry.first, entry.second);
        }
    }
    modelComponents.transitionMatrix = smb.build();

    for (auto const& pomdpRewardModel : pomdp.getRewardModels()) {
        std::vector<storm::RationalFunction> stateRewards;

        if (pomdpRewardModel.second.hasStateRewards()) {
            stateRewards = storm::utility::vector::convertNumericVector<storm::RationalFunction>(pomdpRewardModel.second.getStateRewardVector());
        } else {
            stateRewards.resize(nrStates, storm::utility::zero<storm::RationalFunction>());
        }
        if (pomdpRewardModel.second.hasStateActionRewards()) {
            std::vector<ValueType> pomdpActionRewards = pomdpRewardModel.second.getStateActionRewardVector();
            for (uint64_t state = 0; state < nrStates; ++state) {
                auto& stateReward = stateRewards[state];
                auto const& weights = observationChoiceWeights.at(pomdp.getObservation(state));
                uint64_t offset = pomdp.getTransitionMatrix().getRowGroupIndices()[state];
                for (uint64_t action = 0; action < pomdp.getNumberOfChoices(state); ++action) {
                    if (!storm::utility::isZero(pomdpActionRewards[offset + action])) {
                        stateReward += storm::utility::convertNumber<storm::RationalFunction>(pomdpActionRewards[offset + action]) * weights[action];
                    }
                }
            }
        }
        storm::models::sparse::StandardRewardModel<storm::RationalFunction> rewardModel(std::move(stateRewards));
        modelComponents.rewardModels.emplace(pomdpRewardModel.first, std::move(rewardModel));
    }

    modelComponents.stateLabeling = pomdp.getStateLabeling();
    modelComponents.stateValuations = pomdp.getOptionalStateValuations();

    return std::make_shared<storm::models::sparse::Dtmc<storm::RationalFunction>>(modelComponents);
}

template class ApplyFiniteSchedulerToPomdp<storm::RationalNumber>;

template class ApplyFiniteSchedulerToPomdp<double>;
template class ApplyFiniteSchedulerToPomdp<storm::RationalFunction>;
}  // namespace transformer
}  // namespace storm