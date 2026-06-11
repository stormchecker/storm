#include "storm/storage/umb/utility/ValuationDescriptionBuilder.h"

#include <bit>
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Variable.h"
#include "storm/storage/umb/model/Valuations.h"
#include "storm/utility/macros.h"

namespace storm::umb {

ValuationDescriptionBuilder::ValuationDescriptionBuilder(std::shared_ptr<storm::expressions::ExpressionManager const> const& expressionManager)
    : manager(expressionManager) {
    STORM_LOG_ASSERT(this->manager != nullptr, "Initilization with empty expression manager is not allowed.");
}

storm::expressions::ExpressionManager const& ValuationDescriptionBuilder::getManager() const {
    return *manager;
}

void ValuationDescriptionBuilder::addBooleanVariable(storm::expressions::Variable const& variable, bool optional) {
    STORM_LOG_ASSERT(*manager == variable.getManager(), "Variable " << variable.getName() << " has a different manager than previously specified.");
    descr.variables.emplace_back(storm::umb::ValuationClassDescription::Variable{.name{variable.getName()},
                                                                                 .isOptional{optional ? std::optional<bool>(true) : std::nullopt},
                                                                                 .type{storm::umb::Type::Bool, std::nullopt},
                                                                                 .lower{},
                                                                                 .upper{},
                                                                                 .offset{}});
}

void ValuationDescriptionBuilder::addIntegerVariable(storm::expressions::Variable const& variable, int64_t const lowerBound, int64_t const upperBound,
                                                     bool optional) {
    STORM_LOG_ASSERT(*manager == variable.getManager(), "Variable " << variable.getName() << " has a different manager than previously specified.");
    STORM_LOG_ASSERT(lowerBound <= upperBound, "Lower bound " << lowerBound << " must not be above upper bound" << upperBound << ".");
    // Cast to uint64_t *before* subtracting to avoid signed overflow UB.
    uint64_t const bitSize = storm::utility::bitsize(static_cast<uint64_t>(upperBound) - static_cast<uint64_t>(lowerBound));
    storm::umb::SizedType const t{.type{storm::umb::Type::Uint}, .size{std::max<uint64_t>(1, bitSize)}};
    descr.variables.emplace_back(storm::umb::ValuationClassDescription::Variable{.name{variable.getName()},
                                                                                 .isOptional{optional ? std::optional<bool>(true) : std::nullopt},
                                                                                 .type{t},
                                                                                 .lower{lowerBound},
                                                                                 .upper{upperBound},
                                                                                 .offset{lowerBound}});
}

void ValuationDescriptionBuilder::addIntegerVariable(storm::expressions::Variable const& variable, Integer const lowerBound, Integer const upperBound,
                                                     bool optional) {
    STORM_LOG_ASSERT(*manager == variable.getManager(), "Variable " << variable.getName() << " has a different manager than previously specified.");
    STORM_LOG_ASSERT(lowerBound <= upperBound, "Lower bound " << lowerBound << " must not be above upper bound" << upperBound << ".");
    if (lowerBound >= storm::utility::convertNumber<storm::RationalNumber>(std::numeric_limits<int64_t>::min()) &&
        upperBound <= storm::utility::convertNumber<storm::RationalNumber>(std::numeric_limits<int64_t>::max())) {
        // If the values fit into int64_t, we use that.
        addIntegerVariable(variable, storm::utility::convertNumber<int64_t>(lowerBound), storm::utility::convertNumber<int64_t>(upperBound));
    } else {
        uint64_t const bitSize = storm::utility::bitsize<Integer>(upperBound - lowerBound);
        storm::umb::SizedType const t{.type{lowerBound < 0 ? storm::umb::Type::Int : storm::umb::Type::Uint}, .size{std::max<uint64_t>(1, bitSize)}};
        descr.variables.emplace_back(storm::umb::ValuationClassDescription::Variable{
            .name{variable.getName()}, .isOptional{optional ? std::optional<bool>(true) : std::nullopt}, .type{t}, .lower{}, .upper{}, .offset{}});
    }
}

void ValuationDescriptionBuilder::addDoubleVariable(storm::expressions::Variable const& variable, bool optional) {
    STORM_LOG_ASSERT(*manager == variable.getManager(), "Variable " << variable.getName() << " has a different manager than previously specified.");
    descr.variables.emplace_back(storm::umb::ValuationClassDescription::Variable{.name{variable.getName()},
                                                                                 .isOptional{optional ? std::optional<bool>(true) : std::nullopt},
                                                                                 .type{storm::umb::Type::Double, std::nullopt},
                                                                                 .lower{},
                                                                                 .upper{},
                                                                                 .offset{}});
}

void ValuationDescriptionBuilder::addRationalVariable(storm::expressions::Variable const& variable, uint64_t bitSize, bool optional) {
    STORM_LOG_ASSERT(*manager == variable.getManager(), "Variable " << variable.getName() << " has a different manager than previously specified.");
    STORM_LOG_ASSERT(bitSize % 2 == 0, "Bit size for rational variables must be a multiple of 2.");
    storm::umb::SizedType const t{.type{storm::umb::Type::Rational}, .size{std::max<uint64_t>(2, bitSize)}};
    descr.variables.emplace_back(storm::umb::ValuationClassDescription::Variable{
        .name{variable.getName()}, .isOptional{optional ? std::optional<bool>(true) : std::nullopt}, .type{t}, .lower{}, .upper{}, .offset{}});
}

void ValuationDescriptionBuilder::addStringVariable(storm::expressions::Variable const& variable, bool optional) {
    STORM_LOG_ASSERT(*manager == variable.getManager(), "Variable " << variable.getName() << " has a different manager than previously specified.");
    descr.variables.emplace_back(storm::umb::ValuationClassDescription::Variable{.name{variable.getName()},
                                                                                 .isOptional{optional ? std::optional<bool>(true) : std::nullopt},
                                                                                 .type{storm::umb::Type::String, std::nullopt},
                                                                                 .lower{},
                                                                                 .upper{},
                                                                                 .offset{}});
}

void ValuationDescriptionBuilder::addVariable(storm::umb::ValuationClassDescription::Variable const& variable) {
    STORM_LOG_ASSERT(manager->hasVariable(variable.name), "Variable " << variable.name << " is not declared in the expression manager.");
    descr.variables.push_back(variable);
}
void ValuationDescriptionBuilder::addVariables(storm::umb::ValuationClassDescription const& description, bool addPadding) {
    for (auto const& varVariant : description.variables) {
        if (std::holds_alternative<storm::umb::ValuationClassDescription::Variable>(varVariant)) {
            addVariable(std::get<storm::umb::ValuationClassDescription::Variable>(varVariant));
        } else if (addPadding && std::holds_alternative<storm::umb::ValuationClassDescription::Padding>(varVariant)) {
            descr.variables.push_back(varVariant);
        }
    }
}

void ValuationDescriptionBuilder::finalize() {
    if (uint64_t const padding = descr.sizeInBits() % 8; padding > 0) {
        descr.variables.emplace_back(storm::umb::ValuationClassDescription::Padding(8 - padding));
    }
}

ValuationClassDescription ValuationDescriptionBuilder::buildClassDescription() {
    STORM_LOG_ASSERT(!descr.variables.empty(), "At least one variable must be added to the valuation description.");
    finalize();
    return descr;
}

}  // namespace storm::umb
