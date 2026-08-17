#pragma once

#include <memory>
#include <type_traits>
#include <unordered_map>

#include "storm-pars/transformer/BigStep.h"
#include "storm-pars/utility/parametric.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StochasticTwoPlayerGame.h"
#include "storm/utility/constants.h"

namespace storm {
namespace utility {

/*!
 * This class allows efficient instantiation of the given parametric model.
 * The key to efficiency is to evaluate every distinct transition- (or reward-) function only once
 * instead of evaluating the same function for each occurrence in the model.
 */
template<typename ParametricSparseModelType, typename ConstantSparseModelType>
class ModelInstantiator {
   public:
    typedef typename ParametricSparseModelType::ValueType ParametricType;
    typedef typename storm::utility::parametric::VariableType<ParametricType>::type VariableType;
    typedef typename storm::utility::parametric::CoefficientType<ParametricType>::type CoefficientType;
    typedef typename ConstantSparseModelType::ValueType ConstantType;

    /*!
     * Constructs a ModelInstantiator
     * @param parametricModel The model that is to be instantiated
     */
    ModelInstantiator(ParametricSparseModelType const& parametricModel);

    /*!
     * Destructs the ModelInstantiator
     */
    virtual ~ModelInstantiator();

    /*!
     * Evaluates the occurring parametric functions and retrieves the instantiated model
     * @param valuation Maps each occurring variables to the value with which it should be substituted
     * @return The instantiated model
     */
    ConstantSparseModelType const& instantiate(storm::utility::parametric::Valuation<ParametricType> const& valuation);

    /*!
     *  Check validity
     */
    void checkValid() const;

   private:
    /*!
     * Initializes the instantiatedModel with dummy data by considering the model-specific ingredients.
     * Also initializes other model-specific data, e.g., the exitRate vector of a markov automaton
     */
    template<typename PMT = ParametricSparseModelType>
    typename std::enable_if<std::is_same<PMT, storm::models::sparse::Dtmc<typename ParametricSparseModelType::ValueType>>::value ||
                            std::is_same<PMT, storm::models::sparse::Mdp<typename ParametricSparseModelType::ValueType>>::value>::type
    initializeModelSpecificData(PMT const& parametricModel) {
        storm::storage::sparse::ModelComponents<ConstantType, typename ConstantSparseModelType::RewardModelType> components(
            buildDummyMatrix(parametricModel.getTransitionMatrix()));
        components.stateLabeling = parametricModel.getStateLabeling();
        components.rewardModels = buildDummyRewardModels(parametricModel.getRewardModels());
        components.choiceLabeling = parametricModel.getOptionalChoiceLabeling();

        this->instantiatedModel = std::make_shared<ConstantSparseModelType>(std::move(components));
    }

    template<typename PMT = ParametricSparseModelType>
    typename std::enable_if<std::is_same<PMT, storm::models::sparse::Ctmc<typename ParametricSparseModelType::ValueType>>::value>::type
    initializeModelSpecificData(PMT const& parametricModel) {
        storm::storage::sparse::ModelComponents<ConstantType, typename ConstantSparseModelType::RewardModelType> components(
            buildDummyMatrix(parametricModel.getTransitionMatrix()));
        components.stateLabeling = parametricModel.getStateLabeling();
        components.rewardModels = buildDummyRewardModels(parametricModel.getRewardModels());
        components.exitRates = std::vector<ConstantType>(parametricModel.getExitRateVector().size(), storm::utility::one<ConstantType>());
        components.rateTransitions = true;
        components.choiceLabeling = parametricModel.getOptionalChoiceLabeling();
        this->instantiatedModel = std::make_shared<ConstantSparseModelType>(std::move(components));

        initializeVectorMapping(this->instantiatedModel->getExitRateVector(), this->functions, this->vectorMapping, parametricModel.getExitRateVector());
    }

    template<typename PMT = ParametricSparseModelType>
    typename std::enable_if<std::is_same<PMT, storm::models::sparse::MarkovAutomaton<typename ParametricSparseModelType::ValueType>>::value>::type
    initializeModelSpecificData(PMT const& parametricModel) {
        storm::storage::sparse::ModelComponents<ConstantType, typename ConstantSparseModelType::RewardModelType> components(
            buildDummyMatrix(parametricModel.getTransitionMatrix()));
        components.stateLabeling = parametricModel.getStateLabeling();
        components.rewardModels = buildDummyRewardModels(parametricModel.getRewardModels());
        components.exitRates = std::vector<ConstantType>(parametricModel.getExitRates().size(), storm::utility::one<ConstantType>());
        components.markovianStates = parametricModel.getMarkovianStates();
        components.choiceLabeling = parametricModel.getOptionalChoiceLabeling();
        this->instantiatedModel = std::make_shared<ConstantSparseModelType>(std::move(components));

        initializeVectorMapping(this->instantiatedModel->getExitRates(), this->functions, this->vectorMapping, parametricModel.getExitRates());
    }

    template<typename PMT = ParametricSparseModelType>
    typename std::enable_if<std::is_same<PMT, storm::models::sparse::StochasticTwoPlayerGame<typename ParametricSparseModelType::ValueType>>::value>::type
    initializeModelSpecificData(PMT const& parametricModel) {
        storm::storage::sparse::ModelComponents<ConstantType, typename ConstantSparseModelType::RewardModelType> components(
            buildDummyMatrix(parametricModel.getTransitionMatrix()));
        components.stateLabeling = parametricModel.getStateLabeling();
        components.rewardModels = buildDummyRewardModels(parametricModel.getRewardModels());
        components.player1Matrix = parametricModel.getPlayer1Matrix();
        components.choiceLabeling = parametricModel.getOptionalChoiceLabeling();

        this->instantiatedModel = std::make_shared<ConstantSparseModelType>(std::move(components));
    }

    template<typename PMT = ParametricSparseModelType>
    typename std::enable_if<std::is_same<PMT, ConstantSparseModelType>::value>::type instantiate_helper(
        storm::utility::parametric::Valuation<ParametricType> const& valuation) {
        for (auto& functionResult : this->functions) {
            functionResult.second = storm::utility::parametric::substitute(functionResult.first, valuation);
        }
    }

    template<typename PMT = ParametricSparseModelType>
    typename std::enable_if<!std::is_same<PMT, ConstantSparseModelType>::value>::type instantiate_helper(
        storm::utility::parametric::Valuation<ParametricType> const& valuation) {
        for (auto& functionResult : this->functions) {
            if (!transformer::BigStep::lastSavedAnnotations.empty() && functionResult.first.gatherVariables().size() == 1 &&
                transformer::BigStep::lastSavedAnnotations.count(functionResult.first)) {
                auto const& annotation = transformer::BigStep::lastSavedAnnotations.at(functionResult.first);
                functionResult.second = annotation.evaluate(storm::utility::convertNumber<ConstantType>(valuation.at(annotation.getParameter())));
            } else {
                functionResult.second = storm::utility::parametric::evaluate<ConstantType>(functionResult.first, valuation);
            }
        }
    }

    /*!
     * Creates a matrix that has entries at the same position as the given matrix.
     * The returned matrix is a stochastic matrix, i.e., the rows sum up to one.
     */
    storm::storage::SparseMatrix<ConstantType> buildDummyMatrix(storm::storage::SparseMatrix<ParametricType> const& parametricMatrix) const;

    /*!
     * Creates a copy of the given reward models with the same names and with state(action)rewards / transitionrewards having the same entry-count and
     * entry-positions.
     */
    std::unordered_map<std::string, typename ConstantSparseModelType::RewardModelType> buildDummyRewardModels(
        std::unordered_map<std::string, typename ParametricSparseModelType::RewardModelType> const& parametricRewardModel) const;

    /*!
     * Connects the occurring functions with the corresponding matrix entries
     *
     * @note constantMatrix and parametricMatrix should have entries at the same positions
     *
     * @param constantMatrix The matrix to which the evaluation results are written
     * @param functions Occurring functions are inserted in this map
     * @param mapping The connections of functions to matrix entries are push_backed  into this
     * @param parametricMatrix the source matrix with the functions to consider.
     */
    void initializeMatrixMapping(storm::storage::SparseMatrix<ConstantType>& constantMatrix, std::unordered_map<ParametricType, ConstantType>& functions,
                                 std::vector<std::pair<typename storm::storage::SparseMatrix<ConstantType>::iterator, ConstantType*>>& mapping,
                                 storm::storage::SparseMatrix<ParametricType> const& parametricMatrix) const;

    /*!
     * Connects the occurring functions with the corresponding vector entries
     *
     * @note constantVector and parametricVector should have the same size
     *
     * @param constantVector The vector to which the evaluation results are written
     * @param functions Occurring functions with their placeholders are inserted in this map
     * @param mapping The connections of functions to vector entries are push_backed  into this
     * @param parametricVector the source vector with the functions to consider.
     */
    void initializeVectorMapping(std::vector<ConstantType>& constantVector, std::unordered_map<ParametricType, ConstantType>& functions,
                                 std::vector<std::pair<typename std::vector<ConstantType>::iterator, ConstantType*>>& mapping,
                                 std::vector<ParametricType> const& parametricVector) const;

    /// The resulting model
    std::shared_ptr<ConstantSparseModelType> instantiatedModel;
    /// the occurring functions together with the corresponding placeholders for their evaluated result
    std::unordered_map<ParametricType, ConstantType> functions;
    /// Connection of matrix entries with placeholders
    std::vector<std::pair<typename storm::storage::SparseMatrix<ConstantType>::iterator, ConstantType*>> matrixMapping;
    /// Connection of Vector entries with placeholders
    std::vector<std::pair<typename std::vector<ConstantType>::iterator, ConstantType*>> vectorMapping;
};
}  // Namespace utility
}  // namespace storm
