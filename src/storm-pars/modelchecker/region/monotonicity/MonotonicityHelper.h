#pragma once

#include <map>
#include "AssumptionMaker.h"
#include "LocalMonotonicityResult.h"
#include "MonotonicityResult.h"
#include "Order.h"
#include "OrderExtender.h"

#include "storm/logic/Formula.h"

#include "storm/models/ModelBase.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"

#include "storm/solver/Z3SmtSolver.h"

#include "storm/storage/SparseMatrix.h"
#include "storm/storage/expressions/BinaryRelationExpression.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/RationalFunctionToExpression.h"

#include "storm/utility/constants.h"

#include "storm-pars/api/region.h"

namespace storm {
namespace analysis {

template<typename ValueType, typename ConstantType>
class MonotonicityHelper {
   public:
    typedef typename utility::parametric::VariableType<ValueType>::type VariableType;
    typedef typename utility::parametric::CoefficientType<ValueType>::type CoefficientType;
    typedef typename MonotonicityResult<VariableType>::Monotonicity Monotonicity;
    typedef typename storage::ParameterRegion<ValueType> Region;

    /*!
     * Constructor of MonotonicityHelper.
     *
     * @param model The model considered.
     * @param formulas The formulas considered.
     * @param regions The regions to consider.
     * @param numberOfSamples Number of samples taken for monotonicity checking, default 0,
     *          if 0 then no check on samples is executed.
     * @param precision Precision on which the samples are compared
     * @param dotOutput Whether or not dot output should be generated for the ROs.
     */
    MonotonicityHelper(std::shared_ptr<models::sparse::Model<ValueType>> model, std::vector<std::shared_ptr<logic::Formula const>> formulas,
                       std::vector<storage::ParameterRegion<ValueType>> regions, uint_fast64_t numberOfSamples = 0, double const& precision = 0.000001,
                       bool dotOutput = false);

    /*!
     * Checks if a derivative >=0 or/and <=0
     *
     * @param derivative The derivative you want to check
     * @return pair of bools, >= 0 and <= 0
     */
    static std::pair<bool, bool> checkDerivative(ValueType derivative, storage::ParameterRegion<ValueType> reg) {
        bool monIncr = false;
        bool monDecr = false;

        if (derivative.isZero()) {
            monIncr = true;
            monDecr = true;
        } else if (derivative.isConstant()) {
            monIncr = derivative.constantPart() >= 0;
            monDecr = derivative.constantPart() <= 0;
        } else {
            std::shared_ptr<utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<utility::solver::MathsatSmtSolverFactory>();
            std::shared_ptr<expressions::ExpressionManager> manager(new expressions::ExpressionManager());
            solver::Z3SmtSolver s(*manager);
            std::set<VariableType> variables = derivative.gatherVariables();

            expressions::Expression exprBounds = manager->boolean(true);
            for (auto variable : variables) {
                auto managerVariable = manager->declareRationalVariable(variable.name());
                auto lb = utility::convertNumber<RationalNumber>(reg.getLowerBoundary(variable));
                auto ub = utility::convertNumber<RationalNumber>(reg.getUpperBoundary(variable));
                exprBounds = exprBounds && manager->rational(lb) < managerVariable && managerVariable < manager->rational(ub);
            }

            auto converter = expressions::RationalFunctionToExpression<ValueType>(manager);

            // < 0, so not monotone increasing. If this is unsat, then it should be monotone increasing.
            expressions::Expression exprToCheck = converter.toExpression(derivative) < manager->rational(0);
            s.add(exprBounds);
            s.add(exprToCheck);
            monIncr = s.check() == solver::SmtSolver::CheckResult::Unsat;

            // > 0, so not monotone decreasing. If this is unsat it should be monotone decreasing.
            exprToCheck = converter.toExpression(derivative) > manager->rational(0);
            s.reset();
            s.add(exprBounds);
            s.add(exprToCheck);
            monDecr = s.check() == solver::SmtSolver::CheckResult::Unsat;
        }
        STORM_LOG_ASSERT(!(monIncr && monDecr) || derivative.isZero(), "Monotonicity both increasing and decreasing but derivative not zero.");

        return std::pair<bool, bool>(monIncr, monDecr);
    }

    /*!
     * Builds Reachability Orders for the given model and simultaneously uses them to check for Monotonicity.
     *
     * @param outfile Outfile to which results are written.
     * @param dotOutfileName Name for the files of the dot outputs should they be generated
     * @return Map which maps each order to its Reachability Order and used assumptions.
     */
    std::map<std::shared_ptr<Order>,
             std::pair<std::shared_ptr<MonotonicityResult<VariableType>>, std::vector<std::shared_ptr<expressions::BinaryRelationExpression>>>>
    checkMonotonicityInBuild(std::ostream& outfile, bool usePLA = false, std::string dotOutfileName = "dotOutput");

    /*!
     * Builds Reachability Orders for the given model and simultaneously uses them to check for Monotonicity.
     *
     */
    std::shared_ptr<LocalMonotonicityResult<VariableType>> createLocalMonotonicityResult(std::shared_ptr<Order> order,
                                                                                         storage::ParameterRegion<ValueType> region);

   private:
    void createOrder();

    void checkMonotonicityOnSamples(std::shared_ptr<models::sparse::Dtmc<ValueType>> model, uint_fast64_t numberOfSamples);

    void checkMonotonicityOnSamples(std::shared_ptr<models::sparse::Mdp<ValueType>> model, uint_fast64_t numberOfSamples);

    void extendOrderWithAssumptions(std::shared_ptr<Order> order, uint_fast64_t val1, uint_fast64_t val2,
                                    std::vector<std::shared_ptr<expressions::BinaryRelationExpression>> assumptions,
                                    std::shared_ptr<MonotonicityResult<VariableType>> monRes);

    std::shared_ptr<models::ModelBase> model;

    std::vector<std::shared_ptr<logic::Formula const>> formulas;

    bool dotOutput;

    bool checkSamples;

    bool onlyCheckOnOrder;

    MonotonicityResult<VariableType> resultCheckOnSamples;

    std::map<VariableType, std::vector<uint_fast64_t>> occuringStatesAtVariable;

    std::map<std::shared_ptr<Order>,
             std::pair<std::shared_ptr<MonotonicityResult<VariableType>>, std::vector<std::shared_ptr<expressions::BinaryRelationExpression>>>>
        monResults;

    OrderExtender<ValueType, ConstantType>* extender;

    ConstantType precision;

    Region region;

    storage::SparseMatrix<ValueType> matrix;

    std::unordered_map<ValueType, std::unordered_map<VariableType, ValueType>> derivatives;

    AssumptionMaker<ValueType, ConstantType> assumptionMaker;
};
}  // namespace analysis
}  // namespace storm
