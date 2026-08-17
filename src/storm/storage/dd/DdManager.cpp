#include "storm/storage/dd/DdManager.h"

#include <cmath>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/environment/Environment.h"
#include "storm/environment/dd/DdEnvironment.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {
template<DdType LibraryType>
DdManager<LibraryType>::DdManager(storm::Environment const& env)
    : internalDdManager(env.dd().get<LibraryType>()), metaVariableMap(), manager(new storm::expressions::ExpressionManager()) {
    // Intentionally left empty.
}

template<DdType LibraryType>
std::shared_ptr<DdManager<LibraryType>> DdManager<LibraryType>::createWithDefaultEnvironment() {
    return std::make_shared<DdManager<LibraryType>>(storm::Environment());
}

template<DdType LibraryType>
std::shared_ptr<DdManager<LibraryType>> DdManager<LibraryType>::asSharedPointer() {
    return this->shared_from_this();
}

template<DdType LibraryType>
std::shared_ptr<DdManager<LibraryType> const> DdManager<LibraryType>::asSharedPointer() const {
    return this->shared_from_this();
}

template<DdType LibraryType>
Bdd<LibraryType> DdManager<LibraryType>::getBddOne() const {
    return Bdd<LibraryType>(*this, internalDdManager.getBddOne());
}

template<DdType LibraryType>
template<typename ValueType>
Add<LibraryType, ValueType> DdManager<LibraryType>::getAddOne() const {
    return Add<LibraryType, ValueType>(*this, internalDdManager.template getAddOne<ValueType>());
}

template<DdType LibraryType>
Bdd<LibraryType> DdManager<LibraryType>::getBddZero() const {
    return Bdd<LibraryType>(*this, internalDdManager.getBddZero());
}

template<DdType LibraryType>
template<typename ValueType>
Add<LibraryType, ValueType> DdManager<LibraryType>::getAddZero() const {
    return Add<LibraryType, ValueType>(*this, internalDdManager.template getAddZero<ValueType>());
}

template<DdType LibraryType>
template<typename ValueType>
Add<LibraryType, ValueType> DdManager<LibraryType>::getAddUndefined() const {
    return Add<LibraryType, ValueType>(*this, internalDdManager.template getAddUndefined<ValueType>());
}

template<DdType LibraryType>
template<typename ValueType>
Add<LibraryType, ValueType> DdManager<LibraryType>::getInfinity() const {
    return getConstant(storm::utility::infinity<ValueType>());
}

template<DdType LibraryType>
template<typename ValueType>
Add<LibraryType, ValueType> DdManager<LibraryType>::getConstant(ValueType const& value) const {
    return Add<LibraryType, ValueType>(*this, internalDdManager.getConstant(value));
}

template<DdType LibraryType>
Bdd<LibraryType> DdManager<LibraryType>::getEncoding(storm::expressions::Variable const& variable, int_fast64_t value, bool mostSignificantBitAtTop) const {
    DdMetaVariable<LibraryType> const& metaVariable = this->getMetaVariable(variable);

    STORM_LOG_THROW(metaVariable.canRepresent(value), storm::exceptions::InvalidArgumentException,
                    "Illegal value " << value << " for meta variable '" << variable.getName() << "'.");

    // Now compute the encoding relative to the low value of the meta variable.
    value -= metaVariable.getLow();

    std::vector<Bdd<LibraryType>> const& ddVariables = metaVariable.getDdVariables();

    Bdd<LibraryType> result;
    if (mostSignificantBitAtTop) {
        if (value & (1ull << (ddVariables.size() - 1))) {
            result = ddVariables[0];
        } else {
            result = !ddVariables[0];
        }

        for (std::size_t i = 1; i < ddVariables.size(); ++i) {
            if (value & (1ull << (ddVariables.size() - i - 1))) {
                result &= ddVariables[i];
            } else {
                result &= !ddVariables[i];
            }
        }
    } else {
        if (value & 1ull) {
            result = ddVariables[0];
        } else {
            result = !ddVariables[0];
        }
        value >>= 1;

        for (std::size_t i = 1; i < ddVariables.size(); ++i) {
            if (value & 1ull) {
                result &= ddVariables[i];
            } else {
                result &= !ddVariables[i];
            }
            value >>= 1;
        }
    }

    return result;
}

template<DdType LibraryType>
Bdd<LibraryType> DdManager<LibraryType>::getRange(storm::expressions::Variable const& variable) const {
    storm::dd::DdMetaVariable<LibraryType> const& metaVariable = this->getMetaVariable(variable);

    if (metaVariable.hasHigh()) {
        return Bdd<LibraryType>(*this,
                                internalDdManager.getBddEncodingLessOrEqualThan(static_cast<uint64_t>(metaVariable.getHigh() - metaVariable.getLow()),
                                                                                metaVariable.getCube().getInternalBdd(), metaVariable.getNumberOfDdVariables()),
                                {variable});
    } else {
        // If there is no upper bound on this variable, the whole range is valid.
        Bdd<LibraryType> result = this->getBddOne();
        result.addMetaVariable(variable);
        return result;
    }
}

template<DdType LibraryType>
template<typename ValueType>
Add<LibraryType, ValueType> DdManager<LibraryType>::getIdentity(storm::expressions::Variable const& variable) const {
    storm::dd::DdMetaVariable<LibraryType> const& metaVariable = this->getMetaVariable(variable);
    STORM_LOG_THROW(metaVariable.hasHigh(), storm::exceptions::InvalidOperationException, "Cannot create identity for meta variable.");

    Add<LibraryType, ValueType> result = this->getAddZero<ValueType>();
    for (int_fast64_t value = metaVariable.getLow(); value <= metaVariable.getHigh(); ++value) {
        result += this->getEncoding(variable, value).template toAdd<ValueType>() * this->getConstant(storm::utility::convertNumber<ValueType>(value));
    }
    return result;
}

template<DdType LibraryType>
Bdd<LibraryType> DdManager<LibraryType>::getIdentity(std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& variablePairs,
                                                     bool restrictToFirstRange) const {
    auto result = this->getBddOne();
    for (auto const& pair : variablePairs) {
        result &= this->getIdentity(pair.first, pair.second, restrictToFirstRange);
    }
    return result;
}

template<DdType LibraryType>
Bdd<LibraryType> DdManager<LibraryType>::getIdentity(storm::expressions::Variable const& first, storm::expressions::Variable const& second,
                                                     bool restrictToFirstRange) const {
    auto const& firstMetaVariable = this->getMetaVariable(first);
    auto const& secondMetaVariable = this->getMetaVariable(second);

    STORM_LOG_THROW(firstMetaVariable.getNumberOfDdVariables() == secondMetaVariable.getNumberOfDdVariables(), storm::exceptions::InvalidOperationException,
                    "Mismatching sizes of meta variables.");

    auto const& firstDdVariables = firstMetaVariable.getDdVariables();
    auto const& secondDdVariables = secondMetaVariable.getDdVariables();

    auto result = restrictToFirstRange ? this->getRange(first) : this->getBddOne();
    for (auto it1 = firstDdVariables.begin(), it2 = secondDdVariables.begin(), ite1 = firstDdVariables.end(); it1 != ite1; ++it1, ++it2) {
        result &= it1->iff(*it2);
    }

    return result;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winfinite-recursion"
template<DdType LibraryType>
Bdd<LibraryType> DdManager<LibraryType>::getCube(storm::expressions::Variable const& variable) const {
    return getCube({variable});
}
#pragma GCC diagnostic pop

template<DdType LibraryType>
Bdd<LibraryType> DdManager<LibraryType>::getCube(std::set<storm::expressions::Variable> const& variables) const {
    Bdd<LibraryType> result = this->getBddOne();
    for (auto const& variable : variables) {
        storm::dd::DdMetaVariable<LibraryType> const& metaVariable = this->getMetaVariable(variable);
        result &= metaVariable.getCube();
    }
    return result;
}

template<DdType LibraryType>
std::vector<storm::expressions::Variable> DdManager<LibraryType>::cloneVariable(storm::expressions::Variable const& variable,
                                                                                std::string const& newMetaVariableName,
                                                                                boost::optional<uint64_t> const& numberOfLayers) {
    std::vector<storm::expressions::Variable> newMetaVariables;
    auto const& ddMetaVariable = this->getMetaVariable(variable);
    if (ddMetaVariable.getType() == storm::dd::MetaVariableType::Bool) {
        newMetaVariables = this->addMetaVariable(newMetaVariableName, 3);
    } else if (ddMetaVariable.getType() == storm::dd::MetaVariableType::Int) {
        newMetaVariables = this->addMetaVariable(newMetaVariableName, ddMetaVariable.getLow(), ddMetaVariable.getHigh(), 3);
    } else if (ddMetaVariable.getType() == storm::dd::MetaVariableType::BitVector) {
        newMetaVariables = this->addBitVectorMetaVariable(newMetaVariableName, ddMetaVariable.getNumberOfDdVariables(), 3);
    }
    return newMetaVariables;
}

template<DdType LibraryType>
std::pair<storm::expressions::Variable, storm::expressions::Variable> DdManager<LibraryType>::addMetaVariable(
    std::string const& name, int_fast64_t low, int_fast64_t high,
    boost::optional<std::pair<MetaVariablePosition, storm::expressions::Variable>> const& position) {
    std::vector<storm::expressions::Variable> result = addMetaVariable(name, low, high, 2, position);
    return std::make_pair(result[0], result[1]);
}

template<DdType LibraryType>
std::vector<storm::expressions::Variable> DdManager<LibraryType>::addMetaVariable(
    std::string const& name, int_fast64_t low, int_fast64_t high, uint64_t numberOfLayers,
    boost::optional<std::pair<MetaVariablePosition, storm::expressions::Variable>> const& position) {
    return this->addMetaVariableHelper(MetaVariableType::Int, name,
                                       std::max(static_cast<uint64_t>(std::ceil(std::log2(high - low + 1))), static_cast<uint64_t>(1)), numberOfLayers,
                                       position, std::make_pair(low, high));
}

template<DdType LibraryType>
std::vector<storm::expressions::Variable> DdManager<LibraryType>::addBitVectorMetaVariable(
    std::string const& variableName, uint64_t bits, uint64_t numberOfLayers,
    boost::optional<std::pair<MetaVariablePosition, storm::expressions::Variable>> const& position) {
    return this->addMetaVariableHelper(MetaVariableType::BitVector, variableName, bits, numberOfLayers, position);
}

template<DdType LibraryType>
std::pair<storm::expressions::Variable, storm::expressions::Variable> DdManager<LibraryType>::addMetaVariable(
    std::string const& name, boost::optional<std::pair<MetaVariablePosition, storm::expressions::Variable>> const& position) {
    std::vector<storm::expressions::Variable> result = this->addMetaVariableHelper(MetaVariableType::Bool, name, 1, 2, position);
    return std::make_pair(result[0], result[1]);
}

template<DdType LibraryType>
std::vector<storm::expressions::Variable> DdManager<LibraryType>::addMetaVariable(
    std::string const& name, uint64_t numberOfLayers, boost::optional<std::pair<MetaVariablePosition, storm::expressions::Variable>> const& position) {
    return this->addMetaVariableHelper(MetaVariableType::Bool, name, 1, numberOfLayers, position);
}

template<DdType LibraryType>
std::vector<storm::expressions::Variable> DdManager<LibraryType>::addMetaVariableHelper(
    MetaVariableType const& type, std::string const& name, uint64_t numberOfDdVariables, uint64_t numberOfLayers,
    boost::optional<std::pair<MetaVariablePosition, storm::expressions::Variable>> const& position,
    boost::optional<std::pair<int_fast64_t, int_fast64_t>> const& bounds) {
    // Check whether number of layers is legal.
    STORM_LOG_THROW(numberOfLayers >= 1, storm::exceptions::InvalidArgumentException, "Layers must be at least 1.");

    // Check that the number of DD variables is legal.
    STORM_LOG_THROW(numberOfDdVariables >= 1, storm::exceptions::InvalidArgumentException, "Illegal number of DD variables.");

    // Check whether the variable name is legal.
    STORM_LOG_THROW(name != "" && name.back() != '\'', storm::exceptions::InvalidArgumentException, "Illegal name of meta variable: '" << name << "'.");

    // Check whether a meta variable already exists.
    STORM_LOG_THROW(!this->hasMetaVariable(name), storm::exceptions::InvalidArgumentException, "A meta variable '" << name << "' already exists.");

    // If a specific position was requested, we compute it now.
    boost::optional<uint_fast64_t> level;
    if (position) {
        storm::dd::DdMetaVariable<LibraryType> beforeVariable = this->getMetaVariable(position.get().second);
        level = position.get().first == MetaVariablePosition::Above ? std::numeric_limits<uint_fast64_t>::max() : std::numeric_limits<uint_fast64_t>::min();
        for (auto const& ddVariable : beforeVariable.getDdVariables()) {
            level = position.get().first == MetaVariablePosition::Above ? std::min(level.get(), ddVariable.getLevel())
                                                                        : std::max(level.get(), ddVariable.getLevel());
        }
        if (position.get().first == MetaVariablePosition::Below) {
            ++level.get();
        }
    }

    STORM_LOG_TRACE("Creating meta variable with " << numberOfDdVariables << " bit(s) and " << numberOfLayers << " layer(s).");

    std::stringstream tmp1;
    std::vector<storm::expressions::Variable> result;
    for (uint64_t layer = 0; layer < numberOfLayers; ++layer) {
        if (type == MetaVariableType::Int) {
            result.emplace_back(manager->declareIntegerVariable(name + tmp1.str()));
        } else if (type == MetaVariableType::Bool) {
            result.emplace_back(manager->declareBooleanVariable(name + tmp1.str()));
        } else if (type == MetaVariableType::BitVector) {
            result.emplace_back(manager->declareBitVectorVariable(name + tmp1.str(), numberOfDdVariables));
        }
        tmp1 << "'";
    }

    std::vector<std::vector<Bdd<LibraryType>>> variables(numberOfLayers);
    for (std::size_t i = 0; i < numberOfDdVariables; ++i) {
        std::vector<InternalBdd<LibraryType>> ddVariables = internalDdManager.createDdVariables(numberOfLayers, level);
        for (uint64_t layer = 0; layer < numberOfLayers; ++layer) {
            variables[layer].emplace_back(Bdd<LibraryType>(*this, ddVariables[layer], {result[layer]}));
        }

        // If we are inserting the variable at a specific level, we need to prepare the level for the next pair
        // of variables.
        if (level) {
            level.get() += numberOfLayers;
        }
    }

    std::stringstream tmp2;
    for (uint64_t layer = 0; layer < numberOfLayers; ++layer) {
        if (bounds) {
            metaVariableMap.emplace(result[layer], DdMetaVariable<LibraryType>(name + tmp2.str(), bounds.get().first, bounds.get().second, variables[layer]));
        } else {
            metaVariableMap.emplace(result[layer], DdMetaVariable<LibraryType>(type, name + tmp2.str(), variables[layer]));
        }
        tmp2 << "'";
    }

    return result;
}

template<DdType LibraryType>
DdMetaVariable<LibraryType> const& DdManager<LibraryType>::getMetaVariable(storm::expressions::Variable const& variable) const {
    auto const& variablePair = metaVariableMap.find(variable);

    // Check whether the meta variable exists.
    STORM_LOG_THROW(variablePair != metaVariableMap.end(), storm::exceptions::InvalidArgumentException,
                    "Unknown meta variable name '" << variable.getName() << "'.");

    return variablePair->second;
}

template<DdType LibraryType>
std::set<std::string> DdManager<LibraryType>::getAllMetaVariableNames() const {
    std::set<std::string> result;
    for (auto const& variablePair : metaVariableMap) {
        result.insert(variablePair.first.getName());
    }
    return result;
}

template<DdType LibraryType>
std::size_t DdManager<LibraryType>::getNumberOfMetaVariables() const {
    return this->metaVariableMap.size();
}

template<DdType LibraryType>
bool DdManager<LibraryType>::hasMetaVariable(std::string const& metaVariableName) const {
    return manager->hasVariable(metaVariableName);
}

template<DdType LibraryType>
storm::expressions::Variable DdManager<LibraryType>::getMetaVariable(std::string const& metaVariableName) const {
    // Check whether the meta variable exists.
    STORM_LOG_THROW(hasMetaVariable(metaVariableName), storm::exceptions::InvalidArgumentException, "Unknown meta variable name '" << metaVariableName << "'.");

    return manager->getVariable(metaVariableName);
}

template<DdType LibraryType>
bool DdManager<LibraryType>::supportsOrderedInsertion() const {
    return internalDdManager.supportsOrderedInsertion();
}

template<DdType LibraryType>
storm::expressions::ExpressionManager const& DdManager<LibraryType>::getExpressionManager() const {
    return *manager;
}

template<DdType LibraryType>
storm::expressions::ExpressionManager& DdManager<LibraryType>::getExpressionManager() {
    return *manager;
}

template<DdType LibraryType>
std::vector<std::string> DdManager<LibraryType>::getDdVariableNames() const {
    // First, we initialize a list DD variables and their names.
    std::vector<std::pair<uint_fast64_t, std::string>> variablePairs;
    for (auto const& variablePair : this->metaVariableMap) {
        DdMetaVariable<LibraryType> const& metaVariable = variablePair.second;
        // If the meta variable is of type bool, we don't need to suffix it with the bit number.
        if (metaVariable.getType() == MetaVariableType::Bool) {
            variablePairs.emplace_back(metaVariable.getDdVariables().front().getIndex(), variablePair.first.getName());
        } else {
            // For integer-valued meta variables, we, however, have to add the suffix.
            for (uint_fast64_t variableIndex = 0; variableIndex < metaVariable.getNumberOfDdVariables(); ++variableIndex) {
                variablePairs.emplace_back(metaVariable.getDdVariables()[variableIndex].getIndex(),
                                           variablePair.first.getName() + '.' + std::to_string(variableIndex));
            }
        }
    }

    // Then, we sort this list according to the indices of the ADDs.
    std::sort(variablePairs.begin(), variablePairs.end(),
              [](std::pair<uint_fast64_t, std::string> const& a, std::pair<uint_fast64_t, std::string> const& b) { return a.first < b.first; });

    // Now, we project the sorted vector to its second component.
    std::vector<std::string> result;
    for (auto const& element : variablePairs) {
        result.push_back(element.second);
    }

    return result;
}

template<DdType LibraryType>
std::vector<storm::expressions::Variable> DdManager<LibraryType>::getDdVariables() const {
    // First, we initialize a list DD variables and their names.
    std::vector<std::pair<uint_fast64_t, storm::expressions::Variable>> variablePairs;
    for (auto const& variablePair : this->metaVariableMap) {
        DdMetaVariable<LibraryType> const& metaVariable = variablePair.second;
        // If the meta variable is of type bool, we don't need to suffix it with the bit number.
        if (metaVariable.getType() == MetaVariableType::Bool) {
            variablePairs.emplace_back(metaVariable.getDdVariables().front().getIndex(), variablePair.first);
        } else {
            // For integer-valued meta variables, we, however, have to add the suffix.
            for (uint_fast64_t variableIndex = 0; variableIndex < metaVariable.getNumberOfDdVariables(); ++variableIndex) {
                variablePairs.emplace_back(metaVariable.getDdVariables()[variableIndex].getIndex(), variablePair.first);
            }
        }
    }

    // Then, we sort this list according to the indices of the ADDs.
    std::sort(variablePairs.begin(), variablePairs.end(),
              [](std::pair<uint_fast64_t, storm::expressions::Variable> const& a, std::pair<uint_fast64_t, storm::expressions::Variable> const& b) {
                  return a.first < b.first;
              });

    // Now, we project the sorted vector to its second component.
    std::vector<storm::expressions::Variable> result;
    for (auto const& element : variablePairs) {
        result.push_back(element.second);
    }

    return result;
}

template<DdType LibraryType>
void DdManager<LibraryType>::allowDynamicReordering(bool value) {
    internalDdManager.allowDynamicReordering(value);
}

template<DdType LibraryType>
bool DdManager<LibraryType>::isDynamicReorderingAllowed() const {
    return internalDdManager.isDynamicReorderingAllowed();
}

template<DdType LibraryType>
void DdManager<LibraryType>::triggerReordering() {
    internalDdManager.triggerReordering();
}

template<DdType LibraryType>
std::set<storm::expressions::Variable> DdManager<LibraryType>::getAllMetaVariables() const {
    std::set<storm::expressions::Variable> result;
    for (auto const& variable : this->metaVariableMap) {
        result.insert(variable.first);
    }
    return result;
}

template<DdType LibraryType>
std::vector<uint_fast64_t> DdManager<LibraryType>::getSortedVariableIndices() const {
    return this->getSortedVariableIndices(this->getAllMetaVariables());
}

template<DdType LibraryType>
std::vector<uint_fast64_t> DdManager<LibraryType>::getSortedVariableIndices(std::set<storm::expressions::Variable> const& metaVariables) const {
    std::vector<uint_fast64_t> ddVariableIndices;
    for (auto const& metaVariable : metaVariables) {
        for (auto const& ddVariable : metaVariableMap.at(metaVariable).getDdVariables()) {
            ddVariableIndices.push_back(ddVariable.getIndex());
        }
    }

    // Next, we need to sort them, since they may be arbitrarily ordered otherwise.
    std::ranges::sort(ddVariableIndices);
    return ddVariableIndices;
}

template<DdType LibraryType>
InternalDdManager<LibraryType>& DdManager<LibraryType>::getInternalDdManager() {
    return internalDdManager;
}

template<DdType LibraryType>
InternalDdManager<LibraryType> const& DdManager<LibraryType>::getInternalDdManager() const {
    return internalDdManager;
}

template<DdType LibraryType>
InternalDdManager<LibraryType>* DdManager<LibraryType>::getInternalDdManagerPointer() {
    return &internalDdManager;
}

template<DdType LibraryType>
InternalDdManager<LibraryType> const* DdManager<LibraryType>::getInternalDdManagerPointer() const {
    return &internalDdManager;
}

template<DdType LibraryType>
void DdManager<LibraryType>::debugCheck() const {
    internalDdManager.debugCheck();
}

template<DdType LibraryType>
void DdManager<LibraryType>::execute(std::function<void()> const& f) const {
    internalDdManager.execute(f);
}

template class DdManager<DdType::CUDD>;

template Add<DdType::CUDD, double> DdManager<DdType::CUDD>::getAddZero() const;
template Add<DdType::CUDD, uint_fast64_t> DdManager<DdType::CUDD>::getAddZero() const;
template Add<DdType::CUDD, storm::RationalNumber> DdManager<DdType::CUDD>::getAddZero() const;

template Add<DdType::CUDD, double> DdManager<DdType::CUDD>::getAddOne() const;
template Add<DdType::CUDD, uint_fast64_t> DdManager<DdType::CUDD>::getAddOne() const;
template Add<DdType::CUDD, storm::RationalNumber> DdManager<DdType::CUDD>::getAddOne() const;

template Add<DdType::CUDD, double> DdManager<DdType::CUDD>::getInfinity<double>() const;
template Add<DdType::CUDD, uint_fast64_t> DdManager<DdType::CUDD>::getInfinity<uint_fast64_t>() const;

template Add<DdType::CUDD, double> DdManager<DdType::CUDD>::getConstant(double const& value) const;
template Add<DdType::CUDD, uint_fast64_t> DdManager<DdType::CUDD>::getConstant(uint_fast64_t const& value) const;
template Add<DdType::CUDD, storm::RationalNumber> DdManager<DdType::CUDD>::getConstant(storm::RationalNumber const& value) const;

template Add<DdType::CUDD, double> DdManager<DdType::CUDD>::getIdentity(storm::expressions::Variable const& variable) const;
template Add<DdType::CUDD, uint_fast64_t> DdManager<DdType::CUDD>::getIdentity(storm::expressions::Variable const& variable) const;

template class DdManager<DdType::Sylvan>;

template Add<DdType::Sylvan, double> DdManager<DdType::Sylvan>::getAddZero() const;
template Add<DdType::Sylvan, uint_fast64_t> DdManager<DdType::Sylvan>::getAddZero() const;
template Add<DdType::Sylvan, storm::RationalNumber> DdManager<DdType::Sylvan>::getAddZero() const;
template Add<DdType::Sylvan, storm::RationalFunction> DdManager<DdType::Sylvan>::getAddZero() const;

template Add<DdType::Sylvan, double> DdManager<DdType::Sylvan>::getAddUndefined() const;
template Add<DdType::Sylvan, uint_fast64_t> DdManager<DdType::Sylvan>::getAddUndefined() const;
template Add<DdType::Sylvan, storm::RationalNumber> DdManager<DdType::Sylvan>::getAddUndefined() const;
template Add<DdType::Sylvan, storm::RationalFunction> DdManager<DdType::Sylvan>::getAddUndefined() const;

template Add<DdType::Sylvan, double> DdManager<DdType::Sylvan>::getAddOne() const;
template Add<DdType::Sylvan, uint_fast64_t> DdManager<DdType::Sylvan>::getAddOne() const;
template Add<DdType::Sylvan, storm::RationalNumber> DdManager<DdType::Sylvan>::getAddOne() const;
template Add<DdType::Sylvan, storm::RationalFunction> DdManager<DdType::Sylvan>::getAddOne() const;

template Add<DdType::Sylvan, double> DdManager<DdType::Sylvan>::getInfinity<double>() const;
template Add<DdType::Sylvan, uint_fast64_t> DdManager<DdType::Sylvan>::getInfinity<uint_fast64_t>() const;
template Add<DdType::Sylvan, storm::RationalNumber> DdManager<DdType::Sylvan>::getInfinity<storm::RationalNumber>() const;
template Add<DdType::Sylvan, storm::RationalFunction> DdManager<DdType::Sylvan>::getInfinity<storm::RationalFunction>() const;

template Add<DdType::Sylvan, double> DdManager<DdType::Sylvan>::getConstant(double const& value) const;
template Add<DdType::Sylvan, uint_fast64_t> DdManager<DdType::Sylvan>::getConstant(uint_fast64_t const& value) const;
template Add<DdType::Sylvan, storm::RationalNumber> DdManager<DdType::Sylvan>::getConstant(storm::RationalNumber const& value) const;
template Add<DdType::Sylvan, storm::RationalFunction> DdManager<DdType::Sylvan>::getConstant(storm::RationalFunction const& value) const;

template Add<DdType::Sylvan, double> DdManager<DdType::Sylvan>::getIdentity(storm::expressions::Variable const& variable) const;
template Add<DdType::Sylvan, uint_fast64_t> DdManager<DdType::Sylvan>::getIdentity(storm::expressions::Variable const& variable) const;
template Add<DdType::Sylvan, storm::RationalNumber> DdManager<DdType::Sylvan>::getIdentity(storm::expressions::Variable const& variable) const;
template Add<DdType::Sylvan, storm::RationalFunction> DdManager<DdType::Sylvan>::getIdentity(storm::expressions::Variable const& variable) const;
}  // namespace dd
}  // namespace storm
