#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <boost/optional.hpp>

#include "Composition.h"

namespace storm {
namespace jani {

class SynchronizationVector {
   public:
    SynchronizationVector(std::vector<std::string> const& input, std::string const& output);
    SynchronizationVector(std::vector<std::string> const& input);

    std::size_t size() const;
    std::vector<std::string> const& getInput() const;
    std::string const& getInput(uint64_t index) const;
    std::string const& getOutput() const;

    static bool isNoActionInput(std::string const& action);

    /*!
     * Retrieves the action name that is the last participating action before the given input index. If there is
     * no such action none is returned.
     */
    boost::optional<std::string> getPrecedingParticipatingAction(uint64_t index) const;

    /*!
     * Retrieves the position of the last participating action before the given input index. If there is
     * no such action none is returned.
     */
    boost::optional<uint64_t> getPositionOfPrecedingParticipatingAction(uint64_t index) const;

    /*!
     * Retrieves the position of the first participating action.
     */
    uint64_t getPositionOfFirstParticipatingAction() const;

    /*!
     * Retrieves the number of action inputs, i.e. participating subcompositions.
     */
    uint64_t getNumberOfActionInputs() const;

    // A marker that can be used as one of the inputs. The semantics is that no action of the corresponding
    // automaton takes part in the synchronization.
    static const std::string NO_ACTION_INPUT;

   private:
    /// The input to the synchronization.
    std::vector<std::string> input;

    /// The output of the synchronization.
    std::string output;
};

bool operator==(SynchronizationVector const& vector1, SynchronizationVector const& vector2);
bool operator!=(SynchronizationVector const& vector1, SynchronizationVector const& vector2);

struct SynchronizationVectorLexicographicalLess {
    bool operator()(SynchronizationVector const& vector1, SynchronizationVector const& vector2) const;
};

std::ostream& operator<<(std::ostream& stream, SynchronizationVector const& synchronizationVector);

class ParallelComposition : public Composition {
   public:
    /*!
     * Creates a parallel composition of the subcomposition and the provided synchronization vectors.
     */
    ParallelComposition(std::shared_ptr<Composition> const& subcomposition, std::vector<SynchronizationVector> const& synchronizationVectors);

    /*!
     * Creates a parallel composition of the subcompositions and the provided synchronization vectors.
     */
    ParallelComposition(std::vector<std::shared_ptr<Composition>> const& subcompositions, std::vector<SynchronizationVector> const& synchronizationVectors);

    /*!
     * Creates a parallel composition of the subcompositions over the provided synchronization alphabet.
     */
    ParallelComposition(std::vector<std::shared_ptr<Composition>> const& subcompositions, std::set<std::string> const& synchronizationAlphabet);

    /*!
     * Creates a parallel composition of the subcompositions over the provided synchronization alphabet.
     */
    ParallelComposition(std::shared_ptr<Composition> const& leftSubcomposition, std::shared_ptr<Composition> const& rightSubcomposition,
                        std::set<std::string> const& synchronizationAlphabet);

    virtual bool isParallelComposition() const override;

    /*!
     * Retrieves the subcomposition with the given index.
     */
    Composition const& getSubcomposition(uint64_t index) const;

    /*!
     * Retrieves the subcompositions of the parallel composition.
     */
    std::vector<std::shared_ptr<Composition>> const& getSubcompositions() const;

    /*!
     * Retrieves the number of subcompositions of this parallel composition.
     */
    uint64_t getNumberOfSubcompositions() const;

    /*!
     * Retrieves the synchronization vector with the given index.
     */
    SynchronizationVector const& getSynchronizationVector(uint64_t index) const;

    /*!
     * Retrieves the synchronization vectors of the parallel composition.
     */
    std::vector<SynchronizationVector> const& getSynchronizationVectors() const;

    /*!
     * Retrieves the number of synchronization vectors.
     */
    std::size_t getNumberOfSynchronizationVectors() const;

    virtual boost::any accept(CompositionVisitor& visitor, boost::any const& data) const override;

    virtual void write(std::ostream& stream) const override;

    bool areActionsReused() const;

   private:
    /*!
     * Checks the synchronization vectors for validity.
     */
    void checkSynchronizationVectors() const;

    /// The subcompositions.
    std::vector<std::shared_ptr<Composition>> subcompositions;

    /// The synchronization vectors of the parallel composition.
    std::vector<SynchronizationVector> synchronizationVectors;
};

}  // namespace jani
}  // namespace storm
