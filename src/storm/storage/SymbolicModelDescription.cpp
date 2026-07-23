#include "storm/storage/SymbolicModelDescription.h"

#include "storm/storage/jani/Automaton.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/Property.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/utility/cli.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {

SymbolicModelDescription::SymbolicModelDescription(storm::jani::Model const& model) : modelDescription(model) {
    // Intentionally left empty.
}

SymbolicModelDescription::SymbolicModelDescription(storm::prism::Program const& program) : modelDescription(program) {
    // Intentionally left empty.
}

SymbolicModelDescription& SymbolicModelDescription::operator=(storm::jani::Model const& model) {
    this->modelDescription = model;
    return *this;
}

SymbolicModelDescription& SymbolicModelDescription::operator=(storm::prism::Program const& program) {
    this->modelDescription = program;
    return *this;
}

bool SymbolicModelDescription::hasModel() const {
    return static_cast<bool>(modelDescription);
}

bool SymbolicModelDescription::isJaniModel() const {
    return modelDescription.get().which() == 0;
}

bool SymbolicModelDescription::isPrismProgram() const {
    return modelDescription.get().which() == 1;
}

SymbolicModelDescription::ModelType SymbolicModelDescription::getModelType() const {
    if (this->isJaniModel()) {
        storm::jani::Model const& janiModel = this->asJaniModel();
        switch (janiModel.getModelType()) {
            case storm::jani::ModelType::DTMC:
                return SymbolicModelDescription::ModelType::DTMC;
            case storm::jani::ModelType::CTMC:
                return SymbolicModelDescription::ModelType::CTMC;
            case storm::jani::ModelType::MDP:
                return SymbolicModelDescription::ModelType::MDP;
            case storm::jani::ModelType::MA:
                return SymbolicModelDescription::ModelType::MA;
            default:
                STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException, "Expected other JANI model type.");
        }
    } else {
        storm::prism::Program const& prismProgram = this->asPrismProgram();
        switch (prismProgram.getModelType()) {
            case storm::prism::Program::ModelType::DTMC:
                return SymbolicModelDescription::ModelType::DTMC;
            case storm::prism::Program::ModelType::CTMC:
                return SymbolicModelDescription::ModelType::CTMC;
            case storm::prism::Program::ModelType::MDP:
                return SymbolicModelDescription::ModelType::MDP;
            case storm::prism::Program::ModelType::POMDP:
                return SymbolicModelDescription::ModelType::POMDP;
            case storm::prism::Program::ModelType::MA:
                return SymbolicModelDescription::ModelType::MA;
            case storm::prism::Program::ModelType::SMG:
                return SymbolicModelDescription::ModelType::SMG;
            default:
                STORM_LOG_THROW(false, storm::exceptions::InvalidTypeException, "Expected other PRISM model type.");
        }
    }
}

storm::expressions::ExpressionManager& SymbolicModelDescription::getManager() const {
    if (this->isPrismProgram()) {
        return this->asPrismProgram().getManager();
    } else {
        return this->asJaniModel().getManager();
    }
}

void SymbolicModelDescription::setModel(storm::jani::Model const& model) {
    modelDescription = model;
}

void SymbolicModelDescription::setModel(storm::prism::Program const& program) {
    modelDescription = program;
}

storm::jani::Model const& SymbolicModelDescription::asJaniModel() const {
    STORM_LOG_THROW(isJaniModel(), storm::exceptions::InvalidOperationException,
                    "Cannot retrieve JANI model, because the symbolic description has a different type.");
    return boost::get<storm::jani::Model>(modelDescription.get());
}

storm::jani::Model& SymbolicModelDescription::asJaniModel() {
    STORM_LOG_THROW(isJaniModel(), storm::exceptions::InvalidOperationException,
                    "Cannot retrieve JANI model, because the symbolic description has a different type.");
    return boost::get<storm::jani::Model>(modelDescription.get());
}

storm::prism::Program const& SymbolicModelDescription::asPrismProgram() const {
    STORM_LOG_THROW(isPrismProgram(), storm::exceptions::InvalidOperationException,
                    "Cannot retrieve JANI model, because the symbolic description has a different type.");
    return boost::get<storm::prism::Program>(modelDescription.get());
}

storm::prism::Program& SymbolicModelDescription::asPrismProgram() {
    STORM_LOG_THROW(isPrismProgram(), storm::exceptions::InvalidOperationException,
                    "Cannot retrieve JANI model, because the symbolic description has a different type.");
    return boost::get<storm::prism::Program>(modelDescription.get());
}

std::vector<std::string> SymbolicModelDescription::getParameterNames() const {
    std::vector<std::string> result;
    if (isJaniModel()) {
        for (auto const& c : asJaniModel().getUndefinedConstants()) {
            result.push_back(c.get().getName());
        }
    } else {
        for (auto const& c : asPrismProgram().getUndefinedConstants()) {
            result.push_back(c.get().getName());
        }
    }
    return result;
}

SymbolicModelDescription SymbolicModelDescription::toJani(bool makeVariablesGlobal) const {
    if (this->isJaniModel()) {
        return *this;
    }
    if (this->isPrismProgram()) {
        return SymbolicModelDescription(this->asPrismProgram().toJani(makeVariablesGlobal, ""));
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot transform model description to the JANI format.");
    }
}

std::pair<SymbolicModelDescription, std::vector<storm::jani::Property>> SymbolicModelDescription::toJani(std::vector<storm::jani::Property> const& properties,
                                                                                                         bool makeVariablesGlobal) const {
    if (this->isJaniModel()) {
        return std::make_pair(*this, std::vector<storm::jani::Property>());
    }
    if (this->isPrismProgram()) {
        auto modelProperties = this->asPrismProgram().toJani(properties, makeVariablesGlobal, "");
        return std::make_pair(SymbolicModelDescription(modelProperties.first), modelProperties.second);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot transform model description to the JANI format.");
    }
}

SymbolicModelDescription SymbolicModelDescription::preprocess(std::string const& constantDefinitionString) const {
    return this->preprocess(this->parseConstantDefinitions(constantDefinitionString));
}

SymbolicModelDescription SymbolicModelDescription::preprocess(
    std::map<storm::expressions::Variable, storm::expressions::Expression> const& constantDefinitions) const {
    if (this->isJaniModel()) {
        return SymbolicModelDescription(this->asJaniModel().preprocess(constantDefinitions));
    } else if (this->isPrismProgram()) {
        return SymbolicModelDescription(this->asPrismProgram().preprocess(constantDefinitions));
    }
    return *this;
}

std::map<storm::expressions::Variable, storm::expressions::Expression> SymbolicModelDescription::parseConstantDefinitions(
    std::string const& constantDefinitionString) const {
    if (this->isJaniModel()) {
        return storm::utility::cli::parseConstantDefinitionString(this->asJaniModel().getManager(), constantDefinitionString);
    } else {
        return storm::utility::cli::parseConstantDefinitionString(this->asPrismProgram().getManager(), constantDefinitionString);
    }
}

bool SymbolicModelDescription::hasUndefinedConstants() const {
    if (this->isPrismProgram()) {
        return this->asPrismProgram().hasUndefinedConstants();
    } else {
        return this->asJaniModel().hasUndefinedConstants();
    }
}

std::vector<storm::expressions::Variable> SymbolicModelDescription::getUndefinedConstants() const {
    std::vector<storm::expressions::Variable> result;
    if (this->isPrismProgram()) {
        std::vector<std::reference_wrapper<storm::prism::Constant const>> constants = this->asPrismProgram().getUndefinedConstants();
        for (auto const& constant : constants) {
            result.emplace_back(constant.get().getExpressionVariable());
        }
    } else {
        std::vector<std::reference_wrapper<storm::jani::Constant const>> constants = this->asJaniModel().getUndefinedConstants();
        for (auto const& constant : constants) {
            result.emplace_back(constant.get().getExpressionVariable());
        }
    }
    return result;
}

std::ostream& operator<<(std::ostream& out, SymbolicModelDescription const& model) {
    if (model.isPrismProgram()) {
        out << model.asPrismProgram();
    } else if (model.isJaniModel()) {
        out << model.asJaniModel();
    } else {
        out << "unkown symbolic model description";
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, SymbolicModelDescription::ModelType const& type) {
    switch (type) {
        case SymbolicModelDescription::ModelType::DTMC:
            out << "dtmc";
            break;
        case SymbolicModelDescription::ModelType::CTMC:
            out << "ctmc";
            break;
        case SymbolicModelDescription::ModelType::MDP:
            out << "mdp";
            break;
        case SymbolicModelDescription::ModelType::MA:
            out << "ma";
            break;
        case SymbolicModelDescription::ModelType::POMDP:
            out << "pomdp";
            break;
        case SymbolicModelDescription::ModelType::SMG:
            out << "smg";
            break;
    }
    return out;
}
}  // namespace storage
}  // namespace storm
