#include "storm-pomdp/analysis/JaniBeliefSupportMdpGenerator.h"

#include "storm-parsers/api/properties.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/api/builder.h"
#include "storm/api/verification.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/storage/SymbolicModelDescription.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/jani/Model.h"

namespace storm {
namespace pomdp {
namespace qualitative {
namespace detail {
struct ObsActPair {
    ObsActPair(uint64_t observation, uint64_t action) : observation(observation), action(action) {}

    uint64_t observation;
    uint64_t action;
};

bool operator<(ObsActPair const& o1, ObsActPair const& o2) {
    if (o1.observation == o2.observation) {
        return o1.action < o2.action;
    }
    return o1.observation < o2.observation;
}
}  // namespace detail

template<typename ValueType>
JaniBeliefSupportMdpGenerator<ValueType>::JaniBeliefSupportMdpGenerator(storm::models::sparse::Pomdp<ValueType> const& pomdp)
    : pomdp(pomdp), model("beliefsupport", jani::ModelType::MDP) {
    // Intentionally left empty.
}

template<typename ValueType>
void JaniBeliefSupportMdpGenerator<ValueType>::generate(storm::storage::BitVector const& targetStates, storm::storage::BitVector const& badStates) {
    //
    std::map<uint64_t, std::map<detail::ObsActPair, std::vector<uint64_t>>> predecessorInfo;
    std::map<detail::ObsActPair, std::map<uint64_t, std::set<uint64_t>>> obsPredInfo;
    std::set<detail::ObsActPair> obsactpairs;
    // Initialize predecessorInfo for clean steps.
    for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
        predecessorInfo[state] = {};
    }

    // Fill predecessorInfo
    for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
        uint64_t curObs = pomdp.getObservation(state);
        for (uint64_t act = 0; act < pomdp.getNumberOfChoices(state); ++act) {
            detail::ObsActPair oap(curObs, act);
            obsactpairs.insert(oap);
            if (obsPredInfo.count(oap) == 0) {
                obsPredInfo[oap] = std::map<uint64_t, std::set<uint64_t>>();
            }
            for (auto const& entry : pomdp.getTransitionMatrix().getRow(state, act)) {
                if (predecessorInfo[entry.getColumn()].count(oap) == 0) {
                    predecessorInfo[entry.getColumn()][oap] = {state};
                } else {
                    predecessorInfo[entry.getColumn()][oap].push_back(state);
                }
                uint64_t newObs = pomdp.getObservation(entry.getColumn());
                if (obsPredInfo[oap].count(newObs) == 0) {
                    obsPredInfo[oap][newObs] = {state};
                } else {
                    obsPredInfo[oap][newObs].insert(state);
                }
            }
        }
    }
    uint64_t initialObs = pomdp.getObservation(*(pomdp.getInitialStates().begin()));

    expressions::ExpressionManager& exprManager = model.getManager();
    storm::expressions::Variable posProbVar = exprManager.declareRationalVariable("posProb");
    model.addConstant(jani::Constant("posProb", posProbVar));
    std::map<uint64_t, const jani::Variable*> stateVariables;
    const jani::Variable* obsVar =
        &model.addVariable(*storm::jani::Variable::makeBoundedIntegerVariable("obs", exprManager.declareIntegerVariable("obs"), exprManager.integer(initialObs),
                                                                              false, exprManager.integer(0), exprManager.integer(pomdp.getNrObservations())));
    const jani::Variable* oldobsActVar =
        &model.addVariable(*storm::jani::Variable::makeBoundedIntegerVariable("prevact", exprManager.declareIntegerVariable("prevact"), exprManager.integer(0),
                                                                              false, exprManager.integer(0), exprManager.integer(obsactpairs.size())));

    for (uint64_t i = 0; i < pomdp.getNumberOfStates(); ++i) {
        std::string name = "s" + std::to_string(i);
        bool isInitial = pomdp.getInitialStates().get(i);
        stateVariables.emplace(i, &model.addVariable(*storm::jani::Variable::makeBooleanVariable(name, exprManager.declareBooleanVariable(name),
                                                                                                 exprManager.boolean(isInitial), false)));
    }

    std::map<detail::ObsActPair, uint64_t> actionIndices;
    for (auto const& oap : obsactpairs) {
        std::string actname = "act" + std::to_string(oap.action) + "from" + std::to_string(oap.observation);
        uint64_t actindex = model.addAction(jani::Action(actname));
        actionIndices[oap] = actindex;
    }

    for (uint64_t i = 0; i < pomdp.getNumberOfStates(); ++i) {
        std::string name = "aut-s" + std::to_string(i);
        jani::Automaton aut(name, exprManager.declareIntegerVariable(name));
        uint64_t primeloc = aut.addLocation(jani::Location("default"));
        aut.addInitialLocation(primeloc);
        for (auto const& oaps : actionIndices) {
            if (obsPredInfo[oaps.first].count(pomdp.getObservation(i)) == 0 && pomdp.getObservation(i) != oaps.first.observation) {
                continue;
            }
            std::shared_ptr<jani::TemplateEdge> tedge = std::make_shared<jani::TemplateEdge>(exprManager.boolean(true));
            jani::TemplateEdgeDestination edgedest;
            std::vector<storm::expressions::Expression> exprVec;
            for (auto const& pred : predecessorInfo[i][oaps.first]) {
                exprVec.push_back(stateVariables.at(pred)->getExpressionVariable().getExpression());
            }
            if (exprVec.empty()) {
                edgedest.addAssignment(jani::Assignment(storm::jani::LValue(*stateVariables.at(i)), exprManager.boolean(false)));
            } else {
                edgedest.addAssignment(jani::Assignment(storm::jani::LValue(*stateVariables.at(i)),
                                                        (exprManager.integer(pomdp.getObservation(i)) == obsVar->getExpressionVariable().getExpression()) &&
                                                            storm::expressions::disjunction(exprVec)));
            }
            tedge->addDestination(edgedest);
            aut.addEdge(jani::Edge(primeloc, oaps.second, boost::none, tedge, {primeloc}, {exprManager.rational(1.0)}));
        }
        model.addAutomaton(aut);
    }
    jani::Automaton obsAut("obsAut", exprManager.declareIntegerVariable("obsAut"));
    auto const& targetVar = model.addVariable(
        *storm::jani::Variable::makeBooleanVariable("target", exprManager.declareBooleanVariable("target"), exprManager.boolean(false), true));
    std::vector<storm::expressions::Expression> notTargetExpression;
    for (auto const state : ~targetStates) {
        notTargetExpression.push_back(!stateVariables.at(state)->getExpressionVariable().getExpression());
    }
    auto const& badVar =
        model.addVariable(*storm::jani::Variable::makeBooleanVariable("bad", exprManager.declareBooleanVariable("bad"), exprManager.boolean(false), true));
    std::vector<storm::expressions::Expression> badExpression;
    for (auto const state : badStates) {
        badExpression.push_back(stateVariables.at(state)->getExpressionVariable().getExpression());
    }

    auto primeLocation = jani::Location("primary");
    primeLocation.addTransientAssignment(jani::Assignment(targetVar, expressions::conjunction(notTargetExpression)));
    primeLocation.addTransientAssignment(
        jani::Assignment(badVar, badExpression.empty() ? exprManager.boolean(false) : expressions::disjunction(badExpression)));
    uint64_t primeloc = obsAut.addLocation(primeLocation);
    obsAut.addInitialLocation(primeloc);
    uint64_t secloc = obsAut.addLocation(jani::Location("secondary"));
    // First edges, select observation
    for (auto const& oaps : actionIndices) {
        std::shared_ptr<jani::TemplateEdge> tedge =
            std::make_shared<jani::TemplateEdge>(exprManager.integer(oaps.first.observation) == obsVar->getExpressionVariable().getExpression());
        std::vector<uint64_t> destLocs;
        std::vector<storm::expressions::Expression> probs;

        for (auto const& obsAndPredStates : obsPredInfo[oaps.first]) {
            jani::TemplateEdgeDestination tedgedest;
            tedgedest.addAssignment(jani::Assignment(jani::LValue(*obsVar), exprManager.integer(obsAndPredStates.first)), true);
            tedgedest.addAssignment(jani::Assignment(jani::LValue(*oldobsActVar), exprManager.integer(oaps.second)), true);

            tedge->addDestination(tedgedest);

            destLocs.push_back(secloc);
            std::vector<storm::expressions::Expression> predExpressions;
            for (auto predstate : obsAndPredStates.second) {
                predExpressions.push_back(stateVariables.at(predstate)->getExpressionVariable().getExpression());
            }
            probs.push_back(storm::expressions::ite(storm::expressions::disjunction(predExpressions), posProbVar.getExpression(), exprManager.rational(0)));
        }
        jani::Edge edge(primeloc, jani::Model::SILENT_ACTION_INDEX, boost::none, tedge, destLocs, probs);
        obsAut.addEdge(edge);
    }
    // Back edges
    for (auto const& oaps : obsactpairs) {
        std::shared_ptr<jani::TemplateEdge> tedge =
            std::make_shared<jani::TemplateEdge>(oldobsActVar->getExpressionVariable() == exprManager.integer(actionIndices[oaps]));

        jani::TemplateEdgeDestination tedgedest;
        tedgedest.addAssignment(jani::Assignment(jani::LValue(*oldobsActVar), exprManager.integer(0)));
        tedge->addDestination(tedgedest);
        jani::Edge edge(secloc, actionIndices[oaps], boost::none, tedge, {primeloc}, {exprManager.rational(1.0)});
        obsAut.addEdge(edge);
    }
    model.addAutomaton(obsAut);
    model.setStandardSystemComposition();
    model.finalize();
}

template<typename ValueType>
void JaniBeliefSupportMdpGenerator<ValueType>::verifySymbolic(storm::Environment const& env, bool onlyInitial) {
    storage::SymbolicModelDescription symdesc(model);
    // This trick only works because we do not explictly check that the model is stochastic!
    symdesc = symdesc.preprocess("posProb=0.1");
    auto property = storm::api::parsePropertiesForJaniModel("Pmax>=1 [!\"bad\" U \"target\"]", model)[0];
    auto mdp = storm::api::buildSymbolicModel<storm::dd::DdType::Sylvan, ValueType>(env, symdesc, {property.getRawFormula()});
    std::unique_ptr<modelchecker::CheckResult> result =
        storm::api::verifyWithDdEngine(env, mdp, storm::api::createTask<ValueType>(property.getRawFormula(), onlyInitial));
    std::unique_ptr<storm::modelchecker::CheckResult> filter;

    if (onlyInitial) {
        filter = std::make_unique<storm::modelchecker::SymbolicQualitativeCheckResult<storm::dd::DdType::Sylvan>>(mdp->getReachableStates(),
                                                                                                                  mdp->getInitialStates());
    } else {
        auto relstates = storm::api::parsePropertiesForJaniModel("prevact=0", model)[0];
        filter =
            storm::api::verifyWithDdEngine<storm::dd::DdType::Sylvan, ValueType>(env, mdp, storm::api::createTask<ValueType>(relstates.getRawFormula(), false));
    }
    if (result && filter) {
        result->filter(filter->asQualitativeCheckResult());
    }

    if (result && !onlyInitial) {
        auto vars = result->asSymbolicQualitativeCheckResult<storm::dd::DdType::Sylvan>().getTruthValuesVector().getContainedMetaVariables();
        // TODO Export these results.
    } else if (result) {
        initialIsWinning = result->asQualitativeCheckResult().existsTrue();
    }
}

template<typename ValueType>
bool JaniBeliefSupportMdpGenerator<ValueType>::isInitialWinning() const {
    return initialIsWinning;
}

template class JaniBeliefSupportMdpGenerator<double>;
template class JaniBeliefSupportMdpGenerator<storm::RationalNumber>;
template class JaniBeliefSupportMdpGenerator<storm::RationalFunction>;

}  // namespace qualitative
}  // namespace pomdp
}  // namespace storm