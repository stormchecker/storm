#include "storm/storage/dd/bisimulation/Partition.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/logic/Formulas.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/propositional/SymbolicPropositionalModelChecker.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/dd/bisimulation/PreservationInformation.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {
namespace bisimulation {

template<storm::dd::DdType DdType, typename ValueType>
Partition<DdType, ValueType>::Partition() : nextFreeBlockIndex(0) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
Partition<DdType, ValueType>::Partition(storm::dd::Add<DdType, ValueType> const& partitionAdd,
                                        std::pair<storm::expressions::Variable, storm::expressions::Variable> const& blockVariables, uint64_t numberOfBlocks,
                                        uint64_t nextFreeBlockIndex, boost::optional<storm::dd::Add<DdType, ValueType>> const& changedStates)
    : partition(partitionAdd),
      changedStates(changedStates),
      blockVariables(blockVariables),
      numberOfBlocks(numberOfBlocks),
      nextFreeBlockIndex(nextFreeBlockIndex) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
Partition<DdType, ValueType>::Partition(storm::dd::Bdd<DdType> const& partitionBdd,
                                        std::pair<storm::expressions::Variable, storm::expressions::Variable> const& blockVariables, uint64_t numberOfBlocks,
                                        uint64_t nextFreeBlockIndex, boost::optional<storm::dd::Bdd<DdType>> const& changedStates)
    : partition(partitionBdd),
      changedStates(changedStates),
      blockVariables(blockVariables),
      numberOfBlocks(numberOfBlocks),
      nextFreeBlockIndex(nextFreeBlockIndex) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
bool Partition<DdType, ValueType>::operator==(Partition<DdType, ValueType> const& other) const {
    return this->partition == other.partition && this->blockVariables == other.blockVariables && this->numberOfBlocks == other.numberOfBlocks &&
           this->nextFreeBlockIndex == other.nextFreeBlockIndex;
}

template<storm::dd::DdType DdType, typename ValueType>
Partition<DdType, ValueType> Partition<DdType, ValueType>::replacePartition(storm::dd::Add<DdType, ValueType> const& newPartitionAdd, uint64_t numberOfBlocks,
                                                                            uint64_t nextFreeBlockIndex,
                                                                            boost::optional<storm::dd::Add<DdType, ValueType>> const& changedStates) const {
    return Partition<DdType, ValueType>(newPartitionAdd, blockVariables, numberOfBlocks, nextFreeBlockIndex, changedStates);
}

template<storm::dd::DdType DdType, typename ValueType>
Partition<DdType, ValueType> Partition<DdType, ValueType>::replacePartition(storm::dd::Bdd<DdType> const& newPartitionBdd, uint64_t numberOfBlocks,
                                                                            uint64_t nextFreeBlockIndex,
                                                                            boost::optional<storm::dd::Bdd<DdType>> const& changedStates) const {
    return Partition<DdType, ValueType>(newPartitionBdd, blockVariables, numberOfBlocks, nextFreeBlockIndex, changedStates);
}

template<storm::dd::DdType DdType, typename ValueType>
boost::optional<std::pair<std::shared_ptr<storm::logic::Formula const>, std::shared_ptr<storm::logic::Formula const>>>
Partition<DdType, ValueType>::extractConstraintTargetFormulas(storm::logic::Formula const& formula) {
    boost::optional<std::pair<std::shared_ptr<storm::logic::Formula const>, std::shared_ptr<storm::logic::Formula const>>> result;
    if (formula.isProbabilityOperatorFormula()) {
        return extractConstraintTargetFormulas(formula.asProbabilityOperatorFormula().getSubformula());
    } else if (formula.isRewardOperatorFormula()) {
        return extractConstraintTargetFormulas(formula.asRewardOperatorFormula().getSubformula());
    } else if (formula.isUntilFormula()) {
        storm::logic::UntilFormula const& untilFormula = formula.asUntilFormula();
        storm::logic::FragmentSpecification propositional = storm::logic::propositional();
        if (untilFormula.getLeftSubformula().isInFragment(propositional) && untilFormula.getRightSubformula().isInFragment(propositional)) {
            result = std::pair<std::shared_ptr<storm::logic::Formula const>, std::shared_ptr<storm::logic::Formula const>>();
            result.get().first = untilFormula.getLeftSubformula().asSharedPointer();
            result.get().second = untilFormula.getRightSubformula().asSharedPointer();
        }
    } else if (formula.isEventuallyFormula()) {
        storm::logic::EventuallyFormula const& eventuallyFormula = formula.asEventuallyFormula();
        storm::logic::FragmentSpecification propositional = storm::logic::propositional();
        if (eventuallyFormula.getSubformula().isInFragment(propositional)) {
            result = std::pair<std::shared_ptr<storm::logic::Formula const>, std::shared_ptr<storm::logic::Formula const>>();
            result.get().first = std::make_shared<storm::logic::BooleanLiteralFormula>(true);
            result.get().second = eventuallyFormula.getSubformula().asSharedPointer();
        }
    }
    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
Partition<DdType, ValueType> Partition<DdType, ValueType>::create(storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                  storm::storage::BisimulationType const& bisimulationType,
                                                                  std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas,
                                                                  BisimulationOptions const& bisimulationOptions) {
    boost::optional<std::pair<std::shared_ptr<storm::logic::Formula const>, std::shared_ptr<storm::logic::Formula const>>> constraintTargetFormulas;
    if (bisimulationOptions.initialPartitionMode == InitialPartitionMode::Finer && formulas.size() == 1) {
        constraintTargetFormulas = extractConstraintTargetFormulas(*formulas.front());
    }

    if (constraintTargetFormulas && bisimulationType == storm::storage::BisimulationType::Strong) {
        return createDistanceBased(model, *constraintTargetFormulas.get().first, *constraintTargetFormulas.get().second);
    } else {
        return create(model, bisimulationType, PreservationInformation<DdType, ValueType>(model, formulas));
    }
}

template<storm::dd::DdType DdType, typename ValueType>
Partition<DdType, ValueType> Partition<DdType, ValueType>::create(storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                  storm::storage::BisimulationType const& bisimulationType,
                                                                  PreservationInformation<DdType, ValueType> const& preservationInformation) {
    std::vector<storm::expressions::Expression> expressionVector;
    for (auto const& expression : preservationInformation.getExpressions()) {
        expressionVector.emplace_back(expression);
    }

    return create(model, expressionVector, bisimulationType);
}

template<storm::dd::DdType DdType, typename ValueType>
Partition<DdType, ValueType> Partition<DdType, ValueType>::createDistanceBased(storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                               storm::logic::Formula const& constraintFormula,
                                                                               storm::logic::Formula const& targetFormula) {
    storm::modelchecker::SymbolicPropositionalModelChecker<storm::models::symbolic::Model<DdType, ValueType>> propositionalChecker(model);

    std::unique_ptr<storm::modelchecker::CheckResult> subresult = propositionalChecker.check(constraintFormula);
    storm::dd::Bdd<DdType> constraintStates = subresult->asSymbolicQualitativeCheckResult<DdType>().getTruthValuesVector();
    subresult = propositionalChecker.check(targetFormula);
    storm::dd::Bdd<DdType> targetStates = subresult->asSymbolicQualitativeCheckResult<DdType>().getTruthValuesVector();

    return createDistanceBased(model, constraintStates, targetStates);
}

template<storm::dd::DdType DdType, typename ValueType>
Partition<DdType, ValueType> Partition<DdType, ValueType>::createDistanceBased(storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                               storm::dd::Bdd<DdType> const& constraintStates,
                                                                               storm::dd::Bdd<DdType> const& targetStates) {
    STORM_LOG_TRACE("Creating distance-based partition.");

    std::pair<storm::expressions::Variable, storm::expressions::Variable> blockVariables = createBlockVariables(model);

    auto start = std::chrono::high_resolution_clock::now();

    // Set up the construction.
    storm::dd::DdManager<DdType>& manager = model.getManager();
    storm::dd::Bdd<DdType> partitionBdd = manager.getBddZero();
    storm::dd::Bdd<DdType> transitionMatrixBdd = model.getTransitionMatrix().notZero().existsAbstract(model.getNondeterminismVariables());
    storm::dd::Bdd<DdType> coveredStates = manager.getBddZero();
    uint64_t blockCount = 0;

    // Backward BFS.
    storm::dd::Bdd<DdType> backwardFrontier = targetStates;
    while (!backwardFrontier.isZero()) {
        partitionBdd |= backwardFrontier && manager.getEncoding(blockVariables.first, blockCount++, false);
        coveredStates |= backwardFrontier;
        backwardFrontier = backwardFrontier.inverseRelationalProduct(transitionMatrixBdd, model.getRowVariables(), model.getColumnVariables()) &&
                           !coveredStates && constraintStates;
    }

    // If there are states that cannot reach the target states (via only constraint states) at all, put them in one block.
    if (coveredStates != model.getReachableStates()) {
        partitionBdd |= (model.getReachableStates() && !coveredStates) && manager.getEncoding(blockVariables.first, blockCount++, false);
    }

    // Move the partition over to the primed variables.
    partitionBdd = partitionBdd.swapVariables(model.getRowColumnMetaVariablePairs());

    auto end = std::chrono::high_resolution_clock::now();
    STORM_LOG_INFO("Created distance and label-based initial partition in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                                                                            << "ms.");

    // Store the partition as an ADD only in the case of CUDD.
    if (DdType == storm::dd::DdType::CUDD) {
        return Partition<DdType, ValueType>(partitionBdd.template toAdd<ValueType>(), blockVariables, blockCount, blockCount);
    } else {
        return Partition<DdType, ValueType>(partitionBdd, blockVariables, blockCount, blockCount);
    }
}

template<storm::dd::DdType DdType, typename ValueType>
std::pair<storm::expressions::Variable, storm::expressions::Variable> Partition<DdType, ValueType>::createBlockVariables(
    storm::models::symbolic::Model<DdType, ValueType> const& model) {
    storm::dd::DdManager<DdType>& manager = model.getManager();

    uint64_t numberOfDdVariables = 0;
    for (auto const& metaVariable : model.getRowVariables()) {
        auto const& ddMetaVariable = manager.getMetaVariable(metaVariable);
        numberOfDdVariables += ddMetaVariable.getNumberOfDdVariables();
    }
    if (model.getType() == storm::models::ModelType::Mdp) {
        auto mdp = model.template as<storm::models::symbolic::Mdp<DdType, ValueType>>();
        for (auto const& metaVariable : mdp->getNondeterminismVariables()) {
            auto const& ddMetaVariable = manager.getMetaVariable(metaVariable);
            numberOfDdVariables += ddMetaVariable.getNumberOfDdVariables();
        }
    }

    return createBlockVariables(manager, numberOfDdVariables);
}

template<storm::dd::DdType DdType, typename ValueType>
Partition<DdType, ValueType> Partition<DdType, ValueType>::create(storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                  std::vector<storm::expressions::Expression> const& expressions,
                                                                  storm::storage::BisimulationType const& bisimulationType) {
    STORM_LOG_THROW(bisimulationType == storm::storage::BisimulationType::Strong, storm::exceptions::NotSupportedException,
                    "Currently only strong bisimulation is supported.");

    std::pair<storm::expressions::Variable, storm::expressions::Variable> blockVariables = createBlockVariables(model);

    std::vector<storm::dd::Bdd<DdType>> stateSets;
    for (auto const& expression : expressions) {
        stateSets.emplace_back(model.getStates(expression));
    }
    auto start = std::chrono::high_resolution_clock::now();
    std::pair<storm::dd::Bdd<DdType>, uint64_t> partitionBddAndBlockCount = createPartitionBdd(model.getManager(), model, stateSets, blockVariables.first);
    auto end = std::chrono::high_resolution_clock::now();
    STORM_LOG_INFO("Created label-based initial partition in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

    // Store the partition as an ADD only in the case of CUDD.
    if (DdType == storm::dd::DdType::CUDD) {
        return Partition<DdType, ValueType>(partitionBddAndBlockCount.first.template toAdd<ValueType>(), blockVariables, partitionBddAndBlockCount.second,
                                            partitionBddAndBlockCount.second);
    } else {
        return Partition<DdType, ValueType>(partitionBddAndBlockCount.first, blockVariables, partitionBddAndBlockCount.second,
                                            partitionBddAndBlockCount.second);
    }
}

template<storm::dd::DdType DdType, typename ValueType>
Partition<DdType, ValueType> Partition<DdType, ValueType>::createTrivialChoicePartition(
    storm::models::symbolic::NondeterministicModel<DdType, ValueType> const& model,
    std::pair<storm::expressions::Variable, storm::expressions::Variable> const& blockVariables) {
    storm::dd::Bdd<DdType> choicePartitionBdd =
        (!model.getIllegalMask() && model.getReachableStates()) && model.getManager().getEncoding(blockVariables.first, 0, false);

    // Store the partition as an ADD only in the case of CUDD.
    if (DdType == storm::dd::DdType::CUDD) {
        return Partition<DdType, ValueType>(choicePartitionBdd.template toAdd<ValueType>(), blockVariables, 1, 1);
    } else {
        return Partition<DdType, ValueType>(choicePartitionBdd, blockVariables, 1, 1);
    }
}

template<storm::dd::DdType DdType, typename ValueType>
uint64_t Partition<DdType, ValueType>::getNumberOfStates() const {
    return this->getStates().getNonZeroCount();
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Bdd<DdType> Partition<DdType, ValueType>::getStates() const {
    if (this->storedAsAdd()) {
        return this->asAdd().notZero().existsAbstract({this->getBlockVariable()});
    } else {
        return this->asBdd().existsAbstract({this->getBlockVariable()});
    }
}

template<storm::dd::DdType DdType, typename ValueType>
bool Partition<DdType, ValueType>::hasChangedStates() const {
    return static_cast<bool>(changedStates);
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> const& Partition<DdType, ValueType>::changedStatesAsAdd() const {
    return boost::get<storm::dd::Add<DdType, ValueType>>(changedStates.get());
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Bdd<DdType> const& Partition<DdType, ValueType>::changedStatesAsBdd() const {
    return boost::get<storm::dd::Bdd<DdType>>(changedStates.get());
}

template<storm::dd::DdType DdType, typename ValueType>
uint64_t Partition<DdType, ValueType>::getNumberOfBlocks() const {
    return numberOfBlocks;
}

template<storm::dd::DdType DdType, typename ValueType>
bool Partition<DdType, ValueType>::storedAsAdd() const {
    return partition.which() == 1;
}

template<storm::dd::DdType DdType, typename ValueType>
bool Partition<DdType, ValueType>::storedAsBdd() const {
    return partition.which() == 0;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> const& Partition<DdType, ValueType>::asAdd() const {
    return boost::get<storm::dd::Add<DdType, ValueType>>(partition);
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Bdd<DdType> const& Partition<DdType, ValueType>::asBdd() const {
    return boost::get<storm::dd::Bdd<DdType>>(partition);
}

template<storm::dd::DdType DdType, typename ValueType>
std::pair<storm::expressions::Variable, storm::expressions::Variable> const& Partition<DdType, ValueType>::getBlockVariables() const {
    return blockVariables;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::expressions::Variable const& Partition<DdType, ValueType>::getBlockVariable() const {
    return blockVariables.first;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::expressions::Variable const& Partition<DdType, ValueType>::getPrimedBlockVariable() const {
    return blockVariables.second;
}

template<storm::dd::DdType DdType, typename ValueType>
uint64_t Partition<DdType, ValueType>::getNextFreeBlockIndex() const {
    return nextFreeBlockIndex;
}

template<storm::dd::DdType DdType, typename ValueType>
uint64_t Partition<DdType, ValueType>::getNodeCount() const {
    if (this->storedAsBdd()) {
        return asBdd().getNodeCount();
    } else {
        return asAdd().getNodeCount();
    }
}

template<storm::dd::DdType DdType>
void enumerateBlocksRec(std::vector<storm::dd::Bdd<DdType>> const& stateSets, storm::dd::Bdd<DdType> const& currentStateSet, uint64_t offset,
                        storm::expressions::Variable const& blockVariable, std::function<void(storm::dd::Bdd<DdType> const&)> const& callback) {
    if (currentStateSet.isZero()) {
        return;
    }
    if (offset == stateSets.size()) {
        callback(currentStateSet);
    } else {
        enumerateBlocksRec(stateSets, currentStateSet && stateSets[offset], offset + 1, blockVariable, callback);
        enumerateBlocksRec(stateSets, currentStateSet && !stateSets[offset], offset + 1, blockVariable, callback);
    }
}

template<storm::dd::DdType DdType, typename ValueType>
std::pair<storm::dd::Bdd<DdType>, uint64_t> Partition<DdType, ValueType>::createPartitionBdd(storm::dd::DdManager<DdType> const& manager,
                                                                                             storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                                             std::vector<storm::dd::Bdd<DdType>> const& stateSets,
                                                                                             storm::expressions::Variable const& blockVariable) {
    uint64_t blockCount = 0;
    storm::dd::Bdd<DdType> partitionBdd = manager.getBddZero();

    // Enumerate all realizable blocks.
    enumerateBlocksRec<DdType>(stateSets, model.getReachableStates(), 0, blockVariable,
                               [&manager, &partitionBdd, &blockVariable, &blockCount](storm::dd::Bdd<DdType> const& stateSet) {
                                   partitionBdd |= (stateSet && manager.getEncoding(blockVariable, blockCount, false));
                                   blockCount++;
                               });

    // Move the partition over to the primed variables.
    partitionBdd = partitionBdd.swapVariables(model.getRowColumnMetaVariablePairs());

    return std::make_pair(partitionBdd, blockCount);
}

template<storm::dd::DdType DdType, typename ValueType>
std::pair<storm::expressions::Variable, storm::expressions::Variable> Partition<DdType, ValueType>::createBlockVariables(storm::dd::DdManager<DdType>& manager,
                                                                                                                         uint64_t numberOfDdVariables) {
    std::vector<storm::expressions::Variable> blockVariables;
    if (manager.hasMetaVariable("blocks")) {
        int64_t counter = 0;
        while (manager.hasMetaVariable("block" + std::to_string(counter))) {
            ++counter;
        }
        blockVariables = manager.addBitVectorMetaVariable("blocks" + std::to_string(counter), numberOfDdVariables, 2);
    } else {
        blockVariables = manager.addBitVectorMetaVariable("blocks", numberOfDdVariables, 2);
    }
    return std::make_pair(blockVariables[0], blockVariables[1]);
}

template class Partition<storm::dd::DdType::CUDD, double>;

template class Partition<storm::dd::DdType::Sylvan, double>;
template class Partition<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class Partition<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
