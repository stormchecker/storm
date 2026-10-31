#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "storm/environment/Environment.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/modelchecker/cvar/helper/SspParetoValueIterationOperator.h"
#include "storm/modelchecker/cvar/helper/SspZeroWeightComponentLpSolver.h"
#include "storm/modelchecker/cvar/preprocessing/SspCvarPreprocessingResult.h"
#include "storm/transformer/zeroWeight/ZeroWeightAnalysis.h"
#include "storm/utility/macros.h"

namespace storm::modelchecker::cvar {

/*! Computes one Pareto-VI layer with zero-weight component entry fronts. */
template<typename ValueType, SspParetoFrontKind FrontKind = SspParetoFrontKind::CostUpperTail>
class SspZeroWeightParetoViOperator {
   public:
    using PositiveOperator = SspParetoValueIterationOperator<ValueType, FrontKind>;
    using ComponentSolver = SspZeroWeightComponentLpSolver<ValueType, FrontKind>;
    using FrontierLayer = typename PositiveOperator::FrontierLayer;
    using FrontierWindow = typename PositiveOperator::FrontierWindow;

    /*! Prepares the positive-state update and the validated split components.
     * @param environment LP solver settings.
     * @param preprocessingResult Split matrix, values, states, and component analysis.
     */
    SspZeroWeightParetoViOperator(storm::Environment const& environment, preprocessing::SspCvarPreprocessingResult<ValueType> const& preprocessingResult)
        : positiveOperator(preprocessingResult) {
        STORM_LOG_THROW(preprocessingResult.zeroWeightAnalysis.has_value(), storm::exceptions::InvalidArgumentException,
                        "Expected validated zero-weight component analysis.");
        auto const& analysis = preprocessingResult.zeroWeightAnalysis.value();
        zeroWeightStates = analysis.statesWithZeroWeightChoices;
        STORM_LOG_ASSERT(zeroWeightStates.size() == positiveOperator.getStateCount(), "Expected zero-weight analysis of the split matrix.");
        for (auto const& component : analysis.weakComponents) {
            STORM_LOG_ASSERT(!component.entryStates.empty(), "Expected validated zero-weight component entries.");
            components.push_back({component.entryStates, std::make_unique<ComponentSolver>(environment, preprocessingResult.transitionMatrix,
                                                                                           preprocessingResult.choiceCosts, component)});
        }
    }

    /*! Computes positive-state fronts before entry fronts at the same threshold.
     * @param threshold Current cost or reward threshold.
     * @param frontierWindow Earlier-threshold fronts, including the negative layer needed at threshold zero.
     * @param outputLayer Current-threshold fronts; non-entry zero-weight states remain empty.
     */
    void apply(uint64_t threshold, FrontierWindow const& frontierWindow, FrontierLayer& outputLayer) {
        positiveOperator.applyPositiveStates(threshold, frontierWindow, outputLayer, zeroWeightStates);
        for (auto& component : components) {
            for (uint64_t entry : component.entryStates) {
                outputLayer[entry] = component.solver->frontier(entry, outputLayer);
            }
            component.solver->finishQueryBatch();
        }
    }

   private:
    struct Component {
        std::vector<uint64_t> entryStates;
        std::unique_ptr<ComponentSolver> solver;
    };

    PositiveOperator positiveOperator;
    storm::storage::BitVector zeroWeightStates;
    std::vector<Component> components;
};

}  // namespace storm::modelchecker::cvar
