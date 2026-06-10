#include "storm/transformer/MakePOMDPCanonic.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/AmbiguousModelException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/sparse/ModelComponents.h"

namespace storm {
namespace transformer {

namespace detail {
struct ActionIdentifier {
    uint64_t choiceLabelId;
    uint64_t choiceOriginId;

    bool compatibleWith(ActionIdentifier const& other) const {
        if (choiceLabelId != other.choiceLabelId) {
            return false;  // different labels.
        }
        if (choiceLabelId > 0) {
            // Notice that we assume that we already have ensured that names
            // are not used more than once.
            return true;  // actions have a name, name coincides.
        } else {
            // action is unnamed.
            // We only call this method  (at least we only should call this method)
            // if there are multiple actions. Then two tau actions are only compatible
            // if they are described by the same choice origin.
            return choiceOriginId == other.choiceOriginId;
        }
    }

    friend bool operator<(ActionIdentifier const& lhs, ActionIdentifier const& rhs);
};

template<typename iterator1, typename iterator2>
bool compatibleWith(iterator1 start1, iterator1 end1, iterator2 start2, iterator2 end2) {
    iterator1 it1 = start1;
    iterator2 it2 = start2;
    while (it1 != end1 && it2 != end2) {
        if (!it1->compatibleWith(it2->first)) {
            return false;
        }
        ++it1;
        ++it2;
    }
    return it1 == end1 && it2 == end2;
}

bool operator<(ActionIdentifier const& lhs, ActionIdentifier const& rhs) {
    if (lhs.choiceLabelId == rhs.choiceLabelId) {
        return lhs.choiceOriginId < rhs.choiceOriginId;
    }
    return lhs.choiceLabelId < rhs.choiceLabelId;
}

class ChoiceLabelIdStorage {
   public:
    uint64_t registerLabel(std::string const& label) {
        auto it = std::find(storage.begin(), storage.end(), label);
        if (it == storage.end()) {
            storage.push_back(label);
            return storage.size() - 1;
        } else {
            return it - storage.begin();
        }
    }

    std::string const& getLabel(uint64_t id) const {
        STORM_LOG_ASSERT(id < storage.size(), "Id must be in storage");
        return storage[id];
    }

    friend std::ostream& operator<<(std::ostream& os, ChoiceLabelIdStorage const& labelStorage);

   private:
    std::vector<std::string> storage = {""};
};

std::ostream& operator<<(std::ostream& os, ChoiceLabelIdStorage const& labelStorage) {
    os << "LabelStorage: {";
    uint64_t i = 0;
    for (auto const& entry : labelStorage.storage) {
        os << i << " -> " << entry << " ;";
        ++i;
    }
    os << "}";
    return os;
}

void actionIdentifiersToStream(std::ostream& stream, std::vector<ActionIdentifier> const& actionIdentifiers, ChoiceLabelIdStorage const& labelStorage) {
    stream << "actions: {";
    bool first = true;
    for (auto ai : actionIdentifiers) {
        if (first) {
            first = false;
        } else {
            stream << " ";
        }
        stream << "[" << ai.choiceLabelId << " (" << labelStorage.getLabel(ai.choiceLabelId) << ")";
        stream << ", " << ai.choiceOriginId << "]";
    }
    stream << "}";
}

template<typename IrrelevantType>
void actionIdentifiersToStream(std::ostream& stream, std::map<ActionIdentifier, IrrelevantType> const& actionIdentifiers,
                               ChoiceLabelIdStorage const& labelStorage) {
    stream << "actions: {";
    bool first = true;
    for (auto ai : actionIdentifiers) {
        if (first) {
            first = false;
        } else {
            stream << " ";
        }
        stream << "[" << ai.first.choiceLabelId << " (" << labelStorage.getLabel(ai.first.choiceLabelId) << ")";
        stream << ", " << ai.first.choiceOriginId << "]";
    }
    stream << "}";
}

}  // namespace detail

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> MakePOMDPCanonic<ValueType>::transform() const {
    STORM_LOG_THROW(pomdp.hasChoiceLabeling(), storm::exceptions::InvalidArgumentException,
                    "Model must have been built with choice labels (--buildchoicelab for command line users)");
    std::vector<uint64_t> permutation = computeCanonicalPermutation();
    return applyPermutationOnPomdp(permutation);
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> MakePOMDPCanonic<ValueType>::applyPermutationOnPomdp(std::vector<uint64_t> permutation) const {
    auto rewardModels = pomdp.getRewardModels();
    std::unordered_map<std::string, storm::models::sparse::StandardRewardModel<ValueType>> newRewardModels;
    for (auto& rewardModelNameAndModel : rewardModels) {
        newRewardModels.emplace(rewardModelNameAndModel.first, rewardModelNameAndModel.second.permuteActions(permutation));
    }
    storm::storage::sparse::ModelComponents<ValueType> modelcomponents(pomdp.getTransitionMatrix().permuteRows(permutation), pomdp.getStateLabeling(),
                                                                       newRewardModels, false, boost::none);
    modelcomponents.observabilityClasses = pomdp.getObservations();
    modelcomponents.stateValuations = pomdp.getOptionalStateValuations();
    modelcomponents.choiceLabeling = pomdp.getChoiceLabeling();
    modelcomponents.choiceLabeling->permuteItems(permutation);
    modelcomponents.observationValuations = pomdp.getOptionalObservationValuations();
    return std::make_shared<storm::models::sparse::Pomdp<ValueType>>(modelcomponents, true);
}

template<typename ValueType>
std::string MakePOMDPCanonic<ValueType>::getStateInformation(uint64_t state) const {
    if (pomdp.hasStateValuations()) {
        return std::to_string(state) + " " + pomdp.getStateValuations().getStateInfo(state);
    } else {
        return std::to_string(state);
    }
}

template<typename ValueType>
std::string MakePOMDPCanonic<ValueType>::getObservationInformation(uint32_t obs) const {
    if (pomdp.hasObservationValuations()) {
        return std::to_string(obs) + " " + pomdp.getObservationValuations().getStateInfo(obs);
    } else {
        return std::to_string(obs);
    }
}

template<typename ValueType>
std::vector<uint64_t> MakePOMDPCanonic<ValueType>::computeCanonicalPermutation() const {
    std::map<uint32_t, std::vector<detail::ActionIdentifier>> observationActionIdentifiers;
    std::map<uint32_t, uint64_t> actionIdentifierDefinition;
    auto const& choiceLabeling = pomdp.getChoiceLabeling();
    detail::ChoiceLabelIdStorage labelStorage;

    std::vector<uint64_t> permutation;
    uint64_t nrObservations = pomdp.getNrObservations();
    storm::storage::BitVector oneActionObservations(nrObservations);
    storm::storage::BitVector moreActionObservations(nrObservations);
    uint64_t freshChoiceOriginId = 0;  // Only used if no choice origins are available.

    for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
        uint64_t rowIndexFrom = pomdp.getTransitionMatrix().getRowGroupIndices()[state];
        uint64_t rowIndexTo = pomdp.getTransitionMatrix().getRowGroupIndices()[state + 1];

        uint64_t observation = pomdp.getObservation(state);
        if (rowIndexFrom + 1 == rowIndexTo) {
            permutation.push_back(rowIndexFrom);
            if (moreActionObservations.get(observation)) {
                // We have seen this observation previously with multiple actions. Error!
                // TODO provide more diagnostic information
                std::string actionval = "";
                if (pomdp.hasChoiceLabeling()) {
                    auto labelsOfChoice = pomdp.getChoiceLabeling().getLabelsOfChoice(rowIndexFrom);
                    if (labelsOfChoice.empty()) {
                        actionval = "[[no-label]]";
                    } else {
                        actionval = *pomdp.getChoiceLabeling().getLabelsOfChoice(rowIndexFrom).begin();
                    }
                }
                STORM_LOG_THROW(false, storm::exceptions::AmbiguousModelException,
                                "Observation " << getObservationInformation(observation) << " sometimes provides multiple actions, but in state "
                                               << getStateInformation(state) << " provides only one action " << actionval << ".");
            }
            oneActionObservations.set(observation);

            // One action is ALWAYS fine.
            continue;
        } else {
            if (oneActionObservations.get(observation)) {
                // We have seen this observation previously with one action. Error!
                //                        std::string actionval= "";
                //                        if (pomdp.hasChoiceLabeling()) {
                //                            actionval = *pomdp.getChoiceLabeling().getLabelsOfChoice(rowIndexFrom).begin();
                //                        }
                STORM_LOG_THROW(false, storm::exceptions::AmbiguousModelException,
                                "Observation " << getObservationInformation(observation) << " sometimes provides one action, but in state "
                                               << getStateInformation(state) << " provides " << rowIndexTo - rowIndexFrom << " actions.");
            }
            moreActionObservations.set(observation);
        }

        std::map<detail::ActionIdentifier, uint64_t> actionIdentifiers;
        std::set<uint64_t> actionLabels;
        for (uint64_t actionIndex = rowIndexFrom; actionIndex < rowIndexTo; ++actionIndex) {
            // While this is in full generality a set of labels,
            // for models modelled with prism, we actually know that these are singleton sets.
            std::set<std::string> labels = choiceLabeling.getLabelsOfChoice(actionIndex);
            STORM_LOG_ASSERT(labels.size() <= 1, "We expect choice labels to be single sets");
            // Generate action identifier
            uint64_t labelId = -1;
            if (labels.size() == 1) {
                labelId = labelStorage.registerLabel(*labels.begin());
                STORM_LOG_THROW(actionLabels.count(labelId) == 0, storm::exceptions::AmbiguousModelException,
                                "Multiple actions with label '" << *labels.begin() << "' exist in state id " << state << ".");
                actionLabels.emplace(labelId);
            } else {
                labelId = labelStorage.registerLabel("");
            }

            detail::ActionIdentifier ai;
            ai.choiceLabelId = labelId;
            if (pomdp.hasChoiceOrigins()) {
                ai.choiceOriginId = pomdp.getChoiceOrigins()->getIdentifier(actionIndex);
            } else {
                ai.choiceOriginId = freshChoiceOriginId++;
            }
            STORM_LOG_ASSERT(actionIdentifiers.count(ai) == 0, "Action with this identifier already exists for this state");
            actionIdentifiers.emplace(ai, actionIndex);
        }
        STORM_LOG_ASSERT(actionIdentifiers.size() == rowIndexTo - rowIndexFrom, "Number of action identifiers should match number of actions");

        if (observationActionIdentifiers.count(observation) == 0) {
            // First state with this observation
            // store the corresponding vector.
            std::vector<detail::ActionIdentifier> ais;
            for (auto const& als : actionIdentifiers) {
                ais.push_back(als.first);
            }

            observationActionIdentifiers.emplace(observation, ais);
            actionIdentifierDefinition.emplace(observation, state);
        } else {
            auto referenceStart = observationActionIdentifiers[observation].begin();
            auto referenceEnd = observationActionIdentifiers[observation].end();
            STORM_LOG_ASSERT(observationActionIdentifiers[observation].size() == pomdp.getNumberOfChoices(actionIdentifierDefinition[observation]),
                             "Number of actions recorded for state does not coincide with number of actions.");
            if (observationActionIdentifiers[observation].size() != actionIdentifiers.size()) {
                STORM_LOG_THROW(false, storm::exceptions::AmbiguousModelException,
                                "Number of actions in state '" << getStateInformation(state) << "' (nr actions:" << actionIdentifiers.size() << ") and state '"
                                                               << getStateInformation(actionIdentifierDefinition[observation])
                                                               << "' (actions: " << observationActionIdentifiers[observation].size()
                                                               << " ), both having observation " << getObservationInformation(observation) << " do not match.");
            }
            if (!detail::compatibleWith(referenceStart, referenceEnd, actionIdentifiers.begin(), actionIdentifiers.end())) {
                std::cout << "Observation " << getObservationInformation(observation) << ": \n";
                detail::actionIdentifiersToStream(std::cout, observationActionIdentifiers[observation], labelStorage);
                std::cout << " according to state " << actionIdentifierDefinition[observation] << ".\n";
                detail::actionIdentifiersToStream(std::cout, actionIdentifiers, labelStorage);
                std::cout << " according to state " << state << ".\n";
                std::cout << "(Actions are given as [id (label), originId])\n";

                for (auto const& ai : actionIdentifiers) {
                    if (labelStorage.getLabel(ai.first.choiceLabelId) == "") {
                        for (auto const& ai2 : observationActionIdentifiers[observation]) {
                            STORM_LOG_WARN_COND(ai2.choiceLabelId != ai.first.choiceLabelId,
                                                "Actions with empty label are only considered the same action if they originate from the same command.");
                        }
                    }
                }

                STORM_LOG_THROW(false, storm::exceptions::AmbiguousModelException,
                                "Actions identifiers do not align between states \n\t"
                                    << getStateInformation(state) << "\nand\n\t" << getStateInformation(actionIdentifierDefinition[observation])
                                    << "\nboth having observation " << getObservationInformation(observation) << ". See output above for more information.");
            }
        }

        for (auto const& al : actionIdentifiers) {
            permutation.push_back(al.second);
        }
    }
    return permutation;
}

template class MakePOMDPCanonic<double>;
template class MakePOMDPCanonic<storm::RationalNumber>;
template class MakePOMDPCanonic<storm::Interval>;
template class MakePOMDPCanonic<storm::RationalInterval>;
template class MakePOMDPCanonic<storm::RationalFunction>;
}  // namespace transformer
}  // namespace storm
