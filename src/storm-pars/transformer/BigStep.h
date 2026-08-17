#pragma once

#include <memory>
#include <set>

#include "storm/adapters/IntervalForward.h"
#include "storm/adapters/RationalFunctionAdapter.h"  // TODO: use forward header
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/StateLabeling.h"

namespace storm {

namespace logic {
class Formula;
}  // namespace logic

namespace modelchecker {
template<typename FormulaType, typename ValueType>
class CheckTask;
}  // namespace modelchecker

namespace storage {
template<typename ValueType>
class FlexibleSparseMatrix;
}  // namespace storage

namespace transformer {

using UniPoly = RawUnivariatePolynomial;

// optimization for the polynomial cache - built in comparison is slow
struct UniPolyCompare {
    bool operator()(const UniPoly& lhs, const UniPoly& rhs) const;
};

struct PolynomialCache : std::unordered_map<RationalFunctionVariable, std::pair<std::map<UniPoly, uint64_t, UniPolyCompare>, std::vector<UniPoly>>> {
    /**
     * Look up the index of this polynomial in the cache. If it doesn't exist, adds it to the cache.
     *
     * @param f The polynomial.
     * @param p The main parameter of the polynomial.
     * @return uint64_t The index of the polynomial.
     */
    uint64_t lookUpInCache(UniPoly const& f, RationalFunctionVariable const& p);

    /**
     * @brief Computes a univariate polynomial from a factorization.
     *
     * @param factorization The factorization (a vector of exponents, indices = position in cache).
     * @param p The parameter.
     * @return UniPoly The univariate polynomial.
     */
    UniPoly polynomialFromFactorization(std::vector<uint64_t> const& factorization, RationalFunctionVariable const& p) const;
};

template<typename Container>
struct container_hash {
    std::size_t operator()(Container const& c) const {
        return boost::hash_range(c.begin(), c.end());
    }
};

class Annotation : public std::unordered_map<std::vector<uint64_t>, RationalFunctionCoefficient, container_hash<std::vector<uint64_t>>> {
   public:
    Annotation(RationalFunctionVariable parameter, std::shared_ptr<PolynomialCache> polynomialCache);

    /**
     * Add another annotation to this annotation.
     *
     * @param other The other annotation.
     */
    void operator+=(const Annotation other);

    /**
     * Multiply this annotation with a rational number.
     *
     * @param n The rational number.
     */
    void operator*=(RationalFunctionCoefficient n);

    /**
     * Multiply this annotation with a rational number to get a new annotation.
     *
     * @param n The rational number.
     */
    Annotation operator*(RationalFunctionCoefficient n) const;

    /**
     * Adds another annotation times a constant to this annotation.
     *
     * @param other The other annotation.
     * @param timesConstant The constant.
     */
    void addAnnotationTimesConstant(Annotation const& other, RationalFunctionCoefficient timesConstant);

    /**
     * Adds another annotation times a polynomial to this annotation.
     *
     * @param other The other annotation.
     * @param polynomial The polynomial.
     */
    void addAnnotationTimesPolynomial(Annotation const& other, UniPoly&& polynomial);

    /**
     * Adds another annotation times an annotation to this annotation.
     *
     * @param anno1 The first annotation.
     * @param anno2 The second annotation.
     */
    void addAnnotationTimesAnnotation(Annotation const& anno1, Annotation const& anno2);

    /**
     * @brief Get the probability of this annotation as a univariate polynomial (which isn't factorized).
     *
     * @return UniPoly The probability.
     */
    UniPoly getProbability() const;

    /**
     * @brief Get all of the terms of the UniPoly.
     *
     * @return std::vector<UniPoly> The terms.
     */
    std::vector<UniPoly> getTerms() const;

    template<typename ConstantType>
    ConstantType evaluate(ConstantType input) const {
        ConstantType sumOfTerms = utility::zero<ConstantType>();
        for (auto const& [info, constant] : *this) {
            ConstantType outerMult = utility::one<ConstantType>();
            for (uint64_t i = 0; i < info.size(); i++) {
                auto polynomial = this->polynomialCache->at(parameter).second[i];
                // Evaluate the inner polynomial by its coefficients
                auto coefficients = polynomial.coefficients();
                ConstantType innerSum = utility::zero<ConstantType>();
                for (uint64_t exponent = 0; exponent < coefficients.size(); exponent++) {
                    if (exponent != 0) {
                        innerSum += carl::pow(input, exponent) * utility::convertNumber<ConstantType>(coefficients[exponent]);
                    } else {
                        innerSum += utility::convertNumber<ConstantType>(coefficients[exponent]);
                    }
                }
                // Inner polynomial ^ exponent
                outerMult *= carl::pow(innerSum, info[i]);
            }
            sumOfTerms += outerMult * utility::convertNumber<ConstantType>(constant);
        }
        return sumOfTerms;
    }

    Interval evaluateOnIntervalMidpointTheorem(Interval input, bool higherOrderBounds = false) const;

    RationalFunctionVariable getParameter() const;

    void computeDerivative(uint64_t nth);

    uint64_t maxDegree() const;

    std::shared_ptr<Annotation> derivative();

    friend std::ostream& operator<<(std::ostream& os, const Annotation& annotation);

   private:
    const RationalFunctionVariable parameter;
    const std::shared_ptr<PolynomialCache> polynomialCache;
    std::shared_ptr<Annotation> derivativeOfThis;
};

std::ostream& operator<<(std::ostream& os, const Annotation& annotation);

/**
 * Shorthand for std::unordered_map<T, uint64_t>. Counts elements (which elements, how many of them).
 *
 * @tparam T
 */
class BigStep {
   public:
    /**
     * This class re-orders parameteric transitions of a pMC so bounds computed by Parameter Lifting will eventually be better.
     * The parametric reachability probability for the given check task will be the same in the time-travelled and in the original model.
     */
    BigStep() : polynomialCache(std::make_shared<PolynomialCache>()) {
        // Intentionally left empty
    }

    RationalFunction uniPolyToRationalFunction(UniPoly poly);

    /**
     * Perform big-step on the given model and the given checkTask.
     *
     * @param model A pMC.
     * @param checkTask A property (probability or reward) on the pMC.
     * @return models::sparse::Dtmc<RationalFunction> The time-travelled pMC.
     */
    std::pair<models::sparse::Dtmc<RationalFunction>, std::map<UniPoly, Annotation>> bigStep(
        models::sparse::Dtmc<RationalFunction> const& model, modelchecker::CheckTask<logic::Formula, RationalFunction> const& checkTask);

    static std::unordered_map<RationalFunction, Annotation> lastSavedAnnotations;

   private:
    /**
     * Find the paths we can big-step from this state using this parameter.
     *
     * @param start Root state of search.
     * @param parameter Parameter w.r.t. which we search.
     * @param flexibleMatrix Matrix of the pMC.
     * @param backwardsFlexibleMatrix Transposed matrix of the pMC.
     * @param treeStates Tree states (see updateTreeStates).
     * @param stateRewardVector State-reward vector of the pMC (because we are not big-stepping states with rewards.)
     * @return std::pair<std::vector<std::shared_ptr<searchingPath>>, std::vector<uint64_t>> Resulting paths, all states we visited while searching paths.
     */
    std::pair<std::map<uint64_t, Annotation>, std::pair<std::vector<uint64_t>, std::map<uint64_t, std::set<uint64_t>>>> bigStepBFS(
        uint64_t start, const RationalFunctionVariable& parameter, const storage::FlexibleSparseMatrix<RationalFunction>& flexibleMatrix,
        const storage::FlexibleSparseMatrix<RationalFunction>& backwardsFlexibleMatrix,
        const std::map<RationalFunctionVariable, std::map<uint64_t, std::set<uint64_t>>>& treeStates,
        const boost::optional<std::vector<RationalFunction>>& stateRewardVector, const std::map<UniPoly, Annotation>& storedAnnotations);

    /**
     * Find time-travelling on the given big-step paths, i.e., identify transitions that are linear to each other and put them into seperate states,
     * transforming the pMC's topology. This modifies the matrix because it's already putting in the new states.
     *
     * @param bigStepPaths The big-step paths that time-travelling will be performed on.
     * @param parameter The parameter w.r.t. which we are time-travelling.
     * @param flexibleMatrix Flexible matrix, modifies this!
     * @param backwardsFlexibleMatrix Backwards matrix, modifies this!
     * @param treeStatesNeedUpdate Map of the tree states that need updating (modifies this!)
     * @param originalNumStates Numbers of original states in pMC (for alreadyTimeTravelledToThis map)
     * @return std::optional<std::vector<std::shared_ptr<searchingPath>>>
     */
    std::vector<std::pair<uint64_t, Annotation>> findBigStep(const std::map<uint64_t, Annotation> bigStepAnnotations, const RationalFunctionVariable& parameter,
                                                             storage::FlexibleSparseMatrix<RationalFunction>& flexibleMatrix,
                                                             storage::FlexibleSparseMatrix<RationalFunction>& backwardsFlexibleMatrix,
                                                             std::map<RationalFunctionVariable, std::set<std::set<uint64_t>>>& alreadyTimeTravelledToThis,
                                                             std::map<RationalFunctionVariable, std::set<uint64_t>>& treeStatesNeedUpdate, uint64_t root,
                                                             uint64_t originalNumStates);

    /**
     * Actually eliminate transitions in flexibleMatrix and backwardFlexibleMatrix according to the paths we found and want to eliminate.
     *
     * @param state The root state for the paths.
     * @param paths The paths.
     * @param flexibleMatrix The flexible matrix (modifies this!)
     * @param backwardsFlexibleMatrix The backwards flexible matrix (modifies this!)
     * @param treeStatesNeedUpdate The map of tree states that need updating (modifies this!)
     */
    std::map<UniPoly, Annotation> replaceWithNewTransitions(uint64_t state, const std::vector<std::pair<uint64_t, Annotation>> transitions,
                                                            storage::FlexibleSparseMatrix<RationalFunction>& flexibleMatrix,
                                                            storage::FlexibleSparseMatrix<RationalFunction>& backwardsFlexibleMatrix,
                                                            storage::BitVector& reachableStates,
                                                            std::map<RationalFunctionVariable, std::set<uint64_t>>& treeStatesNeedUpdate);

    /**
     * Updates which states are unreachable after the previous transformation without needing a model checking procedure.
     *
     * @param reachableStates Reachable states to the best of our knowledge (modifies this!)
     * @param statesMaybeUnreachable States that may have become unreachable (= the visited states during the big step search)
     * @param backwardsFlexibleMatrix The backwards flexible matrix in which to look for predecessors.
     */
    void updateUnreachableStates(storage::BitVector& reachableStates, std::vector<uint64_t> const& statesMaybeUnreachable,
                                 storage::FlexibleSparseMatrix<RationalFunction> const& backwardsFlexibleMatrix, uint64_t initialState);

    /**
     * updateTreeStates updates the `treeStates` map on the given states.
     * The `treeStates` map keeps track of the parametric transitions reachable with constants from any given state: for some parameter, for some state, this
     * set of parametric transitions is reachable by constant transitions. This function creates or updates this map by searching from the transitions in the
     * working sets upwards.
     *
     * @param treeStates The tree states map to update (mutates this).
     * @param workingSets Where to start the search. When creating the tree states map: set this to all states with parametric transitions (mutates this).
     * @param flexibleMatrix The flexible matrix of the pMC.
     * @param flexibleMatrix The transposed flexibleMatrix.
     * @param allParameters The set of all parameters of the pMC.
     * @param stateRewardVector The state reward vector of the pMC.
     * @param stateLabelling The state labelling of the pMC.
     */
    void updateTreeStates(std::map<RationalFunctionVariable, std::map<uint64_t, std::set<uint64_t>>>& treeStates,
                          std::map<RationalFunctionVariable, std::set<uint64_t>>& workingSets,
                          const storage::FlexibleSparseMatrix<RationalFunction>& flexibleMatrix,
                          const storage::FlexibleSparseMatrix<RationalFunction>& backwardsTransitions, const std::set<RationalFunctionVariable>& allParameters,
                          const boost::optional<std::vector<RationalFunction>>& stateRewardVector, const models::sparse::StateLabeling stateLabelling);

    /**
     * extendStateLabeling extends the given state labeling to newly created states. It will set the new labels to the labels on the given state.
     *
     * @param oldLabeling The old labeling.
     * @param oldSize The size of the old labeling.
     * @param newSize The size of the new labeling (>= oldSize).
     * @param stateWithLabels The new states will have the labels that this state has.
     * @param labelsInFormula The labels that occur in the property.
     * @return models::sparse::StateLabeling
     */
    models::sparse::StateLabeling extendStateLabeling(const models::sparse::StateLabeling& oldLabeling, uint64_t oldSize, uint64_t newSize,
                                                      uint64_t stateWithLabels, const std::set<std::string>& labelsInFormula);
    /**
     * Sums duplicate transitions in a vector of MatrixEntries into one MatrixEntry.
     *
     * @param entries
     * @return std::vector<storm::storage::MatrixEntry<uint64_t, RationalFunction>>
     */
    std::vector<storm::storage::MatrixEntry<uint64_t, RationalFunction>> joinDuplicateTransitions(
        std::vector<storm::storage::MatrixEntry<uint64_t, RationalFunction>> const& entries);

    std::shared_ptr<RawPolynomialCache> rawPolynomialCache;

    const std::shared_ptr<PolynomialCache> polynomialCache;
};

}  // namespace transformer
}  // namespace storm
