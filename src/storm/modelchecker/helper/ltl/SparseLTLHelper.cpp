#include "storm/modelchecker/helper/ltl/SparseLTLHelper.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/automata/DeterministicAutomaton.h"
#include "storm/automata/LTL2DeterministicAutomaton.h"
#include "storm/environment/modelchecker/ModelCheckerEnvironment.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/ExtractMaximalStateFormulasVisitor.h"
#include "storm/modelchecker/prctl/helper/SparseDtmcPrctlHelper.h"
#include "storm/modelchecker/prctl/helper/SparseMdpPrctlHelper.h"
#include "storm/solver/SolveGoal.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<typename ValueType, bool Nondeterministic>
SparseLTLHelper<ValueType, Nondeterministic>::SparseLTLHelper(storm::storage::SparseMatrix<ValueType> const& transitionMatrix)
    : _transitionMatrix(transitionMatrix) {
    // Intentionally left empty.
}

template<typename ValueType, bool Nondeterministic>
storm::storage::Scheduler<ValueType> SparseLTLHelper<ValueType, Nondeterministic>::SparseLTLHelper::extractScheduler(
    storm::models::sparse::Model<ValueType> const& model) {
    STORM_LOG_ASSERT(this->isProduceSchedulerSet(), "Trying to get the produced optimal choices although no scheduler was requested.");
    STORM_LOG_ASSERT(this->_schedulerHelper.is_initialized(),
                     "Trying to get the produced optimal choices but none were available. Was there a computation call before?");

    return this->_schedulerHelper.get().extractScheduler(model, this->hasRelevantStates());
}

template<typename ValueType, bool Nondeterministic>
std::vector<ValueType> SparseLTLHelper<ValueType, Nondeterministic>::computeLTLProbabilities(Environment const& env, storm::logic::PathFormula const& formula,
                                                                                             CheckFormulaCallback const& formulaChecker) {
    // Replace state-subformulae by atomic propositions (APs)
    storm::logic::ExtractMaximalStateFormulasVisitor::ApToFormulaMap extracted;
    std::shared_ptr<storm::logic::Formula> ltlFormula = storm::logic::ExtractMaximalStateFormulasVisitor::extract(formula, extracted);
    STORM_LOG_ASSERT(ltlFormula->isPathFormula(), "Unexpected formula type.");

    // Compute Satisfaction sets for the APs (which represent the state-subformulae
    auto apSets = computeApSets(extracted, formulaChecker);

    STORM_LOG_INFO("Computing LTL probabilities for formula with " << apSets.size() << " atomic proposition(s).");

    // Compute the resulting LTL probabilities
    return computeLTLProbabilities(env, ltlFormula->asPathFormula(), apSets);
}

template<typename ValueType, bool Nondeterministic>
std::map<std::string, storm::storage::BitVector> SparseLTLHelper<ValueType, Nondeterministic>::computeApSets(
    std::map<std::string, std::shared_ptr<storm::logic::Formula const>> const& extracted, CheckFormulaCallback const& formulaChecker) {
    std::map<std::string, storm::storage::BitVector> apSets;
    for (auto& p : extracted) {
        STORM_LOG_DEBUG(" Computing satisfaction set for atomic proposition \"" << p.first << "\" <=> " << *p.second << "...");
        apSets[p.first] = formulaChecker(*p.second);
    }
    return apSets;
}

template<typename ValueType, bool Nondeterministic>
storm::storage::BitVector SparseLTLHelper<ValueType, Nondeterministic>::computeAcceptingECs(automata::AcceptanceCondition const& acceptance,
                                                                                            storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                            storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                                            typename transformer::DAProduct<productModelType>::ptr product) {
    STORM_LOG_INFO("Computing accepting states for acceptance condition " << *acceptance.getAcceptanceExpression());
    if (acceptance.getAcceptanceExpression()->isTRUE()) {
        STORM_LOG_INFO(" TRUE -> all states accepting (assumes no deadlock in the model)");
        return storm::storage::BitVector(transitionMatrix.getRowGroupCount(), true);
    } else if (acceptance.getAcceptanceExpression()->isFALSE()) {
        STORM_LOG_INFO(" FALSE -> all states rejecting");
        return storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false);
    }

    std::vector<std::vector<automata::AcceptanceCondition::acceptance_expr::ptr>> dnf = acceptance.extractFromDNF();

    storm::storage::BitVector acceptingStates(transitionMatrix.getRowGroupCount(), false);

    std::size_t accMECs = 0;
    std::size_t allMECs = 0;

    // All accepting states will be on a MEC. For efficiency, we compute the MECs of the MDP first to restrict the possible candidates.
    // Compute MECs for the entire MDP
    storm::storage::MaximalEndComponentDecomposition<ValueType> mecs(transitionMatrix, backwardTransitions);
    // Maps every state to the MEC it is in, or to InvalidMecIndex if it does not belong to any MEC.
    std::vector<uint64_t> stateToMec(transitionMatrix.getRowGroupCount(), std::numeric_limits<uint64_t>::max());
    // Contains states that are on a mec
    storm::storage::BitVector mecStates(transitionMatrix.getRowGroupCount(), false);
    for (uint64_t mec_counter = 0; auto const& mec : mecs) {
        for (auto const& [state, _] : mec) {
            stateToMec[state] = mec_counter;
            mecStates.set(state, true);
        }
        ++mec_counter;
    }

    for (auto const& conjunction : dnf) {
        // get the states of the mdp that (a) are on a MEC, (b) are not already known to be accepting, and (c) don't violate Fins of the conjunction
        storm::storage::BitVector allowed = mecStates & ~acceptingStates;

        if (allowed.empty()) {
            break;  // no more candidates
        }

        for (auto const& literal : conjunction) {
            if (literal->isTRUE()) {
                // skip
            } else if (literal->isFALSE()) {
                allowed.clear();
                break;
            } else if (literal->isAtom()) {
                const cpphoafparser::AtomAcceptance& atom = literal->getAtom();
                if (atom.getType() == cpphoafparser::AtomAcceptance::TEMPORAL_FIN) {
                    // only deal with FIN, ignore INF here
                    const storm::storage::BitVector& accSet = acceptance.getAcceptanceSet(atom.getAcceptanceSet());
                    if (atom.isNegated()) {
                        // allowed = allowed \ ~accSet = allowed & accSet
                        allowed &= accSet;
                    } else {
                        // allowed = allowed \ accSet = allowed & ~accSet
                        allowed &= ~accSet;
                    }
                }
            }
        }

        if (allowed.empty()) {
            // skip
            continue;
        }

        // Compute MECs in the allowed fragment
        storm::storage::MaximalEndComponentDecomposition<ValueType> allowedECs(transitionMatrix, backwardTransitions, allowed);
        allMECs += allowedECs.size();
        for (const auto& ec : allowedECs) {
            auto const representativeEcState = ec.begin()->first;
            if (acceptingStates.get(representativeEcState)) {
                // The ec is part of a mec that is already known to be accepting
                continue;
            }

            bool accepting = true;
            for (auto const& literal : conjunction) {
                if (literal->isTRUE()) {
                    // skip

                } else if (literal->isFALSE()) {
                    accepting = false;
                    break;
                } else if (literal->isAtom()) {
                    const cpphoafparser::AtomAcceptance& atom = literal->getAtom();
                    const storm::storage::BitVector& accSet = acceptance.getAcceptanceSet(atom.getAcceptanceSet());
                    if (atom.getType() == cpphoafparser::AtomAcceptance::TEMPORAL_INF) {
                        if (atom.isNegated()) {
                            if (!ec.containsAnyState(~accSet)) {
                                accepting = false;
                                break;
                            }
                        } else {
                            if (!ec.containsAnyState(accSet)) {
                                accepting = false;
                                break;
                            }
                        }

                    } else if (atom.getType() == cpphoafparser::AtomAcceptance::TEMPORAL_FIN) {
                        // Do only sanity checks here.
                        STORM_LOG_ASSERT(atom.isNegated() ? !ec.containsAnyState(~accSet) : !ec.containsAnyState(accSet),
                                         "EC contains Fin-states, which should have been removed");
                    }
                }
            }

            if (accepting) {
                accMECs++;

                // get the MEC containing the current EC
                auto const mec_index = stateToMec[representativeEcState];
                STORM_LOG_ASSERT(mec_index < mecs.size(), "MEC index out of range.");
                auto const& mec = mecs[mec_index];

                // All states of the (global) mec are accepting since we can almost surely reach the inner ec
                for (auto const& [state, _] : mec) {
                    acceptingStates.set(state);
                }

                if (this->isProduceSchedulerSet()) {
                    // save choices for states that weren't assigned to any other MEC yet.
                    this->_schedulerHelper.get().saveProductEcChoices(acceptance, ec, mec, conjunction, product);
                }
            }
        }
    }

    STORM_LOG_INFO("Found " << acceptingStates.getNumberOfSetBits() << " states in " << accMECs << " accepting MECs (considered " << allMECs << " MECs).");

    return acceptingStates;
}

template<typename ValueType, bool Nondeterministic>
storm::storage::BitVector SparseLTLHelper<ValueType, Nondeterministic>::computeAcceptingBCCs(automata::AcceptanceCondition const& acceptance,
                                                                                             storm::storage::SparseMatrix<ValueType> const& transitionMatrix) {
    storm::storage::StronglyConnectedComponentDecomposition<ValueType> bottomSccs(
        transitionMatrix, storage::StronglyConnectedComponentDecompositionOptions().onlyBottomSccs().dropNaiveSccs());
    storm::storage::BitVector acceptingStates(transitionMatrix.getRowGroupCount(), false);

    std::size_t checkedBSCCs = 0, acceptingBSCCs = 0, acceptingBSCCStates = 0;
    for (auto& scc : bottomSccs) {
        checkedBSCCs++;
        if (acceptance.isAccepting(scc)) {
            acceptingBSCCs++;
            for (auto& state : scc) {
                acceptingStates.set(state);
                acceptingBSCCStates++;
            }
        }
    }
    STORM_LOG_INFO("BSCC analysis: " << acceptingBSCCs << " of " << checkedBSCCs << " BSCCs were acceptingStates (" << acceptingBSCCStates
                                     << " states in acceptingStates BSCCs).");
    return acceptingStates;
}

template<typename ValueType, bool Nondeterministic>
std::vector<ValueType> SparseLTLHelper<ValueType, Nondeterministic>::computeDAProductProbabilities(
    Environment const& env, storm::automata::DeterministicAutomaton const& da, std::map<std::string, storm::storage::BitVector>& apSatSets) {
    const storm::automata::APSet& apSet = da.getAPSet();

    std::vector<storm::storage::BitVector> statesForAP;
    for (const std::string& ap : apSet.getAPs()) {
        auto it = apSatSets.find(ap);
        STORM_LOG_THROW(it != apSatSets.end(), storm::exceptions::InvalidOperationException,
                        "Deterministic automaton has AP " << ap << ", does not appear in formula.");

        statesForAP.push_back(std::move(it->second));
    }

    storm::storage::BitVector statesOfInterest;

    if (this->hasRelevantStates()) {
        statesOfInterest = this->getRelevantStates();
    } else {
        // Product from all model states
        statesOfInterest = storm::storage::BitVector(this->_transitionMatrix.getRowGroupCount(), true);
    }

    STORM_LOG_INFO("Building " + (Nondeterministic ? std::string("MDP-DA") : std::string("DTMC-DA")) + " product with deterministic automaton, starting from "
                   << statesOfInterest.getNumberOfSetBits() << " model states...");
    transformer::DAProductBuilder productBuilder(da, statesForAP);

    auto product = productBuilder.build<productModelType>(this->_transitionMatrix, statesOfInterest);

    STORM_LOG_INFO("Product " + (Nondeterministic ? std::string("MDP-DA") : std::string("DTMC-DA")) + " has "
                   << product->getProductModel().getNumberOfStates() << " states and " << product->getProductModel().getNumberOfTransitions()
                   << " transitions.");

    // Prepare scheduler
    if (this->isProduceSchedulerSet()) {
        STORM_LOG_THROW(Nondeterministic, storm::exceptions::InvalidOperationException, "Scheduler export only supported for nondeterministic models.");
        this->_schedulerHelper.emplace(product->getProductModel().getNumberOfStates());
    }

    // Compute accepting states
    storm::storage::BitVector acceptingStates;
    if (Nondeterministic) {
        STORM_LOG_INFO("Computing MECs and checking for acceptance...");
        acceptingStates = computeAcceptingECs(*product->getAcceptance(), product->getProductModel().getTransitionMatrix(),
                                              product->getProductModel().getBackwardTransitions(), product);

    } else {
        STORM_LOG_INFO("Computing BSCCs and checking for acceptance...");
        acceptingStates = computeAcceptingBCCs(*product->getAcceptance(), product->getProductModel().getTransitionMatrix());
    }

    if (acceptingStates.empty()) {
        STORM_LOG_INFO("No accepting states, skipping probability computation.");
        if (this->isProduceSchedulerSet()) {
            this->_schedulerHelper.get().setRandom();
        }
        std::vector<ValueType> numericResult(this->_transitionMatrix.getRowGroupCount(), storm::utility::zero<ValueType>());
        return numericResult;
    }

    STORM_LOG_INFO("Computing probabilities for reaching accepting components...");

    storm::storage::BitVector bvTrue(product->getProductModel().getNumberOfStates(), true);
    storm::storage::BitVector soiProduct(product->getStatesOfInterest());

    // Create goal for computeUntilProbabilities, always compute maximizing probabilities
    storm::solver::SolveGoal<ValueType> solveGoalProduct;
    if (this->isValueThresholdSet()) {
        solveGoalProduct = storm::solver::SolveGoal<ValueType>(OptimizationDirection::Maximize, this->getValueThresholdComparisonType(),
                                                               this->getValueThresholdValue(), std::move(soiProduct));
    } else {
        solveGoalProduct = storm::solver::SolveGoal<ValueType>(OptimizationDirection::Maximize);
        solveGoalProduct.setRelevantValues(std::move(soiProduct));
    }

    std::vector<ValueType> prodNumericResult;

    if (Nondeterministic) {
        MDPSparseModelCheckingHelperReturnType<ValueType> prodCheckResult =
            storm::modelchecker::helper::SparseMdpPrctlHelper<ValueType>::computeUntilProbabilities(
                env, std::move(solveGoalProduct), product->getProductModel().getTransitionMatrix(), product->getProductModel().getBackwardTransitions(), bvTrue,
                acceptingStates, this->isQualitativeSet(),
                this->isProduceSchedulerSet()  // Whether to create memoryless scheduler for the Model-DA Product.
            );
        prodNumericResult = std::move(prodCheckResult.values);

        if (this->isProduceSchedulerSet()) {
            this->_schedulerHelper.get().prepareScheduler(da.getNumberOfStates(), acceptingStates, std::move(prodCheckResult.scheduler), productBuilder,
                                                          product, statesOfInterest, this->_transitionMatrix);
        }

    } else {
        prodNumericResult = storm::modelchecker::helper::SparseDtmcPrctlHelper<ValueType>::computeUntilProbabilities(
            env, std::move(solveGoalProduct), product->getProductModel().getTransitionMatrix(), product->getProductModel().getBackwardTransitions(), bvTrue,
            acceptingStates, this->isQualitativeSet());
    }

    std::vector<ValueType> numericResult = product->projectToOriginalModel(this->_transitionMatrix.getRowGroupCount(), prodNumericResult);

    return numericResult;
}

template<typename ValueType, bool Nondeterministic>
std::vector<ValueType> SparseLTLHelper<ValueType, Nondeterministic>::computeLTLProbabilities(Environment const& env, storm::logic::PathFormula const& formula,
                                                                                             std::map<std::string, storm::storage::BitVector>& apSatSets) {
    std::shared_ptr<storm::logic::Formula const> ltlFormula;
    STORM_LOG_THROW((!Nondeterministic) || this->isOptimizationDirectionSet(), storm::exceptions::InvalidPropertyException,
                    "Formula needs to specify whether minimal or maximal values are to be computed on nondeterministic model.");
    if (Nondeterministic && this->getOptimizationDirection() == OptimizationDirection::Minimize) {
        // negate formula in order to compute 1-Pmax[!formula]
        ltlFormula = std::make_shared<storm::logic::UnaryBooleanPathFormula>(storm::logic::UnaryBooleanOperatorType::Not, formula.asSharedPointer());
        STORM_LOG_INFO("Computing Pmin, proceeding with negated LTL formula.");
    } else {
        ltlFormula = formula.asSharedPointer();
    }

    STORM_LOG_INFO("Resulting LTL path formula: " << ltlFormula->toString());
    STORM_LOG_INFO(" in prefix format: " << ltlFormula->toPrefixString());

    // Convert LTL formula to a deterministic automaton
    std::shared_ptr<storm::automata::DeterministicAutomaton> da;
    if (env.modelchecker().isLtl2daToolSet()) {
        // Use the external tool given via ltl2da
        da = storm::automata::LTL2DeterministicAutomaton::ltl2daExternalTool(*ltlFormula, env.modelchecker().getLtl2daTool());
    } else {
        // Use the internal tool (Spot)
        // For nondeterministic models the acceptance condition is transformed into DNF
        da = storm::automata::LTL2DeterministicAutomaton::ltl2daSpot(*ltlFormula, Nondeterministic);
    }

    STORM_LOG_INFO("Deterministic automaton for LTL formula has " << da->getNumberOfStates() << " states, " << da->getAPSet().size()
                                                                  << " atomic propositions and " << *da->getAcceptance()->getAcceptanceExpression()
                                                                  << " as acceptance condition.\n");

    std::vector<ValueType> numericResult = computeDAProductProbabilities(env, *da, apSatSets);

    if (Nondeterministic && this->getOptimizationDirection() == OptimizationDirection::Minimize) {
        // compute 1-Pmax[!fomula]
        for (auto& value : numericResult) {
            value = storm::utility::one<ValueType>() - value;
        }
    }

    return numericResult;
}

template class SparseLTLHelper<double, false>;
template class SparseLTLHelper<double, true>;

template class SparseLTLHelper<storm::RationalNumber, false>;
template class SparseLTLHelper<storm::RationalNumber, true>;
template class SparseLTLHelper<storm::RationalFunction, false>;

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
