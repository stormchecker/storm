#include "PreservationInformation.h"

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/Formulas.h"
#include "storm/models/symbolic/Model.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {
namespace bisimulation {

template<storm::dd::DdType DdType, typename ValueType>
PreservationInformation<DdType, ValueType>::PreservationInformation(storm::models::symbolic::Model<DdType, ValueType> const& model)
    : PreservationInformation(model, model.getLabels()) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
PreservationInformation<DdType, ValueType>::PreservationInformation(storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                    std::vector<std::string> const& labels) {
    for (auto const& label : labels) {
        this->addLabel(label);
        this->addExpression(model.getExpression(label));
    }
}

template<storm::dd::DdType DdType, typename ValueType>
PreservationInformation<DdType, ValueType>::PreservationInformation(storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                    std::vector<storm::expressions::Expression> const& expressions) {
    for (auto const& e : expressions) {
        this->addExpression(e);
    }
}

template<storm::dd::DdType DdType, typename ValueType>
PreservationInformation<DdType, ValueType>::PreservationInformation(storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                    std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) {
    if (formulas.empty()) {
        // Default to respect all labels if no formulas are given.
        for (auto const& label : model.getLabels()) {
            this->addLabel(label);
            this->addExpression(model.getExpression(label));
        }
        for (auto const& rewardModel : model.getRewardModels()) {
            this->addRewardModel(rewardModel.first);
        }
    } else {
        for (auto const& formula : formulas) {
            for (auto const& expressionFormula : formula->getAtomicExpressionFormulas()) {
                this->addExpression(expressionFormula->getExpression());
            }
            for (auto const& labelFormula : formula->getAtomicLabelFormulas()) {
                this->addLabel(labelFormula->getLabel());
                std::string const& label = labelFormula->getLabel();
                STORM_LOG_THROW(model.hasLabel(label), storm::exceptions::InvalidPropertyException, "Property refers to illegal label '" << label << "'.");
                this->addExpression(model.getExpression(label));
            }
            for (auto const& rewardModel : formula->getReferencedRewardModels()) {
                if (rewardModel == "") {
                    if (model.hasRewardModel("")) {
                        this->addRewardModel(rewardModel);
                    } else {
                        STORM_LOG_THROW(model.hasUniqueRewardModel(), storm::exceptions::InvalidPropertyException,
                                        "Property refers to the default reward model, but it does not exist or is not unique.");
                        this->addRewardModel(model.getUniqueRewardModelName());
                    }
                } else {
                    this->addRewardModel(rewardModel);
                }
            }
        }
    }
}

template<storm::dd::DdType DdType, typename ValueType>
void PreservationInformation<DdType, ValueType>::addLabel(std::string const& label) {
    labels.insert(label);
}

template<storm::dd::DdType DdType, typename ValueType>
void PreservationInformation<DdType, ValueType>::addExpression(storm::expressions::Expression const& expression) {
    expressions.insert(expression);
}

template<storm::dd::DdType DdType, typename ValueType>
void PreservationInformation<DdType, ValueType>::addRewardModel(std::string const& name) {
    rewardModelNames.insert(name);
}

template<storm::dd::DdType DdType, typename ValueType>
std::set<std::string> const& PreservationInformation<DdType, ValueType>::getLabels() const {
    return labels;
}

template<storm::dd::DdType DdType, typename ValueType>
std::set<storm::expressions::Expression> const& PreservationInformation<DdType, ValueType>::getExpressions() const {
    return expressions;
}

template<storm::dd::DdType DdType, typename ValueType>
std::set<std::string> const& PreservationInformation<DdType, ValueType>::getRewardModelNames() const {
    return rewardModelNames;
}

template class PreservationInformation<storm::dd::DdType::CUDD, double>;

template class PreservationInformation<storm::dd::DdType::Sylvan, double>;
template class PreservationInformation<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class PreservationInformation<storm::dd::DdType::Sylvan, storm::RationalFunction>;
}  // namespace bisimulation
}  // namespace dd
}  // namespace storm
