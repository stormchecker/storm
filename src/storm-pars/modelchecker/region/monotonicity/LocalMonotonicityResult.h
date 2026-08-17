#pragma once

#include <vector>

#include "storm-pars/modelchecker/region/monotonicity/MonotonicityResult.h"
#include "storm/storage/BitVector.h"

namespace storm {
namespace analysis {
template<typename VariableType>
class LocalMonotonicityResult {
   public:
    typedef typename MonotonicityResult<VariableType>::Monotonicity Monotonicity;

    /*!
     * Constructs a new LocalMonotonicityResult object.
     *
     * @param numberOfStates The number of states in the considered model.
     * @return The new object.
     */
    LocalMonotonicityResult(uint_fast64_t numberOfStates);

    LocalMonotonicityResult(std::shared_ptr<MonotonicityResult<VariableType>> globalResult, uint_fast64_t numberOfStates);

    /*!
     * Returns the local Monotonicity of a parameter at a given state.
     *
     * @param state The state.
     * @param var The parameter.
     * @return The local Monotonicity.
     */
    Monotonicity getMonotonicity(uint_fast64_t state, VariableType var) const;
    std::shared_ptr<MonotonicityResult<VariableType>> getMonotonicity(uint_fast64_t state) const;

    /*!
     * Sets the local Monotonicity of a parameter at a given state.
     *
     * @param mon The Monotonicity.
     * @param state The state.
     * @param var The parameter.
     */
    void setMonotonicity(uint_fast64_t state, VariableType var, Monotonicity mon);

    /*!
     * Returns the Global MonotonicityResult object that corresponds to this object.
     *
     * @return The pointer to the global MonotonicityResult.
     */
    std::shared_ptr<MonotonicityResult<VariableType>> getGlobalMonotonicityResult() const;

    /*!
     * Constructs a new LocalMonotonicityResult object that is a copy of the current one.
     *
     * @return Pointer to the copy.
     */
    std::shared_ptr<LocalMonotonicityResult<VariableType>> copy();

    /*!
     * Checks if the LocalMonotonicity is done yet.
     *
     * @return true if done is set to true, false otherwise.
     */
    bool isDone() const;
    void setDone(bool done = true);

    bool isNoMonotonicity() const;

    void setConstant(uint_fast64_t state);

    void setIndexMinimize(int index);
    void setIndexMaximize(int index);
    int getIndexMinimize() const;
    int getIndexMaximize() const;

    /*!
     * Constructs a string output of all variables and their corresponding Monotonicity
     * @return Results so far
     */
    std::string toString() const;

    void setMonotoneIncreasing(VariableType var);
    void setMonotoneDecreasing(VariableType var);

    bool isFixedParametersSet() const;

   private:
    std::vector<std::shared_ptr<MonotonicityResult<VariableType>>> stateMonRes;

    std::shared_ptr<MonotonicityResult<VariableType>> globalMonotonicityResult;

    void setMonotonicityResult(uint_fast64_t state, std::shared_ptr<MonotonicityResult<VariableType>> monRes);

    void setGlobalMonotonicityResult(std::shared_ptr<MonotonicityResult<VariableType>> monRes);

    void setStatesMonotone(storm::storage::BitVector statesMonotone);

    storm::storage::BitVector statesMonotone;
    bool done;

    int indexMinimize = -1;
    int indexMaximize = -1;
    std::shared_ptr<MonotonicityResult<VariableType>> dummyPointer;

    bool setFixedParameters = false;
};
}  // namespace analysis
}  // namespace storm
