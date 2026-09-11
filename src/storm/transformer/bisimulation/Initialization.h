#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "storm/logic/FormulasForwardDeclarations.h"
#include "storm/models/sparse/ModelForward.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/transformer/bisimulation/Options.h"
#include "storm/transformer/bisimulation/Partition.h"
#include "storm/transformer/bisimulation/PreservationInformation.h"
#include "storm/transformer/bisimulation/WeakBisimulationData.h"

namespace storm::bisimulation {

template<typename ValueType>
class Initialization {
   public:
    Initialization(storm::models::sparse::Model<ValueType> const& model, Options const& options,
                   std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas);

    PreservationInformation getPreservationInformation() const;

    std::optional<std::vector<uint64_t>> getChoiceClasses() const;

    Partition getInitialStatePartition(std::optional<std::vector<uint64_t>> const& choiceClasses = {}) const;

    /*!
     * Computes the state-level information that weak bisimulation requires on top of the partition, and refines the given (initial) partition so that it
     * becomes homogeneous with respect to that information.
     *
     * @param partition the initial partition, which already has to respect the preserved annotations. Is split so that no block contains both divergent and
     * non-divergent states.
     * @param backwardTransitions the transposed transition matrix of the model.
     * @param preservationInformation what the minimization has to preserve, cf. getPreservationInformation.
     * @note only applicable to deterministic models and if weak bisimulation was requested.
     */
    WeakBisimulationData getWeakBisimulationData(Partition& partition, storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                 PreservationInformation const& preservationInformation) const;

   private:
    storm::models::sparse::Model<ValueType> const& model;
    Options const options;
    std::vector<std::shared_ptr<storm::logic::Formula const>> const formulas;

    /*!
     * Bookkeeping of the labels/rewards that need to be preserved by the bisimulation, split into their boolean-, integer-, and value-typed parts.
     */
    struct PreservedAnnotations {
        std::vector<std::reference_wrapper<storm::storage::BitVector const>> booleans;
        std::vector<std::span<uint64_t const>> integers;
        std::vector<std::span<ValueType const>> values;

        /*!
         * @return true iff there are no preserved annotations
         */
        bool empty() const;

        /*!
         * Splits all blocks in the partition with respect to the stored annotations.
         * @post Each two elements in a block of the partition have the same annotations.
         * @param partition The partition to be refined
         * @param tolerance When splitting by ValueType annotations, two values are considered equal if they differ by at most this tolerance.
         * @param extraAnnotation If non-empty, the partition is also split according to this additional annotation
         */
        void applySplit(Partition& partition, ValueType const& tolerance, std::vector<uint64_t> const& extraAnnotation = {}) const;
    } preservedStateAnnotations, preservedChoiceAnnotations;
};

}  // namespace storm::bisimulation
