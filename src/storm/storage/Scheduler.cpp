#include "storm/storage/Scheduler.h"

#include <boost/algorithm/string/join.hpp>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace storage {

template<typename ValueType>
Scheduler<ValueType>::Scheduler(uint_fast64_t numberOfModelStates, boost::optional<storm::storage::MemoryStructure> const& memoryStructure)
    : memoryStructure(memoryStructure) {
    uint_fast64_t numOfMemoryStates = memoryStructure ? memoryStructure->getNumberOfStates() : 1;
    schedulerChoices = std::vector<std::vector<SchedulerChoice<ValueType>>>(numOfMemoryStates, std::vector<SchedulerChoice<ValueType>>(numberOfModelStates));
    dontCareStates = std::vector<storm::storage::BitVector>(numOfMemoryStates, storm::storage::BitVector(numberOfModelStates, false));
    numOfUndefinedChoices = numOfMemoryStates * numberOfModelStates;
    numOfDeterministicChoices = 0;
    numOfDontCareStates = 0;
}

template<typename ValueType>
Scheduler<ValueType>::Scheduler(uint_fast64_t numberOfModelStates, boost::optional<storm::storage::MemoryStructure>&& memoryStructure)
    : memoryStructure(std::move(memoryStructure)) {
    uint_fast64_t numOfMemoryStates = this->memoryStructure ? this->memoryStructure->getNumberOfStates() : 1;
    schedulerChoices = std::vector<std::vector<SchedulerChoice<ValueType>>>(numOfMemoryStates, std::vector<SchedulerChoice<ValueType>>(numberOfModelStates));
    dontCareStates = std::vector<storm::storage::BitVector>(numOfMemoryStates, storm::storage::BitVector(numberOfModelStates, false));
    numOfUndefinedChoices = numOfMemoryStates * numberOfModelStates;
    numOfDeterministicChoices = 0;
    numOfDontCareStates = 0;
}

template<typename ValueType>
void Scheduler<ValueType>::setChoice(SchedulerChoice<ValueType> const& choice, uint_fast64_t modelState, uint_fast64_t memoryState) {
    STORM_LOG_ASSERT(memoryState < getNumberOfMemoryStates(), "Illegal memory state index");
    STORM_LOG_ASSERT(modelState < schedulerChoices[memoryState].size(), "Illegal model state index");

    auto& schedulerChoice = schedulerChoices[memoryState][modelState];

    if (schedulerChoice.isDefined()) {
        if (!choice.isDefined()) {
            ++numOfUndefinedChoices;
        }
    } else {
        if (choice.isDefined()) {
            assert(numOfUndefinedChoices > 0);
            --numOfUndefinedChoices;
        }
    }
    if (schedulerChoice.isDeterministic()) {
        if (!choice.isDeterministic()) {
            assert(numOfDeterministicChoices > 0);
            --numOfDeterministicChoices;
        }
    } else {
        if (choice.isDeterministic()) {
            ++numOfDeterministicChoices;
        }
    }

    schedulerChoice = choice;
}

template<typename ValueType>
bool Scheduler<ValueType>::isChoiceSelected(BitVector const& selectedStates, uint64_t memoryState) const {
    for (auto selectedState : selectedStates) {
        auto& schedulerChoice = schedulerChoices[memoryState][selectedState];
        if (!schedulerChoice.isDefined()) {
            return false;
        }
    }
    return true;
}

template<typename ValueType>
bool Scheduler<ValueType>::isChoiceSelected(uint64_t modelState, uint64_t memoryState) const {
    STORM_LOG_ASSERT(memoryState < getNumberOfMemoryStates(), "Illegal memory state index");
    STORM_LOG_ASSERT(modelState < schedulerChoices[memoryState].size(), "Illegal model state index");
    return schedulerChoices[memoryState][modelState].isDefined();
}

template<typename ValueType>
void Scheduler<ValueType>::clearChoice(uint_fast64_t modelState, uint_fast64_t memoryState) {
    STORM_LOG_ASSERT(memoryState < getNumberOfMemoryStates(), "Illegal memory state index");
    STORM_LOG_ASSERT(modelState < schedulerChoices[memoryState].size(), "Illegal model state index");
    setChoice(SchedulerChoice<ValueType>(), modelState, memoryState);
}

template<typename ValueType>
SchedulerChoice<ValueType> const& Scheduler<ValueType>::getChoice(uint_fast64_t modelState, uint_fast64_t memoryState) const {
    STORM_LOG_ASSERT(memoryState < getNumberOfMemoryStates(), "Illegal memory state index");
    STORM_LOG_ASSERT(modelState < schedulerChoices[memoryState].size(), "Illegal model state index");
    return schedulerChoices[memoryState][modelState];
}

template<typename ValueType>
void Scheduler<ValueType>::setDontCare(uint_fast64_t modelState, uint_fast64_t memoryState, bool setArbitraryChoice) {
    STORM_LOG_ASSERT(memoryState < getNumberOfMemoryStates(), "Illegal memory state index");
    STORM_LOG_ASSERT(modelState < schedulerChoices[memoryState].size(), "Illegal model state index");

    if (!dontCareStates[memoryState].get(modelState)) {
        auto& schedulerChoice = schedulerChoices[memoryState][modelState];
        if (!schedulerChoice.isDefined() && setArbitraryChoice) {
            // Set an arbitrary choice
            this->setChoice(0, modelState, memoryState);
        }
        dontCareStates[memoryState].set(modelState, true);
        ++numOfDontCareStates;
    }
}

template<typename ValueType>
void Scheduler<ValueType>::unSetDontCare(uint_fast64_t modelState, uint_fast64_t memoryState) {
    STORM_LOG_ASSERT(memoryState < getNumberOfMemoryStates(), "Illegal memory state index");
    STORM_LOG_ASSERT(modelState < schedulerChoices[memoryState].size(), "Illegal model state index");

    if (dontCareStates[memoryState].get(modelState)) {
        dontCareStates[memoryState].set(modelState, false);
        --numOfDontCareStates;
    }
}

template<typename ValueType>
bool Scheduler<ValueType>::isDontCare(uint_fast64_t modelState, uint64_t memoryState) const {
    return dontCareStates[memoryState].get(modelState);
}

template<typename ValueType>
storm::storage::BitVector Scheduler<ValueType>::computeActionSupport(std::vector<uint_fast64_t> const& nondeterministicChoiceIndices) const {
    auto nrActions = nondeterministicChoiceIndices.back();
    storm::storage::BitVector result(nrActions);

    for (auto const& choicesPerMemoryNode : schedulerChoices) {
        STORM_LOG_ASSERT(nondeterministicChoiceIndices.size() - 2 < choicesPerMemoryNode.size(), "Illegal model state index");
        for (uint64_t stateId = 0; stateId < nondeterministicChoiceIndices.size() - 1; ++stateId) {
            for (auto const& schedChoice : choicesPerMemoryNode[stateId].getChoiceAsDistribution()) {
                STORM_LOG_ASSERT(schedChoice.first < nondeterministicChoiceIndices[stateId + 1] - nondeterministicChoiceIndices[stateId],
                                 "Scheduler chooses action indexed " << schedChoice.first << " in state id " << stateId << " but state contains only "
                                                                     << nondeterministicChoiceIndices[stateId + 1] - nondeterministicChoiceIndices[stateId]
                                                                     << " choices .");
                result.set(nondeterministicChoiceIndices[stateId] + schedChoice.first);
            }
        }
    }
    return result;
}

template<typename ValueType>
bool Scheduler<ValueType>::isPartialScheduler() const {
    return numOfUndefinedChoices != 0;
}

template<typename ValueType>
bool Scheduler<ValueType>::isDeterministicScheduler() const {
    return numOfDeterministicChoices == (schedulerChoices.size() * schedulerChoices.begin()->size()) - numOfUndefinedChoices;
}

template<typename ValueType>
bool Scheduler<ValueType>::isMemorylessScheduler() const {
    return getNumberOfMemoryStates() == 1;
}

template<typename ValueType>
uint_fast64_t Scheduler<ValueType>::getNumberOfMemoryStates() const {
    return memoryStructure ? memoryStructure->getNumberOfStates() : 1;
}

template<typename ValueType>
uint_fast64_t Scheduler<ValueType>::getNumberOfModelStates() const {
    return schedulerChoices.empty() ? 0 : schedulerChoices.front().size();
}

template<typename ValueType>
boost::optional<storm::storage::MemoryStructure> const& Scheduler<ValueType>::getMemoryStructure() const {
    return memoryStructure;
}

template<typename ValueType>
Scheduler<ValueType> Scheduler<ValueType>::getMemorylessSchedulerForMemoryState(uint64_t memoryState) const {
    STORM_LOG_ASSERT(memoryState < getNumberOfMemoryStates(), "Illegal memory state index");

    Scheduler<ValueType> memorylessScheduler(getNumberOfModelStates());
    for (uint64_t modelState = 0; modelState < getNumberOfModelStates(); ++modelState) {
        if (schedulerChoices[memoryState][modelState].isDefined()) {
            memorylessScheduler.setChoice(schedulerChoices[memoryState][modelState], modelState);
        }
    }
    return memorylessScheduler;
}

template<typename ValueType>
void Scheduler<ValueType>::printToStream(std::ostream& out, std::shared_ptr<storm::models::sparse::Model<ValueType>> model, bool skipUniqueChoices,
                                         bool skipDontCareStates) const {
    STORM_LOG_THROW(model == nullptr || model->getNumberOfStates() == schedulerChoices.front().size(), storm::exceptions::InvalidOperationException,
                    "The given model is not compatible with this scheduler.");

    bool const stateValuationsGiven = model != nullptr && model->hasStateValuations();
    bool const choiceLabelsGiven = model != nullptr && model->hasChoiceLabeling();
    bool const choiceOriginsGiven = model != nullptr && model->hasChoiceOrigins();
    uint_fast64_t widthOfStates = std::to_string(schedulerChoices.front().size()).length();
    if (stateValuationsGiven) {
        widthOfStates += model->getStateValuations().getStateInfo(schedulerChoices.front().size() - 1).length() + 5;
    }
    widthOfStates = std::max(widthOfStates, (uint_fast64_t)12);
    uint_fast64_t numOfSkippedStatesWithUniqueChoice = 0;

    out << "___________________________________________________________________\n";
    out << (isPartialScheduler() ? "Partially" : "Fully") << " defined ";
    out << (isMemorylessScheduler() ? "memoryless " : "");
    out << (isDeterministicScheduler() ? "deterministic" : "randomized") << " scheduler";
    if (!isMemorylessScheduler()) {
        out << " with " << getNumberOfMemoryStates() << " memory states";
    }
    out << ":\n";
    STORM_LOG_WARN_COND(!(skipUniqueChoices && model == nullptr), "Can not skip unique choices if the model is not given.");
    out << std::setw(widthOfStates) << "model state:" << "    " << (isMemorylessScheduler() ? "" : " memory:     ") << "choice(s)"
        << (isMemorylessScheduler() ? "" : "     memory updates:     ") << '\n';
    for (uint_fast64_t state = 0; state < schedulerChoices.front().size(); ++state) {
        // Check whether the state is skipped
        if (skipUniqueChoices && model != nullptr && model->getTransitionMatrix().getRowGroupSize(state) == 1) {
            ++numOfSkippedStatesWithUniqueChoice;
            continue;
        }

        // Print the state info
        if (stateValuationsGiven) {
            out << std::setw(widthOfStates) << (std::to_string(state) + ": " + model->getStateValuations().getStateInfo(state));
        } else {
            out << std::setw(widthOfStates) << state;
        }
        out << "    ";

        bool firstMemoryState = true;
        for (uint_fast64_t memoryState = 0; memoryState < getNumberOfMemoryStates(); ++memoryState) {
            // Ignore dontCare states
            if (skipDontCareStates && isDontCare(state, memoryState)) {
                continue;
            }

            // Indent if this is not the first memory state
            if (firstMemoryState) {
                firstMemoryState = false;
            } else {
                out << std::setw(widthOfStates) << "";
                out << "    ";
            }
            // Print the memory state info
            if (!isMemorylessScheduler()) {
                out << "m=" << memoryState << std::setw(8) << "";
            }

            // Print choice info
            SchedulerChoice<ValueType> const& choice = schedulerChoices[memoryState][state];
            if (choice.isDefined()) {
                if (choice.isDeterministic()) {
                    if (choiceOriginsGiven) {
                        out << model->getChoiceOrigins()->getChoiceInfo(model->getTransitionMatrix().getRowGroupIndices()[state] +
                                                                        choice.getDeterministicChoice());
                    } else {
                        out << choice.getDeterministicChoice();
                    }
                    if (choiceLabelsGiven) {
                        auto choiceLabels = model->getChoiceLabeling().getLabelsOfChoice(model->getTransitionMatrix().getRowGroupIndices()[state] +
                                                                                         choice.getDeterministicChoice());
                        out << " {" << boost::join(choiceLabels, ", ") << "}";
                    }
                } else {
                    bool firstChoice = true;
                    for (auto const& choiceProbPair : choice.getChoiceAsDistribution()) {
                        if (firstChoice) {
                            firstChoice = false;
                        } else {
                            out << "   +    ";
                        }
                        out << choiceProbPair.second << ": (";
                        if (choiceOriginsGiven) {
                            out << model->getChoiceOrigins()->getChoiceInfo(model->getTransitionMatrix().getRowGroupIndices()[state] + choiceProbPair.first);
                        } else {
                            out << choiceProbPair.first;
                        }
                        if (choiceLabelsGiven) {
                            auto choiceLabels = model->getChoiceLabeling().getLabelsOfChoice(model->getTransitionMatrix().getRowGroupIndices()[state] +
                                                                                             choice.getDeterministicChoice());
                            out << " {" << boost::join(choiceLabels, ", ") << "}";
                        }
                        out << ")";
                    }
                }
            } else {
                out << "undefined.";
            }

            // Print memory updates
            if (!isMemorylessScheduler()) {
                STORM_LOG_THROW(model != nullptr, storm::exceptions::InvalidOperationException,
                                "Schedulers with memory can only be printed when the model is passed.");
                out << std::setw(widthOfStates) << "";
                // The memory updates do not depend on the actual choice, they only depend on the current model- and memory state as well as the successor model
                // state.
                for (auto const& choiceProbPair : choice.getChoiceAsDistribution()) {
                    uint64_t row = model->getTransitionMatrix().getRowGroupIndices()[state] + choiceProbPair.first;
                    bool firstUpdate = true;
                    for (auto entryIt = model->getTransitionMatrix().getRow(row).begin(); entryIt < model->getTransitionMatrix().getRow(row).end(); ++entryIt) {
                        if (firstUpdate) {
                            firstUpdate = false;
                        } else {
                            out << ", ";
                        }
                        out << "model state' = " << entryIt->getColumn() << ": -> "
                            << "(m' = " << this->memoryStructure->getSuccessorMemoryState(memoryState, entryIt - model->getTransitionMatrix().begin()) << ")";
                        // out << "model state' = " << entryIt->getColumn() << ": (transition = " << entryIt - model->getTransitionMatrix().begin() << ") -> "
                        // << "(m' = "<<this->memoryStructure->getSuccessorMemoryState(memoryState, entryIt - model->getTransitionMatrix().begin()) <<")";
                    }
                }
            }

            out << '\n';
        }
    }
    if (numOfSkippedStatesWithUniqueChoice > 0) {
        out << "Skipped " << numOfSkippedStatesWithUniqueChoice << " deterministic states with unique choice.\n";
    }
    out << "___________________________________________________________________\n";
}

template<typename ValueType>
void Scheduler<ValueType>::printJsonToStream(std::ostream& out, std::shared_ptr<storm::models::sparse::Model<ValueType>> model, bool skipUniqueChoices,
                                             bool skipDontCareStates) const {
    STORM_LOG_THROW(model == nullptr || model->getNumberOfStates() == schedulerChoices.front().size(), storm::exceptions::InvalidOperationException,
                    "The given model is not compatible with this scheduler.");
    STORM_LOG_WARN_COND(!(skipUniqueChoices && model == nullptr), "Can not skip unique choices if the model is not given.");
    storm::json<storm::RationalNumber> output;
    for (uint64_t state = 0; state < schedulerChoices.front().size(); ++state) {
        // Check whether the state is skipped
        if (skipUniqueChoices && model != nullptr && model->getTransitionMatrix().getRowGroupSize(state) == 1) {
            continue;
        }

        for (uint_fast64_t memoryState = 0; memoryState < getNumberOfMemoryStates(); ++memoryState) {
            // Ignore dontCare states
            if (skipDontCareStates && isDontCare(state, memoryState)) {
                continue;
            }

            storm::json<storm::RationalNumber> stateChoicesJson;
            if (model && model->hasStateValuations()) {
                stateChoicesJson["s"] = model->getStateValuations().template toJson<storm::RationalNumber>(state);
            } else {
                stateChoicesJson["s"] = state;
            }

            if (!isMemorylessScheduler()) {
                stateChoicesJson["m"] = memoryState;
            }

            auto const& choice = schedulerChoices[memoryState][state];
            storm::json<storm::RationalNumber> choicesJson;
            if (choice.isDefined()) {
                for (auto const& choiceProbPair : choice.getChoiceAsDistribution()) {
                    uint64_t globalChoiceIndex = model->getTransitionMatrix().getRowGroupIndices()[state] + choiceProbPair.first;
                    storm::json<storm::RationalNumber> choiceJson;
                    if (model && model->hasChoiceOrigins() &&
                        model->getChoiceOrigins()->getIdentifier(globalChoiceIndex) != model->getChoiceOrigins()->getIdentifierForChoicesWithNoOrigin()) {
                        choiceJson["origin"] = model->getChoiceOrigins()->getChoiceAsJson(globalChoiceIndex);
                    }
                    if (model && model->hasChoiceLabeling()) {
                        auto choiceLabels = model->getChoiceLabeling().getLabelsOfChoice(globalChoiceIndex);
                        choiceJson["labels"] = std::vector<std::string>(choiceLabels.begin(), choiceLabels.end());
                    }
                    choiceJson["index"] = globalChoiceIndex;
                    choiceJson["prob"] = storm::utility::convertNumber<storm::RationalNumber>(choiceProbPair.second);

                    // Memory updates
                    if (!isMemorylessScheduler()) {
                        STORM_LOG_THROW(model != nullptr, storm::exceptions::InvalidOperationException,
                                        "Schedulers with memory can only be printed when the model is passed.");
                        choiceJson["memory-updates"] = std::vector<storm::json<storm::RationalNumber>>();
                        uint64_t row = model->getTransitionMatrix().getRowGroupIndices()[state] + choiceProbPair.first;
                        for (auto entryIt = model->getTransitionMatrix().getRow(row).begin(); entryIt < model->getTransitionMatrix().getRow(row).end();
                             ++entryIt) {
                            storm::json<storm::RationalNumber> updateJson;
                            // next model state
                            if (model && model->hasStateValuations()) {
                                updateJson["s'"] = model->getStateValuations().template toJson<storm::RationalNumber>(entryIt->getColumn());
                            } else {
                                updateJson["s'"] = entryIt->getColumn();
                            }
                            // next memory state
                            updateJson["m'"] = this->memoryStructure->getSuccessorMemoryState(memoryState, entryIt - model->getTransitionMatrix().begin());
                            choiceJson["memory-updates"].push_back(std::move(updateJson));
                        }
                    }

                    choicesJson.push_back(std::move(choiceJson));
                }
            } else {
                choicesJson = "undefined";
            }
            stateChoicesJson["c"] = std::move(choicesJson);
            output.push_back(std::move(stateChoicesJson));
        }
    }
    out << storm::dumpJson(output);
}

template class Scheduler<double>;
template class Scheduler<storm::RationalNumber>;
template class Scheduler<storm::RationalFunction>;
template class Scheduler<storm::Interval>;
template class Scheduler<storm::RationalInterval>;

}  // namespace storage
}  // namespace storm
