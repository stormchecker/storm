#pragma once

#include <boost/variant.hpp>
#include <map>

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/prism/Program.h"

namespace storm {
namespace storage {

class SymbolicModelDescription {
   public:
    enum class ModelType { DTMC, CTMC, MDP, MA, POMDP, SMG };

    SymbolicModelDescription() = default;
    SymbolicModelDescription(storm::jani::Model const& model);
    SymbolicModelDescription(storm::prism::Program const& program);

    SymbolicModelDescription& operator=(storm::jani::Model const& model);
    SymbolicModelDescription& operator=(storm::prism::Program const& program);

    bool hasModel() const;
    bool isJaniModel() const;
    bool isPrismProgram() const;

    ModelType getModelType() const;
    storm::expressions::ExpressionManager& getManager() const;

    void setModel(storm::jani::Model const& model);
    void setModel(storm::prism::Program const& program);

    storm::jani::Model const& asJaniModel() const;
    storm::jani::Model& asJaniModel();
    storm::prism::Program const& asPrismProgram() const;
    storm::prism::Program& asPrismProgram();

    std::vector<std::string> getParameterNames() const;

    SymbolicModelDescription toJani(bool makeVariablesGlobal = true) const;

    /*!
     * Ensures that this model is a JANI model by, e.g., converting prism to jani.
     * If labels or reward models had to be converted during conversion, the renamings are applied to the given properties
     *
     * @return The jani model of this and either the new set of properties or an empty vector if no renamings were necessary
     *
     * @note The returned property vector might be empty in case no renaming is necessary.
     */
    std::pair<SymbolicModelDescription, std::vector<storm::jani::Property>> toJani(std::vector<storm::jani::Property> const& properties,
                                                                                   bool makeVariablesGlobal) const;

    SymbolicModelDescription preprocess(std::string const& constantDefinitionString = "") const;
    SymbolicModelDescription preprocess(std::map<storm::expressions::Variable, storm::expressions::Expression> const& constantDefinitions) const;

    std::map<storm::expressions::Variable, storm::expressions::Expression> parseConstantDefinitions(std::string const& constantDefinitionString) const;

    bool hasUndefinedConstants() const;
    std::vector<storm::expressions::Variable> getUndefinedConstants() const;

   private:
    boost::optional<boost::variant<storm::jani::Model, storm::prism::Program>> modelDescription;
};

std::ostream& operator<<(std::ostream& out, SymbolicModelDescription const& model);

std::ostream& operator<<(std::ostream& out, SymbolicModelDescription::ModelType const& type);

/*!
 * Parses a comma-separated string of constant definitions (e.g. "k=5,epsilon=0.01")
 * into a map from variables to their corresponding expressions.
 * @throws WrongFormatException if the string is malformed or references unknown constants.
 */
std::map<storm::expressions::Variable, storm::expressions::Expression> parseConstantDefinitionString(storm::expressions::ExpressionManager const& manager,
                                                                                                     std::string const& constantDefinitionString);
}  // namespace storage
}  // namespace storm
