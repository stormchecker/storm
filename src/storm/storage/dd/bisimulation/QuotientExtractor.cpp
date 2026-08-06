#include "storm/storage/dd/bisimulation/QuotientExtractor.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomma"
#include <gtl/phmap.hpp>
#pragma clang diagnostic pop
#include <numeric>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/symbolic/Ctmc.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/MarkovAutomaton.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/BisimulationSettings.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/dd/DdManager.h"

namespace storm {
namespace dd {
namespace bisimulation {

template<storm::dd::DdType DdType>
class InternalRepresentativeComputer;

template<storm::dd::DdType DdType>
class InternalRepresentativeComputerBase {
   public:
    InternalRepresentativeComputerBase(storm::dd::Bdd<DdType> const& partitionBdd, std::set<storm::expressions::Variable> const& rowVariables)
        : rowVariables(rowVariables), partitionBdd(partitionBdd) {
        ddManager = &partitionBdd.getDdManager();
        internalDdManager = &ddManager->getInternalDdManager();

        // Create state variables cube.
        this->rowVariablesCube = ddManager->getBddOne();
        for (auto const& var : rowVariables) {
            auto const& metaVariable = ddManager->getMetaVariable(var);
            this->rowVariablesCube &= metaVariable.getCube();
        }
    }

   protected:
    storm::dd::DdManager<DdType> const* ddManager;
    storm::dd::InternalDdManager<DdType> const* internalDdManager;

    std::set<storm::expressions::Variable> const& rowVariables;
    storm::dd::Bdd<DdType> rowVariablesCube;

    storm::dd::Bdd<DdType> partitionBdd;
};

template<>
class InternalRepresentativeComputer<storm::dd::DdType::CUDD> : public InternalRepresentativeComputerBase<storm::dd::DdType::CUDD> {
   public:
    InternalRepresentativeComputer(storm::dd::Bdd<storm::dd::DdType::CUDD> const& partitionBdd, std::set<storm::expressions::Variable> const& rowVariables)
        : InternalRepresentativeComputerBase<storm::dd::DdType::CUDD>(partitionBdd, rowVariables) {
#ifdef STORM_HAVE_CUDD
        this->ddman = this->internalDdManager->getCuddManager().getManager();
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with CUDD support.");
#endif
    }

    storm::dd::Bdd<storm::dd::DdType::CUDD> getRepresentatives() {
#ifdef STORM_HAVE_CUDD
        return storm::dd::Bdd<storm::dd::DdType::CUDD>(
            *this->ddManager,
            storm::dd::InternalBdd<storm::dd::DdType::CUDD>(
                this->internalDdManager,
                cudd::BDD(this->internalDdManager->getCuddManager(), this->getRepresentativesRec(this->partitionBdd.getInternalBdd().getCuddDdNode(),
                                                                                                 this->rowVariablesCube.getInternalBdd().getCuddDdNode()))),
            this->rowVariables);
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with CUDD support.");
#endif
    }

   private:
#ifdef STORM_HAVE_CUDD
    DdNodePtr getRepresentativesRec(DdNodePtr partitionNode, DdNodePtr stateVariablesCube) {
        if (partitionNode == Cudd_ReadLogicZero(ddman)) {
            return partitionNode;
        }

        // If we visited the node before, there is no block that we still need to cover.
        if (visitedNodes.find(partitionNode) != visitedNodes.end()) {
            return Cudd_ReadLogicZero(ddman);
        }

        // If we hit a block variable and have not yet terminated the DFS earlier, it means we have a new representative.
        if (Cudd_IsConstant(stateVariablesCube)) {
            visitedNodes.emplace(partitionNode, true);
            return Cudd_ReadOne(ddman);
        } else {
            bool skipped = false;
            DdNodePtr elsePartitionNode;
            DdNodePtr thenPartitionNode;
            if (Cudd_NodeReadIndex(partitionNode) == Cudd_NodeReadIndex(stateVariablesCube)) {
                elsePartitionNode = Cudd_E(partitionNode);
                thenPartitionNode = Cudd_T(partitionNode);

                if (Cudd_IsComplement(partitionNode)) {
                    elsePartitionNode = Cudd_Not(elsePartitionNode);
                    thenPartitionNode = Cudd_Not(thenPartitionNode);
                }
            } else {
                elsePartitionNode = thenPartitionNode = partitionNode;
                skipped = true;
            }

            if (!skipped) {
                visitedNodes.emplace(partitionNode, true);
            }

            // Otherwise, recursively proceed with DFS.
            DdNodePtr elseResult = getRepresentativesRec(elsePartitionNode, Cudd_T(stateVariablesCube));
            Cudd_Ref(elseResult);

            DdNodePtr thenResult = nullptr;
            if (!skipped) {
                thenResult = getRepresentativesRec(thenPartitionNode, Cudd_T(stateVariablesCube));
                Cudd_Ref(thenResult);

                if (thenResult == elseResult) {
                    Cudd_Deref(elseResult);
                    Cudd_Deref(thenResult);
                    return elseResult;
                } else {
                    bool complement = Cudd_IsComplement(thenResult);
                    auto result = cuddUniqueInter(ddman, Cudd_NodeReadIndex(stateVariablesCube), Cudd_Regular(thenResult),
                                                  complement ? Cudd_Not(elseResult) : elseResult);
                    Cudd_Deref(elseResult);
                    Cudd_Deref(thenResult);
                    return complement ? Cudd_Not(result) : result;
                }
            } else {
                DdNodePtr result;
                if (elseResult == Cudd_ReadLogicZero(ddman)) {
                    result = elseResult;
                } else {
                    result = Cudd_Not(cuddUniqueInter(ddman, Cudd_NodeReadIndex(stateVariablesCube), Cudd_ReadOne(ddman), Cudd_Not(elseResult)));
                }
                Cudd_Deref(elseResult);
                return result;
            }
        }
    }

    ::DdManager* ddman;
    gtl::flat_hash_map<DdNode const*, bool> visitedNodes;
#endif
};

template<>
class InternalRepresentativeComputer<storm::dd::DdType::Sylvan> : public InternalRepresentativeComputerBase<storm::dd::DdType::Sylvan> {
   public:
    InternalRepresentativeComputer(storm::dd::Bdd<storm::dd::DdType::Sylvan> const& partitionBdd, std::set<storm::expressions::Variable> const& rowVariables)
        : InternalRepresentativeComputerBase<storm::dd::DdType::Sylvan>(partitionBdd, rowVariables) {
#ifndef STORM_HAVE_SYLVAN
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
#endif
        // Intentionally left empty.
    }

    storm::dd::Bdd<storm::dd::DdType::Sylvan> getRepresentatives() {
#ifdef STORM_HAVE_SYLVAN
        return storm::dd::Bdd<storm::dd::DdType::Sylvan>(
            *this->ddManager,
            storm::dd::InternalBdd<storm::dd::DdType::Sylvan>(
                this->internalDdManager, sylvan::Bdd(this->getRepresentativesRec(this->partitionBdd.getInternalBdd().getSylvanBdd().GetBDD(),
                                                                                 this->rowVariablesCube.getInternalBdd().getSylvanBdd().GetBDD()))),
            this->rowVariables);
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
#endif
    }

   private:
#ifdef STORM_HAVE_SYLVAN
    BDD getRepresentativesRec(BDD partitionNode, BDD stateVariablesCube) {
        if (partitionNode == sylvan_false) {
            return sylvan_false;
        }

        // If we visited the node before, there is no block that we still need to cover.
        if (visitedNodes.find(partitionNode) != visitedNodes.end()) {
            return sylvan_false;
        }

        // If we hit a block variable and have not yet terminated the DFS earlier, it means we have a new representative.
        if (sylvan_isconst(stateVariablesCube)) {
            visitedNodes.emplace(partitionNode, true);
            return sylvan_true;
        } else {
            bool skipped = false;
            BDD elsePartitionNode;
            BDD thenPartitionNode;
            if (sylvan_bdd_matches_variable_index(partitionNode, sylvan_var(stateVariablesCube))) {
                elsePartitionNode = sylvan_low(partitionNode);
                thenPartitionNode = sylvan_high(partitionNode);
            } else {
                elsePartitionNode = thenPartitionNode = partitionNode;
                skipped = true;
            }

            if (!skipped) {
                visitedNodes.emplace(partitionNode, true);
            }

            // Otherwise, recursively proceed with DFS.
            BDD elseResult = getRepresentativesRec(elsePartitionNode, sylvan_high(stateVariablesCube));
            mtbdd_refs_push(elseResult);

            BDD thenResult;
            if (!skipped) {
                thenResult = getRepresentativesRec(thenPartitionNode, sylvan_high(stateVariablesCube));
                mtbdd_refs_push(thenResult);

                if (thenResult == elseResult) {
                    mtbdd_refs_pop(2);
                    return elseResult;
                } else {
                    auto result = sylvan_makenode(sylvan_var(stateVariablesCube), elseResult, thenResult);
                    mtbdd_refs_pop(2);
                    return result;
                }
            } else {
                BDD result;
                if (elseResult == sylvan_false) {
                    result = elseResult;
                } else {
                    result = sylvan_makenode(sylvan_var(stateVariablesCube), elseResult, sylvan_false);
                }
                mtbdd_refs_pop(1);
                return result;
            }
        }
    }

    gtl::flat_hash_map<BDD, bool> visitedNodes;
#endif
};

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType = ValueType>
class InternalSparseQuotientExtractor;

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType = ValueType>
class InternalSparseQuotientExtractorBase {
   public:
    InternalSparseQuotientExtractorBase(storm::models::symbolic::Model<DdType, ValueType> const& model, storm::dd::Bdd<DdType> const& partitionBdd,
                                        storm::expressions::Variable const& blockVariable, uint64_t numberOfBlocks,
                                        storm::dd::Bdd<DdType> const& representatives)
        : model(model),
          manager(model.getManager()),
          isNondeterministic(false),
          partitionBdd(partitionBdd),
          numberOfBlocks(numberOfBlocks),
          blockVariable(blockVariable),
          representatives(representatives),
          matrixEntriesCreated(false) {
        // Create cubes.
        rowVariablesCube = manager.getBddOne();
        for (auto const& variable : model.getRowVariables()) {
            auto const& ddMetaVariable = manager.getMetaVariable(variable);
            rowVariablesCube &= ddMetaVariable.getCube();
        }
        columnVariablesCube = manager.getBddOne();
        for (auto const& variable : model.getColumnVariables()) {
            auto const& ddMetaVariable = manager.getMetaVariable(variable);
            columnVariablesCube &= ddMetaVariable.getCube();
        }
        nondeterminismVariablesCube = manager.getBddOne();
        for (auto const& variable : model.getNondeterminismVariables()) {
            auto const& ddMetaVariable = manager.getMetaVariable(variable);
            nondeterminismVariablesCube &= ddMetaVariable.getCube();
        }
        allSourceVariablesCube = rowVariablesCube && nondeterminismVariablesCube;
        isNondeterministic = !nondeterminismVariablesCube.isOne();

        // Create ODDs.
        this->odd = representatives.createOdd();
        if (this->isNondeterministic) {
            this->nondeterminismOdd = (model.getQualitativeTransitionMatrix().existsAbstract(model.getColumnVariables()) && this->representatives).createOdd();
        }

        STORM_LOG_TRACE("Partition has " << partitionBdd.existsAbstract(model.getRowVariables()).getNonZeroCount() << " states in " << this->numberOfBlocks
                                         << " blocks.");
    }

    virtual ~InternalSparseQuotientExtractorBase() = default;

    storm::storage::SparseMatrix<ExportValueType> extractTransitionMatrix(storm::dd::Add<DdType, ValueType> const& transitionMatrix) {
        return extractMatrixInternal(transitionMatrix);
    }

    std::vector<ExportValueType> extractStateVector(storm::dd::Add<DdType, ValueType> const& vector) {
        return extractVectorInternal(vector, this->rowVariablesCube, this->odd);
    }

    std::vector<ExportValueType> extractStateActionVector(storm::dd::Add<DdType, ValueType> const& vector) {
        if (!this->isNondeterministic) {
            return extractStateVector(vector);
        } else {
            STORM_LOG_ASSERT(!this->rowPermutation.empty(), "Expected proper row permutation.");
            std::vector<ExportValueType> valueVector = extractVectorInternal(vector, this->allSourceVariablesCube, this->nondeterminismOdd);

            // Reorder the values according to the known row permutation.
            std::vector<ExportValueType> reorderedValues(valueVector.size());
            for (uint64_t pos = 0; pos < valueVector.size(); ++pos) {
                reorderedValues[pos] = valueVector[rowPermutation[pos]];
            }
            return reorderedValues;
        }
    }

    storm::storage::BitVector extractSetAll(storm::dd::Bdd<DdType> const& set) {
        return (set && representatives).toVector(this->odd);
    }

    storm::storage::BitVector extractSetExists(storm::dd::Bdd<DdType> const& set) {
        return ((set && partitionBdd).existsAbstract(model.getRowVariables()) && partitionBdd && representatives)
            .existsAbstract({this->blockVariable})
            .toVector(this->odd);
    }

   protected:
    virtual storm::storage::SparseMatrix<ExportValueType> extractMatrixInternal(storm::dd::Add<DdType, ValueType> const& matrix) = 0;

    virtual std::vector<ExportValueType> extractVectorInternal(storm::dd::Add<DdType, ValueType> const& vector, storm::dd::Bdd<DdType> const& variablesCube,
                                                               storm::dd::Odd const& odd) = 0;

    storm::storage::SparseMatrix<ExportValueType> createMatrixFromEntries() {
        for (auto& row : matrixEntries) {
            std::sort(row.begin(), row.end(),
                      [](storm::storage::MatrixEntry<uint_fast64_t, ExportValueType> const& a,
                         storm::storage::MatrixEntry<uint_fast64_t, ExportValueType> const& b) { return a.getColumn() < b.getColumn(); });
        }

        rowPermutation = std::vector<uint64_t>(matrixEntries.size());
        std::iota(rowPermutation.begin(), rowPermutation.end(), 0ull);
        if (this->isNondeterministic) {
            std::stable_sort(rowPermutation.begin(), rowPermutation.end(),
                             [this](uint64_t first, uint64_t second) { return this->rowToState[first] < this->rowToState[second]; });
        }

        uint64_t rowCounter = 0;
        uint64_t lastState = this->isNondeterministic ? rowToState[rowPermutation.front()] : 0;
        storm::storage::SparseMatrixBuilder<ExportValueType> builder(matrixEntries.size(), this->numberOfBlocks, 0, true, this->isNondeterministic);
        if (this->isNondeterministic) {
            builder.newRowGroup(0);
        }
        for (auto& rowIdx : rowPermutation) {
            // For nondeterministic models, open a new row group.
            if (this->isNondeterministic && rowToState[rowIdx] != lastState) {
                builder.newRowGroup(rowCounter);
                lastState = rowToState[rowIdx];
            }

            auto& row = matrixEntries[rowIdx];
            for (auto const& entry : row) {
                builder.addNextValue(rowCounter, entry.getColumn(), entry.getValue());
            }

            // Free storage for row.
            row.clear();
            row.shrink_to_fit();

            ++rowCounter;
        }

        rowToState.clear();
        rowToState.shrink_to_fit();
        matrixEntries.clear();
        matrixEntries.shrink_to_fit();

        return builder.build();
    }

    void addMatrixEntry(uint64_t row, uint64_t column, ExportValueType const& value) {
        this->matrixEntries[row].emplace_back(column, value);
    }

    void createMatrixEntryStorage() {
        if (matrixEntriesCreated) {
            matrixEntries.clear();
            if (isNondeterministic) {
                rowToState.clear();
            }
        }
        matrixEntries.resize(this->isNondeterministic ? nondeterminismOdd.getTotalOffset() : odd.getTotalOffset());
        if (isNondeterministic) {
            rowToState.resize(matrixEntries.size());
        }
    }

    void assignRowToState(uint64_t row, uint64_t state) {
        rowToState[row] = state;
    }

    storm::models::symbolic::Model<DdType, ValueType> const& model;

    // The manager responsible for the DDs.
    storm::dd::DdManager<DdType> const& manager;

    // A flag that stores whether we need to take care of nondeterminism.
    bool isNondeterministic;

    // Useful cubes needed in the translation.
    storm::dd::Bdd<DdType> rowVariablesCube;
    storm::dd::Bdd<DdType> columnVariablesCube;
    storm::dd::Bdd<DdType> allSourceVariablesCube;
    storm::dd::Bdd<DdType> nondeterminismVariablesCube;

    // Information about the state partition.
    storm::dd::Bdd<DdType> partitionBdd;
    uint64_t numberOfBlocks;
    storm::expressions::Variable blockVariable;
    storm::dd::Bdd<DdType> representatives;
    storm::dd::Odd odd;
    storm::dd::Odd nondeterminismOdd;

    // A flag that stores whether the underlying storage for matrix entries has been created.
    bool matrixEntriesCreated;

    // The entries of the quotient matrix that is built.
    std::vector<std::vector<storm::storage::MatrixEntry<uint_fast64_t, ExportValueType>>> matrixEntries;

    // A vector storing for each row which state it belongs to.
    std::vector<uint64_t> rowToState;

    // A vector storing the row permutation for nondeterministic models.
    std::vector<uint64_t> rowPermutation;
};

template<typename ValueType>
class InternalSparseQuotientExtractor<storm::dd::DdType::CUDD, ValueType> : public InternalSparseQuotientExtractorBase<storm::dd::DdType::CUDD, ValueType> {
   public:
#ifdef STORM_HAVE_CUDD
    InternalSparseQuotientExtractor(storm::models::symbolic::Model<storm::dd::DdType::CUDD, ValueType> const& model,
                                    storm::dd::Bdd<storm::dd::DdType::CUDD> const& partitionBdd, storm::expressions::Variable const& blockVariable,
                                    uint64_t numberOfBlocks, storm::dd::Bdd<storm::dd::DdType::CUDD> const& representatives)
        : InternalSparseQuotientExtractorBase<storm::dd::DdType::CUDD, ValueType>(model, partitionBdd, blockVariable, numberOfBlocks, representatives),
          ddman(this->manager.getInternalDdManager().getCuddManager().getManager()) {
        this->createBlockToOffsetMapping();
    }
#else
    InternalSparseQuotientExtractor(storm::models::symbolic::Model<storm::dd::DdType::CUDD, ValueType> const& model,
                                    storm::dd::Bdd<storm::dd::DdType::CUDD> const& partitionBdd, storm::expressions::Variable const& blockVariable,
                                    uint64_t numberOfBlocks, storm::dd::Bdd<storm::dd::DdType::CUDD> const& representatives)
        : InternalSparseQuotientExtractorBase<storm::dd::DdType::CUDD, ValueType>(model, partitionBdd, blockVariable, numberOfBlocks, representatives) {
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with CUDD support.");
    }
#endif

   private:
    virtual storm::storage::SparseMatrix<ValueType> extractMatrixInternal(storm::dd::Add<storm::dd::DdType::CUDD, ValueType> const& matrix) override {
#ifdef STORM_HAVE_CUDD

        this->createMatrixEntryStorage();
        extractTransitionMatrixRec(matrix.getInternalAdd().getCuddDdNode(), this->isNondeterministic ? this->nondeterminismOdd : this->odd, 0,
                                   this->partitionBdd.getInternalBdd().getCuddDdNode(), this->representatives.getInternalBdd().getCuddDdNode(),
                                   this->allSourceVariablesCube.getInternalBdd().getCuddDdNode(),
                                   this->nondeterminismVariablesCube.getInternalBdd().getCuddDdNode(), this->isNondeterministic ? &this->odd : nullptr, 0);
        return this->createMatrixFromEntries();
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with CUDD support.");
#endif
    }

    virtual std::vector<ValueType> extractVectorInternal(storm::dd::Add<storm::dd::DdType::CUDD, ValueType> const& vector,
                                                         storm::dd::Bdd<storm::dd::DdType::CUDD> const& variablesCube, storm::dd::Odd const& odd) override {
#ifdef STORM_HAVE_CUDD
        std::vector<ValueType> result(odd.getTotalOffset());
        extractVectorRec(vector.getInternalAdd().getCuddDdNode(), this->representatives.getInternalBdd().getCuddDdNode(),
                         variablesCube.getInternalBdd().getCuddDdNode(), odd, 0, result);
        return result;
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with CUDD support.");
#endif
    }
#ifdef STORM_HAVE_CUDD
    void createBlockToOffsetMapping() {
        this->createBlockToOffsetMappingRec(this->partitionBdd.getInternalBdd().getCuddDdNode(), this->representatives.getInternalBdd().getCuddDdNode(),
                                            this->rowVariablesCube.getInternalBdd().getCuddDdNode(), this->odd, 0);
        STORM_LOG_ASSERT(blockToOffset.size() == this->numberOfBlocks,
                         "Mismatching block-to-offset mapping: " << blockToOffset.size() << " vs. " << this->numberOfBlocks << ".");
    }

    void createBlockToOffsetMappingRec(DdNodePtr partitionNode, DdNodePtr representativesNode, DdNodePtr variables, storm::dd::Odd const& odd,
                                       uint64_t offset) {
        STORM_LOG_ASSERT(partitionNode != Cudd_ReadLogicZero(ddman) || representativesNode == Cudd_ReadLogicZero(ddman),
                         "Expected representative to be zero if the partition is zero.");
        if (representativesNode == Cudd_ReadLogicZero(ddman) || partitionNode == Cudd_ReadLogicZero(ddman)) {
            return;
        }

        if (Cudd_IsConstant(variables)) {
            STORM_LOG_ASSERT(odd.isTerminalNode(), "Expected terminal node.");
            STORM_LOG_ASSERT(blockToOffset.find(partitionNode) == blockToOffset.end(), "Duplicate entry.");
            blockToOffset[partitionNode] = offset;
        } else {
            STORM_LOG_ASSERT(!odd.isTerminalNode(), "Expected non-terminal node.");
            DdNodePtr partitionT;
            DdNodePtr partitionE;
            if (Cudd_NodeReadIndex(partitionNode) == Cudd_NodeReadIndex(variables)) {
                partitionT = Cudd_T(partitionNode);
                partitionE = Cudd_E(partitionNode);

                if (Cudd_IsComplement(partitionNode)) {
                    partitionE = Cudd_Not(partitionE);
                    partitionT = Cudd_Not(partitionT);
                }
            } else {
                partitionT = partitionE = partitionNode;
            }

            DdNodePtr representativesT;
            DdNodePtr representativesE;
            if (Cudd_NodeReadIndex(representativesNode) == Cudd_NodeReadIndex(variables)) {
                representativesT = Cudd_T(representativesNode);
                representativesE = Cudd_E(representativesNode);

                if (Cudd_IsComplement(representativesNode)) {
                    representativesE = Cudd_Not(representativesE);
                    representativesT = Cudd_Not(representativesT);
                }
            } else {
                representativesT = representativesE = representativesNode;
            }

            createBlockToOffsetMappingRec(partitionE, representativesE, Cudd_T(variables), odd.getElseSuccessor(), offset);
            createBlockToOffsetMappingRec(partitionT, representativesT, Cudd_T(variables), odd.getThenSuccessor(), offset + odd.getElseOffset());
        }
    }

    void extractVectorRec(DdNodePtr vector, DdNodePtr representativesNode, DdNodePtr variables, storm::dd::Odd const& odd, uint64_t offset,
                          std::vector<ValueType>& result) {
        if (representativesNode == Cudd_ReadLogicZero(ddman) || vector == Cudd_ReadZero(ddman)) {
            return;
        }

        if (Cudd_IsConstant(variables)) {
            result[offset] = Cudd_V(vector);
        } else {
            DdNodePtr vectorT;
            DdNodePtr vectorE;
            if (Cudd_NodeReadIndex(vector) == Cudd_NodeReadIndex(variables)) {
                vectorT = Cudd_T(vector);
                vectorE = Cudd_E(vector);
            } else {
                vectorT = vectorE = vector;
            }

            DdNodePtr representativesT;
            DdNodePtr representativesE;
            if (Cudd_NodeReadIndex(representativesNode) == Cudd_NodeReadIndex(variables)) {
                representativesT = Cudd_T(representativesNode);
                representativesE = Cudd_E(representativesNode);

                if (Cudd_IsComplement(representativesNode)) {
                    representativesT = Cudd_Not(representativesT);
                    representativesE = Cudd_Not(representativesE);
                }
            } else {
                representativesT = representativesE = representativesNode;
            }

            extractVectorRec(vectorE, representativesE, Cudd_T(variables), odd.getElseSuccessor(), offset, result);
            extractVectorRec(vectorT, representativesT, Cudd_T(variables), odd.getThenSuccessor(), offset + odd.getElseOffset(), result);
        }
    }

    void extractTransitionMatrixRec(DdNodePtr transitionMatrixNode, storm::dd::Odd const& sourceOdd, uint64_t sourceOffset, DdNodePtr targetPartitionNode,
                                    DdNodePtr representativesNode, DdNodePtr variables, DdNodePtr nondeterminismVariables, storm::dd::Odd const* stateOdd,
                                    uint64_t stateOffset) {
        // For the empty DD, we do not need to add any entries. Note that the partition nodes cannot be zero
        // as all states of the model have to be contained.
        if (transitionMatrixNode == Cudd_ReadZero(ddman) || representativesNode == Cudd_ReadLogicZero(ddman)) {
            return;
        }

        // If we have moved through all source variables, we must have arrived at a target block encoding.
        if (Cudd_IsConstant(variables)) {
            STORM_LOG_ASSERT(Cudd_IsConstant(transitionMatrixNode), "Expected constant node.");
            this->addMatrixEntry(sourceOffset, blockToOffset.at(targetPartitionNode), Cudd_V(transitionMatrixNode));
            if (stateOdd) {
                this->assignRowToState(sourceOffset, stateOffset);
            }
        } else {
            // Determine whether the next variable is a nondeterminism variable.
            bool nextVariableIsNondeterminismVariable =
                !Cudd_IsConstant(nondeterminismVariables) && Cudd_NodeReadIndex(nondeterminismVariables) == Cudd_NodeReadIndex(variables);

            if (nextVariableIsNondeterminismVariable) {
                DdNodePtr t;
                DdNodePtr e;

                // Determine whether the variable was skipped in the matrix.
                if (Cudd_NodeReadIndex(transitionMatrixNode) == Cudd_NodeReadIndex(variables)) {
                    t = Cudd_T(transitionMatrixNode);
                    e = Cudd_E(transitionMatrixNode);
                } else {
                    t = e = transitionMatrixNode;
                }

                STORM_LOG_ASSERT(stateOdd, "Expected separate state ODD.");
                extractTransitionMatrixRec(e, sourceOdd.getElseSuccessor(), sourceOffset, targetPartitionNode, representativesNode, Cudd_T(variables),
                                           Cudd_T(nondeterminismVariables), stateOdd, stateOffset);
                extractTransitionMatrixRec(t, sourceOdd.getThenSuccessor(), sourceOffset + sourceOdd.getElseOffset(), targetPartitionNode, representativesNode,
                                           Cudd_T(variables), Cudd_T(nondeterminismVariables), stateOdd, stateOffset);
            } else {
                DdNodePtr t;
                DdNodePtr tt;
                DdNodePtr te;
                DdNodePtr e;
                DdNodePtr et;
                DdNodePtr ee;
                if (Cudd_NodeReadIndex(transitionMatrixNode) == Cudd_NodeReadIndex(variables)) {
                    // Source node was not skipped in transition matrix.
                    t = Cudd_T(transitionMatrixNode);
                    e = Cudd_E(transitionMatrixNode);
                } else {
                    t = e = transitionMatrixNode;
                }

                if (Cudd_NodeReadIndex(t) == Cudd_NodeReadIndex(variables) + 1) {
                    // Target node was not skipped in transition matrix.
                    tt = Cudd_T(t);
                    te = Cudd_E(t);
                } else {
                    // Target node was skipped in transition matrix.
                    tt = te = t;
                }
                if (t != e) {
                    if (Cudd_NodeReadIndex(e) == Cudd_NodeReadIndex(variables) + 1) {
                        // Target node was not skipped in transition matrix.
                        et = Cudd_T(e);
                        ee = Cudd_E(e);
                    } else {
                        // Target node was skipped in transition matrix.
                        et = ee = e;
                    }
                } else {
                    et = tt;
                    ee = te;
                }

                DdNodePtr targetT;
                DdNodePtr targetE;
                if (Cudd_NodeReadIndex(targetPartitionNode) == Cudd_NodeReadIndex(variables)) {
                    // Node was not skipped in target partition.
                    targetT = Cudd_T(targetPartitionNode);
                    targetE = Cudd_E(targetPartitionNode);

                    if (Cudd_IsComplement(targetPartitionNode)) {
                        targetT = Cudd_Not(targetT);
                        targetE = Cudd_Not(targetE);
                    }
                } else {
                    // Node was skipped in target partition.
                    targetT = targetE = targetPartitionNode;
                }

                DdNodePtr representativesT;
                DdNodePtr representativesE;
                if (Cudd_NodeReadIndex(representativesNode) == Cudd_NodeReadIndex(variables)) {
                    // Node was not skipped in representatives.
                    representativesT = Cudd_T(representativesNode);
                    representativesE = Cudd_E(representativesNode);
                } else {
                    // Node was skipped in representatives.
                    representativesT = representativesE = representativesNode;
                }

                if (representativesT != representativesE && Cudd_IsComplement(representativesNode)) {
                    representativesT = Cudd_Not(representativesT);
                    representativesE = Cudd_Not(representativesE);
                }

                extractTransitionMatrixRec(ee, sourceOdd.getElseSuccessor(), sourceOffset, targetE, representativesE, Cudd_T(variables),
                                           nondeterminismVariables, stateOdd ? &stateOdd->getElseSuccessor() : stateOdd, stateOffset);
                extractTransitionMatrixRec(et, sourceOdd.getElseSuccessor(), sourceOffset, targetT, representativesE, Cudd_T(variables),
                                           nondeterminismVariables, stateOdd ? &stateOdd->getElseSuccessor() : stateOdd, stateOffset);
                extractTransitionMatrixRec(te, sourceOdd.getThenSuccessor(), sourceOffset + sourceOdd.getElseOffset(), targetE, representativesT,
                                           Cudd_T(variables), nondeterminismVariables, stateOdd ? &stateOdd->getThenSuccessor() : stateOdd,
                                           stateOffset + (stateOdd ? stateOdd->getElseOffset() : 0));
                extractTransitionMatrixRec(tt, sourceOdd.getThenSuccessor(), sourceOffset + sourceOdd.getElseOffset(), targetT, representativesT,
                                           Cudd_T(variables), nondeterminismVariables, stateOdd ? &stateOdd->getThenSuccessor() : stateOdd,
                                           stateOffset + (stateOdd ? stateOdd->getElseOffset() : 0));
            }
        }
    }

    ::DdManager* ddman;

    // A mapping from blocks (stored in terms of a DD node) to the offset of the corresponding block.
    gtl::flat_hash_map<DdNode const*, uint64_t> blockToOffset;
#endif
};

template<typename ValueType, typename ExportValueType>
class InternalSparseQuotientExtractor<storm::dd::DdType::Sylvan, ValueType, ExportValueType>
    : public InternalSparseQuotientExtractorBase<storm::dd::DdType::Sylvan, ValueType, ExportValueType> {
   public:
    InternalSparseQuotientExtractor(storm::models::symbolic::Model<storm::dd::DdType::Sylvan, ValueType> const& model,
                                    storm::dd::Bdd<storm::dd::DdType::Sylvan> const& partitionBdd, storm::expressions::Variable const& blockVariable,
                                    uint64_t numberOfBlocks, storm::dd::Bdd<storm::dd::DdType::Sylvan> const& representatives)
        : InternalSparseQuotientExtractorBase<storm::dd::DdType::Sylvan, ValueType, ExportValueType>(model, partitionBdd, blockVariable, numberOfBlocks,
                                                                                                     representatives) {
#ifdef STORM_HAVE_SYLVAN
        this->createBlockToOffsetMapping();
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
#endif
    }

   private:
    virtual storm::storage::SparseMatrix<ExportValueType> extractMatrixInternal(storm::dd::Add<storm::dd::DdType::Sylvan, ValueType> const& matrix) override {
#ifdef STORM_HAVE_SYLVAN
        this->createMatrixEntryStorage();
        extractTransitionMatrixRec(matrix.getInternalAdd().getSylvanMtbdd().GetMTBDD(), this->isNondeterministic ? this->nondeterminismOdd : this->odd, 0,
                                   this->partitionBdd.getInternalBdd().getSylvanBdd().GetBDD(), this->representatives.getInternalBdd().getSylvanBdd().GetBDD(),
                                   this->allSourceVariablesCube.getInternalBdd().getSylvanBdd().GetBDD(),
                                   this->nondeterminismVariablesCube.getInternalBdd().getSylvanBdd().GetBDD(), this->isNondeterministic ? &this->odd : nullptr,
                                   0);
        return this->createMatrixFromEntries();
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
#endif
    }

    virtual std::vector<ExportValueType> extractVectorInternal(storm::dd::Add<storm::dd::DdType::Sylvan, ValueType> const& vector,
                                                               storm::dd::Bdd<storm::dd::DdType::Sylvan> const& variablesCube,
                                                               storm::dd::Odd const& odd) override {
#ifdef STORM_HAVE_SYLVAN
        std::vector<ExportValueType> result(odd.getTotalOffset());
        extractVectorRec(vector.getInternalAdd().getSylvanMtbdd().GetMTBDD(), this->representatives.getInternalBdd().getSylvanBdd().GetBDD(),
                         variablesCube.getInternalBdd().getSylvanBdd().GetBDD(), odd, 0, result);
        return result;
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
#endif
    }

#ifdef STORM_HAVE_SYLVAN
    void extractVectorRec(MTBDD vector, BDD representativesNode, BDD variables, storm::dd::Odd const& odd, uint64_t offset,
                          std::vector<ExportValueType>& result) {
        if (representativesNode == sylvan_false || mtbdd_iszero(vector)) {
            return;
        }

        if (sylvan_isconst(variables)) {
            result[offset] = storm::utility::convertNumber<ExportValueType>(storm::dd::InternalAdd<storm::dd::DdType::Sylvan, ValueType>::getValue(vector));
        } else {
            MTBDD vectorT;
            MTBDD vectorE;
            if (sylvan_mtbdd_matches_variable_index(vector, sylvan_var(variables))) {
                vectorT = sylvan_high(vector);
                vectorE = sylvan_low(vector);
            } else {
                vectorT = vectorE = vector;
            }

            BDD representativesT;
            BDD representativesE;
            if (sylvan_bdd_matches_variable_index(representativesNode, sylvan_var(variables))) {
                representativesT = sylvan_high(representativesNode);
                representativesE = sylvan_low(representativesNode);
            } else {
                representativesT = representativesE = representativesNode;
            }

            extractVectorRec(vectorE, representativesE, sylvan_high(variables), odd.getElseSuccessor(), offset, result);
            extractVectorRec(vectorT, representativesT, sylvan_high(variables), odd.getThenSuccessor(), offset + odd.getElseOffset(), result);
        }
    }

    void createBlockToOffsetMapping() {
        this->createBlockToOffsetMappingRec(this->partitionBdd.getInternalBdd().getSylvanBdd().GetBDD(),
                                            this->representatives.getInternalBdd().getSylvanBdd().GetBDD(),
                                            this->rowVariablesCube.getInternalBdd().getSylvanBdd().GetBDD(), this->odd, 0);
        STORM_LOG_ASSERT(blockToOffset.size() == this->numberOfBlocks,
                         "Mismatching block-to-offset mapping: " << blockToOffset.size() << " vs. " << this->numberOfBlocks << ".");
    }

    void createBlockToOffsetMappingRec(BDD partitionNode, BDD representativesNode, BDD variables, storm::dd::Odd const& odd, uint64_t offset) {
        STORM_LOG_ASSERT(partitionNode != sylvan_false || representativesNode == sylvan_false, "Expected representative to be zero if the partition is zero.");
        if (representativesNode == sylvan_false || partitionNode == sylvan_false) {
            return;
        }

        if (sylvan_isconst(variables)) {
            STORM_LOG_ASSERT(odd.isTerminalNode(), "Expected terminal node.");
            STORM_LOG_ASSERT(blockToOffset.find(partitionNode) == blockToOffset.end(), "Duplicate entry.");
            blockToOffset[partitionNode] = offset;
        } else {
            STORM_LOG_ASSERT(!odd.isTerminalNode(), "Expected non-terminal node.");
            BDD partitionT;
            BDD partitionE;
            if (sylvan_bdd_matches_variable_index(partitionNode, sylvan_var(variables))) {
                partitionT = sylvan_high(partitionNode);
                partitionE = sylvan_low(partitionNode);
            } else {
                partitionT = partitionE = partitionNode;
            }

            BDD representativesT;
            BDD representativesE;
            if (sylvan_bdd_matches_variable_index(representativesNode, sylvan_var(variables))) {
                representativesT = sylvan_high(representativesNode);
                representativesE = sylvan_low(representativesNode);
            } else {
                representativesT = representativesE = representativesNode;
            }

            createBlockToOffsetMappingRec(partitionE, representativesE, sylvan_high(variables), odd.getElseSuccessor(), offset);
            createBlockToOffsetMappingRec(partitionT, representativesT, sylvan_high(variables), odd.getThenSuccessor(), offset + odd.getElseOffset());
        }
    }

    void extractTransitionMatrixRec(MTBDD transitionMatrixNode, storm::dd::Odd const& sourceOdd, uint64_t sourceOffset, BDD targetPartitionNode,
                                    BDD representativesNode, BDD variables, BDD nondeterminismVariables, storm::dd::Odd const* stateOdd, uint64_t stateOffset) {
        // For the empty DD, we do not need to add any entries. Note that the partition nodes cannot be zero
        // as all states of the model have to be contained.
        if (mtbdd_iszero(transitionMatrixNode) || representativesNode == sylvan_false) {
            return;
        }

        // If we have moved through all source variables, we must have arrived at a target block encoding.
        if (sylvan_isconst(variables)) {
            STORM_LOG_ASSERT(mtbdd_isleaf(transitionMatrixNode), "Expected constant node.");
            this->addMatrixEntry(
                sourceOffset, blockToOffset.at(targetPartitionNode),
                storm::utility::convertNumber<ExportValueType>(storm::dd::InternalAdd<storm::dd::DdType::Sylvan, ValueType>::getValue(transitionMatrixNode)));
            if (stateOdd) {
                this->assignRowToState(sourceOffset, stateOffset);
            }
        } else {
            // Determine whether the next variable is a nondeterminism variable.
            bool nextVariableIsNondeterminismVariable =
                !sylvan_isconst(nondeterminismVariables) && sylvan_var(nondeterminismVariables) == sylvan_var(variables);

            if (nextVariableIsNondeterminismVariable) {
                MTBDD t;
                MTBDD e;

                // Determine whether the variable was skipped in the matrix.
                if (sylvan_mtbdd_matches_variable_index(transitionMatrixNode, sylvan_var(variables))) {
                    t = sylvan_high(transitionMatrixNode);
                    e = sylvan_low(transitionMatrixNode);
                } else {
                    t = e = transitionMatrixNode;
                }

                STORM_LOG_ASSERT(stateOdd, "Expected separate state ODD.");
                extractTransitionMatrixRec(e, sourceOdd.getElseSuccessor(), sourceOffset, targetPartitionNode, representativesNode, sylvan_high(variables),
                                           sylvan_high(nondeterminismVariables), stateOdd, stateOffset);
                extractTransitionMatrixRec(t, sourceOdd.getThenSuccessor(), sourceOffset + sourceOdd.getElseOffset(), targetPartitionNode, representativesNode,
                                           sylvan_high(variables), sylvan_high(nondeterminismVariables), stateOdd, stateOffset);
            } else {
                MTBDD t;
                MTBDD tt;
                MTBDD te;
                MTBDD e;
                MTBDD et;
                MTBDD ee;
                if (sylvan_mtbdd_matches_variable_index(transitionMatrixNode, sylvan_var(variables))) {
                    // Source node was not skipped in transition matrix.
                    t = sylvan_high(transitionMatrixNode);
                    e = sylvan_low(transitionMatrixNode);
                } else {
                    t = e = transitionMatrixNode;
                }

                if (sylvan_mtbdd_matches_variable_index(t, sylvan_var(variables) + 1)) {
                    // Target node was not skipped in transition matrix.
                    tt = sylvan_high(t);
                    te = sylvan_low(t);
                } else {
                    // Target node was skipped in transition matrix.
                    tt = te = t;
                }
                if (t != e) {
                    if (sylvan_mtbdd_matches_variable_index(e, sylvan_var(variables) + 1)) {
                        // Target node was not skipped in transition matrix.
                        et = sylvan_high(e);
                        ee = sylvan_low(e);
                    } else {
                        // Target node was skipped in transition matrix.
                        et = ee = e;
                    }
                } else {
                    et = tt;
                    ee = te;
                }

                BDD targetT;
                BDD targetE;
                if (sylvan_bdd_matches_variable_index(targetPartitionNode, sylvan_var(variables))) {
                    // Node was not skipped in target partition.
                    targetT = sylvan_high(targetPartitionNode);
                    targetE = sylvan_low(targetPartitionNode);
                } else {
                    // Node was skipped in target partition.
                    targetT = targetE = targetPartitionNode;
                }

                BDD representativesT;
                BDD representativesE;
                if (sylvan_bdd_matches_variable_index(representativesNode, sylvan_var(variables))) {
                    // Node was not skipped in representatives.
                    representativesT = sylvan_high(representativesNode);
                    representativesE = sylvan_low(representativesNode);
                } else {
                    // Node was skipped in representatives.
                    representativesT = representativesE = representativesNode;
                }

                extractTransitionMatrixRec(ee, sourceOdd.getElseSuccessor(), sourceOffset, targetE, representativesE, sylvan_high(variables),
                                           nondeterminismVariables, stateOdd ? &stateOdd->getElseSuccessor() : stateOdd, stateOffset);
                extractTransitionMatrixRec(et, sourceOdd.getElseSuccessor(), sourceOffset, targetT, representativesE, sylvan_high(variables),
                                           nondeterminismVariables, stateOdd ? &stateOdd->getElseSuccessor() : stateOdd, stateOffset);
                extractTransitionMatrixRec(te, sourceOdd.getThenSuccessor(), sourceOffset + sourceOdd.getElseOffset(), targetE, representativesT,
                                           sylvan_high(variables), nondeterminismVariables, stateOdd ? &stateOdd->getThenSuccessor() : stateOdd,
                                           stateOffset + (stateOdd ? stateOdd->getElseOffset() : 0));
                extractTransitionMatrixRec(tt, sourceOdd.getThenSuccessor(), sourceOffset + sourceOdd.getElseOffset(), targetT, representativesT,
                                           sylvan_high(variables), nondeterminismVariables, stateOdd ? &stateOdd->getThenSuccessor() : stateOdd,
                                           stateOffset + (stateOdd ? stateOdd->getElseOffset() : 0));
            }
        }
    }

    // A mapping from blocks (stored in terms of a DD node) to the offset of the corresponding block.
    gtl::flat_hash_map<BDD, uint64_t> blockToOffset;
#endif
};

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
QuotientExtractor<DdType, ValueType, ExportValueType>::QuotientExtractor(storm::dd::bisimulation::QuotientFormat const& quotientFormat)
    : useRepresentatives(false), quotientFormat(quotientFormat) {
    auto const& settings = storm::settings::getModule<storm::settings::modules::BisimulationSettings>();
    this->useRepresentatives = settings.isUseRepresentativesSet();
    this->useOriginalVariables = settings.isUseOriginalVariablesSet();
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
std::shared_ptr<storm::models::Model<ExportValueType>> QuotientExtractor<DdType, ValueType, ExportValueType>::extract(
    storm::models::symbolic::Model<DdType, ValueType> const& model, Partition<DdType, ValueType> const& partition,
    PreservationInformation<DdType, ValueType> const& preservationInformation) {
    auto start = std::chrono::high_resolution_clock::now();
    std::shared_ptr<storm::models::Model<ExportValueType>> result;
    if (quotientFormat == storm::dd::bisimulation::QuotientFormat::Sparse) {
        result = extractSparseQuotient(model, partition, preservationInformation);
    } else {
        result = extractDdQuotient(model, partition, preservationInformation);
    }
    auto end = std::chrono::high_resolution_clock::now();
    STORM_LOG_INFO("Quotient extraction completed in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

    STORM_LOG_THROW(result, storm::exceptions::NotSupportedException, "Quotient could not be extracted.");

    return result;
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
std::shared_ptr<storm::models::sparse::Model<ExportValueType>> QuotientExtractor<DdType, ValueType, ExportValueType>::extractSparseQuotient(
    storm::models::symbolic::Model<DdType, ValueType> const& model, Partition<DdType, ValueType> const& partition,
    PreservationInformation<DdType, ValueType> const& preservationInformation) {
    auto states = partition.getStates().swapVariables(model.getRowColumnMetaVariablePairs());

    storm::dd::Bdd<DdType> partitionAsBdd = partition.storedAsAdd() ? partition.asAdd().toBdd() : partition.asBdd();
    partitionAsBdd = partitionAsBdd.renameVariables(model.getColumnVariables(), model.getRowVariables());

    auto start = std::chrono::high_resolution_clock::now();
    auto representatives = InternalRepresentativeComputer<DdType>(partitionAsBdd, model.getRowVariables()).getRepresentatives();
    STORM_LOG_ASSERT(
        representatives.getNonZeroCount() == partition.getNumberOfBlocks(),
        "Representatives size does not match that of the partition: " << representatives.getNonZeroCount() << " vs. " << partition.getNumberOfBlocks() << ".");
    STORM_LOG_ASSERT((representatives && partitionAsBdd).existsAbstract(model.getRowVariables()) == partitionAsBdd.existsAbstract(model.getRowVariables()),
                     "Representatives do not cover all blocks.");
    InternalSparseQuotientExtractor<DdType, ValueType, ExportValueType> sparseExtractor(model, partitionAsBdd, partition.getBlockVariable(),
                                                                                        partition.getNumberOfBlocks(), representatives);
    storm::storage::SparseMatrix<ExportValueType> quotientTransitionMatrix = sparseExtractor.extractTransitionMatrix(model.getTransitionMatrix());
    auto end = std::chrono::high_resolution_clock::now();
    STORM_LOG_INFO("Quotient transition matrix extracted in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

    start = std::chrono::high_resolution_clock::now();
    storm::models::sparse::StateLabeling quotientStateLabeling(partition.getNumberOfBlocks());
    quotientStateLabeling.addLabel("init", sparseExtractor.extractSetExists(model.getInitialStates()));
    quotientStateLabeling.addLabel("deadlock", sparseExtractor.extractSetExists(model.getDeadlockStates()));

    for (auto const& label : preservationInformation.getLabels()) {
        quotientStateLabeling.addLabel(label, sparseExtractor.extractSetAll(model.getStates(label)));
    }
    for (auto const& expression : preservationInformation.getExpressions()) {
        std::stringstream stream;
        stream << expression;
        std::string expressionAsString = stream.str();

        if (quotientStateLabeling.containsLabel(expressionAsString)) {
            STORM_LOG_WARN("Duplicate label '" << expressionAsString << "', dropping second label definition.");
        } else {
            quotientStateLabeling.addLabel(stream.str(), sparseExtractor.extractSetAll(model.getStates(expression)));
        }
    }
    end = std::chrono::high_resolution_clock::now();
    STORM_LOG_INFO("Quotient labels extracted in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

    start = std::chrono::high_resolution_clock::now();
    std::unordered_map<std::string, storm::models::sparse::StandardRewardModel<ExportValueType>> quotientRewardModels;
    for (auto const& rewardModelName : preservationInformation.getRewardModelNames()) {
        auto const& rewardModel = model.getRewardModel(rewardModelName);

        std::optional<std::vector<ExportValueType>> quotientStateRewards;
        if (rewardModel.hasStateRewards()) {
            quotientStateRewards = sparseExtractor.extractStateVector(rewardModel.getStateRewardVector());
        }

        std::optional<std::vector<ExportValueType>> quotientStateActionRewards;
        if (rewardModel.hasStateActionRewards()) {
            quotientStateActionRewards = sparseExtractor.extractStateActionVector(rewardModel.getStateActionRewardVector());
        }

        quotientRewardModels.emplace(rewardModelName, storm::models::sparse::StandardRewardModel<ExportValueType>(
                                                          std::move(quotientStateRewards), std::move(quotientStateActionRewards), std::nullopt));
    }
    end = std::chrono::high_resolution_clock::now();
    STORM_LOG_INFO("Reward models extracted in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

    std::shared_ptr<storm::models::sparse::Model<ExportValueType>> result;
    if (model.getType() == storm::models::ModelType::Dtmc) {
        result = std::make_shared<storm::models::sparse::Dtmc<ExportValueType>>(std::move(quotientTransitionMatrix), std::move(quotientStateLabeling),
                                                                                std::move(quotientRewardModels));
    } else if (model.getType() == storm::models::ModelType::Ctmc) {
        result = std::make_shared<storm::models::sparse::Ctmc<ExportValueType>>(std::move(quotientTransitionMatrix), std::move(quotientStateLabeling),
                                                                                std::move(quotientRewardModels));
    } else if (model.getType() == storm::models::ModelType::Mdp) {
        result = std::make_shared<storm::models::sparse::Mdp<ExportValueType>>(std::move(quotientTransitionMatrix), std::move(quotientStateLabeling),
                                                                               std::move(quotientRewardModels));
    } else if (model.getType() == storm::models::ModelType::MarkovAutomaton) {
        storm::models::symbolic::MarkovAutomaton<DdType, ValueType> const& markovAutomaton =
            *model.template as<storm::models::symbolic::MarkovAutomaton<DdType, ValueType>>();

        boost::optional<storm::storage::BitVector> markovianStates = sparseExtractor.extractSetExists(markovAutomaton.getMarkovianStates());
        storm::storage::sparse::ModelComponents<ExportValueType> modelComponents(std::move(quotientTransitionMatrix), std::move(quotientStateLabeling),
                                                                                 std::move(quotientRewardModels), false, std::move(markovianStates));
        modelComponents.exitRates = sparseExtractor.extractStateVector(markovAutomaton.getExitRateVector());

        result = std::make_shared<storm::models::sparse::MarkovAutomaton<ExportValueType>>(std::move(modelComponents));
    }

    return result;
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
std::shared_ptr<storm::models::symbolic::Model<DdType, ExportValueType>> QuotientExtractor<DdType, ValueType, ExportValueType>::extractDdQuotient(
    storm::models::symbolic::Model<DdType, ValueType> const& model, Partition<DdType, ValueType> const& partition,
    PreservationInformation<DdType, ValueType> const& preservationInformation) {
    if (this->useOriginalVariables) {
        return extractQuotientUsingOriginalVariables(model, partition, preservationInformation);
    } else {
        return extractQuotientUsingBlockVariables(model, partition, preservationInformation);
    }
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
std::shared_ptr<storm::models::symbolic::Model<DdType, ExportValueType>>
QuotientExtractor<DdType, ValueType, ExportValueType>::extractQuotientUsingBlockVariables(
    storm::models::symbolic::Model<DdType, ValueType> const& model, Partition<DdType, ValueType> const& partition,
    PreservationInformation<DdType, ValueType> const& preservationInformation) {
    auto modelType = model.getType();

    bool useRepresentativesForThisExtraction = this->useRepresentatives;
    if (modelType == storm::models::ModelType::Dtmc || modelType == storm::models::ModelType::Ctmc || modelType == storm::models::ModelType::Mdp ||
        modelType == storm::models::ModelType::MarkovAutomaton) {
        // Sanity checks.
        STORM_LOG_ASSERT(partition.getNumberOfStates() == model.getNumberOfStates(), "Mismatching partition size.");
        STORM_LOG_ASSERT(partition.getStates().renameVariables(model.getColumnVariables(), model.getRowVariables()) == model.getReachableStates(),
                         "Mismatching partition.");

        std::set<storm::expressions::Variable> blockVariableSet = {partition.getBlockVariable()};
        std::set<storm::expressions::Variable> blockPrimeVariableSet = {partition.getPrimedBlockVariable()};
        std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> blockMetaVariablePairs = {
            std::make_pair(partition.getBlockVariable(), partition.getPrimedBlockVariable())};

        auto start = std::chrono::high_resolution_clock::now();

        // Compute representatives.
        storm::dd::Bdd<DdType> partitionAsBdd = partition.storedAsBdd() ? partition.asBdd() : partition.asAdd().notZero();
        partitionAsBdd = partitionAsBdd.renameVariables(model.getColumnVariables(), model.getRowVariables());
        auto representatives = InternalRepresentativeComputer<DdType>(partitionAsBdd, model.getRowVariables()).getRepresentatives();

        if (useRepresentativesForThisExtraction) {
            storm::dd::Bdd<DdType> partitionAsBddOverPrimedBlockVariables = partitionAsBdd.renameVariables(blockVariableSet, blockPrimeVariableSet);
            storm::dd::Bdd<DdType> tmp =
                (representatives && partitionAsBddOverPrimedBlockVariables).renameVariablesConcretize(model.getRowVariables(), blockVariableSet);
            partitionAsBdd = (tmp && partitionAsBddOverPrimedBlockVariables).existsAbstract(blockPrimeVariableSet);
        }

        storm::dd::Bdd<DdType> reachableStates = partitionAsBdd.existsAbstract(model.getRowVariables());
        storm::dd::Bdd<DdType> initialStates = (model.getInitialStates() && partitionAsBdd).existsAbstract(model.getRowVariables());

        std::map<std::string, storm::dd::Bdd<DdType>> preservedLabelBdds;
        for (auto const& label : preservationInformation.getLabels()) {
            preservedLabelBdds.emplace(label, (model.getStates(label) && partitionAsBdd).existsAbstract(model.getRowVariables()));
        }
        for (auto const& expression : preservationInformation.getExpressions()) {
            std::stringstream stream;
            stream << expression;
            std::string expressionAsString = stream.str();

            auto it = preservedLabelBdds.find(expressionAsString);
            if (it != preservedLabelBdds.end()) {
                STORM_LOG_WARN("Duplicate label '" << expressionAsString << "', dropping second label definition.");
            } else {
                preservedLabelBdds.emplace(stream.str(), (model.getStates(expression) && partitionAsBdd).existsAbstract(model.getRowVariables()));
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        STORM_LOG_INFO("Quotient labels extracted in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

        start = std::chrono::high_resolution_clock::now();
        std::set<storm::expressions::Variable> blockAndRowVariables;
        std::set_union(blockVariableSet.begin(), blockVariableSet.end(), model.getRowVariables().begin(), model.getRowVariables().end(),
                       std::inserter(blockAndRowVariables, blockAndRowVariables.end()));
        std::set<storm::expressions::Variable> blockPrimeAndColumnVariables;
        std::set_union(blockPrimeVariableSet.begin(), blockPrimeVariableSet.end(), model.getColumnVariables().begin(), model.getColumnVariables().end(),
                       std::inserter(blockPrimeAndColumnVariables, blockPrimeAndColumnVariables.end()));
        storm::dd::Add<DdType, ValueType> partitionAsAdd = partitionAsBdd.template toAdd<ValueType>();
        storm::dd::Add<DdType, ValueType> quotientTransitionMatrix = model.getTransitionMatrix().multiplyMatrix(
            partitionAsAdd.renameVariables(blockAndRowVariables, blockPrimeAndColumnVariables), model.getColumnVariables());

        // Pick a representative from each block.
        partitionAsBdd &= representatives;
        partitionAsAdd *= partitionAsBdd.template toAdd<ValueType>();

        quotientTransitionMatrix = quotientTransitionMatrix.multiplyMatrix(partitionAsAdd, model.getRowVariables());
        end = std::chrono::high_resolution_clock::now();

        // Check quotient matrix for sanity.
        if (std::is_same<ValueType, storm::RationalNumber>::value) {
            STORM_LOG_ASSERT(quotientTransitionMatrix.greater(storm::utility::one<ValueType>()).isZero(), "Illegal entries in quotient matrix.");
        } else if (std::is_same<ValueType, storm::RationalFunction>::value) {
            // No comparison for rational functions
        } else {
            STORM_LOG_ASSERT(quotientTransitionMatrix.greater(storm::utility::one<ValueType>() + storm::utility::convertNumber<ValueType>(1e-6)).isZero(),
                             "Illegal entries in quotient matrix.");
        }
        STORM_LOG_ASSERT(quotientTransitionMatrix.sumAbstract(blockPrimeVariableSet)
                             .equalModuloPrecision(quotientTransitionMatrix.notZero().existsAbstract(blockPrimeVariableSet).template toAdd<ValueType>(),
                                                   storm::utility::convertNumber<ValueType>(1e-6)),
                         "Illegal non-probabilistic matrix.");

        STORM_LOG_INFO("Quotient transition matrix extracted in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

        storm::dd::Bdd<DdType> quotientTransitionMatrixBdd = quotientTransitionMatrix.notZero();

        std::set<storm::expressions::Variable> blockPrimeAndNondeterminismVariables = model.getNondeterminismVariables();
        blockPrimeAndNondeterminismVariables.insert(blockPrimeVariableSet.begin(), blockPrimeVariableSet.end());
        storm::dd::Bdd<DdType> deadlockStates = !quotientTransitionMatrixBdd.existsAbstract(blockPrimeAndNondeterminismVariables) && reachableStates;

        start = std::chrono::high_resolution_clock::now();
        std::unordered_map<std::string, storm::models::symbolic::StandardRewardModel<DdType, ValueType>> quotientRewardModels;
        for (auto const& rewardModelName : preservationInformation.getRewardModelNames()) {
            auto const& rewardModel = model.getRewardModel(rewardModelName);

            boost::optional<storm::dd::Add<DdType, ValueType>> quotientStateRewards;
            if (rewardModel.hasStateRewards()) {
                quotientStateRewards = rewardModel.getStateRewardVector().multiplyMatrix(partitionAsAdd, model.getRowVariables());
            }

            boost::optional<storm::dd::Add<DdType, ValueType>> quotientStateActionRewards;
            if (rewardModel.hasStateActionRewards()) {
                quotientStateActionRewards = rewardModel.getStateActionRewardVector().multiplyMatrix(partitionAsAdd, model.getRowVariables());
            }

            quotientRewardModels.emplace(rewardModelName, storm::models::symbolic::StandardRewardModel<DdType, ValueType>(
                                                              quotientStateRewards, quotientStateActionRewards, boost::none));
        }
        end = std::chrono::high_resolution_clock::now();
        STORM_LOG_INFO("Reward models extracted in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

        std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> result;
        if (modelType == storm::models::ModelType::Dtmc) {
            result = std::shared_ptr<storm::models::symbolic::Dtmc<DdType, ValueType>>(new storm::models::symbolic::Dtmc<DdType, ValueType>(
                model.getManager().asSharedPointer(), reachableStates, initialStates, deadlockStates, quotientTransitionMatrix, blockVariableSet,
                blockPrimeVariableSet, blockMetaVariablePairs, preservedLabelBdds, quotientRewardModels));
        } else if (modelType == storm::models::ModelType::Ctmc) {
            result = std::shared_ptr<storm::models::symbolic::Ctmc<DdType, ValueType>>(new storm::models::symbolic::Ctmc<DdType, ValueType>(
                model.getManager().asSharedPointer(), reachableStates, initialStates, deadlockStates, quotientTransitionMatrix, blockVariableSet,
                blockPrimeVariableSet, blockMetaVariablePairs, preservedLabelBdds, quotientRewardModels));
        } else if (modelType == storm::models::ModelType::Mdp) {
            result = std::shared_ptr<storm::models::symbolic::Mdp<DdType, ValueType>>(new storm::models::symbolic::Mdp<DdType, ValueType>(
                model.getManager().asSharedPointer(), reachableStates, initialStates, deadlockStates, quotientTransitionMatrix, blockVariableSet,
                blockPrimeVariableSet, blockMetaVariablePairs, model.getNondeterminismVariables(), preservedLabelBdds, quotientRewardModels));
        } else {
            result =
                std::shared_ptr<storm::models::symbolic::MarkovAutomaton<DdType, ValueType>>(new storm::models::symbolic::MarkovAutomaton<DdType, ValueType>(
                    model.getManager().asSharedPointer(),
                    model.template as<storm::models::symbolic::MarkovAutomaton<DdType, ValueType>>()->getMarkovianMarker(), reachableStates, initialStates,
                    deadlockStates, quotientTransitionMatrix, blockVariableSet, blockPrimeVariableSet, blockMetaVariablePairs,
                    model.getNondeterminismVariables(), preservedLabelBdds, quotientRewardModels));
        }

        return result->template toValueType<ExportValueType>();
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Cannot extract quotient for this model type.");
    }
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType>
std::shared_ptr<storm::models::symbolic::Model<DdType, ExportValueType>>
QuotientExtractor<DdType, ValueType, ExportValueType>::extractQuotientUsingOriginalVariables(
    storm::models::symbolic::Model<DdType, ValueType> const& model, Partition<DdType, ValueType> const& partition,
    PreservationInformation<DdType, ValueType> const& preservationInformation) {
    auto modelType = model.getType();

    bool useRepresentativesForThisExtraction = this->useRepresentatives;
    if (modelType == storm::models::ModelType::Dtmc || modelType == storm::models::ModelType::Ctmc || modelType == storm::models::ModelType::Mdp ||
        modelType == storm::models::ModelType::MarkovAutomaton) {
        STORM_LOG_WARN_COND(!this->useRepresentatives, "Using representatives is unsupported for this extraction, falling back to regular extraction.");

        // Sanity checks.
        STORM_LOG_ASSERT(partition.getNumberOfStates() == model.getNumberOfStates(), "Mismatching partition size.");
        STORM_LOG_ASSERT(partition.getStates().renameVariables(model.getColumnVariables(), model.getRowVariables()) == model.getReachableStates(),
                         "Mismatching partition.");

        std::set<storm::expressions::Variable> blockVariableSet = {partition.getBlockVariable()};
        std::set<storm::expressions::Variable> blockPrimeVariableSet = {partition.getPrimedBlockVariable()};
        std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> blockMetaVariablePairs = {
            std::make_pair(partition.getBlockVariable(), partition.getPrimedBlockVariable())};

        auto start = std::chrono::high_resolution_clock::now();

        // Compute representatives.
        storm::dd::Bdd<DdType> partitionAsBdd = partition.storedAsBdd() ? partition.asBdd() : partition.asAdd().notZero();
        partitionAsBdd = partitionAsBdd.renameVariables(model.getColumnVariables(), model.getRowVariables());
        auto representatives = InternalRepresentativeComputer<DdType>(partitionAsBdd, model.getRowVariables()).getRepresentatives();

        if (useRepresentativesForThisExtraction) {
            storm::dd::Bdd<DdType> partitionAsBddOverPrimedBlockVariables = partitionAsBdd.renameVariables(blockVariableSet, blockPrimeVariableSet);
            storm::dd::Bdd<DdType> tmp =
                (representatives && partitionAsBddOverPrimedBlockVariables).renameVariablesConcretize(model.getRowVariables(), blockVariableSet);
            partitionAsBdd = (tmp && partitionAsBddOverPrimedBlockVariables).existsAbstract(blockPrimeVariableSet);
        }

        storm::dd::Bdd<DdType> reachableStates =
            partitionAsBdd.existsAbstract(model.getRowVariables()).renameVariablesAbstract(blockVariableSet, model.getRowVariables());
        storm::dd::Bdd<DdType> initialStates = (model.getInitialStates() && partitionAsBdd)
                                                   .existsAbstract(model.getRowVariables())
                                                   .renameVariablesAbstract(blockVariableSet, model.getRowVariables());

        std::map<std::string, storm::dd::Bdd<DdType>> preservedLabelBdds;
        for (auto const& label : preservationInformation.getLabels()) {
            preservedLabelBdds.emplace(label, (model.getStates(label) && partitionAsBdd)
                                                  .existsAbstract(model.getRowVariables())
                                                  .renameVariablesAbstract(blockVariableSet, model.getRowVariables()));
        }
        for (auto const& expression : preservationInformation.getExpressions()) {
            std::stringstream stream;
            stream << expression;
            std::string expressionAsString = stream.str();

            auto it = preservedLabelBdds.find(expressionAsString);
            if (it != preservedLabelBdds.end()) {
                STORM_LOG_WARN("Duplicate label '" << expressionAsString << "', dropping second label definition.");
            } else {
                preservedLabelBdds.emplace(stream.str(), (model.getStates(expression) && partitionAsBdd)
                                                             .existsAbstract(model.getRowVariables())
                                                             .renameVariablesAbstract(blockVariableSet, model.getRowVariables()));
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        STORM_LOG_INFO("Quotient labels extracted in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

        start = std::chrono::high_resolution_clock::now();
        std::set<storm::expressions::Variable> blockAndRowVariables;
        std::set_union(blockVariableSet.begin(), blockVariableSet.end(), model.getRowVariables().begin(), model.getRowVariables().end(),
                       std::inserter(blockAndRowVariables, blockAndRowVariables.end()));
        std::set<storm::expressions::Variable> blockPrimeAndColumnVariables;
        std::set_union(blockPrimeVariableSet.begin(), blockPrimeVariableSet.end(), model.getColumnVariables().begin(), model.getColumnVariables().end(),
                       std::inserter(blockPrimeAndColumnVariables, blockPrimeAndColumnVariables.end()));
        storm::dd::Add<DdType, ValueType> partitionAsAdd = partitionAsBdd.template toAdd<ValueType>();
        storm::dd::Add<DdType, ValueType> quotientTransitionMatrix =
            model.getTransitionMatrix()
                .multiplyMatrix(partitionAsAdd.renameVariables(model.getRowVariables(), model.getColumnVariables()), model.getColumnVariables())
                .renameVariablesAbstract(blockVariableSet, model.getColumnVariables());

        // Pick a representative from each block.
        partitionAsBdd &= representatives;
        partitionAsAdd = partitionAsBdd.template toAdd<ValueType>();

        // Workaround for problem with CUDD. Matrix-Matrix multiplication yields other result than multiplication+sum-abstract...
        if (DdType == storm::dd::DdType::CUDD) {
            quotientTransitionMatrix = (quotientTransitionMatrix * partitionAsAdd)
                                           .sumAbstract(model.getRowVariables())
                                           .renameVariablesAbstract(blockVariableSet, model.getRowVariables());
        } else {
            quotientTransitionMatrix = quotientTransitionMatrix.multiplyMatrix(partitionAsAdd, model.getRowVariables())
                                           .renameVariablesAbstract(blockVariableSet, model.getRowVariables());
        }
        end = std::chrono::high_resolution_clock::now();

        // Check quotient matrix for sanity.
        if (std::is_same<ValueType, storm::RationalNumber>::value) {
            STORM_LOG_ASSERT(quotientTransitionMatrix.greater(storm::utility::one<ValueType>()).isZero(), "Illegal entries in quotient matrix.");
        } else {
            STORM_LOG_ASSERT(quotientTransitionMatrix.greater(storm::utility::one<ValueType>() + storm::utility::convertNumber<ValueType>(1e-6)).isZero(),
                             "Illegal entries in quotient matrix.");
        }
        STORM_LOG_ASSERT(quotientTransitionMatrix.sumAbstract(model.getColumnVariables())
                             .equalModuloPrecision(quotientTransitionMatrix.notZero().existsAbstract(model.getColumnVariables()).template toAdd<ValueType>(),
                                                   storm::utility::convertNumber<ValueType>(1e-6)),
                         "Illegal probabilistic matrix.");

        STORM_LOG_INFO("Quotient transition matrix extracted in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

        storm::dd::Bdd<DdType> quotientTransitionMatrixBdd = quotientTransitionMatrix.notZero();

        std::set<storm::expressions::Variable> columnAndNondeterminismVariables = model.getColumnVariables();
        columnAndNondeterminismVariables.insert(model.getNondeterminismVariables().begin(), model.getNondeterminismVariables().end());
        storm::dd::Bdd<DdType> deadlockStates = !quotientTransitionMatrixBdd.existsAbstract(columnAndNondeterminismVariables) && reachableStates;

        start = std::chrono::high_resolution_clock::now();
        std::unordered_map<std::string, storm::models::symbolic::StandardRewardModel<DdType, ValueType>> quotientRewardModels;
        for (auto const& rewardModelName : preservationInformation.getRewardModelNames()) {
            auto const& rewardModel = model.getRewardModel(rewardModelName);

            boost::optional<storm::dd::Add<DdType, ValueType>> quotientStateRewards;
            if (rewardModel.hasStateRewards()) {
                quotientStateRewards = rewardModel.getStateRewardVector()
                                           .multiplyMatrix(partitionAsAdd, model.getRowVariables())
                                           .renameVariablesAbstract(blockVariableSet, model.getRowVariables());
            }

            boost::optional<storm::dd::Add<DdType, ValueType>> quotientStateActionRewards;
            if (rewardModel.hasStateActionRewards()) {
                quotientStateActionRewards = rewardModel.getStateActionRewardVector()
                                                 .multiplyMatrix(partitionAsAdd, model.getRowVariables())
                                                 .renameVariablesAbstract(blockVariableSet, model.getRowVariables());
            }

            quotientRewardModels.emplace(rewardModelName, storm::models::symbolic::StandardRewardModel<DdType, ValueType>(
                                                              quotientStateRewards, quotientStateActionRewards, boost::none));
        }
        end = std::chrono::high_resolution_clock::now();
        STORM_LOG_INFO("Reward models extracted in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

        std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> result;
        if (modelType == storm::models::ModelType::Dtmc) {
            result = std::shared_ptr<storm::models::symbolic::Dtmc<DdType, ValueType>>(new storm::models::symbolic::Dtmc<DdType, ValueType>(
                model.getManager().asSharedPointer(), reachableStates, initialStates, deadlockStates, quotientTransitionMatrix, model.getRowVariables(),
                model.getColumnVariables(), model.getRowColumnMetaVariablePairs(), preservedLabelBdds, quotientRewardModels));
        } else if (modelType == storm::models::ModelType::Ctmc) {
            result = std::shared_ptr<storm::models::symbolic::Ctmc<DdType, ValueType>>(new storm::models::symbolic::Ctmc<DdType, ValueType>(
                model.getManager().asSharedPointer(), reachableStates, initialStates, deadlockStates, quotientTransitionMatrix, model.getRowVariables(),
                model.getColumnVariables(), model.getRowColumnMetaVariablePairs(), preservedLabelBdds, quotientRewardModels));
        } else if (modelType == storm::models::ModelType::Mdp) {
            result = std::shared_ptr<storm::models::symbolic::Mdp<DdType, ValueType>>(new storm::models::symbolic::Mdp<DdType, ValueType>(
                model.getManager().asSharedPointer(), reachableStates, initialStates, deadlockStates, quotientTransitionMatrix, model.getRowVariables(),
                model.getColumnVariables(), model.getRowColumnMetaVariablePairs(), model.getNondeterminismVariables(), preservedLabelBdds,
                quotientRewardModels));
        } else {
            result =
                std::shared_ptr<storm::models::symbolic::MarkovAutomaton<DdType, ValueType>>(new storm::models::symbolic::MarkovAutomaton<DdType, ValueType>(
                    model.getManager().asSharedPointer(),
                    model.template as<storm::models::symbolic::MarkovAutomaton<DdType, ValueType>>()->getMarkovianMarker(), reachableStates, initialStates,
                    deadlockStates, quotientTransitionMatrix, model.getRowVariables(), model.getColumnVariables(), model.getRowColumnMetaVariablePairs(),
                    model.getNondeterminismVariables(), preservedLabelBdds, quotientRewardModels));
        }

        return result->template toValueType<ExportValueType>();
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Cannot extract quotient for this model type.");
    }
}

template class QuotientExtractor<storm::dd::DdType::CUDD, double>;

template class QuotientExtractor<storm::dd::DdType::Sylvan, double>;
template class QuotientExtractor<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class QuotientExtractor<storm::dd::DdType::Sylvan, storm::RationalNumber, double>;
template class QuotientExtractor<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
