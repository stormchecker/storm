#include "CompositionInformationVisitor.h"

#include "storm/exceptions/WrongFormatException.h"
#include "storm/storage/jani/Compositions.h"
#include "storm/storage/jani/Model.h"
#include "storm/utility/macros.h"

namespace storm {
namespace jani {

CompositionInformation::CompositionInformation() : nonStandardParallelComposition(false), nestedParallelComposition(false), parallelComposition(false) {
    // Intentionally left empty.
}

void CompositionInformation::increaseAutomatonMultiplicity(std::string const& automatonName, uint64_t count) {
    automatonNameToMultiplicity[automatonName] += count;
}

std::map<std::string, uint64_t> const& CompositionInformation::getAutomatonToMultiplicityMap() const {
    return automatonNameToMultiplicity;
}

void CompositionInformation::addMultiplicityMap(std::map<std::string, uint64_t> const& multiplicityMap) {
    automatonNameToMultiplicity = joinMultiplicityMaps(automatonNameToMultiplicity, multiplicityMap);
}

std::map<std::string, uint64_t> CompositionInformation::joinMultiplicityMaps(std::map<std::string, uint64_t> const& first,
                                                                             std::map<std::string, uint64_t> const& second) {
    std::map<std::string, uint64_t> result = first;
    for (auto const& element : second) {
        result[element.first] += element.second;
    }
    return result;
}

void CompositionInformation::setContainsNonStandardParallelComposition(bool value) {
    nonStandardParallelComposition = value;
}

bool CompositionInformation::containsNonStandardParallelComposition() const {
    return nonStandardParallelComposition;
}

void CompositionInformation::setContainsNestedParallelComposition(bool value) {
    nestedParallelComposition = value;
}

bool CompositionInformation::containsNestedParallelComposition() const {
    return nestedParallelComposition;
}

void CompositionInformation::setContainsParallelComposition(bool value) {
    parallelComposition = value;
}

bool CompositionInformation::containsParallelComposition() const {
    return parallelComposition;
}

std::string const& CompositionInformation::getActionName(uint64_t index) const {
    return indexToNameMap.at(index);
}

uint64_t CompositionInformation::getActionIndex(std::string const& name) const {
    return nameToIndexMap.at(name);
}

void CompositionInformation::addNonSilentActionIndex(uint64_t index) {
    nonSilentActionIndices.insert(index);
}

void CompositionInformation::addNonSilentActionIndices(std::set<uint64_t> const& indices) {
    nonSilentActionIndices.insert(indices.begin(), indices.end());
}

bool CompositionInformation::hasNonSilentActionIndex(uint64_t index) {
    return nonSilentActionIndices.find(index) != nonSilentActionIndices.end();
}

void CompositionInformation::addInputEnabledActionIndex(uint64_t index) {
    inputEnabledActionIndices.insert(index);
}

std::set<uint64_t> const& CompositionInformation::getNonSilentActionIndices() const {
    return nonSilentActionIndices;
}

std::set<uint64_t> const& CompositionInformation::getInputEnabledActionIndices() const {
    return inputEnabledActionIndices;
}

void CompositionInformation::setMappings(std::map<uint64_t, std::string> const& indexToNameMap, std::map<std::string, uint64_t> const& nameToIndexMap) {
    this->indexToNameMap = indexToNameMap;
    this->nameToIndexMap = nameToIndexMap;
}

CompositionInformationVisitor::CompositionInformationVisitor(Model const& model, Composition const& composition)
    : model(model), composition(composition), nextFreeActionIndex(0) {
    // Determine the next free index we can give out to a new action.
    for (auto const& action : model.getActions()) {
        uint64_t actionIndex = model.getActionIndex(action.getName());

        nameToIndexMap[action.getName()] = model.getActionIndex(action.getName());
        indexToNameMap[actionIndex] = action.getName();

        nextFreeActionIndex = std::max(nextFreeActionIndex, model.getActionIndex(action.getName()));
    }
    ++nextFreeActionIndex;
}

CompositionInformation CompositionInformationVisitor::getInformation() {
    CompositionInformation result = boost::any_cast<CompositionInformation>(composition.accept(*this, model));
    result.setMappings(indexToNameMap, nameToIndexMap);
    return result;
}

boost::any CompositionInformationVisitor::visit(AutomatonComposition const& composition, boost::any const& data) {
    Model const& model = boost::any_cast<Model const&>(data);
    Automaton const& automaton = model.getAutomaton(composition.getAutomatonName());

    CompositionInformation result;
    result.increaseAutomatonMultiplicity(composition.getAutomatonName());
    for (auto const& actionIndex : automaton.getActionIndices()) {
        if (actionIndex != storm::jani::Model::SILENT_ACTION_INDEX) {
            result.addNonSilentActionIndex(actionIndex);
        }
    }
    for (auto const& action : composition.getInputEnabledActions()) {
        auto it = nameToIndexMap.find(action);
        STORM_LOG_THROW(it != nameToIndexMap.end(), storm::exceptions::WrongFormatException,
                        "Illegal action name '" << action << "' in when input-enabling automaton '" << composition.getAutomatonName() << "'.");
        uint64_t actionIndex = it->second;
        STORM_LOG_THROW(
            result.hasNonSilentActionIndex(actionIndex), storm::exceptions::WrongFormatException,
            "Cannot make automaton '" << composition.getAutomatonName() << "' input enabled for action " << action << ", because it has no such action.");
        result.addInputEnabledActionIndex(actionIndex);
    }
    return result;
}

boost::any CompositionInformationVisitor::visit(ParallelComposition const& composition, boost::any const& data) {
    CompositionInformation result;

    std::vector<CompositionInformation> subinformation;

    std::set<uint64_t> nonsilentSubActionIndices;
    for (auto const& subcomposition : composition.getSubcompositions()) {
        subinformation.push_back(boost::any_cast<CompositionInformation>(subcomposition->accept(*this, data)));
        nonsilentSubActionIndices.insert(subinformation.back().getNonSilentActionIndices().begin(), subinformation.back().getNonSilentActionIndices().end());
    }

    bool containsNonStandardParallelComposition = false;
    bool containsSubParallelComposition = false;

    for (auto const& subinfo : subinformation) {
        containsNonStandardParallelComposition |= subinfo.containsNonStandardParallelComposition();
        containsSubParallelComposition |= subinfo.containsParallelComposition();
        result.addMultiplicityMap(subinfo.getAutomatonToMultiplicityMap());
    }

    // Keep track of the synchronization vectors that are effective, meaning that the subcompositions all have
    // the non-silent actions that are referred to.
    std::set<uint64_t> effectiveSynchVectors;
    for (uint64_t synchVectorIndex = 0; synchVectorIndex < composition.getNumberOfSynchronizationVectors(); ++synchVectorIndex) {
        effectiveSynchVectors.insert(synchVectorIndex);
    }

    // Now compute non-silent actions.
    std::set<uint64_t> nonSilentActionIndices;
    for (uint_fast64_t infoIndex = 0; infoIndex < subinformation.size(); ++infoIndex) {
        auto const& subinfo = subinformation[infoIndex];

        std::set<uint64_t> enabledSynchVectors;
        std::set<uint64_t> actionsInSynch;
        for (uint64_t synchVectorIndex = 0; synchVectorIndex < composition.getNumberOfSynchronizationVectors(); ++synchVectorIndex) {
            auto const& synchVector = composition.getSynchronizationVector(synchVectorIndex);
            if (synchVector.getInput(infoIndex) != SynchronizationVector::NO_ACTION_INPUT) {
                for (auto const& nonSilentActionIndex : subinfo.getNonSilentActionIndices()) {
                    std::string const& nonSilentAction = indexToNameMap.at(nonSilentActionIndex);
                    if (synchVector.getInput(infoIndex) == nonSilentAction) {
                        enabledSynchVectors.insert(synchVectorIndex);
                        actionsInSynch.insert(nonSilentActionIndex);
                    }
                }
            } else {
                enabledSynchVectors.insert(synchVectorIndex);
            }
        }

        std::set_difference(subinfo.getNonSilentActionIndices().begin(), subinfo.getNonSilentActionIndices().end(), actionsInSynch.begin(),
                            actionsInSynch.end(), std::inserter(nonSilentActionIndices, nonSilentActionIndices.begin()));

        std::set<uint64_t> newEffectiveSynchVectors;
        std::set_intersection(effectiveSynchVectors.begin(), effectiveSynchVectors.end(), enabledSynchVectors.begin(), enabledSynchVectors.end(),
                              std::inserter(newEffectiveSynchVectors, newEffectiveSynchVectors.begin()));
        effectiveSynchVectors = std::move(newEffectiveSynchVectors);
    }

    // Now add all outputs of effective synchronization vectors.
    for (auto const& synchVectorIndex : effectiveSynchVectors) {
        auto const& synchVector = composition.getSynchronizationVector(synchVectorIndex);
        if (synchVector.getOutput() != storm::jani::Model::SILENT_ACTION_NAME) {
            nonSilentActionIndices.insert(addOrGetActionIndex(synchVector.getOutput()));
        }
    }

    // Finally check whether it's a non-standard parallel composition. We do that by first constructing a set of
    // all effective synchronization vectors and then checking whether this set is fully contained within the
    // set of expected synchronization vectors.

    std::set<storm::jani::SynchronizationVector, storm::jani::SynchronizationVectorLexicographicalLess> synchVectorSet;
    for (auto synchVectorIndex : effectiveSynchVectors) {
        synchVectorSet.insert(composition.getSynchronizationVector(synchVectorIndex));
    }

    // Construct the set of expected synchronization vectors.
    std::set<storm::jani::SynchronizationVector, storm::jani::SynchronizationVectorLexicographicalLess> expectedSynchVectorSetUnderApprox;
    std::set<storm::jani::SynchronizationVector, storm::jani::SynchronizationVectorLexicographicalLess> expectedSynchVectorSetOverApprox;
    for (auto actionIndex : nonsilentSubActionIndices) {
        std::string const& actionName = indexToNameMap.at(actionIndex);
        std::vector<std::string> input;
        uint64_t numberOfParticipatingAutomata = 0;
        for (auto const& subcomposition : subinformation) {
            if (subcomposition.getNonSilentActionIndices().find(actionIndex) != subcomposition.getNonSilentActionIndices().end()) {
                input.push_back(actionName);
                ++numberOfParticipatingAutomata;
            } else {
                input.push_back(SynchronizationVector::NO_ACTION_INPUT);
            }
        }

        storm::jani::SynchronizationVector newSynchVector(input, actionName);
        expectedSynchVectorSetOverApprox.insert(newSynchVector);
        if (numberOfParticipatingAutomata > 1) {
            expectedSynchVectorSetUnderApprox.insert(newSynchVector);
        }
    }

    containsNonStandardParallelComposition |= !std::includes(expectedSynchVectorSetOverApprox.begin(), expectedSynchVectorSetOverApprox.end(),
                                                             synchVectorSet.begin(), synchVectorSet.end(), SynchronizationVectorLexicographicalLess());

    containsNonStandardParallelComposition |= !std::includes(synchVectorSet.begin(), synchVectorSet.end(), expectedSynchVectorSetUnderApprox.begin(),
                                                             expectedSynchVectorSetUnderApprox.end(), SynchronizationVectorLexicographicalLess());

    result.setContainsNonStandardParallelComposition(containsNonStandardParallelComposition);
    result.setContainsParallelComposition(true);
    result.setContainsNestedParallelComposition(containsSubParallelComposition);

    result.addNonSilentActionIndices(nonSilentActionIndices);
    return result;
}

uint64_t CompositionInformationVisitor::addOrGetActionIndex(std::string const& name) {
    auto it = nameToIndexMap.find(name);
    if (it != nameToIndexMap.end()) {
        return it->second;
    } else {
        nameToIndexMap[name] = nextFreeActionIndex;
        indexToNameMap[nextFreeActionIndex] = name;
        return nextFreeActionIndex++;
    }
}

}  // namespace jani
}  // namespace storm
