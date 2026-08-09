#pragma once

#include <map>

#include "storm/logic/Formula.h"

#include "storm/models/ModelBase.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"

#include "storm/solver/Z3SmtSolver.h"

#include "storm/storage/SparseMatrix.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/RationalFunctionToExpression.h"

#include "storm/utility/constants.h"

#include "storm-pars/api/region.h"
#include "storm-pars/modelchecker/region/monotonicity/Assumption.h"
#include "storm-pars/modelchecker/region/monotonicity/AssumptionMaker.h"
#include "storm-pars/modelchecker/region/monotonicity/LocalMonotonicityResult.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityResult.h"
#include "storm-pars/modelchecker/region/monotonicity/Order.h"
#include "storm-pars/modelchecker/region/monotonicity/OrderExtender.h"

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
        return MonotonicityChecker<ValueType>::checkDerivative(derivative, reg);
    }

    /*!
     * Builds Reachability Orders for the given model and simultaneously uses them to check for Monotonicity.
     *
     * @param outfile Outfile to which results are written.
     * @param dotOutfileName Name for the files of the dot outputs should they be generated
     * @return Map which maps each order to its Reachability Order and used assumptions.
     */
    std::map<std::shared_ptr<Order>, std::pair<std::shared_ptr<MonotonicityResult<VariableType>>, std::vector<Assumption>>> checkMonotonicityInBuild(
        std::ostream& outfile, bool usePLA = false, std::string dotOutfileName = "dotOutput");

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

    void extendOrderWithAssumptions(std::shared_ptr<Order> order, uint_fast64_t val1, uint_fast64_t val2, std::vector<Assumption> assumptions,
                                    std::shared_ptr<MonotonicityResult<VariableType>> monRes);

    std::shared_ptr<models::ModelBase> model;

    std::vector<std::shared_ptr<logic::Formula const>> formulas;

    bool dotOutput;

    bool checkSamples;

    bool onlyCheckOnOrder;

    MonotonicityResult<VariableType> resultCheckOnSamples;

    std::map<VariableType, std::vector<uint_fast64_t>> occuringStatesAtVariable;

    std::map<std::shared_ptr<Order>, std::pair<std::shared_ptr<MonotonicityResult<VariableType>>, std::vector<Assumption>>> monResults;

    std::unique_ptr<OrderExtender<ValueType, ConstantType>> extender;

    ConstantType precision;

    Region region;

    storage::SparseMatrix<ValueType> matrix;

    std::unordered_map<ValueType, std::unordered_map<VariableType, ValueType>> derivatives;

    AssumptionMaker<ValueType, ConstantType> assumptionMaker;
};
}  // namespace analysis
}  // namespace storm
