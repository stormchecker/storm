#include "storm/storage/jani/ParallelComposition.h"

#include <sstream>

#include <boost/algorithm/string/join.hpp>

#include "storm/exceptions/WrongFormatException.h"
#include "storm/storage/jani/Model.h"
#include "storm/utility/macros.h"

namespace storm {
namespace jani {

const std::string SynchronizationVector::NO_ACTION_INPUT = "-";

SynchronizationVector::SynchronizationVector(std::vector<std::string> const& input, std::string const& output) : input(input), output(output) {
    // Intentionally left empty.
}

SynchronizationVector::SynchronizationVector(std::vector<std::string> const& input) : input(input), output(storm::jani::Model::SILENT_ACTION_NAME) {}

std::size_t SynchronizationVector::size() const {
    return input.size();
}

std::vector<std::string> const& SynchronizationVector::getInput() const {
    return input;
}

std::string const& SynchronizationVector::getInput(uint64_t index) const {
    return input[index];
}
std::string const& SynchronizationVector::getOutput() const {
    return output;
}

boost::optional<std::string> SynchronizationVector::getPrecedingParticipatingAction(uint64_t index) const {
    boost::optional<uint64_t> position = getPositionOfPrecedingParticipatingAction(index);
    if (position) {
        return getInput(position.get());
    } else {
        return boost::none;
    }
}

boost::optional<uint64_t> SynchronizationVector::getPositionOfPrecedingParticipatingAction(uint64_t index) const {
    if (index == 0) {
        return boost::none;
    }

    uint64_t i = index - 1;
    for (; i > 0; --i) {
        if (this->getInput(i) != NO_ACTION_INPUT) {
            return boost::make_optional(i);
        }
    }

    // Check the 0-index.
    if (this->getInput(i) != NO_ACTION_INPUT) {
        return boost::make_optional(i);
    }

    return boost::none;
}

uint64_t SynchronizationVector::getPositionOfFirstParticipatingAction() const {
    for (uint64_t result = 0; result < this->size(); ++result) {
        if (this->getInput(result) != NO_ACTION_INPUT) {
            return result;
        }
    }
    STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Synchronization vector must have at least one participating action.");
}

uint64_t SynchronizationVector::getNumberOfActionInputs() const {
    uint64_t result = 0;
    for (auto const& inputEntry : input) {
        if (!isNoActionInput(inputEntry)) {
            ++result;
        }
    }
    return result;
}

bool SynchronizationVector::isNoActionInput(std::string const& action) {
    return action == NO_ACTION_INPUT;
}

std::ostream& operator<<(std::ostream& stream, SynchronizationVector const& synchronizationVector) {
    bool first = true;
    stream << "(";
    for (auto const& element : synchronizationVector.getInput()) {
        if (!first) {
            stream << ", ";
        }
        stream << element;
        first = false;
    }
    stream << ") -> " << synchronizationVector.getOutput();
    return stream;
}

bool operator==(SynchronizationVector const& vector1, SynchronizationVector const& vector2) {
    if (vector1.getOutput() != vector2.getOutput()) {
        return false;
    }
    if (vector1.getInput() != vector2.getInput()) {
        return false;
    }
    return true;
}

bool operator!=(SynchronizationVector const& vector1, SynchronizationVector const& vector2) {
    return !(vector1 == vector2);
}

bool SynchronizationVectorLexicographicalLess::operator()(SynchronizationVector const& vector1, SynchronizationVector const& vector2) const {
    STORM_LOG_THROW(vector1.size() == vector2.size(), storm::exceptions::WrongFormatException, "Cannot compare synchronization vectors of different size.");
    for (uint64_t i = 0; i < vector1.size(); ++i) {
        if (vector1.getInput(i) < vector2.getInput(i)) {
            return true;
        } else if (vector1.getInput(i) > vector2.getInput(i)) {
            return false;
        }
    }
    if (vector1.getOutput() < vector2.getOutput()) {
        return true;
    } else if (vector1.getOutput() > vector2.getOutput()) {
        return false;
    }
    return false;
}

ParallelComposition::ParallelComposition(std::shared_ptr<Composition> const& subcomposition, std::vector<SynchronizationVector> const& synchronizationVectors)
    : ParallelComposition(std::vector<std::shared_ptr<Composition>>{subcomposition}, synchronizationVectors) {
    // Intentionally left empty.
}

ParallelComposition::ParallelComposition(std::vector<std::shared_ptr<Composition>> const& subcompositions,
                                         std::vector<SynchronizationVector> const& synchronizationVectors)
    : subcompositions(subcompositions), synchronizationVectors(synchronizationVectors) {
    STORM_LOG_THROW(!subcompositions.empty(), storm::exceptions::WrongFormatException, "At least one automaton required for parallel composition.");
    this->checkSynchronizationVectors();
}

ParallelComposition::ParallelComposition(std::vector<std::shared_ptr<Composition>> const& subcompositions, std::set<std::string> const& synchronizationAlphabet)
    : subcompositions(subcompositions), synchronizationVectors() {
    STORM_LOG_THROW(!subcompositions.empty(), storm::exceptions::WrongFormatException, "At least one automaton required for parallel composition.");

    // Manually construct the synchronization vectors for all elements of the synchronization alphabet.
    for (auto const& action : synchronizationAlphabet) {
        synchronizationVectors.emplace_back(std::vector<std::string>(this->subcompositions.size(), action), action);
    }
}

ParallelComposition::ParallelComposition(std::shared_ptr<Composition> const& leftSubcomposition, std::shared_ptr<Composition> const& rightSubcomposition,
                                         std::set<std::string> const& synchronizationAlphabet) {
    subcompositions.push_back(leftSubcomposition);
    subcompositions.push_back(rightSubcomposition);

    // Manually construct the synchronization vectors for all elements of the synchronization alphabet.
    for (auto const& action : synchronizationAlphabet) {
        synchronizationVectors.emplace_back(std::vector<std::string>(this->subcompositions.size(), action), action);
    }
}

bool ParallelComposition::isParallelComposition() const {
    return true;
}

Composition const& ParallelComposition::getSubcomposition(uint64_t index) const {
    return *subcompositions[index];
}

std::vector<std::shared_ptr<Composition>> const& ParallelComposition::getSubcompositions() const {
    return subcompositions;
}

uint64_t ParallelComposition::getNumberOfSubcompositions() const {
    return subcompositions.size();
}

SynchronizationVector const& ParallelComposition::getSynchronizationVector(uint64_t index) const {
    return synchronizationVectors[index];
}

std::vector<SynchronizationVector> const& ParallelComposition::getSynchronizationVectors() const {
    return synchronizationVectors;
}

std::size_t ParallelComposition::getNumberOfSynchronizationVectors() const {
    return synchronizationVectors.size();
}

bool ParallelComposition::areActionsReused() const {
    for (uint_fast64_t inputIndex = 0; inputIndex < subcompositions.size(); ++inputIndex) {
        std::set<std::string> actions;
        for (auto const& vector : synchronizationVectors) {
            std::string const& action = vector.getInput(inputIndex);
            if (action != SynchronizationVector::NO_ACTION_INPUT) {
                if (actions.find(action) != actions.end()) {
                    return true;
                }
                actions.insert(action);
            }
        }
        // And check recursively, in case we have nested parallel composition
        if (subcompositions.at(inputIndex)->isParallelComposition()) {
            if (subcompositions.at(inputIndex)->asParallelComposition().areActionsReused()) {
                return true;
            }
        }
    }
    return false;
}

void ParallelComposition::checkSynchronizationVectors() const {
    for (uint_fast64_t inputIndex = 0; inputIndex < subcompositions.size(); ++inputIndex) {
        for (auto const& vector : synchronizationVectors) {
            STORM_LOG_THROW(vector.size() == this->subcompositions.size(), storm::exceptions::WrongFormatException,
                            "Synchronization vectors must match parallel composition size.");
        }
    }

    for (auto const& vector : synchronizationVectors) {
        bool hasInput = false;
        for (auto const& input : vector.getInput()) {
            if (input != SynchronizationVector::NO_ACTION_INPUT) {
                hasInput = true;
                break;
            }
        }
        STORM_LOG_THROW(hasInput, storm::exceptions::WrongFormatException, "Synchronization vector must have at least one proper input.");
    }
}

boost::any ParallelComposition::accept(CompositionVisitor& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

void ParallelComposition::write(std::ostream& stream) const {
    std::vector<std::string> synchronizationVectorsAsStrings;
    for (auto const& synchronizationVector : synchronizationVectors) {
        std::stringstream tmpStream;
        tmpStream << synchronizationVector;
        synchronizationVectorsAsStrings.push_back(tmpStream.str());
    }

    bool first = true;
    bool hasSynchVectors = !synchronizationVectors.empty();
    stream << "(";
    for (auto const& subcomposition : subcompositions) {
        if (!first) {
            stream << (hasSynchVectors ? " || " : " ||| ");
        }
        stream << *subcomposition;
        first = false;
    }
    stream << ")";
    if (hasSynchVectors) {
        stream << "[" << boost::algorithm::join(synchronizationVectorsAsStrings, ", ") << "]";
    }
}

}  // namespace jani
}  // namespace storm
