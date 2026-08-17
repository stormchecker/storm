#include "JSONExporter.h"

#include <fstream>
#include <iostream>
#include <vector>

#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/io/file.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/InvalidJaniException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotImplementedException.h"

#include "storm/storage/expressions/BinaryBooleanFunctionExpression.h"
#include "storm/storage/expressions/BinaryNumericalFunctionExpression.h"
#include "storm/storage/expressions/BinaryRelationExpression.h"
#include "storm/storage/expressions/BooleanLiteralExpression.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/IfThenElseExpression.h"
#include "storm/storage/expressions/IntegerLiteralExpression.h"
#include "storm/storage/expressions/OperatorType.h"
#include "storm/storage/expressions/RationalLiteralExpression.h"
#include "storm/storage/expressions/UnaryBooleanFunctionExpression.h"
#include "storm/storage/expressions/UnaryNumericalFunctionExpression.h"
#include "storm/storage/expressions/VariableExpression.h"

#include "storm/logic/Formulas.h"

#include "storm/storage/jani/AutomatonComposition.h"
#include "storm/storage/jani/ParallelComposition.h"
#include "storm/storage/jani/Property.h"
#include "storm/storage/jani/eliminator/FunctionEliminator.h"
#include "storm/storage/jani/traverser/RewardModelInformation.h"
#include "storm/storage/jani/types/ArrayType.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"
#include "storm/storage/jani/visitor/JaniReduceNestingExpressionVisitor.h"

namespace storm {
namespace jani {

ExportJsonType anyToJson(boost::any&& input) {
    boost::any tmp(std::move(input));
    ExportJsonType res = std::move(*boost::any_cast<ExportJsonType>(&tmp));
    return res;
}

ExportJsonType buildExpression(storm::expressions::Expression const& exp, std::vector<storm::jani::Constant> const& constants,
                               VariableSet const& globalVariables = VariableSet(), VariableSet const& localVariables = VariableSet(),
                               std::unordered_set<std::string> const& auxiliaryVariables = {}) {
    STORM_LOG_TRACE("Exporting " << exp);
    return ExpressionToJson::translate(exp, constants, globalVariables, localVariables, auxiliaryVariables);
}

class CompositionJsonExporter : public CompositionVisitor {
   public:
    CompositionJsonExporter(bool allowRecursion) : allowRecursion(allowRecursion) {}

    static ExportJsonType translate(storm::jani::Composition const& comp, bool allowRecursion = true) {
        CompositionJsonExporter visitor(allowRecursion);
        return anyToJson(comp.accept(visitor, boost::none));
    }

    virtual boost::any visit(AutomatonComposition const& composition, boost::any const&) {
        ExportJsonType compDecl;
        ExportJsonType autDecl;
        autDecl["automaton"] = composition.getAutomatonName();
        std::vector<ExportJsonType> elements;
        elements.push_back(autDecl);
        compDecl["elements"] = elements;
        return compDecl;
    }

    virtual boost::any visit(ParallelComposition const& composition, boost::any const& data) {
        ExportJsonType compDecl;

        std::vector<ExportJsonType> elems;
        for (auto const& subcomp : composition.getSubcompositions()) {
            ExportJsonType elemDecl;
            if (subcomp->isAutomatonComposition()) {
                elemDecl["automaton"] = std::static_pointer_cast<AutomatonComposition>(subcomp)->getAutomatonName();
            } else {
                STORM_LOG_THROW(allowRecursion, storm::exceptions::InvalidJaniException, "Nesting composition " << *subcomp << " is not supported by JANI.");
                elemDecl = anyToJson(subcomp->accept(*this, data));
            }
            elems.push_back(elemDecl);
        }
        compDecl["elements"] = elems;
        std::vector<ExportJsonType> synElems;
        for (auto const& syncs : composition.getSynchronizationVectors()) {
            ExportJsonType syncDecl;
            syncDecl["synchronise"] = std::vector<std::string>();
            for (auto const& syncIn : syncs.getInput()) {
                if (syncIn == SynchronizationVector::NO_ACTION_INPUT) {
                    syncDecl["synchronise"].push_back(nullptr);
                } else {
                    syncDecl["synchronise"].push_back(syncIn);
                }
            }
            syncDecl["result"] = syncs.getOutput();
            synElems.push_back(syncDecl);
        }
        if (!synElems.empty()) {
            compDecl["syncs"] = synElems;
        }

        return compDecl;
    }

   private:
    bool allowRecursion;
};

std::string comparisonTypeToJani(storm::logic::ComparisonType ct) {
    switch (ct) {
        case storm::logic::ComparisonType::Less:
            return "<";
        case storm::logic::ComparisonType::LessEqual:
            return "≤";
        case storm::logic::ComparisonType::Greater:
            return ">";
        case storm::logic::ComparisonType::GreaterEqual:
            return "≥";
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Unknown ComparisonType.");
}

ExportJsonType FormulaToJaniJson::constructPropertyInterval(boost::optional<storm::expressions::Expression> const& lower,
                                                            boost::optional<bool> const& lowerExclusive,
                                                            boost::optional<storm::expressions::Expression> const& upper,
                                                            boost::optional<bool> const& upperExclusive) const {
    STORM_LOG_THROW((lower.is_initialized() || upper.is_initialized()), storm::exceptions::InvalidJaniException,
                    "PropertyInterval needs either a lower or an upper bound, but none was given.");
    STORM_LOG_THROW((lower.is_initialized() || !lowerExclusive.is_initialized()), storm::exceptions::InvalidJaniException,
                    "PropertyInterval defines wether the lower bound is exclusive but no lower bound is given.");
    STORM_LOG_THROW((upper.is_initialized() || !upperExclusive.is_initialized()), storm::exceptions::InvalidJaniException,
                    "PropertyInterval defines wether the upper bound is exclusive but no upper bound is given.");

    ExportJsonType iDecl;
    if (lower) {
        iDecl["lower"] = buildExpression(*lower, model.getConstants(), model.getGlobalVariables());
        if (lowerExclusive) {
            iDecl["lower-exclusive"] = *lowerExclusive;
        }
    }
    if (upper) {
        iDecl["upper"] = buildExpression(*upper, model.getConstants(), model.getGlobalVariables());
        if (upperExclusive) {
            iDecl["upper-exclusive"] = *upperExclusive;
        }
    }
    return iDecl;
}

ExportJsonType FormulaToJaniJson::constructRewardAccumulation(storm::logic::RewardAccumulation const& rewardAccumulation,
                                                              std::string const& rewardModelName) const {
    storm::jani::RewardModelInformation info(model, rewardModelName);

    bool steps = rewardAccumulation.isStepsSet() && (info.hasActionRewards() || info.hasTransitionRewards());
    bool time = rewardAccumulation.isTimeSet() && !model.isDiscreteTimeModel() && info.hasStateRewards();
    bool exit = rewardAccumulation.isExitSet() && info.hasStateRewards();

    return constructRewardAccumulation(storm::logic::RewardAccumulation(steps, time, exit));
}

ExportJsonType FormulaToJaniJson::constructRewardAccumulation(storm::logic::RewardAccumulation const& rewardAccumulation) const {
    std::vector<std::string> res;
    if (rewardAccumulation.isStepsSet()) {
        res.push_back("steps");
    }
    if (rewardAccumulation.isTimeSet()) {
        res.push_back("time");
    }
    if (rewardAccumulation.isExitSet()) {
        stateExitRewards = true;
        res.push_back("exit");
    }
    return res;
}

ExportJsonType FormulaToJaniJson::constructStandardRewardAccumulation(std::string const& rewardModelName) const {
    if (model.isDiscreteTimeModel()) {
        return constructRewardAccumulation(storm::logic::RewardAccumulation(true, false, true), rewardModelName);
    } else {
        return constructRewardAccumulation(storm::logic::RewardAccumulation(true, true, false), rewardModelName);
    }
}

ExportJsonType FormulaToJaniJson::translate(storm::logic::Formula const& formula, storm::jani::Model const& model, storm::jani::ModelFeatures& modelFeatures) {
    FormulaToJaniJson translator(model);
    auto result = anyToJson(formula.accept(translator));
    if (translator.containsStateExitRewards()) {
        modelFeatures.add(storm::jani::ModelFeature::StateExitRewards);
    }
    if (translator.containsMultiObjectiveProperties()) {
        modelFeatures.add(storm::jani::ModelFeature::MultiObjectiveProperties);
    }
    return result;
}

bool FormulaToJaniJson::containsStateExitRewards() const {
    return stateExitRewards;
}

bool FormulaToJaniJson::containsMultiObjectiveProperties() const {
    return multiObjectiveProperties;
}

boost::any FormulaToJaniJson::visit(storm::logic::AtomicExpressionFormula const& f, boost::any const&) const {
    return buildExpression(f.getExpression(), model.getConstants(), model.getGlobalVariables());
}

boost::any FormulaToJaniJson::visit(storm::logic::AtomicLabelFormula const& f, boost::any const&) const {
    ExportJsonType opDecl(f.getLabel());
    return opDecl;
}
boost::any FormulaToJaniJson::visit(storm::logic::BinaryBooleanStateFormula const& f, boost::any const& data) const {
    ExportJsonType opDecl;
    storm::logic::BinaryBooleanStateFormula::OperatorType op = f.getOperator();
    opDecl["op"] = op == storm::logic::BinaryBooleanStateFormula::OperatorType::And ? "∧" : "∨";
    opDecl["left"] = anyToJson(f.getLeftSubformula().accept(*this, data));
    opDecl["right"] = anyToJson(f.getRightSubformula().accept(*this, data));
    return opDecl;
}
boost::any FormulaToJaniJson::visit(storm::logic::BinaryBooleanPathFormula const& f, boost::any const& data) const {
    ExportJsonType opDecl;
    storm::logic::BinaryBooleanPathFormula::OperatorType op = f.getOperator();
    opDecl["op"] = op == storm::logic::BinaryBooleanPathFormula::OperatorType::And ? "∧" : "∨";
    opDecl["left"] = boost::any_cast<ExportJsonType>(f.getLeftSubformula().accept(*this, data));
    opDecl["right"] = boost::any_cast<ExportJsonType>(f.getRightSubformula().accept(*this, data));
    return opDecl;
}
boost::any FormulaToJaniJson::visit(storm::logic::BooleanLiteralFormula const& f, boost::any const&) const {
    ExportJsonType opDecl(f.isTrueFormula());
    return opDecl;
}
boost::any FormulaToJaniJson::visit(storm::logic::BoundedUntilFormula const& f, boost::any const& data) const {
    STORM_LOG_THROW(!f.hasMultiDimensionalSubformulas(), storm::exceptions::NotSupportedException,
                    "Jani export of multi-dimensional bounded until formulas is not supported.");
    ExportJsonType opDecl;
    opDecl["op"] = "U";
    opDecl["left"] = anyToJson(f.getLeftSubformula().accept(*this, data));
    opDecl["right"] = anyToJson(f.getRightSubformula().accept(*this, data));

    bool hasStepBounds(false), hasTimeBounds(false);
    std::vector<ExportJsonType> rewardBounds;

    for (uint64_t i = 0; i < f.getDimension(); ++i) {
        boost::optional<storm::expressions::Expression> lower, upper;
        boost::optional<bool> lowerExclusive, upperExclusive;
        if (f.hasLowerBound(i)) {
            lower = f.getLowerBound(i);
            lowerExclusive = f.isLowerBoundStrict(i);
        }
        if (f.hasUpperBound(i)) {
            upper = f.getUpperBound(i);
            upperExclusive = f.isUpperBoundStrict(i);
        }
        ExportJsonType propertyInterval = constructPropertyInterval(lower, lowerExclusive, upper, upperExclusive);

        auto tbr = f.getTimeBoundReference(i);
        if (tbr.isStepBound() || (model.isDiscreteTimeModel() && tbr.isTimeBound())) {
            STORM_LOG_THROW(!hasStepBounds, storm::exceptions::NotSupportedException,
                            "Jani export of until formulas with multiple step bounds is not supported.");
            hasStepBounds = true;
            opDecl["step-bounds"] = propertyInterval;
        } else if (tbr.isRewardBound()) {
            ExportJsonType rewbound;
            rewbound["exp"] = buildExpression(model.getRewardModelExpression(tbr.getRewardName()), model.getConstants(), model.getGlobalVariables());
            if (tbr.hasRewardAccumulation()) {
                rewbound["accumulate"] = constructRewardAccumulation(tbr.getRewardAccumulation(), tbr.getRewardName());
            } else {
                rewbound["accumulate"] = constructStandardRewardAccumulation(tbr.getRewardName());
            }
            rewbound["bounds"] = propertyInterval;
            rewardBounds.push_back(std::move(rewbound));
        } else {
            STORM_LOG_THROW(!hasTimeBounds, storm::exceptions::NotSupportedException,
                            "Jani export of until formulas with multiple time bounds is not supported.");
            hasTimeBounds = true;
            opDecl["time-bounds"] = propertyInterval;
        }
    }
    if (!rewardBounds.empty()) {
        opDecl["reward-bounds"] = ExportJsonType(rewardBounds);
    }
    return opDecl;
}

boost::any FormulaToJaniJson::visit(storm::logic::ConditionalFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Jani currently does not support conditional formulae.");
}

boost::any FormulaToJaniJson::visit(storm::logic::CumulativeRewardFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Storm currently does not support translating cumulative reward formulae.");
}

boost::any FormulaToJaniJson::visit(storm::logic::EventuallyFormula const& f, boost::any const& data) const {
    ExportJsonType opDecl;
    opDecl["op"] = "U";
    opDecl["left"] = anyToJson(f.getTrueFormula()->accept(*this, data));
    opDecl["right"] = anyToJson(f.getSubformula().accept(*this, data));
    return opDecl;
}

boost::any FormulaToJaniJson::visit(storm::logic::TimeOperatorFormula const& f, boost::any const& data) const {
    ExportJsonType opDecl;

    // Create standard reward accumulation for time operator formulas.
    storm::logic::RewardAccumulation rewAcc(model.isDiscreteTimeModel(), !model.isDiscreteTimeModel(), false);
    if (f.getSubformula().isEventuallyFormula() && f.getSubformula().asEventuallyFormula().hasRewardAccumulation()) {
        rewAcc = f.getSubformula().asEventuallyFormula().getRewardAccumulation();
    }
    auto rewAccJson = constructRewardAccumulation(rewAcc);

    if (f.hasBound()) {
        auto bound = f.getBound();
        opDecl["op"] = comparisonTypeToJani(bound.comparisonType);
        if (f.hasOptimalityType()) {
            opDecl["left"]["op"] = f.getOptimalityType() == storm::solver::OptimizationDirection::Minimize ? "Emin" : "Emax";
            if (f.getSubformula().isEventuallyFormula()) {
                opDecl["left"]["reach"] = anyToJson(f.getSubformula().asEventuallyFormula().getSubformula().accept(*this, data));
            } else {
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Unsupported subformula for time operator formula " << f << ".");
            }
        } else {
            opDecl["left"]["op"] =
                (bound.comparisonType == storm::logic::ComparisonType::Less || bound.comparisonType == storm::logic::ComparisonType::LessEqual) ? "Emax"
                                                                                                                                                : "Emin";
            opDecl["left"]["reach"] = anyToJson(f.getSubformula().asEventuallyFormula().getSubformula().accept(*this, data));
        }
        opDecl["left"]["exp"] = ExportJsonType(1);
        opDecl["left"]["accumulate"] = rewAccJson;
        opDecl["right"] = buildExpression(bound.threshold, model.getConstants(), model.getGlobalVariables());
    } else {
        if (f.hasOptimalityType()) {
            opDecl["op"] = f.getOptimalityType() == storm::solver::OptimizationDirection::Minimize ? "Emin" : "Emax";
            if (f.getSubformula().isEventuallyFormula()) {
                opDecl["reach"] = anyToJson(f.getSubformula().asEventuallyFormula().getSubformula().accept(*this, data));
            } else {
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Unsupported subformula for time operator formula " << f << ".");
            }
        } else {
            // TODO add checks
            opDecl["op"] = "Emin";
            opDecl["reach"] = anyToJson(f.getSubformula().asEventuallyFormula().getSubformula().accept(*this, data));
        }
        opDecl["exp"] = ExportJsonType(1);
        opDecl["accumulate"] = rewAccJson;
    }
    return opDecl;
}

boost::any FormulaToJaniJson::visit(storm::logic::GloballyFormula const& f, boost::any const& data) const {
    ExportJsonType opDecl;
    opDecl["op"] = "G";
    opDecl["exp"] = anyToJson(f.getSubformula().accept(*this, data));
    return opDecl;
}

boost::any FormulaToJaniJson::visit(storm::logic::GameFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Conversion of game formulas to Jani is not supported.");
}

boost::any FormulaToJaniJson::visit(storm::logic::InstantaneousRewardFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Jani currently does not support conversion of an instanteneous reward formula.");
}

boost::any FormulaToJaniJson::visit(storm::logic::LongRunAverageOperatorFormula const& f, boost::any const& data) const {
    ExportJsonType opDecl;
    if (f.hasBound()) {
        auto bound = f.getBound();
        opDecl["op"] = comparisonTypeToJani(bound.comparisonType);
        if (f.hasOptimalityType()) {
            opDecl["left"]["op"] = f.getOptimalityType() == storm::solver::OptimizationDirection::Minimize ? "Smin" : "Smax";
            opDecl["left"]["exp"] = anyToJson(f.getSubformula().accept(*this, data));
        } else {
            opDecl["left"]["op"] =
                (bound.comparisonType == storm::logic::ComparisonType::Less || bound.comparisonType == storm::logic::ComparisonType::LessEqual) ? "Smax"
                                                                                                                                                : "Smin";
            opDecl["left"]["exp"] = anyToJson(f.getSubformula().accept(*this, data));
        }
        opDecl["right"] = buildExpression(bound.threshold, model.getConstants(), model.getGlobalVariables());
    } else {
        if (f.hasOptimalityType()) {
            opDecl["op"] = f.getOptimalityType() == storm::solver::OptimizationDirection::Minimize ? "Smin" : "Smax";
            opDecl["exp"] = anyToJson(f.getSubformula().accept(*this, data));

        } else {
            // TODO add checks
            opDecl["op"] = "Smin";
            opDecl["exp"] = anyToJson(f.getSubformula().accept(*this, data));
        }
    }
    return opDecl;
}

boost::any FormulaToJaniJson::visit(storm::logic::LongRunAverageRewardFormula const&, boost::any const&) const {
    //            ExportJsonType opDecl;
    //            if(f.()) {
    //                auto bound = f.getBound();
    //                opDecl["op"] = comparisonTypeToJani(bound.comparisonType);
    //                if(f.hasOptimalityType()) {
    //                    opDecl["left"]["op"] = f.getOptimalityType() == storm::solver::OptimizationDirection::Minimize ? "Smin" : "Smax";
    //                    opDecl["left"]["exp"] = anyToJson(f.getSubformula().accept(*this, boost::none));
    //                } else {
    //                    opDecl["left"]["op"] = (bound.comparisonType == storm::logic::ComparisonType::Less || bound.comparisonType ==
    //                    storm::logic::ComparisonType::LessEqual) ? "Smax" : "Smin"; opDecl["left"]["exp"] = anyToJson(f.getSubformula().accept(*this,
    //                    boost::none));
    //                }
    //                opDecl["right"] = ExpressionToJson::translate(bound.threshold);
    //            } else {
    //                if(f.hasOptimalityType()) {
    //                    opDecl["op"] = f.getOptimalityType() == storm::solver::OptimizationDirection::Minimize ? "Smin" : "Smax";
    //                    opDecl["exp"] = anyToJson(f.getSubformula().accept(*this, boost::none));
    //
    //                } else {
    //                    // TODO add checks
    //                    opDecl["op"] = "Pmin";
    //                    opDecl["exp"] = anyToJson(f.getSubformula().accept(*this, boost::none));
    //                }
    //            }
    //            return opDecl;

    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Jani currently does not support conversion of an LRA reward formula.");
}

boost::any FormulaToJaniJson::visit(storm::logic::MultiObjectiveFormula const& f, boost::any const& data) const {
    multiObjectiveProperties = true;
    ExportJsonType opDecl;
    opDecl["op"] = "Multi";
    STORM_LOG_ASSERT(f.isTradeoff() || f.isLexicographic(), "Unexpected multi-objective formula type.");
    opDecl["type"] = f.isTradeoff() ? "tradeoff" : "lexicographic";
    opDecl["properties"] = ExportJsonType::array();
    for (auto const& subformula : f.getSubformulas()) {
        auto p = ExportJsonType::object();
        auto subFormulaCopy = subformula->clone();
        if (subFormulaCopy->isOperatorFormula()) {
            auto& operatorFormula = subFormulaCopy->asOperatorFormula();
            if (operatorFormula.hasBound()) {
                if (!operatorFormula.hasOptimalityType()) {
                    // Set a sane optimality type because JANI properties require some
                    auto const derivedOptimalityType = storm::logic::isLowerBound(operatorFormula.getBound().comparisonType)
                                                           ? storm::solver::OptimizationDirection::Maximize
                                                           : storm::solver::OptimizationDirection::Minimize;
                    operatorFormula.setOptimalityType(derivedOptimalityType);
                }
            } else {
                STORM_LOG_THROW(
                    operatorFormula.hasOptimalityType(), storm::exceptions::NotSupportedException,
                    "Unable to export a multi-objective formula since the optimization direction for subformula " << *subFormulaCopy << " can not be derived.");
                p["opt"] = storm::solver::minimize(subformula->asOperatorFormula().getOptimalityType()) ? "min" : "max";
            }
        } else {
            STORM_LOG_THROW(
                subFormulaCopy->hasQualitativeResult(), storm::exceptions::NotSupportedException,
                "Unable to export a multi-objective formula since the optimization direction for subformula " << *subFormulaCopy << " can not be derived.");
        }
        p["exp"] = anyToJson(subFormulaCopy->accept(*this, data));
        opDecl["properties"].push_back(std::move(p));
    }
    return opDecl;
}

boost::any FormulaToJaniJson::visit(storm::logic::QuantileFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Jani currently does not support conversion of a Quantile formula.");
}

boost::any FormulaToJaniJson::visit(storm::logic::NextFormula const& f, boost::any const& data) const {
    ExportJsonType opDecl;
    opDecl["op"] = "U";
    opDecl["left"] = anyToJson(f.getTrueFormula()->accept(*this, data));
    opDecl["right"] = anyToJson(f.getSubformula().accept(*this, data));
    auto intervalExpressionManager = std::make_shared<storm::expressions::ExpressionManager>();
    opDecl["step-bounds"] = constructPropertyInterval(intervalExpressionManager->integer(1), false, intervalExpressionManager->integer(1), false);
    return opDecl;
}

boost::any FormulaToJaniJson::visit(storm::logic::ProbabilityOperatorFormula const& f, boost::any const& data) const {
    ExportJsonType opDecl;

    if (f.hasBound()) {
        auto bound = f.getBound();
        opDecl["op"] = comparisonTypeToJani(bound.comparisonType);
        if (f.hasOptimalityType()) {
            opDecl["left"]["op"] = f.getOptimalityType() == storm::solver::OptimizationDirection::Minimize ? "Pmin" : "Pmax";
            opDecl["left"]["exp"] = anyToJson(f.getSubformula().accept(*this, data));
        } else {
            opDecl["left"]["op"] =
                (bound.comparisonType == storm::logic::ComparisonType::Less || bound.comparisonType == storm::logic::ComparisonType::LessEqual) ? "Pmax"
                                                                                                                                                : "Pmin";
            opDecl["left"]["exp"] = anyToJson(f.getSubformula().accept(*this, data));
        }
        opDecl["right"] = buildExpression(bound.threshold, model.getConstants(), model.getGlobalVariables());
    } else {
        if (f.hasOptimalityType()) {
            opDecl["op"] = f.getOptimalityType() == storm::solver::OptimizationDirection::Minimize ? "Pmin" : "Pmax";
            opDecl["exp"] = anyToJson(f.getSubformula().accept(*this, data));

        } else {
            // TODO add checks
            opDecl["op"] = "Pmin";
            opDecl["exp"] = anyToJson(f.getSubformula().accept(*this, data));
        }
    }
    return opDecl;
}

boost::any FormulaToJaniJson::visit(storm::logic::RewardOperatorFormula const& f, boost::any const& data) const {
    ExportJsonType opDecl;
    std::string rewardModelName;
    if (f.hasRewardModelName()) {
        rewardModelName = f.getRewardModelName();
    } else {
        if (model.getGlobalVariables().getNumberOfRealTransientVariables() == 1) {
            for (auto const& variable : model.getGlobalVariables().getRealVariables()) {
                if (variable.isTransient()) {
                    rewardModelName = variable.getName();
                    STORM_LOG_WARN("Reward model name was not given, assuming the only global real transient variable '" << rewardModelName
                                                                                                                         << "' to measure the reward.");
                    break;
                }
            }
        }
    }
    STORM_LOG_THROW(!rewardModelName.empty(), storm::exceptions::NotSupportedException, "Reward name has to be specified for Jani-conversion.");

    std::string opString = "";
    if (f.getSubformula().isLongRunAverageRewardFormula()) {
        opString = "S";
    } else {
        opString = "E";
    }
    if (f.hasOptimalityType()) {
        opString += storm::solver::minimize(f.getOptimalityType()) ? "min" : "max";
    } else if (f.hasBound()) {
        opString += storm::logic::isLowerBound(f.getBound().comparisonType) ? "min" : "max";
    } else {
        opString += "min";
    }

    opDecl["op"] = opString;
    auto setRewardAccumulation = [&opDecl, &rewardModelName, this](auto const& subformula) {
        opDecl["accumulate"] = subformula.hasRewardAccumulation() ? constructRewardAccumulation(subformula.getRewardAccumulation(), rewardModelName)
                                                                  : constructStandardRewardAccumulation(rewardModelName);
    };

    if (f.getSubformula().isEventuallyFormula()) {
        opDecl["reach"] = anyToJson(f.getSubformula().asEventuallyFormula().getSubformula().accept(*this, data));
        setRewardAccumulation(f.getSubformula().asEventuallyFormula());
    } else if (f.getSubformula().isCumulativeRewardFormula()) {
        auto const& subf = f.getSubformula().asCumulativeRewardFormula();
        setRewardAccumulation(subf);
        auto isStepInstant = [this, &subf](uint64_t i) {
            return subf.getTimeBoundReference(i).isStepBound() || (model.isDiscreteTimeModel() && subf.getTimeBoundReference(i).isTimeBound());
        };
        auto getInstantExpression = [&subf, &isStepInstant](uint64_t i) {
            auto instantExpr = subf.getBound(i);
            // handle strict bounds
            if (subf.isBoundStrict(i)) {
                // Jani can't represent strict bounds. Only in case of step bounds we can decrease the bound by one as a workaround.
                STORM_LOG_THROW(isStepInstant(i), storm::exceptions::NotSupportedException,
                                "Jani export of cumulative reward formula " << subf << " with strict time or reward bound is not supported.");
                instantExpr = instantExpr - subf.getBound(i).getManager().integer(1);
            }
            // ensure correct type for step instants
            if (isStepInstant(i) && !instantExpr.getType().isIntegerType()) {
                instantExpr = storm::expressions::floor(instantExpr);
            }
            return instantExpr;
        };
        if (subf.isMultiDimensional() || subf.getTimeBoundReference().isRewardBound()) {
            opDecl["reward-instants"] = ExportJsonType::array();
            for (uint64_t i = 0; i < subf.getDimension(); ++i) {
                ExportJsonType instantDecl;
                auto const& tbr = subf.getTimeBoundReference(i);
                instantDecl["exp"] =
                    tbr.isRewardBound() ? buildExpression(model.getRewardModelExpression(tbr.getRewardName()), model.getConstants(), model.getGlobalVariables())
                                        : ExportJsonType(1);
                if (tbr.isRewardBound()) {
                    if (tbr.hasRewardAccumulation()) {
                        instantDecl["accumulate"] = constructRewardAccumulation(tbr.getRewardAccumulation(), tbr.getRewardName());
                    } else {
                        instantDecl["accumulate"] = constructStandardRewardAccumulation(tbr.getRewardName());
                    }
                } else if (isStepInstant(i)) {
                    instantDecl["accumulate"] = constructRewardAccumulation(storm::logic::RewardAccumulation(true, false, false));
                } else {
                    instantDecl["accumulate"] = constructRewardAccumulation(storm::logic::RewardAccumulation(false, true, false));
                }
                instantDecl["instant"] = buildExpression(getInstantExpression(i), model.getConstants(), model.getGlobalVariables());
                opDecl["reward-instants"].push_back(std::move(instantDecl));
            }
        } else {
            opDecl[isStepInstant(0) ? "step-instant" : "time-instant"] =
                buildExpression(getInstantExpression(0), model.getConstants(), model.getGlobalVariables());
        }
    } else if (f.getSubformula().isTotalRewardFormula()) {
        setRewardAccumulation(f.getSubformula().asTotalRewardFormula());
    } else if (f.getSubformula().isInstantaneousRewardFormula()) {
        opDecl[model.isDiscreteTimeModel() ? "step-instant" : "time-instant"] =
            buildExpression(f.getSubformula().asInstantaneousRewardFormula().getBound(), model.getConstants(), model.getGlobalVariables());
    } else if (f.getSubformula().isLongRunAverageRewardFormula()) {
        setRewardAccumulation(f.getSubformula().asLongRunAverageRewardFormula());
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Unhandled subformula for jani export: " << f.getSubformula() << ".");
    }
    opDecl["exp"] = buildExpression(model.getRewardModelExpression(rewardModelName), model.getConstants(), model.getGlobalVariables());

    if (f.hasBound()) {
        ExportJsonType compDecl;
        auto bound = f.getBound();
        compDecl["op"] = comparisonTypeToJani(bound.comparisonType);
        compDecl["left"] = std::move(opDecl);
        compDecl["right"] = buildExpression(bound.threshold, model.getConstants(), model.getGlobalVariables());
        return compDecl;
    } else {
        return opDecl;
    }
}

boost::any FormulaToJaniJson::visit(storm::logic::TotalRewardFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Jani currently does not support a total reward formula.");
}

boost::any FormulaToJaniJson::visit(storm::logic::UnaryBooleanStateFormula const& f, boost::any const& data) const {
    STORM_LOG_ASSERT(f.getOperator() == storm::logic::UnaryBooleanStateFormula::OperatorType::Not, "Unsupported operator.");
    ExportJsonType opDecl;
    opDecl["op"] = "¬";
    opDecl["exp"] = anyToJson(f.getSubformula().accept(*this, data));
    return opDecl;
}

boost::any FormulaToJaniJson::visit(storm::logic::UnaryBooleanPathFormula const& f, boost::any const& data) const {
    STORM_LOG_ASSERT(f.getOperator() == storm::logic::UnaryBooleanStateFormula::OperatorType::Not, "Unsupported operator.");
    ExportJsonType opDecl;
    opDecl["op"] = "¬";
    opDecl["exp"] = boost::any_cast<ExportJsonType>(f.getSubformula().accept(*this, data));
    return opDecl;
}

boost::any FormulaToJaniJson::visit(storm::logic::UntilFormula const& f, boost::any const& data) const {
    ExportJsonType opDecl;
    opDecl["op"] = "U";
    opDecl["left"] = anyToJson(f.getLeftSubformula().accept(*this, data));
    opDecl["right"] = anyToJson(f.getRightSubformula().accept(*this, data));
    return opDecl;
}

boost::any FormulaToJaniJson::visit(storm::logic::HOAPathFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Jani currently does not support HOA path formulae.");
}

boost::any FormulaToJaniJson::visit(storm::logic::DiscountedCumulativeRewardFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Jani currently does not support discounted cumulative reward formulae.");
}

boost::any FormulaToJaniJson::visit(storm::logic::DiscountedTotalRewardFormula const&, boost::any const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Jani currently does not support discounted total reward formulae.");
}

std::string operatorTypeToJaniString(storm::expressions::OperatorType optype) {
    using OpType = storm::expressions::OperatorType;
    switch (optype) {
        case OpType::And:
            return "∧";
        case OpType::Or:
            return "∨";
        case OpType::Xor:
            return "≠";
        case OpType::Implies:
            return "⇒";
        case OpType::Iff:
            return "=";
        case OpType::Plus:
            return "+";
        case OpType::Minus:
            return "-";
        case OpType::Times:
            return "*";
        case OpType::Divide:
            return "/";
        case OpType::Min:
            return "min";
        case OpType::Max:
            return "max";
        case OpType::Power:
            return "pow";
        case OpType::Modulo:
            return "%";
        case OpType::Logarithm:
            return "log";
        case OpType::Equal:
            return "=";
        case OpType::NotEqual:
            return "≠";
        case OpType::Less:
            return "<";
        case OpType::LessOrEqual:
            return "≤";
        case OpType::Greater:
            return ">";
        case OpType::GreaterOrEqual:
            return "≥";
        case OpType::Not:
            return "¬";
        case OpType::Floor:
            return "floor";
        case OpType::Ceil:
            return "ceil";
        case OpType::Ite:
            return "ite";
        case OpType::Sin:
            return "sin";
        case OpType::Cos:
            return "cos";
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException, "Operator not supported by Jani.");
    }
}

ExportJsonType ExpressionToJson::translate(storm::expressions::Expression const& expr, std::vector<storm::jani::Constant> const& constants,
                                           VariableSet const& globalVariables, VariableSet const& localVariables,
                                           std::unordered_set<std::string> const& auxiliaryVariables) {
    // Simplify the expression first and reduce the nesting
    auto simplifiedExpr = storm::jani::reduceNestingInJaniExpression(expr.simplify());

    ExpressionToJson visitor(constants, globalVariables, localVariables, auxiliaryVariables);
    return anyToJson(simplifiedExpr.accept(visitor, boost::none));
}

boost::any ExpressionToJson::visit(storm::expressions::IfThenElseExpression const& expression, boost::any const& data) {
    ExportJsonType opDecl;
    opDecl["op"] = "ite";
    opDecl["if"] = anyToJson(expression.getCondition()->accept(*this, data));
    opDecl["then"] = anyToJson(expression.getThenExpression()->accept(*this, data));
    opDecl["else"] = anyToJson(expression.getElseExpression()->accept(*this, data));
    return opDecl;
}
boost::any ExpressionToJson::visit(storm::expressions::BinaryBooleanFunctionExpression const& expression, boost::any const& data) {
    ExportJsonType opDecl;
    opDecl["op"] = operatorTypeToJaniString(expression.getOperator());
    opDecl["left"] = anyToJson(expression.getOperand(0)->accept(*this, data));
    opDecl["right"] = anyToJson(expression.getOperand(1)->accept(*this, data));
    return opDecl;
}
boost::any ExpressionToJson::visit(storm::expressions::BinaryNumericalFunctionExpression const& expression, boost::any const& data) {
    ExportJsonType opDecl;
    opDecl["op"] = operatorTypeToJaniString(expression.getOperator());
    opDecl["left"] = anyToJson(expression.getOperand(0)->accept(*this, data));
    opDecl["right"] = anyToJson(expression.getOperand(1)->accept(*this, data));
    if (expression.getOperator() == storm::expressions::OperatorType::Power && expression.getType().isIntegerType()) {
        // power expressions that have integer type need to be "type casted"
        ExportJsonType trc;
        trc["op"] = "trc";
        trc["exp"] = std::move(opDecl);
        return trc;
    } else {
        return opDecl;
    }
}

boost::any ExpressionToJson::visit(storm::expressions::BinaryRelationExpression const& expression, boost::any const& data) {
    ExportJsonType opDecl;
    opDecl["op"] = operatorTypeToJaniString(expression.getOperator());
    opDecl["left"] = anyToJson(expression.getOperand(0)->accept(*this, data));
    opDecl["right"] = anyToJson(expression.getOperand(1)->accept(*this, data));
    return opDecl;
}
boost::any ExpressionToJson::visit(storm::expressions::VariableExpression const& expression, boost::any const&) {
    if (auxiliaryVariables.count(expression.getVariableName())) {
        return ExportJsonType(expression.getVariableName());
    } else if (globalVariables.hasVariable(expression.getVariable())) {
        return ExportJsonType(globalVariables.getVariable(expression.getVariable()).getName());
    } else if (localVariables.hasVariable(expression.getVariable())) {
        return ExportJsonType(localVariables.getVariable(expression.getVariable()).getName());
    } else {
        for (auto const& constant : constants) {
            if (constant.getExpressionVariable() == expression.getVariable()) {
                return ExportJsonType(constant.getName());
            }
        }
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidJaniException,
                    "Expression variable '" << expression.getVariableName() << "' not known in Jani data structures.");
    return ExportJsonType();  // should not reach this point.
}
boost::any ExpressionToJson::visit(storm::expressions::UnaryBooleanFunctionExpression const& expression, boost::any const& data) {
    ExportJsonType opDecl;
    opDecl["op"] = operatorTypeToJaniString(expression.getOperator());
    opDecl["exp"] = anyToJson(expression.getOperand()->accept(*this, data));
    return opDecl;
}
boost::any ExpressionToJson::visit(storm::expressions::UnaryNumericalFunctionExpression const& expression, boost::any const& data) {
    ExportJsonType opDecl;
    opDecl["op"] = operatorTypeToJaniString(expression.getOperator());
    if (expression.getOperator() == storm::expressions::OperatorType::Minus) {
        opDecl["left"] = 0;
        opDecl["right"] = anyToJson(expression.getOperand()->accept(*this, data));
    } else {
        opDecl["exp"] = anyToJson(expression.getOperand()->accept(*this, data));
    }
    return opDecl;
}

boost::any ExpressionToJson::visit(storm::expressions::BooleanLiteralExpression const& expression, boost::any const&) {
    return ExportJsonType(expression.getValue());
}
boost::any ExpressionToJson::visit(storm::expressions::IntegerLiteralExpression const& expression, boost::any const&) {
    return ExportJsonType(expression.getValue());
}
boost::any ExpressionToJson::visit(storm::expressions::RationalLiteralExpression const& expression, boost::any const&) {
    ExportJsonType val(expression.getValue());

    if (!storm::isJsonNumberExportAccurate(val)) {
        // Try if exact export is possible as fraction of two literals
        auto [num, den] = storm::utility::asFraction(expression.getValue());
        if (!storm::utility::isOne(den)) {
            ExportJsonType numJson(num), denJson(den);
            if (isJsonNumberExportAccurate(numJson) && isJsonNumberExportAccurate(denJson)) {
                val = ExportJsonType();
                val["op"] = operatorTypeToJaniString(storm::expressions::OperatorType::Divide);
                val["left"] = std::move(numJson);
                val["right"] = std::move(denJson);
            }
        }
    }
    return val;
}

boost::any ExpressionToJson::visit(storm::expressions::ValueArrayExpression const& expression, boost::any const& data) {
    ExportJsonType opDecl;
    opDecl["op"] = "av";
    std::vector<ExportJsonType> elements;
    uint64_t size = expression.size()->evaluateAsInt();
    for (uint64_t i = 0; i < size; ++i) {
        elements.push_back(anyToJson(expression.at(i)->accept(*this, data)));
    }
    opDecl["elements"] = std::move(elements);
    return opDecl;
}

boost::any ExpressionToJson::visit(storm::expressions::ConstructorArrayExpression const& expression, boost::any const& data) {
    ExportJsonType opDecl;
    opDecl["op"] = "ac";
    opDecl["var"] = expression.getIndexVar().getName();
    opDecl["length"] = anyToJson(expression.size()->accept(*this, data));
    bool inserted = auxiliaryVariables.insert(expression.getIndexVar().getName()).second;
    opDecl["exp"] = anyToJson(expression.getElementExpression()->accept(*this, data));
    if (inserted) {
        auxiliaryVariables.erase(expression.getIndexVar().getName());
    }
    return opDecl;
}

boost::any ExpressionToJson::visit(storm::expressions::ArrayAccessExpression const& expression, boost::any const& data) {
    ExportJsonType opDecl;
    opDecl["op"] = "aa";
    opDecl["exp"] = anyToJson(expression.getOperand(0)->accept(*this, data));
    opDecl["index"] = anyToJson(expression.getOperand(1)->accept(*this, data));
    return opDecl;
}

boost::any ExpressionToJson::visit(storm::expressions::FunctionCallExpression const& expression, boost::any const& data) {
    ExportJsonType opDecl;
    opDecl["op"] = "call";
    opDecl["function"] = expression.getFunctionIdentifier();
    std::vector<ExportJsonType> arguments;
    for (uint64_t i = 0; i < expression.getNumberOfArguments(); ++i) {
        arguments.push_back(anyToJson(expression.getArgument(i)->accept(*this, data)));
    }
    opDecl["args"] = std::move(arguments);
    return opDecl;
}

boost::any ExpressionToJson::visit(storm::expressions::TranscendentalNumberLiteralExpression const& expression, boost::any const&) {
    ExportJsonType constantDecl;
    constantDecl["constant"] = expression.asString();
    return constantDecl;
}

void JsonExporter::toFile(storm::jani::Model const& janiModel, std::vector<storm::jani::Property> const& formulas, std::string const& filepath, bool checkValid,
                          bool compact) {
    std::ofstream stream;
    storm::io::openFile(filepath, stream, false, true);
    toStream(janiModel, formulas, stream, checkValid, compact);
    storm::io::closeFile(stream);
}

void JsonExporter::toStream(storm::jani::Model const& janiModel, std::vector<storm::jani::Property> const& formulas, std::ostream& os, bool checkValid,
                            bool compact) {
    if (checkValid) {
        janiModel.checkValid();
    }
    JsonExporter exporter;
    STORM_LOG_INFO("Started to convert model " << janiModel.getName() << ".");
    exporter.convertModel(janiModel, !compact);
    STORM_LOG_INFO("Started to convert properties of model " << janiModel.getName() << ".");
    exporter.convertProperties(formulas, janiModel);
    STORM_LOG_INFO("Producing json output... " << janiModel.getName() << ".");
    os << storm::dumpJson(exporter.finalize(), compact) << '\n';
    STORM_LOG_INFO("Conversion completed " << janiModel.getName() << ".");
}

ExportJsonType buildActionArray(std::vector<storm::jani::Action> const& actions) {
    std::vector<ExportJsonType> actionReprs;
    uint64_t actIndex = 0;
    for (auto const& act : actions) {
        if (actIndex == storm::jani::Model::SILENT_ACTION_INDEX) {
            actIndex++;
            continue;
        }
        actIndex++;
        ExportJsonType actEntry;
        actEntry["name"] = act.getName();
        actionReprs.push_back(actEntry);
    }

    return ExportJsonType(actionReprs);
}

ExportJsonType buildTypeDescription(storm::expressions::Type const& type) {
    ExportJsonType typeDescr;
    if (type.isIntegerType()) {
        typeDescr = "int";
    } else if (type.isRationalType()) {
        typeDescr = "real";
    } else if (type.isBooleanType()) {
        typeDescr = "bool";
    } else if (type.isArrayType()) {
        typeDescr["kind"] = "array";
        typeDescr["base"] = buildTypeDescription(type.getElementType());
    } else {
        STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Unknown expression type.");
    }
    return typeDescr;
}

void getBoundsFromConstraints(ExportJsonType& typeDesc, storm::expressions::Variable const& var, storm::expressions::Expression const& constraint,
                              std::vector<storm::jani::Constant> const& constants) {
    if (constraint.getBaseExpression().isBinaryBooleanFunctionExpression() &&
        constraint.getBaseExpression().getOperator() == storm::expressions::OperatorType::And) {
        getBoundsFromConstraints(typeDesc, var, constraint.getBaseExpression().getOperand(0), constants);
        getBoundsFromConstraints(typeDesc, var, constraint.getBaseExpression().getOperand(1), constants);
    } else if (constraint.getBaseExpression().isBinaryRelationExpression()) {
        bool leq = constraint.getBaseExpression().getOperator() == storm::expressions::OperatorType::LessOrEqual;
        bool geq = constraint.getBaseExpression().getOperator() == storm::expressions::OperatorType::GreaterOrEqual;
        std::vector<bool> varOps(2, false);
        for (int i : {0, 1}) {
            varOps[i] = constraint.getOperand(i).isVariable() && constraint.getOperand(i).getBaseExpression().asVariableExpression().getVariable() == var;
        }
        storm::expressions::Expression boundExpr = varOps[0] ? constraint.getOperand(1) : constraint.getOperand(0);
        if ((leq && varOps[0]) || (geq && varOps[1])) {
            typeDesc["upper-bound"] = buildExpression(boundExpr, constants);
        } else if ((leq && varOps[1]) || (geq && varOps[0])) {
            typeDesc["lower-bound"] = buildExpression(boundExpr, constants);
        } else {
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Jani Export for constant constraint " << constraint << " is not supported.");
        }
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Jani Export for constant constraint " << constraint << " is not supported.");
    }
}

ExportJsonType buildConstantsArray(std::vector<storm::jani::Constant> const& constants) {
    std::vector<ExportJsonType> constantDeclarations;
    for (auto const& constant : constants) {
        ExportJsonType constantEntry;
        constantEntry["name"] = constant.getName();
        ExportJsonType typeDesc;
        if (constant.hasConstraint()) {
            typeDesc["kind"] = "bounded";
            typeDesc["base"] = buildTypeDescription(constant.getType());
            getBoundsFromConstraints(typeDesc, constant.getExpressionVariable(), constant.getConstraintExpression(), constants);
        } else {
            typeDesc = buildTypeDescription(constant.getType());
        }
        constantEntry["type"] = typeDesc;
        if (constant.isDefined()) {
            constantEntry["value"] = buildExpression(constant.getExpression(), constants);
        }
        constantDeclarations.push_back(constantEntry);
    }
    return ExportJsonType(constantDeclarations);
}

ExportJsonType buildType(storm::jani::JaniType const& type, std::vector<storm::jani::Constant> const& constants, VariableSet const& globalVariables,
                         VariableSet const& localVariables = VariableSet()) {
    ExportJsonType typeDesc;
    if (type.isBasicType() || type.isClockType() || type.isContinuousType()) {
        typeDesc = type.getStringRepresentation();
    } else if (type.isBoundedType()) {
        typeDesc["kind"] = "bounded";
        auto const& bndType = type.asBoundedType();
        switch (bndType.getBaseType()) {
            case storm::jani::BoundedType::BaseType::Int:
                typeDesc["base"] = "int";
                break;
            case storm::jani::BoundedType::BaseType::Real:
                typeDesc["base"] = "real";
                break;
        }
        if (bndType.hasLowerBound()) {
            typeDesc["lower-bound"] = buildExpression(bndType.getLowerBound(), constants, globalVariables, localVariables);
        }
        if (bndType.hasUpperBound()) {
            typeDesc["upper-bound"] = buildExpression(bndType.getUpperBound(), constants, globalVariables, localVariables);
        }
    } else if (type.isArrayType()) {
        typeDesc["kind"] = "array";
        typeDesc["base"] = buildType(type.asArrayType().getBaseType(), constants, globalVariables, localVariables);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Variable type not recognized in JSONExporter.");
    }
    return typeDesc;
}

ExportJsonType buildVariablesArray(storm::jani::VariableSet const& varSet, std::vector<storm::jani::Constant> const& constants,
                                   VariableSet const& globalVariables, VariableSet const& localVariables = VariableSet()) {
    ExportJsonType variableDeclarations = std::vector<ExportJsonType>();
    for (auto const& variable : varSet) {
        ExportJsonType varEntry;
        varEntry["name"] = variable.getName();
        if (variable.isTransient()) {
            varEntry["transient"] = variable.isTransient();
        }
        varEntry["type"] = buildType(variable.getType(), constants, globalVariables, localVariables);
        if (variable.hasInitExpression()) {
            varEntry["initial-value"] = buildExpression(variable.getInitExpression(), constants, globalVariables, localVariables);
        }
        variableDeclarations.push_back(std::move(varEntry));
    }
    return variableDeclarations;
}

ExportJsonType buildFunctionsArray(std::unordered_map<std::string, FunctionDefinition> const& functionDefinitions,
                                   std::vector<storm::jani::Constant> const& constants, VariableSet const& globalVariables,
                                   VariableSet const& localVariables = VariableSet()) {
    ExportJsonType functionDeclarations = std::vector<ExportJsonType>();
    for (auto const& nameFunDef : functionDefinitions) {
        storm::jani::FunctionDefinition const& funDef = nameFunDef.second;
        ExportJsonType funDefJson;
        funDefJson["name"] = nameFunDef.first;
        funDefJson["type"] = buildTypeDescription(funDef.getType());
        std::vector<ExportJsonType> parameterDeclarations;
        std::unordered_set<std::string> parameterNames;
        for (auto const& p : funDef.getParameters()) {
            ExportJsonType parDefJson;
            parDefJson["name"] = p.getName();
            parameterNames.insert(p.getName());
            parDefJson["type"] = buildTypeDescription(p.getType());
            parameterDeclarations.push_back(parDefJson);
        }
        funDefJson["parameters"] = parameterDeclarations;
        funDefJson["body"] = buildExpression(funDef.getFunctionBody(), constants, globalVariables, localVariables, parameterNames);
        functionDeclarations.push_back(std::move(funDefJson));
    }
    return functionDeclarations;
}

ExportJsonType buildLValue(storm::jani::LValue const& lValue, std::vector<storm::jani::Constant> const& constants, VariableSet const& globalVariables,
                           VariableSet const& localVariables) {
    if (lValue.isVariable()) {
        return lValue.getVariable().getName();
    } else {
        STORM_LOG_ASSERT(lValue.isArrayAccess(), "Unhandled LValue " << lValue << ".");
        ExportJsonType result = lValue.getVariable().getName();
        for (auto const& indexExpr : lValue.getArrayIndexVector()) {
            ExportJsonType subLValue = std::move(result);
            result.clear();
            result["op"] = "aa";
            result["exp"] = std::move(subLValue);
            result["index"] = buildExpression(indexExpr, constants, globalVariables, localVariables);
        }
        return result;
    }
}

ExportJsonType buildAssignmentArray(storm::jani::OrderedAssignments const& orderedAssignments, std::vector<storm::jani::Constant> const& constants,
                                    VariableSet const& globalVariables, VariableSet const& localVariables, bool commentExpressions) {
    ExportJsonType assignmentDeclarations = std::vector<ExportJsonType>();
    bool addIndex = orderedAssignments.hasMultipleLevels();
    for (auto const& assignment : orderedAssignments) {
        ExportJsonType assignmentEntry;
        assignmentEntry["ref"] = buildLValue(assignment.getLValue(), constants, globalVariables, localVariables);
        assignmentEntry["value"] = buildExpression(assignment.getAssignedExpression(), constants, globalVariables, localVariables);
        if (addIndex) {
            assignmentEntry["index"] = assignment.getLevel();
        }
        if (commentExpressions) {
            assignmentEntry["comment"] = assignment.getLValue().getName() + " <- " + assignment.getAssignedExpression().toString();
        }
        assignmentDeclarations.push_back(std::move(assignmentEntry));
    }
    return assignmentDeclarations;
}

ExportJsonType buildLocationsArray(std::vector<storm::jani::Location> const& locations, std::vector<storm::jani::Constant> const& constants,
                                   VariableSet const& globalVariables, VariableSet const& localVariables, bool commentExpressions) {
    ExportJsonType locationDeclarations = std::vector<ExportJsonType>();
    for (auto const& location : locations) {
        ExportJsonType locEntry;
        locEntry["name"] = location.getName();
        if (location.hasTimeProgressInvariant()) {
            ExportJsonType timeProg;
            timeProg["exp"] = buildExpression(location.getTimeProgressInvariant(), constants, globalVariables, localVariables);
            if (commentExpressions) {
                timeProg["comment"] = location.getTimeProgressInvariant().toString();
            }
            locEntry["time-progress"] = std::move(timeProg);
        }
        if (!location.getAssignments().empty()) {
            locEntry["transient-values"] = buildAssignmentArray(location.getAssignments(), constants, globalVariables, localVariables, commentExpressions);
        }
        locationDeclarations.push_back(std::move(locEntry));
    }
    return locationDeclarations;
}

ExportJsonType buildInitialLocations(storm::jani::Automaton const& automaton) {
    std::vector<std::string> names;
    for (auto const& initLocIndex : automaton.getInitialLocationIndices()) {
        names.push_back(automaton.getLocation(initLocIndex).getName());
    }
    return ExportJsonType(names);
}

ExportJsonType buildDestinations(std::vector<EdgeDestination> const& destinations, std::map<uint64_t, std::string> const& locationNames,
                                 std::vector<storm::jani::Constant> const& constants, VariableSet const& globalVariables, VariableSet const& localVariables,
                                 bool commentExpressions) {
    STORM_LOG_ASSERT(destinations.size() > 0, "Expected at least one destination.");
    ExportJsonType destDeclarations = std::vector<ExportJsonType>();
    for (auto const& destination : destinations) {
        ExportJsonType destEntry;
        destEntry["location"] = locationNames.at(destination.getLocationIndex());
        bool prob1 = false;
        if (destination.getProbability().isLiteral()) {
            if (storm::utility::isOne(destination.getProbability().evaluateAsRational())) {
                prob1 = true;
            }
        }
        if (!prob1) {
            destEntry["probability"]["exp"] = buildExpression(destination.getProbability(), constants, globalVariables, localVariables);
            if (commentExpressions) {
                destEntry["probability"]["comment"] = destination.getProbability().toString();
            }
        }
        if (!destination.getOrderedAssignments().empty()) {
            destEntry["assignments"] =
                buildAssignmentArray(destination.getOrderedAssignments(), constants, globalVariables, localVariables, commentExpressions);
        }
        destDeclarations.push_back(std::move(destEntry));
    }
    return destDeclarations;
}

ExportJsonType buildEdge(Edge const& edge, std::map<uint64_t, std::string> const& actionNames, std::map<uint64_t, std::string> const& locationNames,
                         std::vector<storm::jani::Constant> const& constants, VariableSet const& globalVariables, VariableSet const& localVariables,
                         bool commentExpressions) {
    STORM_LOG_THROW(edge.getDestinations().size() > 0, storm::exceptions::InvalidJaniException, "An edge without destinations is not allowed.");
    ExportJsonType edgeEntry;
    edgeEntry["location"] = locationNames.at(edge.getSourceLocationIndex());
    if (!edge.hasSilentAction()) {
        edgeEntry["action"] = actionNames.at(edge.getActionIndex());
    }
    if (edge.hasRate()) {
        edgeEntry["rate"]["exp"] = buildExpression(edge.getRate(), constants, globalVariables, localVariables);
        if (commentExpressions) {
            edgeEntry["rate"]["comment"] = edge.getRate().toString();
        }
    }
    if (!edge.getGuard().isTrue()) {
        edgeEntry["guard"]["exp"] = buildExpression(edge.getGuard(), constants, globalVariables, localVariables);
        if (commentExpressions) {
            edgeEntry["guard"]["comment"] = edge.getGuard().toString();
        }
    }
    edgeEntry["destinations"] = buildDestinations(edge.getDestinations(), locationNames, constants, globalVariables, localVariables, commentExpressions);
    if (!edge.getAssignments().empty()) {
        edgeEntry["assignments"] = buildAssignmentArray(edge.getAssignments(), constants, globalVariables, localVariables, commentExpressions);
    }
    return edgeEntry;
}

ExportJsonType buildEdges(std::vector<Edge> const& edges, std::map<uint64_t, std::string> const& actionNames,
                          std::map<uint64_t, std::string> const& locationNames, std::vector<storm::jani::Constant> const& constants,
                          VariableSet const& globalVariables, VariableSet const& localVariables, bool commentExpressions) {
    ExportJsonType edgeDeclarations = std::vector<ExportJsonType>();
    for (auto const& edge : edges) {
        if (edge.getGuard().isFalse()) {
            continue;
        }
        edgeDeclarations.push_back(buildEdge(edge, actionNames, locationNames, constants, globalVariables, localVariables, commentExpressions));
    }
    return edgeDeclarations;
}

ExportJsonType buildAutomataArray(std::vector<storm::jani::Automaton> const& automata, std::map<uint64_t, std::string> const& actionNames,
                                  std::vector<storm::jani::Constant> const& constants, VariableSet const& globalVariables, bool commentExpressions) {
    ExportJsonType automataDeclarations = std::vector<ExportJsonType>();
    for (auto const& automaton : automata) {
        ExportJsonType autoEntry;
        autoEntry["name"] = automaton.getName();
        autoEntry["variables"] = buildVariablesArray(automaton.getVariables(), constants, globalVariables, automaton.getVariables());
        if (!automaton.getFunctionDefinitions().empty()) {
            autoEntry["functions"] = buildFunctionsArray(automaton.getFunctionDefinitions(), constants, globalVariables, automaton.getVariables());
        }
        if (automaton.hasRestrictedInitialStates()) {
            autoEntry["restrict-initial"]["exp"] =
                buildExpression(automaton.getInitialStatesRestriction(), constants, globalVariables, automaton.getVariables());
        }
        autoEntry["locations"] = buildLocationsArray(automaton.getLocations(), constants, globalVariables, automaton.getVariables(), commentExpressions);
        autoEntry["initial-locations"] = buildInitialLocations(automaton);
        autoEntry["edges"] = buildEdges(automaton.getEdges(), actionNames, automaton.buildIdToLocationNameMap(), constants, globalVariables,
                                        automaton.getVariables(), commentExpressions);
        automataDeclarations.push_back(std::move(autoEntry));
    }
    return automataDeclarations;
}

void JsonExporter::convertModel(storm::jani::Model const& janiModel, bool commentExpressions) {
    modelFeatures = janiModel.getModelFeatures();
    jsonStruct["jani-version"] = janiModel.getJaniVersion();
    jsonStruct["name"] = janiModel.getName();
    jsonStruct["type"] = to_string(janiModel.getModelType());
    jsonStruct["actions"] = buildActionArray(janiModel.getActions());
    jsonStruct["constants"] = buildConstantsArray(janiModel.getConstants());
    jsonStruct["variables"] = buildVariablesArray(janiModel.getGlobalVariables(), janiModel.getConstants(), janiModel.getGlobalVariables());
    if (!janiModel.getGlobalFunctionDefinitions().empty()) {
        jsonStruct["functions"] = buildFunctionsArray(janiModel.getGlobalFunctionDefinitions(), janiModel.getConstants(), janiModel.getGlobalVariables());
    }
    jsonStruct["restrict-initial"]["exp"] = buildExpression(janiModel.getInitialStatesRestriction(), janiModel.getConstants(), janiModel.getGlobalVariables());
    jsonStruct["automata"] = buildAutomataArray(janiModel.getAutomata(), janiModel.getActionIndexToNameMap(), janiModel.getConstants(),
                                                janiModel.getGlobalVariables(), commentExpressions);
    jsonStruct["system"] = CompositionJsonExporter::translate(janiModel.getSystemComposition());
}

ExportJsonType JsonExporter::getEdgeAsJson(storm::jani::Model const& janiModel, uint64_t automatonIndex, uint64_t edgeIndex, bool commentExpressions) {
    auto const& automaton = janiModel.getAutomaton(automatonIndex);
    return buildEdge(automaton.getEdge(edgeIndex), janiModel.getActionIndexToNameMap(), automaton.buildIdToLocationNameMap(), janiModel.getConstants(),
                     janiModel.getGlobalVariables(), automaton.getVariables(), commentExpressions);
}

std::string janiFilterTypeString(storm::modelchecker::FilterType const& ft) {
    switch (ft) {
        case storm::modelchecker::FilterType::MIN:
            return "min";
        case storm::modelchecker::FilterType::MAX:
            return "max";
        case storm::modelchecker::FilterType::SUM:
            return "sum";
        case storm::modelchecker::FilterType::AVG:
            return "avg";
        case storm::modelchecker::FilterType::COUNT:
            return "count";
        case storm::modelchecker::FilterType::EXISTS:
            return "∃";
        case storm::modelchecker::FilterType::FORALL:
            return "∀";
        case storm::modelchecker::FilterType::ARGMIN:
            return "argmin";
        case storm::modelchecker::FilterType::ARGMAX:
            return "argmax";
        case storm::modelchecker::FilterType::VALUES:
            return "values";
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Unknown FilterType.");
}

ExportJsonType convertFilterExpression(storm::jani::FilterExpression const& fe, storm::jani::Model const& model, storm::jani::ModelFeatures& modelFeatures) {
    ExportJsonType propDecl;
    propDecl["states"]["op"] = "initial";
    propDecl["op"] = "filter";
    propDecl["fun"] = janiFilterTypeString(fe.getFilterType());
    propDecl["values"] = FormulaToJaniJson::translate(*fe.getFormula(), model, modelFeatures);
    return propDecl;
}

void JsonExporter::convertProperties(std::vector<storm::jani::Property> const& formulas, storm::jani::Model const& model) {
    ExportJsonType properties;

    // For now, unset model-features that only relate to properties.
    // If such properties actually exist, these features will be re-added during formula conversion below.
    modelFeatures.remove(storm::jani::ModelFeature::StateExitRewards);
    modelFeatures.remove(storm::jani::ModelFeature::MultiObjectiveProperties);
    if (formulas.empty()) {
        jsonStruct["properties"] = ExportJsonType(ExportJsonType::value_t::array);
        return;
    }

    for (auto const& f : formulas) {
        ExportJsonType propDecl;
        propDecl["name"] = f.getName();
        propDecl["expression"] = convertFilterExpression(f.getFilter(), model, modelFeatures);
        properties.push_back(std::move(propDecl));
    }
    jsonStruct["properties"] = std::move(properties);
}

ExportJsonType JsonExporter::finalize() {
    jsonStruct["features"] = ExportJsonType::parse(modelFeatures.toString());
    return jsonStruct;
}

}  // namespace jani
}  // namespace storm
