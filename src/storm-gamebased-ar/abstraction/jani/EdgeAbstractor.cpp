#include "storm-gamebased-ar/abstraction/jani/EdgeAbstractor.h"

#include <boost/iterator/transform_iterator.hpp>
#include <chrono>

#include "storm-gamebased-ar/abstraction/AbstractionInformation.h"
#include "storm-gamebased-ar/abstraction/BottomStateResult.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/jani/Edge.h"
#include "storm/storage/jani/EdgeDestination.h"
#include "storm/utility/macros.h"
#include "storm/utility/solver.h"

namespace storm::gbar {
namespace abstraction {
namespace jani {
template<storm::dd::DdType DdType, typename ValueType>
EdgeAbstractor<DdType, ValueType>::EdgeAbstractor(uint64_t edgeId, storm::jani::Edge const& edge, AbstractionInformation<DdType>& abstractionInformation,
                                                  std::shared_ptr<storm::utility::solver::SmtSolverFactory> const& smtSolverFactory, bool useDecomposition,
                                                  bool addPredicatesForValidBlocks, bool debug)
    : smtSolver(smtSolverFactory->create(abstractionInformation.getExpressionManager())),
      abstractionInformation(abstractionInformation),
      edgeId(edgeId),
      edge(edge),
      localExpressionInformation(abstractionInformation),
      evaluator(abstractionInformation.getExpressionManager()),
      relevantPredicatesAndVariables(),
      cachedDd(abstractionInformation.getDdManager().getBddZero(), 0),
      decisionVariables(),
      useDecomposition(useDecomposition),
      addPredicatesForValidBlocks(addPredicatesForValidBlocks),
      skipBottomStates(false),
      forceRecomputation(true),
      abstractGuard(abstractionInformation.getDdManager().getBddZero()),
      bottomStateAbstractor(abstractionInformation, {!edge.getGuard()}, smtSolverFactory),
      debug(debug) {
    // Make the second component of relevant predicates have the right size.
    relevantPredicatesAndVariables.second.resize(edge.getNumberOfDestinations());

    // Assert all constraints to enforce legal variable values.
    for (auto const& constraint : abstractionInformation.getConstraints()) {
        smtSolver->add(constraint);
        bottomStateAbstractor.constrain(constraint);
    }

    // Assert the guard of the command.
    smtSolver->add(edge.getGuard());

    // Construct assigned variables.
    for (auto const& destination : edge.getDestinations()) {
        for (auto const& assignment : destination.getOrderedAssignments()) {
            assignedVariables.insert(assignment.getExpressionVariable());
        }
    }

    // Log whether or not predicates are added to ensure valid blocks.
    if (this->addPredicatesForValidBlocks) {
        STORM_LOG_DEBUG("Adding more predicates to ensure valid blocks.");
    }
}

template<storm::dd::DdType DdType, typename ValueType>
void EdgeAbstractor<DdType, ValueType>::refine(std::vector<uint_fast64_t> const& predicates) {
    // Add all predicates to the variable partition.
    for (auto predicateIndex : predicates) {
        localExpressionInformation.addExpression(predicateIndex);
    }

    // Next, we check whether there is work to be done by recomputing the relevant predicates and checking
    // whether they changed.
    std::pair<std::set<uint_fast64_t>, std::vector<std::set<uint_fast64_t>>> newRelevantPredicates = this->computeRelevantPredicates();

    // Check whether we need to recompute the abstraction.
    bool relevantPredicatesChanged = this->relevantPredicatesChanged(newRelevantPredicates);
    if (relevantPredicatesChanged) {
        addMissingPredicates(newRelevantPredicates);
    }
    forceRecomputation |= relevantPredicatesChanged;

    // Refine bottom state abstractor. Note that this does not trigger a recomputation yet.
    bottomStateAbstractor.refine(predicates);
}

template<storm::dd::DdType DdType, typename ValueType>
storm::expressions::Expression const& EdgeAbstractor<DdType, ValueType>::getGuard() const {
    return edge.get().getGuard();
}

template<storm::dd::DdType DdType, typename ValueType>
uint64_t EdgeAbstractor<DdType, ValueType>::getNumberOfUpdates(uint64_t player1Choice) const {
    return edge.get().getNumberOfDestinations();
}

template<storm::dd::DdType DdType, typename ValueType>
std::map<storm::expressions::Variable, storm::expressions::Expression> EdgeAbstractor<DdType, ValueType>::getVariableUpdates(uint64_t auxiliaryChoice) const {
    return edge.get().getDestination(auxiliaryChoice).getAsVariableToExpressionMap();
}

template<storm::dd::DdType DdType, typename ValueType>
std::set<storm::expressions::Variable> const& EdgeAbstractor<DdType, ValueType>::getAssignedVariables() const {
    return assignedVariables;
}

template<storm::dd::DdType DdType, typename ValueType>
void EdgeAbstractor<DdType, ValueType>::recomputeCachedBdd() {
    if (useDecomposition) {
        recomputeCachedBddWithDecomposition();
    } else {
        recomputeCachedBddWithoutDecomposition();
    }
}

template<storm::dd::DdType DdType, typename ValueType>
void EdgeAbstractor<DdType, ValueType>::recomputeCachedBddWithDecomposition() {
    STORM_LOG_TRACE("Recomputing BDD for edge with id " << edgeId << " and guard " << edge.get().getGuard() << " using the decomposition.");
    auto start = std::chrono::high_resolution_clock::now();

    // compute a decomposition of the command
    //  * start with all relevant blocks: blocks of assignment variables and variables in the rhs of assignments
    //  * go through all assignments of all updates and merge relevant blocks that are related via an assignment
    //  * repeat this until nothing changes anymore
    //  * the resulting blocks are the decomposition

    // Start by constructing the relevant blocks.
    std::set<uint64_t> allRelevantBlocks;
    std::map<storm::expressions::Variable, uint64_t> variableToBlockIndex;
    for (auto const& destination : edge.get().getDestinations()) {
        for (auto const& assignment : destination.getOrderedAssignments().getAllAssignments()) {
            allRelevantBlocks.insert(localExpressionInformation.getBlockIndexOfVariable(assignment.getExpressionVariable()));

            auto rhsVariableBlocks = localExpressionInformation.getBlockIndicesOfVariables(assignment.getAssignedExpression().getVariables());
            allRelevantBlocks.insert(rhsVariableBlocks.begin(), rhsVariableBlocks.end());
        }
    }
    STORM_LOG_TRACE("Found " << allRelevantBlocks.size() << " relevant block(s).");

    // Create a block partition.
    std::vector<std::set<uint64_t>> relevantBlockPartition;
    std::map<storm::expressions::Variable, uint64_t> variableToLocalBlockIndex;
    uint64_t index = 0;
    for (auto const& blockIndex : allRelevantBlocks) {
        relevantBlockPartition.emplace_back(std::set<uint64_t>({blockIndex}));
        for (auto const& variable : localExpressionInformation.getVariableBlockWithIndex(blockIndex)) {
            variableToLocalBlockIndex[variable] = index;
        }
        ++index;
    }

    // Merge all blocks that are related via the right-hand side of assignments.
    for (auto const& destination : edge.get().getDestinations()) {
        for (auto const& assignment : destination.getOrderedAssignments().getAllAssignments()) {
            std::set<storm::expressions::Variable> rhsVariables = assignment.getAssignedExpression().getVariables();

            if (!rhsVariables.empty()) {
                uint64_t blockToKeep = variableToLocalBlockIndex.at(*rhsVariables.begin());
                for (auto const& variable : rhsVariables) {
                    uint64_t block = variableToLocalBlockIndex.at(variable);
                    if (block != blockToKeep) {
                        for (auto const& blockIndex : relevantBlockPartition[block]) {
                            for (auto const& variable : localExpressionInformation.getVariableBlockWithIndex(blockIndex)) {
                                variableToLocalBlockIndex[variable] = blockToKeep;
                            }
                        }
                        relevantBlockPartition[blockToKeep].insert(relevantBlockPartition[block].begin(), relevantBlockPartition[block].end());
                        relevantBlockPartition[block].clear();
                    }
                }
            }
        }
    }

    // Proceed by relating the blocks via assignment-variables and the expressions of their assigned expressions.
    bool changed = false;
    do {
        changed = false;
        for (auto const& destination : edge.get().getDestinations()) {
            for (auto const& assignment : destination.getOrderedAssignments().getAllAssignments()) {
                std::set<storm::expressions::Variable> rhsVariables = assignment.getAssignedExpression().getVariables();

                if (!rhsVariables.empty()) {
                    storm::expressions::Variable const& representativeVariable = *rhsVariables.begin();
                    uint64_t representativeBlock = variableToLocalBlockIndex.at(representativeVariable);
                    uint64_t assignmentVariableBlock = variableToLocalBlockIndex.at(assignment.getExpressionVariable());

                    // If the blocks are different, we merge them now
                    if (assignmentVariableBlock != representativeBlock) {
                        changed = true;

                        for (auto const& blockIndex : relevantBlockPartition[assignmentVariableBlock]) {
                            for (auto const& variable : localExpressionInformation.getVariableBlockWithIndex(blockIndex)) {
                                variableToLocalBlockIndex[variable] = representativeBlock;
                            }
                        }
                        relevantBlockPartition[representativeBlock].insert(relevantBlockPartition[assignmentVariableBlock].begin(),
                                                                           relevantBlockPartition[assignmentVariableBlock].end());
                        relevantBlockPartition[assignmentVariableBlock].clear();
                    }
                }
            }
        }
    } while (changed);

    // Now remove all blocks that are empty and obtain the partition.
    std::vector<std::set<uint64_t>> cleanedRelevantBlockPartition;
    for (auto& outerBlock : relevantBlockPartition) {
        if (!outerBlock.empty()) {
            cleanedRelevantBlockPartition.emplace_back();

            for (auto const& innerBlock : outerBlock) {
                if (!localExpressionInformation.getExpressionBlock(innerBlock).empty()) {
                    cleanedRelevantBlockPartition.back().insert(innerBlock);
                }
            }

            if (cleanedRelevantBlockPartition.back().empty()) {
                cleanedRelevantBlockPartition.pop_back();
            }
        }
    }
    relevantBlockPartition = std::move(cleanedRelevantBlockPartition);

    STORM_LOG_TRACE("Decomposition into " << relevantBlockPartition.size() << " blocks.");
    if (this->debug) {
        uint64_t blockIndex = 0;
        for (auto const& block : relevantBlockPartition) {
            STORM_LOG_TRACE("Predicates of block " << blockIndex << ":");
            std::set<uint64_t> blockPredicateIndices;
            for (auto const& innerBlock : block) {
                blockPredicateIndices.insert(localExpressionInformation.getExpressionBlock(innerBlock).begin(),
                                             localExpressionInformation.getExpressionBlock(innerBlock).end());
            }

            for (auto const& predicateIndex : blockPredicateIndices) {
                STORM_LOG_TRACE(abstractionInformation.get().getPredicateByIndex(predicateIndex));
            }

            ++blockIndex;
        }
    }

    std::set<storm::expressions::Variable> variablesContainedInGuard = edge.get().getGuard().getVariables();

    // Check whether we need to enumerate the guard. This is the case if the blocks related by the guard
    // are not contained within a single block of our decomposition.
    bool enumerateAbstractGuard = true;
    std::set<uint64_t> guardBlocks = localExpressionInformation.getBlockIndicesOfVariables(variablesContainedInGuard);
    for (auto const& block : relevantBlockPartition) {
        bool allContained = true;
        for (auto const& guardBlock : guardBlocks) {
            if (block.find(guardBlock) == block.end()) {
                allContained = false;
                break;
            }
        }
        if (allContained) {
            enumerateAbstractGuard = false;
        }
    }

    uint64_t numberOfSolutions = 0;
    uint64_t numberOfTotalSolutions = 0;

    // If we need to enumerate the guard, do it only once now.
    if (enumerateAbstractGuard) {
        std::set<uint64_t> relatedGuardPredicates = localExpressionInformation.getRelatedExpressions(variablesContainedInGuard);
        std::vector<storm::expressions::Variable> guardDecisionVariables;
        std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>> guardVariablesAndPredicates;
        for (auto const& element : relevantPredicatesAndVariables.first) {
            if (relatedGuardPredicates.find(element.second) != relatedGuardPredicates.end()) {
                guardDecisionVariables.push_back(element.first);
                guardVariablesAndPredicates.push_back(element);
            }
        }
        abstractGuard = this->getAbstractionInformation().getDdManager().getBddZero();
        smtSolver->allSat(guardDecisionVariables,
                          [this, &guardVariablesAndPredicates, &numberOfSolutions](storm::solver::SmtSolver::ModelReference const& model) {
                              abstractGuard |= getSourceStateBdd(model, guardVariablesAndPredicates);
                              ++numberOfSolutions;
                              return true;
                          });
        STORM_LOG_TRACE("Enumerated " << numberOfSolutions << " solutions for abstract guard.");

        // Now that we have the abstract guard, we can add it as an assertion to the solver before enumerating
        // the other solutions.

        // Create a new backtracking point before adding the guard.
        smtSolver->push();

        // Create the guard constraint.
        std::pair<std::vector<storm::expressions::Expression>, std::unordered_map<uint_fast64_t, storm::expressions::Variable>> result =
            abstractGuard.toExpression(this->getAbstractionInformation().getExpressionManager());

        // Then add it to the solver.
        for (auto const& expression : result.first) {
            smtSolver->add(expression);
        }

        // Finally associate the level variables with the predicates.
        for (auto const& indexVariablePair : result.second) {
            smtSolver->add(
                storm::expressions::iff(indexVariablePair.second, this->getAbstractionInformation().getPredicateForDdVariableIndex(indexVariablePair.first)));
        }
    }

    // Then enumerate the solutions for each of the blocks of the decomposition
    uint64_t usedNondeterminismVariables = 0;
    uint64_t blockCounter = 0;
    std::vector<storm::dd::Bdd<DdType>> blockBdds;
    for (auto const& block : relevantBlockPartition) {
        std::set<uint64_t> relevantPredicates;
        for (auto const& innerBlock : block) {
            relevantPredicates.insert(localExpressionInformation.getExpressionBlock(innerBlock).begin(),
                                      localExpressionInformation.getExpressionBlock(innerBlock).end());
        }

        if (relevantPredicates.empty()) {
            STORM_LOG_TRACE("Block does not contain relevant predicates, skipping it.");
            continue;
        }

        std::vector<storm::expressions::Variable> transitionDecisionVariables;
        std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>> sourceVariablesAndPredicates;
        for (auto const& element : relevantPredicatesAndVariables.first) {
            if (relevantPredicates.find(element.second) != relevantPredicates.end()) {
                transitionDecisionVariables.push_back(element.first);
                sourceVariablesAndPredicates.push_back(element);
            }
        }

        std::vector<std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>>> destinationVariablesAndPredicates;
        for (uint64_t destinationIndex = 0; destinationIndex < edge.get().getNumberOfDestinations(); ++destinationIndex) {
            destinationVariablesAndPredicates.emplace_back();
            for (auto const& assignment : edge.get().getDestination(destinationIndex).getOrderedAssignments().getAllAssignments()) {
                uint64_t assignmentVariableBlockIndex = localExpressionInformation.getBlockIndexOfVariable(assignment.getVariable().getExpressionVariable());

                if (block.find(assignmentVariableBlockIndex) != block.end()) {
                    std::set<uint64_t> const& assignmentVariableBlock = localExpressionInformation.getExpressionBlock(assignmentVariableBlockIndex);
                    for (auto const& element : relevantPredicatesAndVariables.second[destinationIndex]) {
                        if (assignmentVariableBlock.find(element.second) != assignmentVariableBlock.end()) {
                            destinationVariablesAndPredicates.back().push_back(element);
                            transitionDecisionVariables.push_back(element.first);
                        }
                    }
                }
            }
        }

        std::unordered_map<storm::dd::Bdd<DdType>, std::vector<storm::dd::Bdd<DdType>>> sourceToDistributionsMap;
        numberOfSolutions = 0;
        smtSolver->allSat(transitionDecisionVariables, [&sourceToDistributionsMap, this, &numberOfSolutions, &sourceVariablesAndPredicates,
                                                        &destinationVariablesAndPredicates](storm::solver::SmtSolver::ModelReference const& model) {
            sourceToDistributionsMap[getSourceStateBdd(model, sourceVariablesAndPredicates)].push_back(
                getDistributionBdd(model, destinationVariablesAndPredicates));
            ++numberOfSolutions;
            return true;
        });
        STORM_LOG_TRACE("Enumerated " << numberOfSolutions << " solutions for block " << blockCounter << ".");
        numberOfTotalSolutions += numberOfSolutions;

        // Now we search for the maximal number of choices of player 2 to determine how many DD variables we
        // need to encode the nondeterminism.
        uint_fast64_t maximalNumberOfChoices = 0;
        for (auto const& sourceDistributionsPair : sourceToDistributionsMap) {
            maximalNumberOfChoices = std::max(maximalNumberOfChoices, static_cast<uint_fast64_t>(sourceDistributionsPair.second.size()));
        }

        // We now compute how many variables we need to encode the choices. We add one to the maximal number of
        // choices to account for a possible transition to a bottom state.
        uint_fast64_t numberOfVariablesNeeded = (maximalNumberOfChoices > 1)
                                                    ? (static_cast<uint_fast64_t>(std::ceil(std::log2(maximalNumberOfChoices + (blockCounter == 0 ? 1 : 0)))))
                                                    : (blockCounter == 0 ? 1 : 0);

        // Finally, build overall result.
        storm::dd::Bdd<DdType> resultBdd = this->getAbstractionInformation().getDdManager().getBddZero();

        for (auto const& sourceDistributionsPair : sourceToDistributionsMap) {
            STORM_LOG_ASSERT(!sourceDistributionsPair.first.isZero(), "The source BDD must not be empty.");
            STORM_LOG_ASSERT(!sourceDistributionsPair.second.empty(), "The distributions must not be empty.");

            // We start with the distribution index of 1, because 0 is reserved for a potential bottom choice.
            uint_fast64_t distributionIndex = blockCounter == 0 ? 1 : 0;
            storm::dd::Bdd<DdType> allDistributions = this->getAbstractionInformation().getDdManager().getBddZero();
            for (auto const& distribution : sourceDistributionsPair.second) {
                allDistributions |= distribution && this->getAbstractionInformation().encodePlayer2Choice(
                                                        distributionIndex, usedNondeterminismVariables, usedNondeterminismVariables + numberOfVariablesNeeded);
                ++distributionIndex;
                STORM_LOG_ASSERT(!allDistributions.isZero(), "The BDD must not be empty.");
            }
            resultBdd |= sourceDistributionsPair.first && allDistributions;
            STORM_LOG_ASSERT(!resultBdd.isZero(), "The BDD must not be empty.");
        }
        usedNondeterminismVariables += numberOfVariablesNeeded;

        blockBdds.push_back(resultBdd);
        ++blockCounter;
    }

    if (enumerateAbstractGuard) {
        smtSolver->pop();
    }

    // multiply the results
    storm::dd::Bdd<DdType> resultBdd = getAbstractionInformation().getDdManager().getBddOne();
    for (auto const& blockBdd : blockBdds) {
        resultBdd &= blockBdd;
    }

    // If we did not explicitly enumerate the guard, we can construct it from the result BDD.
    if (!enumerateAbstractGuard) {
        std::set<storm::expressions::Variable> allVariables(getAbstractionInformation().getSuccessorVariables());
        auto player2Variables = getAbstractionInformation().getPlayer2VariableSet(usedNondeterminismVariables);
        allVariables.insert(player2Variables.begin(), player2Variables.end());
        auto auxVariables = getAbstractionInformation().getAuxVariableSet(0, getAbstractionInformation().getAuxVariableCount());
        allVariables.insert(auxVariables.begin(), auxVariables.end());

        std::set<storm::expressions::Variable> variablesToAbstract;
        std::set_intersection(allVariables.begin(), allVariables.end(), resultBdd.getContainedMetaVariables().begin(),
                              resultBdd.getContainedMetaVariables().end(), std::inserter(variablesToAbstract, variablesToAbstract.begin()));

        abstractGuard = resultBdd.existsAbstract(variablesToAbstract);
    } else {
        // Multiply the abstract guard as it can contain predicates that are not mentioned in the blocks.
        resultBdd &= abstractGuard;
    }

    // multiply with missing identities
    resultBdd &= computeMissingDestinationIdentities();

    // cache and return result
    resultBdd &= this->getAbstractionInformation().encodePlayer1Choice(edgeId, this->getAbstractionInformation().getPlayer1VariableCount());

    // Cache the result.
    cachedDd = GameBddResult<DdType>(resultBdd, usedNondeterminismVariables);

    auto end = std::chrono::high_resolution_clock::now();

    STORM_LOG_TRACE("Enumerated " << numberOfTotalSolutions << " solutions in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                                  << "ms.");
    forceRecomputation = false;
}

template<storm::dd::DdType DdType, typename ValueType>
void EdgeAbstractor<DdType, ValueType>::recomputeCachedBddWithoutDecomposition() {
    STORM_LOG_TRACE("Recomputing BDD for edge with id " << edgeId << " and guard " << edge.get().getGuard());
    auto start = std::chrono::high_resolution_clock::now();

    // Create a mapping from source state DDs to their distributions.
    std::unordered_map<storm::dd::Bdd<DdType>, std::vector<storm::dd::Bdd<DdType>>> sourceToDistributionsMap;
    uint64_t numberOfSolutions = 0;
    smtSolver->allSat(decisionVariables, [&sourceToDistributionsMap, this, &numberOfSolutions](storm::solver::SmtSolver::ModelReference const& model) {
        sourceToDistributionsMap[getSourceStateBdd(model, relevantPredicatesAndVariables.first)].push_back(
            getDistributionBdd(model, relevantPredicatesAndVariables.second));
        ++numberOfSolutions;
        return true;
    });

    // Now we search for the maximal number of choices of player 2 to determine how many DD variables we
    // need to encode the nondeterminism.
    uint_fast64_t maximalNumberOfChoices = 0;
    for (auto const& sourceDistributionsPair : sourceToDistributionsMap) {
        maximalNumberOfChoices = std::max(maximalNumberOfChoices, static_cast<uint_fast64_t>(sourceDistributionsPair.second.size()));
    }

    // We now compute how many variables we need to encode the choices. We add one to the maximal number of
    // choices to account for a possible transition to a bottom state.
    uint_fast64_t numberOfVariablesNeeded = static_cast<uint_fast64_t>(std::ceil(std::log2(maximalNumberOfChoices + 1)));

    // Finally, build overall result.
    storm::dd::Bdd<DdType> resultBdd = this->getAbstractionInformation().getDdManager().getBddZero();
    if (!skipBottomStates) {
        abstractGuard = this->getAbstractionInformation().getDdManager().getBddZero();
    }
    for (auto const& sourceDistributionsPair : sourceToDistributionsMap) {
        if (!skipBottomStates) {
            abstractGuard |= sourceDistributionsPair.first;
        }

        STORM_LOG_ASSERT(!sourceDistributionsPair.first.isZero(), "The source BDD must not be empty.");
        STORM_LOG_ASSERT(!sourceDistributionsPair.second.empty(), "The distributions must not be empty.");
        // We start with the distribution index of 1, becase 0 is reserved for a potential bottom choice.
        uint_fast64_t distributionIndex = 1;
        storm::dd::Bdd<DdType> allDistributions = this->getAbstractionInformation().getDdManager().getBddZero();
        for (auto const& distribution : sourceDistributionsPair.second) {
            allDistributions |= distribution && this->getAbstractionInformation().encodePlayer2Choice(distributionIndex, 0, numberOfVariablesNeeded);
            ++distributionIndex;
            STORM_LOG_ASSERT(!allDistributions.isZero(), "The BDD must not be empty.");
        }
        resultBdd |= sourceDistributionsPair.first && allDistributions;
        STORM_LOG_ASSERT(!resultBdd.isZero(), "The BDD must not be empty.");
    }

    resultBdd &= computeMissingDestinationIdentities();
    resultBdd &= this->getAbstractionInformation().encodePlayer1Choice(edgeId, this->getAbstractionInformation().getPlayer1VariableCount());
    STORM_LOG_ASSERT(sourceToDistributionsMap.empty() || !resultBdd.isZero(), "The BDD must not be empty, if there were distributions.");

    // Cache the result.
    cachedDd = GameBddResult<DdType>(resultBdd, numberOfVariablesNeeded);
    auto end = std::chrono::high_resolution_clock::now();

    STORM_LOG_TRACE("Enumerated " << numberOfSolutions << " solutions in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                                  << "ms.");
    forceRecomputation = false;
}

template<storm::dd::DdType DdType, typename ValueType>
std::pair<std::set<uint_fast64_t>, std::set<uint_fast64_t>> EdgeAbstractor<DdType, ValueType>::computeRelevantPredicates(
    storm::jani::OrderedAssignments const& assignments) const {
    std::pair<std::set<uint_fast64_t>, std::set<uint_fast64_t>> result;

    std::set<storm::expressions::Variable> assignedVariables;
    for (auto const& assignment : assignments.getAllAssignments()) {
        // Also, variables appearing on the right-hand side of an assignment are relevant for source state.
        auto const& rightHandSidePredicates = localExpressionInformation.getExpressionsUsingVariables(assignment.getAssignedExpression().getVariables());
        result.first.insert(rightHandSidePredicates.begin(), rightHandSidePredicates.end());

        // Variables that are being assigned are relevant for the successor state.
        storm::expressions::Variable const& assignedVariable = assignment.getExpressionVariable();
        auto const& leftHandSidePredicates = localExpressionInformation.getExpressionsUsingVariable(assignedVariable);
        result.second.insert(leftHandSidePredicates.begin(), leftHandSidePredicates.end());

        // Predicates that are indirectly related to the assigned variables are relevant for the source state (if requested).
        if (this->addPredicatesForValidBlocks) {
            auto const& assignedVariableBlock = localExpressionInformation.getRelatedExpressions(assignedVariable);
            result.first.insert(assignedVariableBlock.begin(), assignedVariableBlock.end());
        }
    }

    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
std::pair<std::set<uint_fast64_t>, std::vector<std::set<uint_fast64_t>>> EdgeAbstractor<DdType, ValueType>::computeRelevantPredicates() const {
    std::pair<std::set<uint_fast64_t>, std::vector<std::set<uint_fast64_t>>> result;

    // To start with, all predicates related to the guard are relevant source predicates.
    result.first = localExpressionInformation.getExpressionsUsingVariables(edge.get().getGuard().getVariables());

    // Then, we add the predicates that become relevant, because of some update.
    for (auto const& destination : edge.get().getDestinations()) {
        std::pair<std::set<uint_fast64_t>, std::set<uint_fast64_t>> relevantUpdatePredicates = computeRelevantPredicates(destination.getOrderedAssignments());
        result.first.insert(relevantUpdatePredicates.first.begin(), relevantUpdatePredicates.first.end());
        result.second.push_back(relevantUpdatePredicates.second);
    }

    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
bool EdgeAbstractor<DdType, ValueType>::relevantPredicatesChanged(
    std::pair<std::set<uint_fast64_t>, std::vector<std::set<uint_fast64_t>>> const& newRelevantPredicates) const {
    if (newRelevantPredicates.first.size() > relevantPredicatesAndVariables.first.size()) {
        return true;
    }

    for (uint_fast64_t index = 0; index < edge.get().getNumberOfDestinations(); ++index) {
        if (newRelevantPredicates.second[index].size() > relevantPredicatesAndVariables.second[index].size()) {
            return true;
        }
    }

    return false;
}

template<storm::dd::DdType DdType, typename ValueType>
void EdgeAbstractor<DdType, ValueType>::addMissingPredicates(
    std::pair<std::set<uint_fast64_t>, std::vector<std::set<uint_fast64_t>>> const& newRelevantPredicates) {
    // Determine and add new relevant source predicates.
    std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>> newSourceVariables =
        this->getAbstractionInformation().declareNewVariables(relevantPredicatesAndVariables.first, newRelevantPredicates.first);
    for (auto const& element : newSourceVariables) {
        allRelevantPredicates.insert(element.second);
        smtSolver->add(storm::expressions::iff(element.first, this->getAbstractionInformation().getPredicateByIndex(element.second)));
        decisionVariables.push_back(element.first);
    }

    // Insert the new variables into the record of relevant source variables.
    relevantPredicatesAndVariables.first.insert(relevantPredicatesAndVariables.first.end(), newSourceVariables.begin(), newSourceVariables.end());
    std::sort(relevantPredicatesAndVariables.first.begin(), relevantPredicatesAndVariables.first.end(),
              [](std::pair<storm::expressions::Variable, uint_fast64_t> const& first, std::pair<storm::expressions::Variable, uint_fast64_t> const& second) {
                  return first.second < second.second;
              });

    // Do the same for every update.
    for (uint_fast64_t index = 0; index < edge.get().getNumberOfDestinations(); ++index) {
        std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>> newSuccessorVariables =
            this->getAbstractionInformation().declareNewVariables(relevantPredicatesAndVariables.second[index], newRelevantPredicates.second[index]);
        for (auto const& element : newSuccessorVariables) {
            allRelevantPredicates.insert(element.second);
            smtSolver->add(storm::expressions::iff(element.first, this->getAbstractionInformation()
                                                                      .getPredicateByIndex(element.second)
                                                                      .substitute(edge.get().getDestination(index).getAsVariableToExpressionMap())));
            decisionVariables.push_back(element.first);
        }

        relevantPredicatesAndVariables.second[index].insert(relevantPredicatesAndVariables.second[index].end(), newSuccessorVariables.begin(),
                                                            newSuccessorVariables.end());
        std::sort(relevantPredicatesAndVariables.second[index].begin(), relevantPredicatesAndVariables.second[index].end(),
                  [](std::pair<storm::expressions::Variable, uint_fast64_t> const& first,
                     std::pair<storm::expressions::Variable, uint_fast64_t> const& second) { return first.second < second.second; });
    }
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Bdd<DdType> EdgeAbstractor<DdType, ValueType>::getSourceStateBdd(
    storm::solver::SmtSolver::ModelReference const& model,
    std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>> const& variablePredicates) const {
    storm::dd::Bdd<DdType> result = this->getAbstractionInformation().getDdManager().getBddOne();
    for (auto variableIndexPairIt = variablePredicates.rbegin(), variableIndexPairIte = variablePredicates.rend(); variableIndexPairIt != variableIndexPairIte;
         ++variableIndexPairIt) {
        auto const& variableIndexPair = *variableIndexPairIt;
        if (model.getBooleanValue(variableIndexPair.first)) {
            result &= this->getAbstractionInformation().encodePredicateAsSource(variableIndexPair.second);
        } else {
            result &= !this->getAbstractionInformation().encodePredicateAsSource(variableIndexPair.second);
        }
    }

    STORM_LOG_ASSERT(!result.isZero(), "Source must not be empty.");
    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Bdd<DdType> EdgeAbstractor<DdType, ValueType>::getDistributionBdd(
    storm::solver::SmtSolver::ModelReference const& model,
    std::vector<std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>>> const& variablePredicates) const {
    storm::dd::Bdd<DdType> result = this->getAbstractionInformation().getDdManager().getBddZero();

    for (uint_fast64_t destinationIndex = 0; destinationIndex < edge.get().getNumberOfDestinations(); ++destinationIndex) {
        storm::dd::Bdd<DdType> updateBdd = this->getAbstractionInformation().getDdManager().getBddOne();

        // Translate block variables for this update into a successor block.
        for (auto variableIndexPairIt = variablePredicates[destinationIndex].rbegin(), variableIndexPairIte = variablePredicates[destinationIndex].rend();
             variableIndexPairIt != variableIndexPairIte; ++variableIndexPairIt) {
            auto const& variableIndexPair = *variableIndexPairIt;
            if (model.getBooleanValue(variableIndexPair.first)) {
                updateBdd &= this->getAbstractionInformation().encodePredicateAsSuccessor(variableIndexPair.second);
            } else {
                updateBdd &= !this->getAbstractionInformation().encodePredicateAsSuccessor(variableIndexPair.second);
            }
        }

        updateBdd &= this->getAbstractionInformation().encodeAux(destinationIndex, 0, this->getAbstractionInformation().getAuxVariableCount());
        result |= updateBdd;
    }

    STORM_LOG_ASSERT(!result.isZero(), "Distribution must not be empty.");
    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Bdd<DdType> EdgeAbstractor<DdType, ValueType>::computeMissingDestinationIdentities() const {
    storm::dd::Bdd<DdType> result = this->getAbstractionInformation().getDdManager().getBddZero();

    for (uint_fast64_t destinationIndex = 0; destinationIndex < edge.get().getNumberOfDestinations(); ++destinationIndex) {
        // Compute the identities that are missing for this update.
        auto updateRelevantIt = relevantPredicatesAndVariables.second[destinationIndex].rbegin();
        auto updateRelevantIte = relevantPredicatesAndVariables.second[destinationIndex].rend();

        storm::dd::Bdd<DdType> updateIdentity = this->getAbstractionInformation().getDdManager().getBddOne();
        for (uint_fast64_t predicateIndex = this->getAbstractionInformation().getNumberOfPredicates() - 1;; --predicateIndex) {
            if (updateRelevantIt == updateRelevantIte || updateRelevantIt->second != predicateIndex) {
                updateIdentity &= this->getAbstractionInformation().getPredicateIdentity(predicateIndex);
            } else {
                ++updateRelevantIt;
            }

            if (predicateIndex == 0) {
                break;
            }
        }

        result |= updateIdentity && this->getAbstractionInformation().encodeAux(destinationIndex, 0, this->getAbstractionInformation().getAuxVariableCount());
    }
    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
GameBddResult<DdType> EdgeAbstractor<DdType, ValueType>::abstract() {
    if (forceRecomputation) {
        this->recomputeCachedBdd();
    } else {
        cachedDd.bdd &= computeMissingDestinationIdentities();
    }

    STORM_LOG_TRACE("Edge produces " << cachedDd.bdd.getNonZeroCount() << " transitions.");

    return cachedDd;
}

template<storm::dd::DdType DdType, typename ValueType>
BottomStateResult<DdType> EdgeAbstractor<DdType, ValueType>::getBottomStateTransitions(
    storm::dd::Bdd<DdType> const& reachableStates, uint_fast64_t numberOfPlayer2Variables,
    boost::optional<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& locationVariables) {
    STORM_LOG_TRACE("Computing bottom state transitions of edge with index " << edgeId << ".");
    BottomStateResult<DdType> result(this->getAbstractionInformation().getDdManager().getBddZero(),
                                     this->getAbstractionInformation().getDdManager().getBddZero());

    // If the guard of this command is a predicate, there are not bottom states/transitions.
    if (skipBottomStates) {
        STORM_LOG_TRACE("Skipping bottom state computation for this edge.");
        return result;
    }

    storm::dd::Bdd<DdType> reachableStatesWithEdge = reachableStates && abstractGuard;
    // needed?
    //                if (locationVariables) {
    //                    reachableStatesWithEdge = (reachableStates && abstractGuard &&
    //                    this->getAbstractionInformation().encodeLocation(locationVariables.get().first,
    //                    edge.get().getSourceLocationIndex())).existsAbstract(this->getAbstractionInformation().getSourceLocationVariables());
    //                } else {
    //                    reachableStatesWithEdge = (reachableStates &&
    //                    abstractGuard).existsAbstract(this->getAbstractionInformation().getSourceLocationVariables());
    //                }

    // Use the state abstractor to compute the set of abstract states that has this command enabled but
    // still has a transition to a bottom state.
    bottomStateAbstractor.constrain(reachableStatesWithEdge);
    if (locationVariables) {
        result.states = bottomStateAbstractor.getAbstractStates() && reachableStatesWithEdge &&
                        this->getAbstractionInformation().encodeLocation(locationVariables.get().first, edge.get().getSourceLocationIndex());
    } else {
        result.states = bottomStateAbstractor.getAbstractStates() && reachableStatesWithEdge;
    }

    // If the result is empty one time, we can skip the bottom state computation from now on.
    if (result.states.isZero()) {
        skipBottomStates = true;
    }

    // Now equip all these states with an actual transition to a bottom state.
    result.transitions =
        result.states && this->getAbstractionInformation().getAllPredicateIdentities() && this->getAbstractionInformation().getBottomStateBdd(false, false);

    // Mark the states as bottom states.
    result.states &= this->getAbstractionInformation().getBottomStateBdd(true, false);

    // Add the command encoding and the next free player 2 encoding.
    result.transitions &= this->getAbstractionInformation().encodePlayer1Choice(edgeId, this->getAbstractionInformation().getPlayer1VariableCount()) &&
                          this->getAbstractionInformation().encodePlayer2Choice(0, 0, numberOfPlayer2Variables) &&
                          this->getAbstractionInformation().encodeAux(0, 0, this->getAbstractionInformation().getAuxVariableCount());

    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> EdgeAbstractor<DdType, ValueType>::getEdgeDecoratorAdd(
    boost::optional<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& locationVariables) const {
    storm::dd::Add<DdType, ValueType> result = this->getAbstractionInformation().getDdManager().template getAddZero<ValueType>();
    for (uint_fast64_t destinationIndex = 0; destinationIndex < edge.get().getNumberOfDestinations(); ++destinationIndex) {
        storm::dd::Add<DdType, ValueType> tmp =
            this->getAbstractionInformation()
                .encodeAux(destinationIndex, 0, this->getAbstractionInformation().getAuxVariableCount())
                .template toAdd<ValueType>() *
            this->getAbstractionInformation().getDdManager().getConstant(evaluator.asRational(edge.get().getDestination(destinationIndex).getProbability()));
        if (locationVariables) {
            tmp *= this->getAbstractionInformation()
                       .encodeLocation(locationVariables.get().second, edge.get().getDestination(destinationIndex).getLocationIndex())
                       .template toAdd<ValueType>();
        }
        result += tmp;
    }

    storm::dd::Add<DdType, ValueType> tmp = this->getAbstractionInformation().getDdManager().template getAddOne<ValueType>();
    if (locationVariables) {
        tmp *= this->getAbstractionInformation().encodeLocation(locationVariables.get().first, edge.get().getSourceLocationIndex()).template toAdd<ValueType>();
    }
    tmp *=
        this->getAbstractionInformation().encodePlayer1Choice(edgeId, this->getAbstractionInformation().getPlayer1VariableCount()).template toAdd<ValueType>();

    result *= tmp;

    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::jani::Edge const& EdgeAbstractor<DdType, ValueType>::getConcreteEdge() const {
    return edge.get();
}

template<storm::dd::DdType DdType, typename ValueType>
AbstractionInformation<DdType> const& EdgeAbstractor<DdType, ValueType>::getAbstractionInformation() const {
    return abstractionInformation.get();
}

template<storm::dd::DdType DdType, typename ValueType>
AbstractionInformation<DdType>& EdgeAbstractor<DdType, ValueType>::getAbstractionInformation() {
    return abstractionInformation.get();
}

template<storm::dd::DdType DdType, typename ValueType>
void EdgeAbstractor<DdType, ValueType>::notifyGuardIsPredicate() {
    skipBottomStates = true;
}

template class EdgeAbstractor<storm::dd::DdType::CUDD, double>;
template class EdgeAbstractor<storm::dd::DdType::Sylvan, double>;
template class EdgeAbstractor<storm::dd::DdType::Sylvan, storm::RationalNumber>;
}  // namespace jani
}  // namespace abstraction
}  // namespace storm::gbar
