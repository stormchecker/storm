#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "storm/environment/Environment.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/transformer/zeroWeight/ZeroWeightAnalysis.h"

namespace storm::transformer {

/*!
 * Answers exit-law queries for a zero-weight component.
 *
 * @tparam ValueType The probability type.
 */
template<typename ValueType>
class ZeroWeightComponentLp {
   public:
    struct Result {
        ValueType objectiveValue;
        std::vector<ValueType> boundaryProbabilities;
        std::vector<ValueType> actionFlows;
    };

    /*!
     * Builds one flow model for all entries of a validated split component.
     *
     * @param environment The LP solver settings.
     * @param transitionMatrix The split transition matrix.
     * @param actionWeights One weight per matrix row.
     * @param component The analyzed zero-weight component.
     */
    ZeroWeightComponentLp(storm::Environment const& environment, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                          std::vector<ValueType> const& actionWeights, ZeroWeightComponent const& component);

    ~ZeroWeightComponentLp();

    /*!
     * Maximizes a boundary score from one component entry.
     *
     * @param entryState An entry state of the component.
     * @param direction One score per boundary state, in getBoundaryStates() order.
     * @return The optimal score, boundary probabilities, and action flows.
     */
    Result maximize(uint64_t entryState, std::vector<ValueType> const& direction);

    /*!
     * Maximizes a boundary score while fixing another score to a given value.
     *
     * @param entryState An entry state of the component.
     * @param direction Objective coefficients in boundary-state order.
     * @param fixedDirection Coefficients of the fixed score in the same order.
     * @param fixedScore Required value of the fixed score.
     * @return The optimal score, boundary probabilities, and action flows.
     */
    Result maximizeWithFixedScore(uint64_t entryState, std::vector<ValueType> const& direction, std::vector<ValueType> const& fixedDirection,
                                  ValueType const& fixedScore);

    std::vector<uint64_t> const& getStates() const;
    std::vector<uint64_t> const& getEntryStates() const;
    std::vector<uint64_t> const& getBoundaryStates() const;
    std::vector<uint64_t> const& getActionRows() const;

   private:
    struct SolverSession;

    std::unique_ptr<SolverSession> buildSolver() const;
    Result solve(SolverSession& session, uint64_t entryState, std::vector<ValueType> const& direction, bool reuse,
                 std::vector<ValueType> const* fixedDirection = nullptr, ValueType const* fixedScore = nullptr);

    storm::Environment environment;
    std::vector<uint64_t> states;
    std::vector<uint64_t> entryStates;
    std::vector<uint64_t> boundaryStates;
    std::vector<uint64_t> actionRows;
    std::vector<std::map<uint64_t, ValueType>> flowCoefficients;
    std::vector<std::map<uint64_t, ValueType>> boundaryCoefficients;
    std::unique_ptr<SolverSession> solverSession;
    bool supportsReuse = true;
};

}  // namespace storm::transformer
