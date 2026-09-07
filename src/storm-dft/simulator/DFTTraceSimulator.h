#pragma once

#include "storm-dft/generator/DftNextStateGenerator.h"
#include "storm-dft/storage/DFT.h"
#include "storm-dft/storage/DFTState.h"
#include "storm-dft/storage/FailableElements.h"
#include "storm/utility/random.h"

namespace storm::dft {
namespace simulator {

/*!
 * Result of a single simulation step.
 *
 */
enum class SimulationStepResult { SUCCESSFUL, UNSUCCESSFUL, INVALID };

/*!
 * Result of a simulation trace.
 * CONTINUE is only used for partial traces to indicate that no conclusive outcome has been reached yet.
 */
enum class SimulationTraceResult { SUCCESSFUL, UNSUCCESSFUL, INVALID, CONTINUE };

/*!
 * Simulator for DFTs.
 * A step in the simulation corresponds to the failure of one BE (either on its own or triggered by a dependency)
 * and the failure propagation through the DFT.
 * The simulator also allows to randomly generate a next failure according to the failure rates.
 */
template<typename ValueType>
class DFTTraceSimulator {
    using DFTStatePointer = std::shared_ptr<storm::dft::storage::DFTState<ValueType>>;

   public:
    /*!
     * Constructor.
     *
     * @param dft DFT.
     * @param stateGenerationInfo Info for state generation.
     * @param randomGenerator Random number generator.
     */
    DFTTraceSimulator(storm::dft::storage::DFT<ValueType> const& dft, storm::dft::storage::DFTStateGenerationInfo const& stateGenerationInfo,
                      boost::mt19937& randomGenerator);

    /*!
     * Set the random number generator.
     *
     * @param randomNumberGenerator Random number generator.
     */
    void setRandomNumberGenerator(boost::mt19937& randomNumberGenerator);

    /*!
     * Set the current state back to the initial state in order to start a new simulation.
     */
    void resetToInitial();

    /*!
     * Set the current state back to the given state.
     */
    void resetToState(DFTStatePointer state);

    /*!
     * Set the elapsed time so far.
     */
    void setTime(double time);

    /*!
     * Get the current DFT state.
     *
     * @return DFTStatePointer DFT state.
     */
    DFTStatePointer getCurrentState() const;

    /*!
     * Get the total elapsed time so far.
     *
     * @return Elapsed time.
     */
    double getCurrentTime() const;

    /*!
     * Perform one simulation step by letting the next element fail.
     *
     * @param nextFailElement Iterator giving the next element which should fail.
     * @param dependencySuccessful Whether the triggering dependency was successful.
     *              If the dependency is unsuccessful, no BE fails and only the dependency is marked as failed.
     * @return Successful if step could be performed, unsuccessful if no element can fail or invalid if the next state is invalid (due to a restrictor).
     */
    SimulationStepResult step(storm::dft::storage::FailableElements::const_iterator nextFailElement, bool dependencySuccessful = true);

    /*!
     * Randomly pick an element which fails next (either a BE or a dependency which triggers a BE) and the time after which it fails.
     * The time is 0 for a dependency and -1 if no failure can take place.
     * In the latter case, the next failable element is not defined.
     *
     * @return Tuple of next failable element, time after which is fails and whether a possible failure through the dependency is successful.
     */
    std::tuple<storm::dft::storage::FailableElements::const_iterator, double, bool> randomNextFailure();

    /*!
     * Perform a random step by using the random number generator.
     *
     * @return Result of the simulation step (successful, unsuccessful, invalid).
     */
    SimulationStepResult randomStep();

    /*!
     * Simulate the (randomly chosen) next step and return the outcome of the current (partial) trace.
     *
     * @param timebound Time bound in which the system failure should occur.
     * @return Result of (partial) trace is (1) SUCCESSFUL if the current state corresponds to a system failure and the current time does not exceed the
     * timebound. (2) UNSUCCESSFUL, if the current time exceeds the timebound, (3) INVALID, if an invalid state (due to a restrictor) was reached, or (4)
     * CONTINUE, if the simulation should continue.
     */
    SimulationTraceResult simulateNextStep(double timebound);

    /*!
     * Perform a complete simulation of a failure trace by using the random number generator.
     * The simulation starts in the initial state and tries to reach a state where the top-level event of the DFT has failed.
     * If this target state can be reached within the given timebound, the simulation was successful.
     * If an invalid state (due to a restrictor) was reached, the simulated trace is invalid.
     *
     * @param timebound Time bound in which the system failure should occur.
     * @return Result of simulation trace is (1) SUCCESSFUL if a system failure occurred for the generated trace within the time bound,
     *                                       (2) UNSUCCESSFUL, if no system failure occurred within the time bound, or
     *                                       (3) INVALID, if an invalid state (due to a restrictor) was reached during the trace generation.
     */
    SimulationTraceResult simulateCompleteTrace(double timebound);

   protected:
    // The DFT used for the generation of next states.
    storm::dft::storage::DFT<ValueType> const& dft;

    // General information for the state generation.
    storm::dft::storage::DFTStateGenerationInfo const& stateGenerationInfo;

    // Generator for creating next state in DFT
    storm::dft::generator::DftNextStateGenerator<ValueType> generator;

    // Current state
    DFTStatePointer state;

    // Currently elapsed time
    double time;

    // Random number generator
    boost::mt19937& randomGenerator;
};

}  // namespace simulator
}  // namespace storm::dft
