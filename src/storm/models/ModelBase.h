#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "storm/models/ModelType.h"
#include "storm/storage/dd/DdType.h"

namespace storm {
namespace models {

class ModelBase : public std::enable_shared_from_this<ModelBase> {
   public:
    /*!
     * Constructs a model of the given type.
     *
     * @param modelType The type of the model.
     */
    ModelBase(ModelType const& modelType) : modelType(modelType) {
        // Intentionally left empty.
    }

    /*
     * Make destructor virtual to allow deleting objects through pointer to base classe(s).
     */
    virtual ~ModelBase() {
        // Intentionally left empty.
    }

    /*!
     * Casts the model into the model type given by the template parameter.
     *
     * @return A shared pointer of the requested type that points to the model if the cast succeeded and a null
     * pointer otherwise.
     */
    template<typename ModelType>
    std::shared_ptr<ModelType> as() {
        return std::dynamic_pointer_cast<ModelType>(this->shared_from_this());
    }

    /*!
     * Casts the model into the model type given by the template parameter.
     *
     * @return A shared pointer of the requested type that points to the model if the cast succeeded and a null
     * pointer otherwise.
     */
    template<typename ModelType>
    std::shared_ptr<ModelType const> as() const {
        return std::dynamic_pointer_cast<ModelType const>(this->shared_from_this());
    }

    /*!
     *	@brief Return the actual type of the model.
     *
     *	Each model must implement this method.
     *
     *	@return	Type of the model.
     */
    virtual ModelType getType() const;

    /*!
     * Returns the number of states of the model.
     *
     * @return The number of states of the model.
     */
    virtual uint_fast64_t getNumberOfStates() const = 0;

    /*!
     * Returns the number of (non-zero) transitions of the model.
     *
     * @return The number of (non-zero) transitions of the model.
     */
    virtual uint_fast64_t getNumberOfTransitions() const = 0;

    /*!
     * Returns the number of choices ine the model.
     *
     * @return The number of choices in of the model.
     */
    virtual uint_fast64_t getNumberOfChoices() const = 0;

    /*!
     * Prints information about the model to the specified stream.
     *
     * @param out The stream the information is to be printed to.
     */
    virtual void printModelInformationToStream(std::ostream& out) const = 0;

    /*!
     * Checks whether the model is a sparse model.
     *
     * @return True iff the model is a sparse model.
     */
    virtual bool isSparseModel() const;

    /*!
     * Checks whether the model is a symbolic model.
     *
     * @return True iff the model is a symbolic model.
     */
    virtual bool isSymbolicModel() const;

    /*!
     * @return the type of Dd library used for this model (if any)
     */
    virtual std::optional<storm::dd::DdType> getDdType() const;

    /*!
     * Does it support uncertainty (e.g., via interval-valued entries).
     * Notice that while parametric Markov models may be seen as uncertain, within storm,these models are not called uncertain.
     *
     * @return
     */
    virtual bool supportsUncertainty() const;

    /*!
     * Checks whether the model is of the given type.
     *
     * @param modelType The model type to check for.
     * @return True iff the model is of the given type.
     */
    bool isOfType(storm::models::ModelType const& modelType) const;

    /*!
     * Returns true if the model is a nondeterministic model.
     *
     * @return True iff the model is a nondeterministic model.
     */
    bool isNondeterministicModel() const;

    /*!
     * Returns true if the model is a descrete-time model.
     */
    bool isDiscreteTimeModel() const;

    /*!
     * Checks whether the model supports parameters.
     *
     * @return True iff the model supports parameters.
     */
    virtual bool supportsParameters() const;

    /*!
     * Checks whether the model has parameters.
     *
     * @return True iff the model has parameters.
     */
    virtual bool hasParameters() const;

    /*!
     * Checks whether the model is exact.
     *
     * @return True iff the model is exact.
     */
    virtual bool isExact() const;

    /*
     * Checks whether the model is partially observable
     */
    virtual bool isPartiallyObservable() const;

    /*!
     * Converts the transition rewards of all reward models to state-based rewards. For deterministic models,
     * this reduces the rewards to state rewards only. For nondeterminstic models, the reward models will
     * contain state rewards and state-action rewards. Note that this transformation does not preserve all
     * properties, but it preserves expected rewards.
     */
    virtual void reduceToStateBasedRewards() = 0;

    /*!
     * Retrieves whether the model has a reward model with the given name.
     *
     * @return True iff the model has a reward model with the given name.
     */
    virtual bool hasRewardModel(std::string const& rewardModelName) const = 0;

    virtual bool hasUniqueRewardModel() const = 0;
    virtual std::string const& getUniqueRewardModelName() const = 0;

   private:
    // The type of the model.
    ModelType modelType;
};

}  // namespace models
}  // namespace storm
