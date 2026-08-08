#include "DFTASFChecker.h"

#include <string>

#include "storm-dft/modelchecker/SmtConstraint.h"
#include "storm-parsers/parser/ExpressionCreator.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/io/file.h"
#include "storm/solver/SmtSolver.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Type.h"
#include "storm/utility/bitoperations.h"

namespace storm::dft {
namespace modelchecker {

DFTASFChecker::DFTASFChecker(storm::dft::storage::DFT<ValueType> const &dft) : dft(dft) {
    // Intentionally left empty.
}

uint64_t DFTASFChecker::getClaimVariableIndex(uint64_t spare, uint64_t child) const {
    return claimVariables.at(SpareAndChildPair(spare, child));
}

void DFTASFChecker::convert() {
    std::vector<uint64_t> beVariables;
    uint64_t failedBeVariables;
    std::vector<uint64_t> failsafeBeVariables;
    bool failedBeIsSet = false;
    notFailed = dft.nrBasicElements() + 1;  // Value indicating the element is not failed

    // Initialize variables
    for (size_t i = 0; i < dft.nrElements(); ++i) {
        std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element = dft.getElement(i);
        varNames.push_back("t_" + element->name());
        timePointVariables.emplace(i, varNames.size() - 1);
        switch (element->type()) {
            case storm::dft::storage::elements::DFTElementType::BE: {
                auto be = std::static_pointer_cast<storm::dft::storage::elements::DFTBE<double> const>(element);
                switch (be->beType()) {
                    case storm::dft::storage::elements::BEType::EXPONENTIAL:
                        beVariables.push_back(varNames.size() - 1);
                        break;
                    case storm::dft::storage::elements::BEType::CONSTANT: {
                        STORM_LOG_WARN("Constant BEs are only experimentally supported in the SMT encoding");
                        // Constant BEs are initially either failed or failsafe, treat them differently
                        auto be = std::static_pointer_cast<storm::dft::storage::elements::BEConst<double> const>(element);
                        if (be->failed()) {
                            STORM_LOG_THROW(!failedBeIsSet, storm::exceptions::NotSupportedException,
                                            "DFTs containing more than one constantly failed BE are not supported.");
                            notFailed = dft.nrBasicElements();
                            failedBeVariables = varNames.size() - 1;
                            failedBeIsSet = true;
                        } else {
                            failsafeBeVariables.push_back(varNames.size() - 1);
                        }
                        break;
                    }
                    default:
                        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "BE type '" << be->beType() << "' not known.");
                        break;
                }
                break;
            }
            case storm::dft::storage::elements::DFTElementType::SPARE: {
                auto spare = std::static_pointer_cast<storm::dft::storage::elements::DFTSpare<double> const>(element);
                for (auto const &spareChild : spare->children()) {
                    varNames.push_back("c_" + element->name() + "_" + spareChild->name());
                    claimVariables.emplace(SpareAndChildPair(element->id(), spareChild->id()), varNames.size() - 1);
                }
                break;
            }
            case storm::dft::storage::elements::DFTElementType::PDEP: {
                varNames.push_back("dep_" + element->name());
                dependencyVariables.emplace(element->id(), varNames.size() - 1);
                break;
            }
            default:
                break;
        }
    }
    // Initialize variables indicating Markovian states
    for (size_t i = 0; i < dft.nrBasicElements(); ++i) {
        varNames.push_back("m_" + std::to_string(i));
        markovianVariables.emplace(i, varNames.size() - 1);
    }

    // Generate constraints

    // All exponential BEs have to fail (first part of constraint 12)
    for (auto const &beV : beVariables) {
        constraints.push_back(std::make_shared<BetweenValues>(beV, 1, notFailed - 1));
    }

    // Constantly failsafe BEs may also be fail-safe
    for (auto const &beV : failsafeBeVariables) {
        constraints.push_back(std::make_shared<BetweenValues>(beV, 1, notFailed));
    }

    // Constantly failed BEs fail before other types
    if (failedBeIsSet) {
        constraints.push_back(std::make_shared<IsConstantValue>(failedBeVariables, 0));
    }

    std::vector<uint64_t> allBeVariables;
    allBeVariables.insert(std::end(allBeVariables), std::begin(beVariables), std::end(beVariables));
    allBeVariables.insert(std::end(allBeVariables), std::begin(failsafeBeVariables), std::end(failsafeBeVariables));

    // No two exponential BEs fail at the same time (second part of constraint 12)
    if (beVariables.size() > 1) {
        constraints.push_back(std::make_shared<PairwiseDifferent>(beVariables));
        constraints.back()->setDescription("No two BEs fail at the same time");
    }

    bool descFlag = true;
    for (auto const &failsafeBe : failsafeBeVariables) {
        std::vector<std::shared_ptr<SmtConstraint>> unequalConstraints;
        for (auto const &otherBe : allBeVariables) {
            if (otherBe != failsafeBe) {
                unequalConstraints.push_back(std::make_shared<IsUnequal>(failsafeBe, otherBe));
            }
        }
        constraints.push_back(
            std::make_shared<Implies>(std::make_shared<IsNotConstantValue>(failsafeBe, notFailed), std::make_shared<And>(unequalConstraints)));
        if (descFlag) {
            constraints.back()->setDescription("Initially failsafe BEs don't fail at the same time as other BEs");
            descFlag = false;
        }
    }

    // Initialize claim variables in [1, |BE|+1]
    for (auto const &claimVariable : claimVariables) {
        constraints.push_back(std::make_shared<BetweenValues>(claimVariable.second, 0, notFailed));
    }

    // Encoding for gates
    for (size_t i = 0; i < dft.nrElements(); ++i) {
        std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element = dft.getElement(i);
        STORM_LOG_ASSERT(i == element->id(), "Id and index should match.");

        // Get indices for gate children
        std::vector<uint64_t> childVarIndices;
        if (element->isGate()) {
            std::shared_ptr<storm::dft::storage::elements::DFTGate<ValueType> const> gate = dft.getGate(i);
            for (auto const &child : gate->children()) {
                childVarIndices.push_back(timePointVariables.at(child->id()));
            }
        }

        switch (element->type()) {
            case storm::dft::storage::elements::DFTElementType::BE:
                // BEs were already considered before
                break;
            case storm::dft::storage::elements::DFTElementType::AND:
                generateAndConstraint(i, childVarIndices, element);
                break;
            case storm::dft::storage::elements::DFTElementType::OR:
                generateOrConstraint(i, childVarIndices, element);
                break;
            case storm::dft::storage::elements::DFTElementType::VOT:
                generateVotConstraint(i, childVarIndices, element);
                break;
            case storm::dft::storage::elements::DFTElementType::PAND:
                generatePandConstraint(i, childVarIndices, element);
                break;
            case storm::dft::storage::elements::DFTElementType::POR:
                generatePorConstraint(i, childVarIndices, element);
                break;
            case storm::dft::storage::elements::DFTElementType::SEQ:
                generateSeqConstraint(childVarIndices, element);
                break;
            case storm::dft::storage::elements::DFTElementType::SPARE:
                generateSpareConstraint(i, childVarIndices, element);
                break;
            case storm::dft::storage::elements::DFTElementType::PDEP:
                generatePdepConstraint(i, childVarIndices, element);
                break;
            default:
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "SMT encoding for type '" << element->type() << "' is not supported.");
                break;
        }
    }

    // Constraint (8) intentionally commented out for testing purposes
    // addClaimingConstraints();

    // Handle dependencies
    addMarkovianConstraints();

    // Failsafe BEs may only fail in non-Markovian states (i.e. if they were triggered)
    std::vector<std::shared_ptr<SmtConstraint>> failsafeNotIConstr;
    for (uint64_t i = 0; i < dft.nrBasicElements(); ++i) {
        failsafeNotIConstr.clear();
        for (auto const &beV : failsafeBeVariables) {
            failsafeNotIConstr.push_back(std::make_shared<IsNotConstantValue>(beV, i + 1));
        }
        // If state i+1 is Markovian (i.e. m_i = true), all failsafeBEVariables are not equal to i+1
        constraints.push_back(
            std::make_shared<Implies>(std::make_shared<IsBoolValue>(markovianVariables.at(i), true), std::make_shared<And>(failsafeNotIConstr)));
        if (i == 0) {
            constraints.back()->setDescription("Failsafe BEs fail only if they are triggered");
        }
    }

    // A failsafe BE only stays failsafe if no trigger has been triggered
    std::vector<std::shared_ptr<SmtConstraint>> triggerConstraints;
    for (size_t i = 0; i < dft.nrElements(); ++i) {
        std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element = dft.getElement(i);
        if (element->isBasicElement()) {
            auto be = std::static_pointer_cast<storm::dft::storage::elements::DFTBE<double> const>(element);
            if (be->beType() == storm::dft::storage::elements::BEType::CONSTANT) {
                triggerConstraints.clear();
                for (auto const &dependency : be->ingoingDependencies()) {
                    triggerConstraints.push_back(std::make_shared<IsConstantValue>(timePointVariables.at(dependency->triggerEvent()->id()), notFailed));
                }
                if (!triggerConstraints.empty()) {
                    constraints.push_back(std::make_shared<Implies>(std::make_shared<IsConstantValue>(timePointVariables.at(be->id()), notFailed),
                                                                    std::make_shared<And>(triggerConstraints)));
                    constraints.back()->setDescription("Failsafe BE " + be->name() + " stays failsafe if no trigger fails");
                }
            }
        }
    }
}

// Constraint Generator Functions

void DFTASFChecker::generateAndConstraint(size_t i, std::vector<uint64_t> childVarIndices,
                                          std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element) {
    // Constraint for AND gate (constraint 1)
    constraints.push_back(std::make_shared<IsMaximum>(timePointVariables.at(i), childVarIndices));
    constraints.back()->setDescription("AND gate " + element->name());
}

void DFTASFChecker::generateOrConstraint(size_t i, std::vector<uint64_t> childVarIndices,
                                         std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element) {
    // Constraint for OR gate (constraint 2)
    constraints.push_back(std::make_shared<IsMinimum>(timePointVariables.at(i), childVarIndices));
    constraints.back()->setDescription("OR gate " + element->name());
}

void DFTASFChecker::generateVotConstraint(size_t i, std::vector<uint64_t> childVarIndices,
                                          std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element) {
    auto vot = std::static_pointer_cast<storm::dft::storage::elements::DFTVot<double> const>(element);
    // VOTs are implemented via OR over ANDs with all possible combinations
    std::vector<uint64_t> tmpVars;
    size_t k = 0;
    // Generate all permutations of k out of n
    uint64_t combination = smallestIntWithNBitsSet(static_cast<uint64_t>(vot->threshold()));
    do {
        // Construct selected children from combination
        std::vector<uint64_t> combinationChildren;
        STORM_LOG_ASSERT(vot->nrChildren() < 64, "Too many children of a VOT Gate.");
        for (uint8_t j = 0; j < static_cast<uint8_t>(vot->nrChildren()); ++j) {
            if (combination & (1ul << j)) {
                combinationChildren.push_back(childVarIndices.at(j));
            }
        }
        // Introduce temporary variable for this AND
        varNames.push_back("v_" + vot->name() + "_" + std::to_string(k));
        size_t index = varNames.size() - 1;
        tmpVars.push_back(index);
        tmpTimePointVariables.push_back(index);
        // AND over the selected children
        constraints.push_back(std::make_shared<IsMaximum>(index, combinationChildren));
        constraints.back()->setDescription("VOT gate " + element->name() + ": AND no. " + std::to_string(k));
        // Generate next permutation
        combination = nextBitPermutation(combination);
        ++k;
    } while (combination < (1ul << vot->nrChildren()) && combination != 0);

    // Constraint is OR over all possible combinations
    constraints.push_back(std::make_shared<IsMinimum>(timePointVariables.at(i), tmpVars));
    constraints.back()->setDescription("VOT gate " + element->name() + ": OR");
}

void DFTASFChecker::generatePandConstraint(size_t i, std::vector<uint64_t> childVarIndices,
                                           std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element) {
    // Constraint for PAND gate (constraint 3)
    std::shared_ptr<SmtConstraint> ifC = std::make_shared<Sorted>(childVarIndices);
    std::shared_ptr<SmtConstraint> thenC = std::make_shared<IsEqual>(timePointVariables.at(i), childVarIndices.back());
    std::shared_ptr<SmtConstraint> elseC = std::make_shared<IsConstantValue>(timePointVariables.at(i), notFailed);
    constraints.push_back(std::make_shared<IfThenElse>(ifC, thenC, elseC));
    constraints.back()->setDescription("PAND gate " + element->name());
}

void DFTASFChecker::generatePorConstraint(size_t i, std::vector<uint64_t> childVarIndices,
                                          std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element) {
    // Constraint for POR gate
    // First child fails before all others
    std::vector<std::shared_ptr<SmtConstraint>> firstSmallestC;
    uint64_t timeFirstChild = childVarIndices.front();
    for (uint64_t i = 1; i < childVarIndices.size(); ++i) {
        firstSmallestC.push_back(std::make_shared<IsLess>(timeFirstChild, childVarIndices.at(i)));
    }
    std::shared_ptr<SmtConstraint> ifC = std::make_shared<And>(firstSmallestC);
    std::shared_ptr<SmtConstraint> thenC = std::make_shared<IsEqual>(timePointVariables.at(i), childVarIndices.front());
    std::shared_ptr<SmtConstraint> elseC = std::make_shared<IsConstantValue>(timePointVariables.at(i), notFailed);
    constraints.push_back(std::make_shared<IfThenElse>(ifC, thenC, elseC));
    constraints.back()->setDescription("POR gate " + element->name());
}

void DFTASFChecker::generateSeqConstraint(std::vector<uint64_t> childVarIndices,
                                          std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element) {
    // Constraint for SEQ gate (constraint 4)
    // As the restriction is not a gate we have to enumerate its children here
    auto seq = std::static_pointer_cast<storm::dft::storage::elements::DFTRestriction<double> const>(element);
    for (auto const &child : seq->children()) {
        childVarIndices.push_back(timePointVariables.at(child->id()));
    }

    constraints.push_back(std::make_shared<Sorted>(childVarIndices));
    constraints.back()->setDescription("SEQ gate " + element->name());
}

void DFTASFChecker::generateSpareConstraint(size_t i, std::vector<uint64_t> childVarIndices,
                                            std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element) {
    auto spare = std::static_pointer_cast<storm::dft::storage::elements::DFTSpare<double> const>(element);
    auto const &children = spare->children();
    uint64_t firstChild = children.front()->id();
    uint64_t lastChild = children.back()->id();

    // First child of each spare is claimed in the beginning
    constraints.push_back(std::make_shared<IsConstantValue>(getClaimVariableIndex(spare->id(), firstChild), 0));
    constraints.back()->setDescription("SPARE gate " + spare->name() + " claims first child");

    // If last child is claimed before failure, then the spare fails when the last child fails (constraint 5)
    std::shared_ptr<SmtConstraint> leftC = std::make_shared<IsLess>(getClaimVariableIndex(spare->id(), lastChild), childVarIndices.back());
    constraints.push_back(std::make_shared<Implies>(leftC, std::make_shared<IsEqual>(timePointVariables.at(i), childVarIndices.back())));
    constraints.back()->setDescription("Last child & claimed -> SPARE fails");

    // Construct constraint for trying to claim next child
    STORM_LOG_ASSERT(children.size() >= 2, "Spare has only one child.");
    for (uint64_t currChild = 0; currChild < children.size() - 1; ++currChild) {
        uint64_t timeCurrChild = childVarIndices.at(currChild);  // Moment when current child fails
        // If i-th child fails after being claimed, then try to claim next child (constraint 6)
        std::shared_ptr<SmtConstraint> tryClaimC = generateTryToClaimConstraint(spare, currChild + 1, timeCurrChild);
        constraints.push_back(
            std::make_shared<Iff>(std::make_shared<IsLess>(getClaimVariableIndex(spare->id(), children.at(currChild)->id()), timeCurrChild), tryClaimC));
        constraints.back()->setDescription("Try to claim " + std::to_string(currChild + 2) + "th child");
    }
}

std::shared_ptr<SmtConstraint> DFTASFChecker::generateTryToClaimConstraint(std::shared_ptr<storm::dft::storage::elements::DFTSpare<ValueType> const> spare,
                                                                           uint64_t childIndex, uint64_t timepoint) const {
    auto child = spare->children().at(childIndex);
    uint64_t timeChild = timePointVariables.at(child->id());                // Moment when the child fails
    uint64_t claimChild = getClaimVariableIndex(spare->id(), child->id());  // Moment the spare claims the child

    std::vector<std::shared_ptr<SmtConstraint>> noClaimingPossible;
    // Child cannot be claimed.
    if (childIndex + 1 < spare->children().size()) {
        // Consider next child for claiming (second case in constraint 7)
        noClaimingPossible.push_back(generateTryToClaimConstraint(spare, childIndex + 1, timepoint));
    } else {
        // Last child: spare fails at same point as this child (third case in constraint 7)
        noClaimingPossible.push_back(std::make_shared<IsEqual>(timePointVariables.at(spare->id()), timepoint));
    }
    std::shared_ptr<SmtConstraint> elseCaseC = std::make_shared<And>(noClaimingPossible);

    // Check if next child is available (first case in constraint 7)
    std::vector<std::shared_ptr<SmtConstraint>> claimingPossibleC;
    // Next child is not yet failed
    claimingPossibleC.push_back(std::make_shared<IsLess>(timepoint, timeChild));
    // Child is not yet claimed by a different spare
    for (auto const &otherSpare : child->parents()) {
        if (otherSpare->id() == spare->id()) {
            // not a different spare.
            continue;
        }
        claimingPossibleC.push_back(std::make_shared<IsLess>(timepoint, getClaimVariableIndex(otherSpare->id(), child->id())));
    }

    // Claim child if available
    std::shared_ptr<SmtConstraint> firstCaseC =
        std::make_shared<IfThenElse>(std::make_shared<And>(claimingPossibleC), std::make_shared<IsEqual>(claimChild, timepoint), elseCaseC);
    return firstCaseC;
}

void DFTASFChecker::addClaimingConstraints() {
    // Only one spare can claim a child (constraint 8)
    // and only not failed children can be claimed (addition to constrain 8)
    for (size_t i = 0; i < dft.nrElements(); ++i) {
        std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element = dft.getElement(i);
        if (element->isSpareGate()) {
            auto spare = std::static_pointer_cast<storm::dft::storage::elements::DFTSpare<double> const>(element);
            for (auto const &child : spare->children()) {
                std::vector<std::shared_ptr<SmtConstraint>> additionalC;
                uint64_t timeClaiming = getClaimVariableIndex(spare->id(), child->id());
                std::shared_ptr<SmtConstraint> leftC = std::make_shared<IsLessConstant>(timeClaiming, notFailed);
                // Child must be operational at time of claiming
                additionalC.push_back(std::make_shared<IsLess>(timeClaiming, timePointVariables.at(child->id())));
                // No other spare claims this child
                for (auto const &parent : child->parents()) {
                    if (parent->isSpareGate() && parent->id() != spare->id()) {
                        // Different spare
                        additionalC.push_back(std::make_shared<IsConstantValue>(getClaimVariableIndex(parent->id(), child->id()), notFailed));
                    }
                }
                constraints.push_back(std::make_shared<Implies>(leftC, std::make_shared<And>(additionalC)));
                constraints.back()->setDescription("Child " + child->name() + " must be operational at time of claiming by spare " + spare->name() +
                                                   " and can only be claimed by one spare.");
            }
        }
    }
}

void DFTASFChecker::generatePdepConstraint(size_t i, std::vector<uint64_t> childVarIndices,
                                           std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element) {
    auto dependency = std::static_pointer_cast<storm::dft::storage::elements::DFTDependency<double> const>(element);
    auto const &dependentEvents = dependency->dependentEvents();
    auto const &trigger = dependency->triggerEvent();
    std::vector<uint64_t> dependentIndices;
    for (size_t j = 0; j < dependentEvents.size(); ++j) {
        dependentIndices.push_back(timePointVariables.at(dependentEvents[j]->id()));
    }

    constraints.push_back(std::make_shared<IsMaximum>(dependencyVariables.at(i), dependentIndices));
    constraints.back()->setDescription("Dependency " + element->name() + ": Last element");
    constraints.push_back(std::make_shared<IsEqual>(timePointVariables.at(i), timePointVariables.at(trigger->id())));
    constraints.back()->setDescription("Dependency " + element->name() + ": Trigger element");
}

void DFTASFChecker::addMarkovianConstraints() {
    uint64_t nrMarkovian = dft.nrBasicElements();
    std::set<size_t> depElements;
    // Vector containing (non-)Markovian constraints for each timepoint
    std::vector<std::vector<std::shared_ptr<SmtConstraint>>> markovianC(nrMarkovian);
    std::vector<std::vector<std::shared_ptr<SmtConstraint>>> nonMarkovianC(nrMarkovian);
    std::vector<std::vector<std::shared_ptr<SmtConstraint>>> notColdC(nrMarkovian);

    // All dependent events of a failed trigger have failed as well (constraint 9)
    for (size_t j = 0; j < dft.nrElements(); ++j) {
        std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element = dft.getElement(j);
        if (element->hasOutgoingDependencies()) {
            for (uint64_t i = 0; i < nrMarkovian; ++i) {
                std::shared_ptr<SmtConstraint> triggerFailed = std::make_shared<IsLessEqualConstant>(timePointVariables.at(j), i);
                std::vector<std::shared_ptr<SmtConstraint>> depFailed;
                for (auto const &dependency : element->outgoingDependencies()) {
                    for (auto const &depElement : dependency->dependentEvents()) {
                        depFailed.push_back(std::make_shared<IsLessEqualConstant>(timePointVariables.at(depElement->id()), i));
                    }
                }
                markovianC[i].push_back(std::make_shared<Implies>(triggerFailed, std::make_shared<And>(depFailed)));
            }
        }
    }
    for (uint64_t i = 0; i < nrMarkovian; ++i) {
        constraints.push_back(std::make_shared<Iff>(std::make_shared<IsBoolValue>(markovianVariables.at(i), true), std::make_shared<And>(markovianC[i])));
        constraints.back()->setDescription("Markovian (" + std::to_string(i) + ") iff all dependent events which trigger failed also failed.");
    }

    // In non-Markovian steps the next failed element is a dependent BE (constraint 10) + additions to specification in paper
    for (size_t j = 0; j < dft.nrElements(); ++j) {
        std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element = dft.getElement(j);
        if (element->isBasicElement()) {
            auto be = std::static_pointer_cast<storm::dft::storage::elements::DFTBE<double> const>(element);

            if (be->hasIngoingDependencies()) {
                depElements.emplace(j);
                for (uint64_t i = 0; i < nrMarkovian - 1; ++i) {
                    std::shared_ptr<SmtConstraint> nextFailure = std::make_shared<IsConstantValue>(timePointVariables.at(j), i + 1);
                    std::vector<std::shared_ptr<SmtConstraint>> triggerFailed;
                    for (auto const &dependency : be->ingoingDependencies()) {
                        triggerFailed.push_back(std::make_shared<IsLessEqualConstant>(timePointVariables.at(dependency->triggerEvent()->id()), i));
                    }
                    nonMarkovianC[i].push_back(std::make_shared<Implies>(nextFailure, std::make_shared<Or>(triggerFailed)));
                }
            }
        }
    }
    for (uint64_t i = 0; i < nrMarkovian; ++i) {
        std::vector<std::shared_ptr<SmtConstraint>> dependentConstr;
        for (auto dependentEvent : depElements) {
            std::shared_ptr<SmtConstraint> nextFailure = std::make_shared<IsConstantValue>(timePointVariables.at(dependentEvent), i + 1);
            dependentConstr.push_back(nextFailure);
        }
        // Add Constraint that any DEPENDENT event has to fail next
        nonMarkovianC[i].push_back(std::make_shared<Or>(dependentConstr));
        constraints.push_back(
            std::make_shared<Implies>(std::make_shared<IsBoolValue>(markovianVariables.at(i), false), std::make_shared<And>(nonMarkovianC[i])));
        constraints.back()->setDescription("Non-Markovian (" + std::to_string(i) + ") -> next failure is dependent BE.");
    }

    // In Markovian steps the failure rate is positive (constraint 11)
    for (size_t j = 0; j < dft.nrElements(); ++j) {
        std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element = dft.getElement(j);
        if (element->isBasicElement()) {
            auto be = std::static_pointer_cast<storm::dft::storage::elements::DFTBE<double> const>(element);
            for (uint64_t i = 0; i < nrMarkovian; ++i) {
                std::shared_ptr<SmtConstraint> nextFailure = std::make_shared<IsConstantValue>(timePointVariables.at(j), i + 1);
                // BE is not cold
                // TODO: implement use of activation variables here
                notColdC[i].push_back(std::make_shared<Implies>(nextFailure, std::make_shared<IsTrue>(be->canFail())));
            }
        }
    }
    for (uint64_t i = 0; i < nrMarkovian; ++i) {
        constraints.push_back(std::make_shared<Implies>(std::make_shared<IsBoolValue>(markovianVariables.at(i), true), std::make_shared<And>(notColdC[i])));
        constraints.back()->setDescription("Markovian (" + std::to_string(i) + ") -> positive failure rate.");
    }
}

void DFTASFChecker::toFile(std::string const &filename) {
    std::ofstream stream;
    storm::io::openFile(filename, stream);
    stream << "; time point variables\n";
    for (auto const &timeVarEntry : timePointVariables) {
        stream << "(declare-fun " << varNames[timeVarEntry.second] << "() Int)\n";
    }
    stream << "; claim variables\n";
    for (auto const &claimVarEntry : claimVariables) {
        stream << "(declare-fun " << varNames[claimVarEntry.second] << "() Int)\n";
    }
    stream << "; Markovian variables\n";
    for (auto const &markovianVarEntry : markovianVariables) {
        stream << "(declare-fun " << varNames[markovianVarEntry.second] << "() Bool)\n";
    }
    stream << "; Dependency variables\n";
    for (auto const &depVarEntry : dependencyVariables) {
        stream << "(declare-fun " << varNames[depVarEntry.second] << "() Int)\n";
    }
    if (!tmpTimePointVariables.empty()) {
        stream << "; Temporary variables\n";
        for (auto const &tmpVar : tmpTimePointVariables) {
            stream << "(declare-fun " << varNames[tmpVar] << "() Int)\n";
        }
    }
    for (auto const &constraint : constraints) {
        if (!constraint->description().empty()) {
            stream << "; " << constraint->description() << '\n';
        }
        stream << "(assert " << constraint->toSmtlib2(varNames) << ")\n";
    }
    stream << "(check-sat)\n";
    storm::io::closeFile(stream);
}

void DFTASFChecker::toSolver() {
    // First convert the DFT
    convert();

    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    solver = storm::utility::solver::SmtSolverFactory().create(*manager);
    // Add variables to manager
    for (auto const &timeVarEntry : timePointVariables) {
        manager->declareIntegerVariable(varNames[timeVarEntry.second]);
    }
    for (auto const &claimVarEntry : claimVariables) {
        manager->declareIntegerVariable(varNames[claimVarEntry.second]);
    }
    for (auto const &markovianVarEntry : markovianVariables) {
        manager->declareBooleanVariable(varNames[markovianVarEntry.second]);
    }
    if (!tmpTimePointVariables.empty()) {
        for (auto const &tmpVar : tmpTimePointVariables) {
            manager->declareIntegerVariable(varNames[tmpVar]);
        }
    }
    for (auto const &depVarEntry : dependencyVariables) {
        manager->declareIntegerVariable(varNames[depVarEntry.second]);
    }
    // Add constraints to solver
    for (auto const &constraint : constraints) {
        solver->add(constraint->toExpression(varNames, manager));
    }
}

storm::solver::SmtSolver::CheckResult DFTASFChecker::checkTleFailsWithEq(uint64_t bound) {
    STORM_LOG_ASSERT(solver, "SMT Solver was not initialized, call toSolver() before checking queries.");

    // Set backtracking marker to check several properties without reconstructing DFT encoding
    solver->push();
    // Constraint that toplevel element can fail with less or equal 'bound' failures
    std::shared_ptr<SmtConstraint> tleFailedConstr = std::make_shared<IsConstantValue>(timePointVariables.at(dft.getTopLevelIndex()), bound);
    std::shared_ptr<storm::expressions::ExpressionManager> manager = solver->getManager().getSharedPointer();
    solver->add(tleFailedConstr->toExpression(varNames, manager));
    storm::solver::SmtSolver::CheckResult res = solver->check();
    solver->pop();
    return res;
}

storm::solver::SmtSolver::CheckResult DFTASFChecker::checkTleFailsWithLeq(uint64_t bound) {
    STORM_LOG_ASSERT(solver, "SMT Solver was not initialized, call toSolver() before checking queries.");

    // Set backtracking marker to check several properties without reconstructing DFT encoding
    solver->push();
    // Constraint that toplevel element can fail with less or equal 'bound' failures
    std::shared_ptr<SmtConstraint> tleNeverFailedConstr = std::make_shared<IsLessEqualConstant>(timePointVariables.at(dft.getTopLevelIndex()), bound);
    std::shared_ptr<storm::expressions::ExpressionManager> manager = solver->getManager().getSharedPointer();
    solver->add(tleNeverFailedConstr->toExpression(varNames, manager));
    storm::solver::SmtSolver::CheckResult res = solver->check();
    solver->pop();
    return res;
}

void DFTASFChecker::setSolverTimeout(uint_fast64_t milliseconds) {
    STORM_LOG_ASSERT(solver, "SMT Solver was not initialized, timeout cannot be set.");
    solver->setTimeout(milliseconds);
}

void DFTASFChecker::unsetSolverTimeout() {
    STORM_LOG_ASSERT(solver, "SMT Solver was not initialized, timeout cannot be unset.");
    solver->unsetTimeout();
}

storm::solver::SmtSolver::CheckResult DFTASFChecker::checkTleNeverFailed() {
    STORM_LOG_ASSERT(solver, "SMT Solver was not initialized, call toSolver() before checking queries.");
    return checkTleFailsWithEq(notFailed);
}

storm::solver::SmtSolver::CheckResult DFTASFChecker::checkFailsLeqWithEqNonMarkovianState(uint64_t checkbound, uint64_t nrNonMarkovian) {
    STORM_LOG_ASSERT(solver, "SMT Solver was not initialized, call toSolver() before checking queries.");
    std::vector<uint64_t> markovianIndices;
    checkbound = std::min<int>(checkbound, markovianVariables.size());
    // Get Markovian variable indices up until given timepoint
    for (uint64_t i = 0; i < checkbound; ++i) {
        markovianIndices.push_back(markovianVariables.at(i));
    }
    // Set backtracking marker to check several properties without reconstructing DFT encoding
    solver->push();
    // Constraint that TLE fails before or during given timepoint
    std::shared_ptr<SmtConstraint> tleFailedConstr = std::make_shared<IsLessEqualConstant>(timePointVariables.at(dft.getTopLevelIndex()), checkbound);
    std::shared_ptr<storm::expressions::ExpressionManager> manager = solver->getManager().getSharedPointer();
    solver->add(tleFailedConstr->toExpression(varNames, manager));
    // Constraint that a given number of non-Markovian states are visited
    std::shared_ptr<SmtConstraint> nonMarkovianConstr = std::make_shared<FalseCountIsEqualConstant>(markovianIndices, nrNonMarkovian);
    solver->add(nonMarkovianConstr->toExpression(varNames, manager));
    storm::solver::SmtSolver::CheckResult res = solver->check();
    solver->pop();
    return res;
}

storm::solver::SmtSolver::CheckResult DFTASFChecker::checkFailsAtTimepointWithEqNonMarkovianState(uint64_t timepoint, uint64_t nrNonMarkovian) {
    STORM_LOG_ASSERT(solver, "SMT Solver was not initialized, call toSolver() before checking queries.");
    std::vector<uint64_t> markovianIndices;
    timepoint = std::min<int>(timepoint, markovianVariables.size());
    // Get Markovian variable indices up until given timepoint
    for (uint64_t i = 0; i < timepoint; ++i) {
        markovianIndices.push_back(markovianVariables.at(i));
    }
    // Set backtracking marker to check several properties without reconstructing DFT encoding
    solver->push();
    // Constraint that TLE fails before or during given timepoint
    std::shared_ptr<SmtConstraint> tleFailedConstr = std::make_shared<IsConstantValue>(timePointVariables.at(dft.getTopLevelIndex()), timepoint);
    std::shared_ptr<storm::expressions::ExpressionManager> manager = solver->getManager().getSharedPointer();
    solver->add(tleFailedConstr->toExpression(varNames, manager));
    // Constraint that a given number of non-Markovian states are visited
    std::shared_ptr<SmtConstraint> nonMarkovianConstr = std::make_shared<FalseCountIsEqualConstant>(markovianIndices, nrNonMarkovian);
    solver->add(nonMarkovianConstr->toExpression(varNames, manager));
    storm::solver::SmtSolver::CheckResult res = solver->check();
    solver->pop();
    return res;
}

storm::solver::SmtSolver::CheckResult DFTASFChecker::checkDependencyConflict(uint64_t dep1Index, uint64_t dep2Index, uint64_t timeout) {
    std::vector<std::shared_ptr<SmtConstraint>> andConstr;
    std::vector<std::shared_ptr<SmtConstraint>> orConstr;
    STORM_LOG_DEBUG("Check " << dft.getElement(dep1Index)->name() << " and " << dft.getElement(dep2Index)->name());
    andConstr.clear();
    // AND FDEP1 is triggered before FDEP2 is resolved
    andConstr.push_back(std::make_shared<IsLessEqual>(timePointVariables.at(dep2Index), timePointVariables.at(dep1Index)));
    andConstr.push_back(std::make_shared<IsLess>(timePointVariables.at(dep1Index), dependencyVariables.at(dep2Index)));
    std::shared_ptr<SmtConstraint> betweenConstr1 = std::make_shared<And>(andConstr);

    andConstr.clear();
    // AND FDEP2 is triggered before FDEP1 is resolved
    andConstr.push_back(std::make_shared<IsLessEqual>(timePointVariables.at(dep1Index), timePointVariables.at(dep2Index)));
    andConstr.push_back(std::make_shared<IsLess>(timePointVariables.at(dep2Index), dependencyVariables.at(dep1Index)));
    std::shared_ptr<SmtConstraint> betweenConstr2 = std::make_shared<And>(andConstr);

    orConstr.clear();
    // Either one of the above constraints holds
    orConstr.push_back(betweenConstr1);
    orConstr.push_back(betweenConstr2);

    // Both FDEPs were triggered before dependent elements have failed
    andConstr.clear();
    andConstr.push_back(std::make_shared<IsLess>(timePointVariables.at(dep1Index), dependencyVariables.at(dep1Index)));
    andConstr.push_back(std::make_shared<IsLess>(timePointVariables.at(dep2Index), dependencyVariables.at(dep2Index)));
    andConstr.push_back(std::make_shared<Or>(orConstr));

    std::shared_ptr<SmtConstraint> checkConstr = std::make_shared<And>(andConstr);

    std::shared_ptr<storm::expressions::ExpressionManager> manager = solver->getManager().getSharedPointer();
    solver->push();
    solver->add(checkConstr->toExpression(varNames, manager));
    setSolverTimeout(timeout * 1000);
    storm::solver::SmtSolver::CheckResult res = solver->check();
    unsetSolverTimeout();
    solver->pop();
    return res;
}

}  // namespace modelchecker
}  // namespace storm::dft
