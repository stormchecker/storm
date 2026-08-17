#pragma once

#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "storm-pars/modelchecker/region/monotonicity/LocalMonotonicityResult.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityResult.h"
#include "storm-pars/utility/parametric.h"
#include "storm/adapters/RationalFunctionAdapter.h"  // TODO: use forward header
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"

namespace storm {

namespace analysis {
class Order;
}  // namespace analysis

namespace storage {
template<typename ParametricType>
class ParameterRegion;
}  // namespace storage

namespace transformer {

/*!
 * This class lifts parameter choices to nondeterminism:
 * For each row in the  given matrix that considerd #par parameters, the resulting matrix will have one row group consisting of 2^#par rows.
 * When specifying a region, each row within the row group is evaluated w.r.t. one vertex of the region.
 * The given vector is handled similarly.
 * However, if a vector entry considers a parameter that does not occur in the corresponding matrix row,
 * the parameter is directly set such that the vector entry is maximized (or minimized, depending on the specified optimization direction).
 *
 * @note The row grouping of the original matrix is ignored.
 */
template<typename ParametricType, typename ConstantType>
class ParameterLifter {
   public:
    typedef typename storm::utility::parametric::VariableType<ParametricType>::type VariableType;
    typedef typename storm::utility::parametric::CoefficientType<ParametricType>::type CoefficientType;
    typedef typename storm::analysis::MonotonicityResult<VariableType>::Monotonicity Monotonicity;

    /*!
     * Lifts the parameter choices to nondeterminisim. The computation is performed on the submatrix specified by the selected rows and columns
     * @param pMatrix the parametric matrix
     * @param pVector the parametric vector (the vector size should equal the row count of the matrix)
     * @param selectedRows a Bitvector that specifies which rows of the matrix and the vector are considered.
     * @param selectedColumns a Bitvector that specifies which columns of the matrix are considered.
     */
    ParameterLifter(storm::storage::SparseMatrix<ParametricType> const& pMatrix, std::vector<ParametricType> const& pVector,
                    storm::storage::BitVector const& selectedRows, storm::storage::BitVector const& selectedColumns, bool generateRowLabels = false,
                    bool useMonotonicity = false);

    void specifyRegion(storm::storage::ParameterRegion<ParametricType> const& region, storm::solver::OptimizationDirection const& dirForParameters);

    // Returns the resulting matrix. Should only be called AFTER specifying a region
    storm::storage::SparseMatrix<ConstantType> const& getMatrix() const;

    // Returns the resulting vector. Should only be called AFTER specifying a region
    std::vector<ConstantType> const& getVector() const;

    std::vector<std::set<VariableType>> const& getOccurringVariablesAtState() const;

    std::map<VariableType, std::set<uint_fast64_t>> const& getOccuringStatesAtVariable() const;

    uint_fast64_t getRowGroupIndex(uint_fast64_t originalState) const;
    uint_fast64_t getOriginalStateNumber(uint_fast64_t newState) const;
    uint_fast64_t getRowGroupSize(uint_fast64_t originalState) const;
    uint_fast64_t getRowGroupCount() const;

    /*
     * During initialization, the actual regions are not known. Hence, we consider abstract valuations,
     * where it is only known whether a parameter will be set to either the lower/upper bound of the region or whether this is unspecified
     */
    class AbstractValuation {
       public:
        AbstractValuation() = default;
        AbstractValuation(AbstractValuation const& other) = default;
        bool operator==(AbstractValuation const& other) const;

        void addParameterLower(VariableType const& var);
        void addParameterUpper(VariableType const& var);
        void addParameterUnspecified(VariableType const& var);

        std::size_t getHashValue() const;
        AbstractValuation getSubValuation(std::set<VariableType> const& pars) const;
        std::set<VariableType> const& getLowerParameters() const;
        std::set<VariableType> const& getUpperParameters() const;
        std::set<VariableType> const& getUnspecifiedParameters() const;

        /*!
         * Returns the concrete valuation(s) (w.r.t. the provided region) represented by this abstract valuation.
         * Note that an abstract valuation represents 2^(#unspecified parameters) many concrete valuations.
         */
        std::vector<storm::utility::parametric::Valuation<ParametricType>> getConcreteValuations(
            storm::storage::ParameterRegion<ParametricType> const& region) const;

       private:
        std::set<VariableType> lowerPars, upperPars, unspecifiedPars;
    };

    // Returns for each row the abstract valuation for this row
    // Note: the returned vector might be empty if row label generaion was disabled initially
    std::vector<AbstractValuation> const& getRowLabels() const;

   private:
    /*
     * We minimize the number of function evaluations by only calling evaluate() once for each unique pair of function and valuation.
     * The result of each evaluation is then written to all positions in the matrix (and the vector) where the corresponding (function,valuation) occurred.
     */

    /*!
     * Collects all occurring pairs of functions and (abstract) valuations.
     * We also store a placeholder for the result of each pair. The result is computed and written into the placeholder whenever a region and optimization
     * direction is specified.
     */
    class FunctionValuationCollector {
       public:
        FunctionValuationCollector() = default;

        /*!
         * Adds the provided function and valuation.
         * Returns a reference to a placeholder in which the evaluation result will be written upon calling evaluateCollectedFunctions)
         */
        ConstantType& add(ParametricType const& function, AbstractValuation const& valuation);

        void evaluateCollectedFunctions(storm::storage::ParameterRegion<ParametricType> const& region,
                                        storm::solver::OptimizationDirection const& dirForUnspecifiedParameters);

       private:
        // Stores a function and a valuation. The valuation is stored as an index of the collectedValuations-vector.
        typedef std::pair<ParametricType, AbstractValuation> FunctionValuation;

        class FuncValHash {
           public:
            std::size_t operator()(FunctionValuation const& fv) const {
                std::size_t seed = 0;
                carl::hash_add(seed, fv.first);
                carl::hash_add(seed, fv.second.getHashValue());
                return seed;
            }
        };

        // Stores the collected functions with the valuations together with a placeholder for the result.
        std::unordered_map<FunctionValuation, ConstantType, FuncValHash> collectedFunctions;
    };

    FunctionValuationCollector functionValuationCollector;

    // Returns the 2^(variables.size()) vertices of the region
    std::vector<AbstractValuation> getVerticesOfAbstractRegion(std::set<VariableType> const& variables) const;

    std::vector<AbstractValuation> rowLabels;

    std::vector<uint_fast64_t> oldToNewColumnIndexMapping;  // Mapping from old to new columnIndex used for monotonicity
    std::vector<uint_fast64_t> rowGroupToStateNumber;       // Mapping from new to old columnIndex used for monotonicity

    storm::storage::SparseMatrix<ConstantType> matrix;  // The resulting matrix;
    std::vector<std::pair<typename storm::storage::SparseMatrix<ConstantType>::iterator, ConstantType&>>
        matrixAssignment;  // Connection of matrix entries with placeholders

    std::vector<ConstantType> vector;                                                                      // The resulting vector
    std::vector<std::pair<typename std::vector<ConstantType>::iterator, ConstantType&>> vectorAssignment;  // Connection of vector entries with placeholders

    // Used for monotonicity in sparsedtmcparameterlifter
    std::vector<std::set<VariableType>> occurringVariablesAtState;
    std::map<VariableType, std::set<uint_fast64_t>> occuringStatesAtVariable;
};

}  // namespace transformer
}  // namespace storm
