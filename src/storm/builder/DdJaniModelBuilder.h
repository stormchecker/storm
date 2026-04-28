#pragma once

#include <boost/optional.hpp>

#include "storm/storage/dd/DdType.h"
#include "storm/storage/expressions/Variable.h"
#include "storm/storage/jani/Property.h"
#include "storm/utility/OptionalRef.h"

#include "storm/builder/TerminalStatesGetter.h"
#include "storm/logic/Formula.h"

namespace storm {
namespace models {
namespace symbolic {
template<storm::dd::DdType Type, typename ValueType>
class Model;
}
}  // namespace models
namespace jani {
class Model;
class ModelFeatures;
}  // namespace jani

namespace builder {

template<storm::dd::DdType Type, typename ValueType = double>
class DdJaniModelBuilder {
   public:
    /*!
     * Returns the jani features with which this builder can deal natively.
     */
    static storm::jani::ModelFeatures getSupportedJaniFeatures();

    /*!
     * A quick check to detect whether the given model is not supported.
     * This method only over-approximates the set of models that can be handled, i.e., if this
     * returns true, the model might still be unsupported.
     */
    static bool canHandle(storm::jani::Model const& model, storm::OptionalRef<std::vector<storm::jani::Property> const> properties = storm::NullRef);

    struct Options {
        /*!
         * Creates an object representing the default building options.
         */
        Options(bool buildAllLabels = false, bool buildAllRewardModels = false, bool applyMaximumProgressAssumption = true);

        /*! Creates an object representing the suggested building options assuming that the given formula is the
         * only one to check. Additional formulas may be preserved by calling <code>preserveFormula</code>.
         *
         * @param formula The formula based on which to choose the building options.
         */
        Options(storm::logic::Formula const& formula);

        /*! Creates an object representing the suggested building options assuming that the given formulas are
         * the only ones to check. Additional formulas may be preserved by calling <code>preserveFormula</code>.
         *
         * @param formula Thes formula based on which to choose the building options.
         */
        Options(std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas);

        /*!
         * Changes the options in a way that ensures that the given formula can be checked on the model once it
         * has been built.
         *
         * @param formula The formula that is to be ''preserved''.
         */
        void preserveFormula(storm::logic::Formula const& formula);

        /*!
         * Analyzes the given formula and sets an expression for the states states of the model that can be
         * treated as terminal states. Note that this may interfere with checking properties different than the
         * one provided.
         *
         * @param formula The formula used to (possibly) derive an expression for the terminal states of the
         * model.
         */
        void setTerminalStatesFromFormula(storm::logic::Formula const& formula);

        /*!
         * Retrieves the names of the reward models to build.
         */
        std::set<std::string> const& getRewardModelNames() const;

        /*!
         * Adds the given label to the ones that are supposed to be built.
         */
        void addLabel(std::string const& labelName);

        /*!
         * Retrieves whether the flag to build all labels is set.
         */
        bool isBuildAllLabelsSet() const;

        /// A flag that indicates whether all labels are to be built. In this case, the label names are to be ignored.
        bool buildAllLabels;

        /*!
         * Retrieves whether the flag to build all reward models is set.
         */
        bool isBuildAllRewardModelsSet() const;

        // A flag that indicates whether or not all reward models are to be build.
        bool buildAllRewardModels;

        /// A flag that indicates whether the maximum progress assumption should be applied.
        bool applyMaximumProgressAssumption;

        /// A set of labels to build.
        std::set<std::string> labelNames;

        // A list of reward models to be build in case not all reward models are to be build.
        std::set<std::string> rewardModelsToBuild;

        // An optional mapping that, if given, contains defining expressions for undefined constants.
        boost::optional<std::map<storm::expressions::Variable, storm::expressions::Expression>> constantDefinitions;

        // An optional set of expression or labels that characterizes (a subset of) the terminal states of the model.
        // If this is set, the outgoing transitions of these states are replaced with a self-loop.
        storm::builder::TerminalStates terminalStates;
    };

    /*!
     * Translates the given program into a symbolic model (i.e. one that stores the transition relation as a
     * decision diagram).
     *
     * @param model The model to translate.
     * @return A pointer to the resulting model.
     */
    std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> build(storm::jani::Model const& model, Options const& options = Options());
};

}  // namespace builder
}  // namespace storm
