#include "storm/transformer/bisimulation/Bisimulation.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/models/sparse/Model.h"

#include "storm/transformer/bisimulation/Initialization.h"
#include "storm/transformer/bisimulation/Partition.h"
#include "storm/transformer/bisimulation/Quotient.h"
#include "storm/transformer/bisimulation/QuotientData.h"
#include "storm/transformer/bisimulation/SignatureRefinement.h"
#include "storm/transformer/bisimulation/Signatures.h"
#include "storm/transformer/bisimulation/SplitterBasedRefinement.h"
#include "storm/transformer/bisimulation/WeakBisimulationData.h"
#include "storm/utility/OptionalRef.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/constants.h"

namespace storm::bisimulation {

template<typename ValueType>
ReturnType<ValueType> performBisimulationMinimization(storm::models::sparse::Model<ValueType> const& model,
                                                      std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas, Options const& options) {
    // Step 0: Sanity checks and set-up
    STORM_LOG_THROW(options.tolerance >= storm::utility::zero<storm::RationalNumber>(), storm::exceptions::InvalidArgumentException,
                    "Tolerance for bisimulation minimization must be non-negative, but was " << options.tolerance << ".");
    bool const isWeak = options.bisimulationType == Options::BisimulationType::Weak;
    if (isWeak) {
        STORM_LOG_THROW(!model.isNondeterministicModel(), storm::exceptions::NotSupportedException,
                        "Weak bisimulation is only supported for deterministic models, but the given model is of type " << model.getType() << ".");
        STORM_LOG_WARN_COND(!options.preferSignatureRefinement,
                            "Using splitter-based refinement because weak bisimulation is not supported for signature-based refinement.");
    }
    // Weak bisimulation for signature-based refinement is not supported.
    // Deterministic models default to splitter-based refinement, which is usually faster
    bool const useSignatureRefinement = !isWeak && (model.isNondeterministicModel() || options.preferSignatureRefinement);
    storm::utility::Stopwatch sw(true);
    STORM_LOG_STATISTICS("-------- Bisimulation Minimization --------");

    // Step 1: Obtain an initial partition based on what needs to be preserved (labels, rewards, ...)
    storm::bisimulation::Initialization<ValueType> initialization(model, options, formulas);
    auto const preservationInformation = initialization.getPreservationInformation();
    auto const choiceClasses = initialization.getChoiceClasses();
    auto partition = initialization.getInitialStatePartition(choiceClasses);
    STORM_LOG_STATISTICS(sw << " seconds for initial partition (" << partition.getNumberOfBlocks() << " blocks).");
    sw.restart();

    // Step 2: Apply refinement using the initial partition and choiceClasses. Initialize QuotientData.
    std::optional<storm::bisimulation::QuotientData<ValueType>> quotientData;
    // commonly called right after refinement.
    auto initializeQuotientData = [&](storm::OptionalRef<storm::storage::BitVector const> preferredRepresentatives = storm::NullRef) {
        STORM_LOG_STATISTICS(sw << " seconds for refinement (" << partition.getNumberOfBlocks() << " blocks).");
        sw.restart();
        quotientData.emplace(model, partition, preferredRepresentatives);
    };
    if constexpr (storm::IsIntervalType<ValueType>) {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Bisimulation is not supported for Interval models.");
    } else if (isWeak) {
        // Weak bisimulation additionally needs to know the divergent, step sensitive and silent states. Computing them further refines the partition.
        // Both that computation and the refinement need the transposed transition matrix, so we build it once and share it.
        auto const backwardTransitions = model.getBackwardTransitions();
        auto weakData = initialization.getWeakBisimulationData(partition, backwardTransitions, preservationInformation);
        STORM_LOG_STATISTICS(sw << " seconds for weak bisimulation initialization (" << partition.getNumberOfBlocks() << " blocks).");
        sw.restart();
        if (model.isDiscreteTimeModel()) {
            storm::bisimulation::performSplitterBasedRefinement<ValueType, SplitterRefinementMode::WeakDiscreteTime>(
                model, backwardTransitions, partition, storm::utility::convertNumber<ValueType>(options.tolerance), weakData);
        } else {
            storm::bisimulation::performSplitterBasedRefinement<ValueType, SplitterRefinementMode::WeakContinuousTime>(
                model, backwardTransitions, partition, storm::utility::convertNumber<ValueType>(options.tolerance), weakData);
        }
        // For weak bisimulation, the quotient transitions have to be derived from a state that can actually leave its block.
        auto const nonSilentStates = ~weakData.silentStates;
        initializeQuotientData(nonSilentStates);
        quotientData->weakData.emplace(std::move(weakData));
        if (options.createQuotientChoiceMapping) {
            quotientData->toQuotientChoice = quotientData->toQuotientState;  // For deterministic models, quotient state and choice mappings are identical.
        }
    } else if (useSignatureRefinement && storm::utility::isZero(options.tolerance)) {
        storm::bisimulation::Signatures<ValueType, SignatureMode::Exact> signatures(model, choiceClasses, partition);
        storm::bisimulation::performSignatureBasedRefinement(model, partition, signatures);
        initializeQuotientData();
        signatures.extendQuotientData(quotientData.value(), options.createQuotientChoiceMapping);
    } else if (useSignatureRefinement && !storm::utility::isZero(options.tolerance)) {
        if constexpr (std::is_same_v<ValueType, storm::RationalFunction>) {
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                            "Bisimulation with positive tolerance " << storm::utility::convertNumber<double>(options.tolerance)
                                                                    << " is not supported for parametric models.");
        } else {
            storm::bisimulation::Signatures<ValueType, SignatureMode::Approximative> signatures(model, choiceClasses, partition,
                                                                                                storm::utility::convertNumber<ValueType>(options.tolerance));
            storm::bisimulation::performSignatureBasedRefinement(model, partition, signatures);
            initializeQuotientData();
            signatures.extendQuotientData(quotientData.value(), options.createQuotientChoiceMapping);
        }
    } else {
        // For deterministic models we do splitter based refinement.
        storm::bisimulation::performSplitterBasedRefinement<ValueType>(model, model.getBackwardTransitions(), partition,
                                                                       storm::utility::convertNumber<ValueType>(options.tolerance));
        initializeQuotientData();
        if (options.createQuotientChoiceMapping) {
            quotientData->toQuotientChoice = quotientData->toQuotientState;  // For deterministic models, quotient state and choice mappings are identical.
        }
    }

    // Step 3: Extract the quotient
    auto quotientModel = storm::bisimulation::Quotient<ValueType>::buildFromPartition(model, options, preservationInformation, quotientData.value());
    STORM_LOG_STATISTICS(sw << " seconds for quotient extraction.");
    STORM_LOG_STATISTICS("-------------------------------------------");

    return {.quotient = std::move(quotientModel),
            .toQuotientStateMapping = std::move(quotientData->toQuotientState),
            .toQuotientChoiceMapping = std::move(quotientData->toQuotientChoice)};
}

template ReturnType<double> performBisimulationMinimization(storm::models::sparse::Model<double> const& model,
                                                            std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas, Options const& options);
template ReturnType<storm::RationalNumber> performBisimulationMinimization(storm::models::sparse::Model<storm::RationalNumber> const& model,
                                                                           std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas,
                                                                           Options const& options);
template ReturnType<storm::RationalFunction> performBisimulationMinimization(storm::models::sparse::Model<storm::RationalFunction> const& model,
                                                                             std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas,
                                                                             Options const& options);
template ReturnType<storm::Interval> performBisimulationMinimization(storm::models::sparse::Model<storm::Interval> const& model,
                                                                     std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas,
                                                                     Options const& options);
template ReturnType<storm::RationalInterval> performBisimulationMinimization(storm::models::sparse::Model<storm::RationalInterval> const& model,
                                                                             std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas,
                                                                             Options const& options);

}  // namespace storm::bisimulation
