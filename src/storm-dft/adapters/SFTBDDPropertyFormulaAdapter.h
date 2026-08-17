#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <memory>

#include "storm-dft/modelchecker/SFTBDDChecker.h"
#include "storm-dft/storage/DFT.h"
#include "storm-dft/storage/SylvanBddManager.h"
#include "storm/environment/Environment.h"
#include "storm/logic/AtomicLabelFormula.h"
#include "storm/logic/BinaryBooleanStateFormula.h"
#include "storm/logic/BoundedUntilFormula.h"
#include "storm/logic/ProbabilityOperatorFormula.h"
#include "storm/logic/StateFormula.h"
#include "storm/logic/UnaryBooleanStateFormula.h"

namespace storm::dft {
namespace adapters {

class SFTBDDPropertyFormulaAdapter {
    using ValueType = double;
#ifdef STORM_HAVE_SYLVAN
    using Bdd = sylvan::Bdd;
#endif
    using FormulaCPointer = std::shared_ptr<storm::logic::Formula const>;
    using StateFormulaCPointer = std::shared_ptr<storm::logic::StateFormula const>;
    using UnaryStateFormulaCPointer = std::shared_ptr<storm::logic::UnaryBooleanStateFormula const>;
    using BinaryStateFormulaCPointer = std::shared_ptr<storm::logic::BinaryBooleanStateFormula const>;
    using AtomicLabelFormulaCPointer = std::shared_ptr<storm::logic::AtomicLabelFormula const>;
    using FormulaVector = std::vector<FormulaCPointer>;

   public:
#ifdef STORM_HAVE_SYLVAN
    SFTBDDPropertyFormulaAdapter(std::shared_ptr<storm::dft::storage::DFT<ValueType>> dft, FormulaVector const &formulas,
                                 std::shared_ptr<storm::dft::storage::SylvanBddManager> sylvanBddManager,
                                 storm::dft::utility::RelevantEvents relevantEvents = {})
        : formulas{formulas} {
        checkForm(formulas);

        relevantEvents.insertNamesFromProperties(formulas.begin(), formulas.end());
        auto const transformator{std::make_shared<storm::dft::transformations::SftToBddTransformator<ValueType>>(dft, sylvanBddManager, relevantEvents)};
        checker = std::make_shared<storm::dft::modelchecker::SFTBDDChecker>(transformator);
    }
#else
    SFTBDDPropertyFormulaAdapter(std::shared_ptr<storm::dft::storage::DFT<ValueType>> dft, FormulaVector const &formulas,
                                 std::shared_ptr<storm::dft::storage::SylvanBddManager> sylvanBddManager,
                                 storm::dft::utility::RelevantEvents relevantEvents = {}) {
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
    }
#endif

    /**
     * \return The internal DFT
     */
    std::shared_ptr<storm::dft::storage::DFT<ValueType>> getDFT() const {
#ifdef STORM_HAVE_SYLVAN
        return checker->getDFT();
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
#endif
    }

    /**
     * \return The internal sylvanBddManager
     */
    std::shared_ptr<storm::dft::storage::SylvanBddManager> getSylvanBddManager() const {
#ifdef STORM_HAVE_SYLVAN
        return checker->getSylvanBddManager();
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
#endif
    }

    /**
     * \return The internal SftToBddTransformator
     */
    std::shared_ptr<storm::dft::transformations::SftToBddTransformator<ValueType>> getTransformator() const {
#ifdef STORM_HAVE_SYLVAN
        return checker->getTransformator();
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
#endif
    }

    /**
     * \return The internal SFTBDDChecker
     */
    std::shared_ptr<storm::dft::modelchecker::SFTBDDChecker> getSFTBDDChecker() const {
#ifdef STORM_HAVE_SYLVAN
        return checker;
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
#endif
    }

    /**
     * Calculate the properties specified by the formulas
     */
    std::vector<ValueType> check(size_t const chunksize = 0) {
#ifdef STORM_HAVE_SYLVAN
        auto const bdds{formulasToBdd()};

        std::map<uint64_t, Bdd> BDDToBdd{};
        for (auto const &bdd : bdds) {
            BDDToBdd[bdd.GetBDD()] = bdd;
        }

        std::map<uint64_t, std::vector<double>> bddToReversedTimepoints{};
        // Note that a vector of timepoints is necessary as formula-BDDs can
        // occur multiple times. The vector is reversed as it later allows to
        // pop the results from the back which is more efficient.
        for (size_t i{0}; i < bdds.size(); ++i) {
            auto const reversedIndex{bdds.size() - i - 1};
            auto const &bdd{bdds[reversedIndex]};
            auto const &formula{formulas[reversedIndex]};
            auto const timebound{getTimebound(formula)};

            bddToReversedTimepoints[bdd.GetBDD()].push_back(timebound);
        }

        std::map<uint64_t, std::vector<double>> bddToReversedProbabilities{};
        for (auto const &pair : bddToReversedTimepoints) {
            auto const bdd{BDDToBdd.at(pair.first)};
            bddToReversedProbabilities[pair.first] = checker->getProbabilitiesAtTimepoints(bdd, pair.second, chunksize);
        }

        std::vector<ValueType> rval{};
        rval.reserve(bdds.size());
        for (size_t i{0}; i < bdds.size(); ++i) {
            auto const &bdd{bdds[i]};
            auto &tmpVec{bddToReversedProbabilities.at(bdd.GetBDD())};
            rval.push_back(tmpVec.back());
            tmpVec.pop_back();
        }

        return rval;
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
#endif
    }

#ifdef STORM_HAVE_SYLVAN
    /**
     * \return
     * The bdds representing the StatesFormulas of the given formulas
     *
     * \param formulas
     * The Properties to extract the StateFormulas of.
     */
    std::vector<Bdd> formulasToBdd() {
        std::vector<Bdd> rval{};
        rval.reserve(formulas.size());
        for (auto const &formula : formulas) {
            rval.push_back(StateFormulaToBdd(toStateFormula(formula), checkBoundsSame(formula)));
        }
        return rval;
    }
#endif

    // TODO: Move formulahandling into its own module
    /**
     * Check if the formulas are of the form 'P=? [F op phi]'
     * where op is in {<=, <, =} and phi is a state formula
     */
    static void checkForm(FormulaVector const &formulas) {
#ifdef STORM_HAVE_SYLVAN
        for (auto const &formula : formulas) {
            if (formula->isProbabilityOperatorFormula()) {
                auto const probabilityOperator{std::static_pointer_cast<storm::logic::ProbabilityOperatorFormula const>(formula)};
                auto const subFormula{probabilityOperator->getSubformula().asSharedPointer()};
                if (subFormula->isBoundedUntilFormula()) {
                    auto const boundedUntil{std::static_pointer_cast<storm::logic::BoundedUntilFormula const>(subFormula)};

                    auto const leftSide{boundedUntil->getLeftSubformula().asSharedPointer()};
                    STORM_LOG_THROW(leftSide->isTrueFormula(), storm::exceptions::NotSupportedException, "Left side is not a TrueFormula.");

                    auto const rightSide{boundedUntil->getRightSubformula().asSharedPointer()};
                    STORM_LOG_THROW(rightSide->isStateFormula(), storm::exceptions::NotSupportedException, "Right side is not a StateFormula.");

                    if (!boundedUntil->hasUpperBound()) {
                        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "UpperBound must be set.");
                    } else if (boundedUntil->hasUpperBound() && boundedUntil->hasLowerBound()) {
                        // Check if '[F = x phi]' was used.
                        STORM_LOG_THROW(boundedUntil->getUpperBound().evaluateAsDouble() == boundedUntil->getLowerBound().evaluateAsDouble(),
                                        storm::exceptions::NotSupportedException, "UpperBound is set wrongly. Only lowerBound == upperBound is Supported.");
                    }
                } else {
                    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "SubFormula is not a BoundedUntilFormula.");
                }
            } else {
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Only ProbabilityOperatorFormulas are supported.");
            }
        }
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
#endif
    }

    /**
     * \return
     * The upper timebound of the given formula
     */
    static double getTimebound(FormulaCPointer const &formula) {
#ifdef STORM_HAVE_SYLVAN
        auto const probabilityOperator{std::static_pointer_cast<storm::logic::ProbabilityOperatorFormula const>(formula)};

        auto const subFormula{probabilityOperator->getSubformula().asSharedPointer()};

        auto const boundedUntil{std::static_pointer_cast<storm::logic::BoundedUntilFormula const>(subFormula)};

        return boundedUntil->getUpperBound().evaluateAsDouble();
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
#endif
    }

   private:
#ifdef STORM_HAVE_SYLVAN
    std::shared_ptr<storm::dft::modelchecker::SFTBDDChecker> checker;
    FormulaVector formulas;

    /**
     * \return
     * true iff the formula is of the form 'P=? [F = x phi]'
     */
    static bool checkBoundsSame(FormulaCPointer const &formula) {
        auto const probabilityOperator{std::static_pointer_cast<storm::logic::ProbabilityOperatorFormula const>(formula)};
        auto const boundedUntil{std::static_pointer_cast<storm::logic::BoundedUntilFormula const>(probabilityOperator->getSubformula().asSharedPointer())};

        if (!boundedUntil->hasLowerBound() || !boundedUntil->hasUpperBound()) {
            return false;
        } else if (boundedUntil->getUpperBound().evaluateAsDouble() == boundedUntil->getLowerBound().evaluateAsDouble()) {
            return true;
        }

        return false;
    }

    /**
     * \return
     * The nested StateFormula of the given formula
     */
    static StateFormulaCPointer toStateFormula(FormulaCPointer const &formula) {
        auto const probabilityOperator{std::static_pointer_cast<storm::logic::ProbabilityOperatorFormula const>(formula)};

        auto const subFormula{probabilityOperator->getSubformula().asSharedPointer()};

        auto const boundedUntil{std::static_pointer_cast<storm::logic::BoundedUntilFormula const>(subFormula)};

        auto const rightSide{boundedUntil->getRightSubformula().asSharedPointer()};

        auto const stateFormula{std::static_pointer_cast<storm::logic::StateFormula const>(rightSide)};

        return stateFormula;
    }

    /**
     * \return
     * The Name of the event referenced by the given AtomicLabelFormula
     */
    std::string getAtomicLabelString(AtomicLabelFormulaCPointer const &formula) const {
        auto const label{formula->getLabel()};
        if (label == "failed") {
            return getDFT()->getTopLevelElement()->name();
        } else if (boost::ends_with(label, "_failed")) {
            auto const name{label.substr(0, label.size() - 7)};
            return name;
        }

        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Illegal AtomicLabelFormula: " << formula->toString() << ".");
        return "__ERROR__";
    }

    /**
     * \return
     * The bdds representing the StatesFormulas of the given formula
     */
    Bdd FormulaToBdd(FormulaCPointer const &formula) const {
        if (formula->isStateFormula()) {
            return StateFormulaToBdd(std::static_pointer_cast<storm::logic::StateFormula const>(formula));
        }

        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Illegal Formula: " << formula->toString() << ".");
        return getSylvanBddManager()->getZero();
    }

    /**
     * \return
     * The bdds representing the given Stateformula
     */
    Bdd StateFormulaToBdd(StateFormulaCPointer const &formula, bool const enableNot = false) const {
        if (formula->isBinaryBooleanStateFormula()) {
            return binaryStateFormulaToBdd(std::static_pointer_cast<storm::logic::BinaryBooleanStateFormula const>(formula));
        } else if (formula->isAtomicLabelFormula()) {
            return atomicLabelFormulaToBdd(std::static_pointer_cast<storm::logic::AtomicLabelFormula const>(formula));
        } else if (formula->isUnaryBooleanStateFormula()) {
            return unaryStateFormulaToBdd(std::static_pointer_cast<storm::logic::UnaryBooleanStateFormula const>(formula), enableNot);
        }

        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Illegal StateFormula: " << formula->toString() << ".");
        return getSylvanBddManager()->getZero();
    }

    /**
     * \return
     * The bdds representing the given Stateformula
     */
    Bdd binaryStateFormulaToBdd(BinaryStateFormulaCPointer const &formula) const {
        auto const leftBdd{FormulaToBdd(formula->getLeftSubformula().asSharedPointer())};
        auto const rightBdd{FormulaToBdd(formula->getRightSubformula().asSharedPointer())};

        if (formula->isAnd()) {
            return leftBdd & rightBdd;
        } else if (formula->isOr()) {
            return leftBdd | rightBdd;
        }

        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Illegal BinaryStateFormula: " << formula->toString() << ".");
        return getSylvanBddManager()->getZero();
    }

    /**
     * \return
     * The bdds representing the given Stateformula
     *
     * \note Only works if enableNot is true
     * as negation only works with timepoints not timebounds.
     */
    Bdd unaryStateFormulaToBdd(UnaryStateFormulaCPointer const &formula, bool const enableNot = false) const {
        if (!enableNot) {
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                            "Illegal UnaryStateFormula: \"" << formula->toString()
                                                            << "\". Can only use negation with a formula "
                                                               "of the form 'P=? [F = x phi]'.");
            return getSylvanBddManager()->getZero();
        }
        auto const subBdd{FormulaToBdd(formula->getSubformula().asSharedPointer())};

        if (formula->isNot()) {
            return !subBdd;
        }

        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Illegal UnaryStateFormula: " << formula->toString() << ".");
        return getSylvanBddManager()->getZero();
    }

    /**
     * \return
     * The bdds representing the given Stateformula
     */
    Bdd atomicLabelFormulaToBdd(AtomicLabelFormulaCPointer const &formula) const {
        return getTransformator()->transformRelevantEvents().at(getAtomicLabelString(formula));
    }
#endif
};

}  // namespace adapters
}  // namespace storm::dft
