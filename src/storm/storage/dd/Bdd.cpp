#include "storm/storage/dd/Bdd.h"

#include <algorithm>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/logic/ComparisonType.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/dd/DdMetaVariable.h"
#include "storm/storage/dd/Odd.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {

template<DdType LibraryType>
Bdd<LibraryType>::Bdd(DdManager<LibraryType> const& ddManager, InternalBdd<LibraryType> const& internalBdd,
                      std::set<storm::expressions::Variable> const& containedMetaVariables)
    : Dd<LibraryType>(ddManager, containedMetaVariables), internalBdd(internalBdd) {
    // Intentionally left empty.
}

template<DdType LibraryType, typename ValueType>
struct FromVectorHelper {
    static Bdd<LibraryType> fromVector(DdManager<LibraryType> const& ddManager, std::vector<ValueType> const& explicitValues, storm::dd::Odd const& odd,
                                       std::set<storm::expressions::Variable> const& metaVariables, storm::logic::ComparisonType comparisonType,
                                       ValueType value) {
        switch (comparisonType) {
            case storm::logic::ComparisonType::Less:
                return Bdd<LibraryType>(
                    ddManager,
                    InternalBdd<LibraryType>::fromVector(&ddManager.getInternalDdManager(), odd, ddManager.getSortedVariableIndices(metaVariables),
                                                         [&value, &explicitValues](uint64_t offset) { return explicitValues[offset] < value; }),
                    metaVariables);
            case storm::logic::ComparisonType::LessEqual:
                return Bdd<LibraryType>(
                    ddManager,
                    InternalBdd<LibraryType>::fromVector(&ddManager.getInternalDdManager(), odd, ddManager.getSortedVariableIndices(metaVariables),
                                                         [&value, &explicitValues](uint64_t offset) { return explicitValues[offset] <= value; }),
                    metaVariables);
            case storm::logic::ComparisonType::Greater:
                return Bdd<LibraryType>(
                    ddManager,
                    InternalBdd<LibraryType>::fromVector(&ddManager.getInternalDdManager(), odd, ddManager.getSortedVariableIndices(metaVariables),
                                                         [&value, &explicitValues](uint64_t offset) { return explicitValues[offset] > value; }),
                    metaVariables);
            case storm::logic::ComparisonType::GreaterEqual:
                return Bdd<LibraryType>(
                    ddManager,
                    InternalBdd<LibraryType>::fromVector(&ddManager.getInternalDdManager(), odd, ddManager.getSortedVariableIndices(metaVariables),
                                                         [&value, &explicitValues](uint64_t offset) { return explicitValues[offset] >= value; }),
                    metaVariables);
        }
        return Bdd<LibraryType>();
    }
};

template<DdType LibraryType>
struct FromVectorHelper<LibraryType, storm::RationalFunction> {
    static Bdd<LibraryType> fromVector(DdManager<LibraryType> const&, std::vector<storm::RationalFunction> const&, storm::dd::Odd const&,
                                       std::set<storm::expressions::Variable> const&, storm::logic::ComparisonType, storm::RationalFunction) {
        STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Cannot compare rational functions to bound.");
        return Bdd<LibraryType>();
    }
};

template<DdType LibraryType>
template<typename ValueType>
Bdd<LibraryType> Bdd<LibraryType>::fromVector(DdManager<LibraryType> const& ddManager, std::vector<ValueType> const& explicitValues, storm::dd::Odd const& odd,
                                              std::set<storm::expressions::Variable> const& metaVariables, storm::logic::ComparisonType comparisonType,
                                              ValueType value) {
    return FromVectorHelper<LibraryType, ValueType>::fromVector(ddManager, explicitValues, odd, metaVariables, comparisonType, value);
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::fromVector(DdManager<LibraryType> const& ddManager, storm::storage::BitVector const& truthValues, storm::dd::Odd const& odd,
                                              std::set<storm::expressions::Variable> const& metaVariables) {
    return Bdd<LibraryType>(ddManager,
                            InternalBdd<LibraryType>::fromVector(&ddManager.getInternalDdManager(), odd, ddManager.getSortedVariableIndices(metaVariables),
                                                                 [&truthValues](uint64_t offset) { return truthValues[offset]; }),
                            metaVariables);
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::getEncoding(DdManager<LibraryType> const& ddManager, uint64_t targetOffset, storm::dd::Odd const& odd,
                                               std::set<storm::expressions::Variable> const& metaVariables) {
    return Bdd<LibraryType>(ddManager,
                            InternalBdd<LibraryType>::fromVector(&ddManager.getInternalDdManager(), odd, ddManager.getSortedVariableIndices(metaVariables),
                                                                 [targetOffset](uint64_t offset) { return offset == targetOffset; }),
                            metaVariables);
}

template<DdType LibraryType>
bool Bdd<LibraryType>::operator==(Bdd<LibraryType> const& other) const {
    return internalBdd == other.internalBdd;
}

template<DdType LibraryType>
bool Bdd<LibraryType>::operator!=(Bdd<LibraryType> const& other) const {
    return internalBdd != other.internalBdd;
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::ite(Bdd<LibraryType> const& thenBdd, Bdd<LibraryType> const& elseBdd) const {
    std::set<storm::expressions::Variable> metaVariables = Dd<LibraryType>::joinMetaVariables(thenBdd, elseBdd);
    metaVariables.insert(this->getContainedMetaVariables().begin(), this->getContainedMetaVariables().end());
    return Bdd<LibraryType>(this->getDdManager(), internalBdd.ite(thenBdd.internalBdd, elseBdd.internalBdd), metaVariables);
}

template<DdType LibraryType>
template<typename ValueType>
Add<LibraryType, ValueType> Bdd<LibraryType>::ite(Add<LibraryType, ValueType> const& thenAdd, Add<LibraryType, ValueType> const& elseAdd) const {
    std::set<storm::expressions::Variable> metaVariables = Dd<LibraryType>::joinMetaVariables(thenAdd, elseAdd);
    metaVariables.insert(this->getContainedMetaVariables().begin(), this->getContainedMetaVariables().end());
    return Add<LibraryType, ValueType>(this->getDdManager(), internalBdd.ite(thenAdd.internalAdd, elseAdd.internalAdd), metaVariables);
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::operator||(Bdd<LibraryType> const& other) const {
    return Bdd<LibraryType>(this->getDdManager(), internalBdd || other.internalBdd, Dd<LibraryType>::joinMetaVariables(*this, other));
}

template<DdType LibraryType>
Bdd<LibraryType>& Bdd<LibraryType>::operator|=(Bdd<LibraryType> const& other) {
    this->addMetaVariables(other.getContainedMetaVariables());
    internalBdd |= other.internalBdd;
    return *this;
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::operator&&(Bdd<LibraryType> const& other) const {
    return Bdd<LibraryType>(this->getDdManager(), internalBdd && other.internalBdd, Dd<LibraryType>::joinMetaVariables(*this, other));
}

template<DdType LibraryType>
Bdd<LibraryType>& Bdd<LibraryType>::operator&=(Bdd<LibraryType> const& other) {
    this->addMetaVariables(other.getContainedMetaVariables());
    internalBdd &= other.internalBdd;
    return *this;
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::iff(Bdd<LibraryType> const& other) const {
    return Bdd<LibraryType>(this->getDdManager(), internalBdd.iff(other.internalBdd), Dd<LibraryType>::joinMetaVariables(*this, other));
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::exclusiveOr(Bdd<LibraryType> const& other) const {
    return Bdd<LibraryType>(this->getDdManager(), internalBdd.exclusiveOr(other.internalBdd), Dd<LibraryType>::joinMetaVariables(*this, other));
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::implies(Bdd<LibraryType> const& other) const {
    return Bdd<LibraryType>(this->getDdManager(), internalBdd.implies(other.internalBdd), Dd<LibraryType>::joinMetaVariables(*this, other));
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::operator!() const {
    return Bdd<LibraryType>(this->getDdManager(), !internalBdd, this->getContainedMetaVariables());
}

template<DdType LibraryType>
Bdd<LibraryType>& Bdd<LibraryType>::complement() {
    internalBdd.complement();
    return *this;
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::existsAbstract(std::set<storm::expressions::Variable> const& metaVariables) const {
    Bdd<LibraryType> cube = getCube(this->getDdManager(), metaVariables);
    return Bdd<LibraryType>(this->getDdManager(), internalBdd.existsAbstract(cube.getInternalBdd()), Dd<LibraryType>::subtractMetaVariables(*this, cube));
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::existsAbstractRepresentative(std::set<storm::expressions::Variable> const& metaVariables) const {
    Bdd<LibraryType> cube = getCube(this->getDdManager(), metaVariables);
    return Bdd<LibraryType>(this->getDdManager(), internalBdd.existsAbstractRepresentative(cube.getInternalBdd()), this->getContainedMetaVariables());
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::universalAbstract(std::set<storm::expressions::Variable> const& metaVariables) const {
    Bdd<LibraryType> cube = getCube(this->getDdManager(), metaVariables);
    return Bdd<LibraryType>(this->getDdManager(), internalBdd.universalAbstract(cube.getInternalBdd()), Dd<LibraryType>::subtractMetaVariables(*this, cube));
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::andExists(Bdd<LibraryType> const& other, std::set<storm::expressions::Variable> const& existentialVariables) const {
    Bdd<LibraryType> cube = getCube(this->getDdManager(), existentialVariables);

    std::set<storm::expressions::Variable> unionOfMetaVariables;
    std::set_union(this->getContainedMetaVariables().begin(), this->getContainedMetaVariables().end(), other.getContainedMetaVariables().begin(),
                   other.getContainedMetaVariables().end(), std::inserter(unionOfMetaVariables, unionOfMetaVariables.begin()));
    std::set<storm::expressions::Variable> containedMetaVariables;
    std::set_difference(unionOfMetaVariables.begin(), unionOfMetaVariables.end(), existentialVariables.begin(), existentialVariables.end(),
                        std::inserter(containedMetaVariables, containedMetaVariables.begin()));

    return Bdd<LibraryType>(this->getDdManager(), internalBdd.andExists(other.getInternalBdd(), cube.getInternalBdd()), containedMetaVariables);
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::constrain(Bdd<LibraryType> const& constraint) const {
    return Bdd<LibraryType>(this->getDdManager(), internalBdd.constrain(constraint.getInternalBdd()), Dd<LibraryType>::joinMetaVariables(*this, constraint));
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::restrict(Bdd<LibraryType> const& constraint) const {
    return Bdd<LibraryType>(this->getDdManager(), internalBdd.restrict(constraint.getInternalBdd()), Dd<LibraryType>::joinMetaVariables(*this, constraint));
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::relationalProduct(Bdd<LibraryType> const& relation, std::set<storm::expressions::Variable> const& rowMetaVariables,
                                                     std::set<storm::expressions::Variable> const& columnMetaVariables) const {
    std::set<storm::expressions::Variable> newMetaVariables;
    std::set_difference(relation.getContainedMetaVariables().begin(), relation.getContainedMetaVariables().end(), columnMetaVariables.begin(),
                        columnMetaVariables.end(), std::inserter(newMetaVariables, newMetaVariables.begin()));

    std::vector<InternalBdd<LibraryType>> rowVariables;
    for (auto const& metaVariable : rowMetaVariables) {
        DdMetaVariable<LibraryType> const& variable = this->getDdManager().getMetaVariable(metaVariable);
        for (auto const& ddVariable : variable.getDdVariables()) {
            rowVariables.push_back(ddVariable.getInternalBdd());
        }
    }

    std::vector<InternalBdd<LibraryType>> columnVariables;
    for (auto const& metaVariable : columnMetaVariables) {
        DdMetaVariable<LibraryType> const& variable = this->getDdManager().getMetaVariable(metaVariable);
        for (auto const& ddVariable : variable.getDdVariables()) {
            columnVariables.push_back(ddVariable.getInternalBdd());
        }
    }

    return Bdd<LibraryType>(this->getDdManager(), internalBdd.relationalProduct(relation.getInternalBdd(), rowVariables, columnVariables), newMetaVariables);
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::inverseRelationalProduct(Bdd<LibraryType> const& relation, std::set<storm::expressions::Variable> const& rowMetaVariables,
                                                            std::set<storm::expressions::Variable> const& columnMetaVariables) const {
    std::set<storm::expressions::Variable> newMetaVariables;
    std::set_difference(relation.getContainedMetaVariables().begin(), relation.getContainedMetaVariables().end(), columnMetaVariables.begin(),
                        columnMetaVariables.end(), std::inserter(newMetaVariables, newMetaVariables.begin()));

    std::vector<InternalBdd<LibraryType>> rowVariables;
    for (auto const& metaVariable : rowMetaVariables) {
        DdMetaVariable<LibraryType> const& variable = this->getDdManager().getMetaVariable(metaVariable);
        for (auto const& ddVariable : variable.getDdVariables()) {
            rowVariables.push_back(ddVariable.getInternalBdd());
        }
    }

    std::vector<InternalBdd<LibraryType>> columnVariables;
    for (auto const& metaVariable : columnMetaVariables) {
        DdMetaVariable<LibraryType> const& variable = this->getDdManager().getMetaVariable(metaVariable);
        for (auto const& ddVariable : variable.getDdVariables()) {
            columnVariables.push_back(ddVariable.getInternalBdd());
        }
    }

    return Bdd<LibraryType>(this->getDdManager(), internalBdd.inverseRelationalProduct(relation.getInternalBdd(), rowVariables, columnVariables),
                            newMetaVariables);
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::inverseRelationalProductWithExtendedRelation(Bdd<LibraryType> const& relation,
                                                                                std::set<storm::expressions::Variable> const& rowMetaVariables,
                                                                                std::set<storm::expressions::Variable> const& columnMetaVariables) const {
    std::set<storm::expressions::Variable> newMetaVariables;
    std::set_difference(relation.getContainedMetaVariables().begin(), relation.getContainedMetaVariables().end(), columnMetaVariables.begin(),
                        columnMetaVariables.end(), std::inserter(newMetaVariables, newMetaVariables.begin()));

    std::vector<InternalBdd<LibraryType>> rowVariables;
    for (auto const& metaVariable : rowMetaVariables) {
        DdMetaVariable<LibraryType> const& variable = this->getDdManager().getMetaVariable(metaVariable);
        for (auto const& ddVariable : variable.getDdVariables()) {
            rowVariables.push_back(ddVariable.getInternalBdd());
        }
    }

    std::vector<InternalBdd<LibraryType>> columnVariables;
    for (auto const& metaVariable : columnMetaVariables) {
        DdMetaVariable<LibraryType> const& variable = this->getDdManager().getMetaVariable(metaVariable);
        for (auto const& ddVariable : variable.getDdVariables()) {
            columnVariables.push_back(ddVariable.getInternalBdd());
        }
    }

    return Bdd<LibraryType>(this->getDdManager(),
                            internalBdd.inverseRelationalProductWithExtendedRelation(relation.getInternalBdd(), rowVariables, columnVariables),
                            newMetaVariables);
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::swapVariables(
    std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& metaVariablePairs) const {
    std::set<storm::expressions::Variable> newContainedMetaVariables;
    std::set<storm::expressions::Variable> deletedMetaVariables;
    std::vector<InternalBdd<LibraryType>> from;
    std::vector<InternalBdd<LibraryType>> to;
    for (auto const& metaVariablePair : metaVariablePairs) {
        DdMetaVariable<LibraryType> const& variable1 = this->getDdManager().getMetaVariable(metaVariablePair.first);
        DdMetaVariable<LibraryType> const& variable2 = this->getDdManager().getMetaVariable(metaVariablePair.second);

        // Keep track of the contained meta variables in the DD.
        if (this->containsMetaVariable(metaVariablePair.first)) {
            if (this->containsMetaVariable(metaVariablePair.second)) {
                // Nothing to do here.
            } else {
                newContainedMetaVariables.insert(metaVariablePair.second);
                deletedMetaVariables.insert(metaVariablePair.first);
            }
        } else {
            if (!this->containsMetaVariable(metaVariablePair.second)) {
                // Nothing to do here.
            } else {
                newContainedMetaVariables.insert(metaVariablePair.first);
                deletedMetaVariables.insert(metaVariablePair.second);
            }
        }

        for (auto const& ddVariable : variable1.getDdVariables()) {
            from.emplace_back(ddVariable.getInternalBdd());
        }
        for (auto const& ddVariable : variable2.getDdVariables()) {
            to.emplace_back(ddVariable.getInternalBdd());
        }
    }

    std::set<storm::expressions::Variable> tmp;
    std::set_difference(this->getContainedMetaVariables().begin(), this->getContainedMetaVariables().end(), deletedMetaVariables.begin(),
                        deletedMetaVariables.end(), std::inserter(tmp, tmp.begin()));
    std::set<storm::expressions::Variable> containedMetaVariables;
    std::set_union(tmp.begin(), tmp.end(), newContainedMetaVariables.begin(), newContainedMetaVariables.end(),
                   std::inserter(containedMetaVariables, containedMetaVariables.begin()));
    return Bdd<LibraryType>(this->getDdManager(), internalBdd.swapVariables(from, to), containedMetaVariables);
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::renameVariables(std::set<storm::expressions::Variable> const& from, std::set<storm::expressions::Variable> const& to) const {
    std::vector<InternalBdd<LibraryType>> fromBdds;
    std::vector<InternalBdd<LibraryType>> toBdds;

    for (auto const& metaVariable : from) {
        STORM_LOG_THROW(this->containsMetaVariable(metaVariable), storm::exceptions::InvalidOperationException,
                        "Cannot rename variable '" << metaVariable.getName() << "' that is not present.");
        DdMetaVariable<LibraryType> const& ddMetaVariable = this->getDdManager().getMetaVariable(metaVariable);
        for (auto const& ddVariable : ddMetaVariable.getDdVariables()) {
            fromBdds.push_back(ddVariable.getInternalBdd());
        }
    }
    for (auto const& metaVariable : to) {
        STORM_LOG_THROW(!this->containsMetaVariable(metaVariable), storm::exceptions::InvalidOperationException,
                        "Cannot rename to variable '" << metaVariable.getName() << "' that is already present.");
        DdMetaVariable<LibraryType> const& ddMetaVariable = this->getDdManager().getMetaVariable(metaVariable);
        for (auto const& ddVariable : ddMetaVariable.getDdVariables()) {
            toBdds.push_back(ddVariable.getInternalBdd());
        }
    }

    std::set<storm::expressions::Variable> newContainedMetaVariables = to;
    std::set_difference(this->getContainedMetaVariables().begin(), this->getContainedMetaVariables().end(), from.begin(), from.end(),
                        std::inserter(newContainedMetaVariables, newContainedMetaVariables.begin()));

    STORM_LOG_THROW(fromBdds.size() == toBdds.size(), storm::exceptions::InvalidArgumentException, "Unable to rename mismatching meta variables.");
    return Bdd<LibraryType>(this->getDdManager(), internalBdd.swapVariables(fromBdds, toBdds), newContainedMetaVariables);
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::renameVariablesAbstract(std::set<storm::expressions::Variable> const& from,
                                                           std::set<storm::expressions::Variable> const& to) const {
    std::vector<InternalBdd<LibraryType>> fromBdds;
    std::vector<InternalBdd<LibraryType>> toBdds;

    for (auto const& metaVariable : from) {
        STORM_LOG_THROW(this->containsMetaVariable(metaVariable), storm::exceptions::InvalidOperationException,
                        "Cannot rename variable '" << metaVariable.getName() << "' that is not present.");
        DdMetaVariable<LibraryType> const& ddMetaVariable = this->getDdManager().getMetaVariable(metaVariable);
        for (auto const& ddVariable : ddMetaVariable.getDdVariables()) {
            fromBdds.push_back(ddVariable.getInternalBdd());
        }
    }
    std::sort(fromBdds.begin(), fromBdds.end(),
              [](InternalBdd<LibraryType> const& a, InternalBdd<LibraryType> const& b) { return a.getLevel() < b.getLevel(); });
    for (auto const& metaVariable : to) {
        STORM_LOG_THROW(!this->containsMetaVariable(metaVariable), storm::exceptions::InvalidOperationException,
                        "Cannot rename to variable '" << metaVariable.getName() << "' that is already present.");
        DdMetaVariable<LibraryType> const& ddMetaVariable = this->getDdManager().getMetaVariable(metaVariable);
        for (auto const& ddVariable : ddMetaVariable.getDdVariables()) {
            toBdds.push_back(ddVariable.getInternalBdd());
        }
    }
    std::sort(toBdds.begin(), toBdds.end(), [](InternalBdd<LibraryType> const& a, InternalBdd<LibraryType> const& b) { return a.getLevel() < b.getLevel(); });

    std::set<storm::expressions::Variable> newContainedMetaVariables = to;
    std::set_difference(this->getContainedMetaVariables().begin(), this->getContainedMetaVariables().end(), from.begin(), from.end(),
                        std::inserter(newContainedMetaVariables, newContainedMetaVariables.begin()));

    STORM_LOG_ASSERT(fromBdds.size() >= toBdds.size(), "Unable to perform rename-abstract with mismatching sizes.");

    if (fromBdds.size() == toBdds.size()) {
        return Bdd<LibraryType>(this->getDdManager(), internalBdd.swapVariables(fromBdds, toBdds), newContainedMetaVariables);
    } else {
        InternalBdd<LibraryType> cube = this->getDdManager().getBddOne().getInternalBdd();
        for (uint64_t index = toBdds.size(); index < fromBdds.size(); ++index) {
            cube &= fromBdds[index];
        }
        fromBdds.resize(toBdds.size());

        return Bdd<LibraryType>(this->getDdManager(), internalBdd.existsAbstract(cube).swapVariables(fromBdds, toBdds), newContainedMetaVariables);
    }
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::renameVariablesConcretize(std::set<storm::expressions::Variable> const& from,
                                                             std::set<storm::expressions::Variable> const& to) const {
    std::vector<InternalBdd<LibraryType>> fromBdds;
    std::vector<InternalBdd<LibraryType>> toBdds;

    for (auto const& metaVariable : from) {
        STORM_LOG_THROW(this->containsMetaVariable(metaVariable), storm::exceptions::InvalidOperationException,
                        "Cannot rename variable '" << metaVariable.getName() << "' that is not present.");
        DdMetaVariable<LibraryType> const& ddMetaVariable = this->getDdManager().getMetaVariable(metaVariable);
        for (auto const& ddVariable : ddMetaVariable.getDdVariables()) {
            fromBdds.push_back(ddVariable.getInternalBdd());
        }
    }
    std::sort(fromBdds.begin(), fromBdds.end(),
              [](InternalBdd<LibraryType> const& a, InternalBdd<LibraryType> const& b) { return a.getLevel() < b.getLevel(); });
    for (auto const& metaVariable : to) {
        STORM_LOG_THROW(!this->containsMetaVariable(metaVariable), storm::exceptions::InvalidOperationException,
                        "Cannot rename to variable '" << metaVariable.getName() << "' that is already present.");
        DdMetaVariable<LibraryType> const& ddMetaVariable = this->getDdManager().getMetaVariable(metaVariable);
        for (auto const& ddVariable : ddMetaVariable.getDdVariables()) {
            toBdds.push_back(ddVariable.getInternalBdd());
        }
    }
    std::sort(toBdds.begin(), toBdds.end(), [](InternalBdd<LibraryType> const& a, InternalBdd<LibraryType> const& b) { return a.getLevel() < b.getLevel(); });

    std::set<storm::expressions::Variable> newContainedMetaVariables = to;
    std::set_difference(this->getContainedMetaVariables().begin(), this->getContainedMetaVariables().end(), from.begin(), from.end(),
                        std::inserter(newContainedMetaVariables, newContainedMetaVariables.begin()));

    STORM_LOG_ASSERT(toBdds.size() >= fromBdds.size(), "Unable to perform rename-concretize with mismatching sizes.");

    if (fromBdds.size() == toBdds.size()) {
        return Bdd<LibraryType>(this->getDdManager(), internalBdd.swapVariables(fromBdds, toBdds), newContainedMetaVariables);
    } else {
        InternalBdd<LibraryType> negatedCube = this->getDdManager().getBddOne().getInternalBdd();
        for (uint64_t index = fromBdds.size(); index < toBdds.size(); ++index) {
            negatedCube &= !toBdds[index];
        }
        toBdds.resize(fromBdds.size());

        return Bdd<LibraryType>(this->getDdManager(), (internalBdd && negatedCube).swapVariables(fromBdds, toBdds), newContainedMetaVariables);
    }
}

template<DdType LibraryType>
template<typename ValueType>
Add<LibraryType, ValueType> Bdd<LibraryType>::toAdd() const {
    return Add<LibraryType, ValueType>(this->getDdManager(), internalBdd.template toAdd<ValueType>(), this->getContainedMetaVariables());
}

template<DdType LibraryType>
std::vector<Bdd<LibraryType>> Bdd<LibraryType>::split(std::set<storm::expressions::Variable> const& variables) const {
    std::set<storm::expressions::Variable> remainingMetaVariables;
    std::set_difference(this->getContainedMetaVariables().begin(), this->getContainedMetaVariables().end(), variables.begin(), variables.end(),
                        std::inserter(remainingMetaVariables, remainingMetaVariables.begin()));

    std::vector<uint_fast64_t> ddGroupVariableIndices;
    for (auto const& variable : variables) {
        DdMetaVariable<LibraryType> const& metaVariable = this->getDdManager().getMetaVariable(variable);
        for (auto const& ddVariable : metaVariable.getDdVariables()) {
            ddGroupVariableIndices.push_back(ddVariable.getIndex());
        }
    }
    std::ranges::sort(ddGroupVariableIndices);

    std::vector<InternalBdd<LibraryType>> internalBddGroups = this->internalBdd.splitIntoGroups(ddGroupVariableIndices);
    std::vector<Bdd<LibraryType>> groups;
    for (auto const& internalBdd : internalBddGroups) {
        groups.emplace_back(Bdd<LibraryType>(this->getDdManager(), internalBdd, remainingMetaVariables));
    }

    return groups;
}

template<DdType LibraryType>
storm::storage::BitVector Bdd<LibraryType>::toVector(storm::dd::Odd const& rowOdd) const {
    return internalBdd.toVector(rowOdd, this->getSortedVariableIndices());
}

template<DdType LibraryType>
std::pair<std::vector<storm::expressions::Expression>, std::unordered_map<uint_fast64_t, storm::expressions::Variable>> Bdd<LibraryType>::toExpression(
    storm::expressions::ExpressionManager& manager) const {
    return internalBdd.toExpression(manager);
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::getSupport() const {
    return Bdd<LibraryType>(this->getDdManager(), internalBdd.getSupport(), this->getContainedMetaVariables());
}

template<DdType LibraryType>
uint_fast64_t Bdd<LibraryType>::getNonZeroCount() const {
    std::size_t numberOfDdVariables = 0;
    for (auto const& metaVariable : this->getContainedMetaVariables()) {
        numberOfDdVariables += this->getDdManager().getMetaVariable(metaVariable).getNumberOfDdVariables();
    }
    return internalBdd.getNonZeroCount(numberOfDdVariables);
}

template<DdType LibraryType>
uint_fast64_t Bdd<LibraryType>::getLeafCount() const {
    return internalBdd.getLeafCount();
}

template<DdType LibraryType>
uint_fast64_t Bdd<LibraryType>::getNodeCount() const {
    return internalBdd.getNodeCount();
}

template<DdType LibraryType>
uint_fast64_t Bdd<LibraryType>::getIndex() const {
    return internalBdd.getIndex();
}

template<DdType LibraryType>
uint_fast64_t Bdd<LibraryType>::getLevel() const {
    return internalBdd.getLevel();
}

template<DdType LibraryType>
bool Bdd<LibraryType>::isOne() const {
    return internalBdd.isOne();
}

template<DdType LibraryType>
bool Bdd<LibraryType>::isZero() const {
    return internalBdd.isZero();
}

template<DdType LibraryType>
void Bdd<LibraryType>::exportToDot(std::string const& filename, bool showVariablesIfPossible) const {
    internalBdd.exportToDot(filename, this->getDdManager().getDdVariableNames(), showVariablesIfPossible);
}

template<DdType LibraryType>
void Bdd<LibraryType>::exportToText(std::string const& filename) const {
    internalBdd.exportToText(filename);
}

template<DdType LibraryType>
Bdd<LibraryType> Bdd<LibraryType>::getCube(DdManager<LibraryType> const& manager, std::set<storm::expressions::Variable> const& metaVariables) {
    Bdd<LibraryType> cube = manager.getBddOne();
    for (auto const& metaVariable : metaVariables) {
        cube &= manager.getMetaVariable(metaVariable).getCube();
    }
    return cube;
}

template<DdType LibraryType>
Odd Bdd<LibraryType>::createOdd() const {
    return internalBdd.createOdd(this->getSortedVariableIndices());
}

template<DdType LibraryType>
InternalBdd<LibraryType> const& Bdd<LibraryType>::getInternalBdd() const {
    return internalBdd;
}

template<DdType LibraryType>
template<typename ValueType>
std::vector<ValueType> Bdd<LibraryType>::filterExplicitVector(Odd const& odd, std::vector<ValueType> const& values) const {
    std::vector<ValueType> result(this->getNonZeroCount());
    internalBdd.filterExplicitVector(odd, this->getSortedVariableIndices(), values, result);
    return result;
}

template<DdType LibraryType>
storm::storage::BitVector Bdd<LibraryType>::filterExplicitVector(Odd const& odd, storm::storage::BitVector const& values) const {
    storm::storage::BitVector result(this->getNonZeroCount());
    internalBdd.filterExplicitVector(odd, this->getSortedVariableIndices(), values, result);
    return result;
}

template class Bdd<DdType::CUDD>;

template Bdd<DdType::CUDD> Bdd<DdType::CUDD>::fromVector(DdManager<DdType::CUDD> const& ddManager, std::vector<double> const& explicitValues,
                                                         storm::dd::Odd const& odd, std::set<storm::expressions::Variable> const& metaVariables,
                                                         storm::logic::ComparisonType comparisonType, double value);

template Add<DdType::CUDD, double> Bdd<DdType::CUDD>::toAdd() const;
template Add<DdType::CUDD, uint_fast64_t> Bdd<DdType::CUDD>::toAdd() const;
template Add<DdType::CUDD, storm::RationalNumber> Bdd<DdType::CUDD>::toAdd() const;

template std::vector<double> Bdd<DdType::CUDD>::filterExplicitVector(Odd const& odd, std::vector<double> const& values) const;
template std::vector<uint_fast64_t> Bdd<DdType::CUDD>::filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& values) const;

template Add<DdType::CUDD, double> Bdd<DdType::CUDD>::ite(Add<DdType::CUDD, double> const& thenAdd, Add<DdType::CUDD, double> const& elseAdd) const;
template Add<DdType::CUDD, uint_fast64_t> Bdd<DdType::CUDD>::ite(Add<DdType::CUDD, uint_fast64_t> const& thenAdd,
                                                                 Add<DdType::CUDD, uint_fast64_t> const& elseAdd) const;
template Add<DdType::CUDD, storm::RationalNumber> Bdd<DdType::CUDD>::ite(Add<DdType::CUDD, storm::RationalNumber> const& thenAdd,
                                                                         Add<DdType::CUDD, storm::RationalNumber> const& elseAdd) const;

template class Bdd<DdType::Sylvan>;

template Bdd<DdType::Sylvan> Bdd<DdType::Sylvan>::fromVector(DdManager<DdType::Sylvan> const& ddManager, std::vector<double> const& explicitValues,
                                                             storm::dd::Odd const& odd, std::set<storm::expressions::Variable> const& metaVariables,
                                                             storm::logic::ComparisonType comparisonType, double value);

template Bdd<DdType::Sylvan> Bdd<DdType::Sylvan>::fromVector(DdManager<DdType::Sylvan> const& ddManager,
                                                             std::vector<storm::RationalNumber> const& explicitValues, storm::dd::Odd const& odd,
                                                             std::set<storm::expressions::Variable> const& metaVariables,
                                                             storm::logic::ComparisonType comparisonType, storm::RationalNumber value);
template Bdd<DdType::Sylvan> Bdd<DdType::Sylvan>::fromVector(DdManager<DdType::Sylvan> const& ddManager,
                                                             std::vector<storm::RationalFunction> const& explicitValues, storm::dd::Odd const& odd,
                                                             std::set<storm::expressions::Variable> const& metaVariables,
                                                             storm::logic::ComparisonType comparisonType, storm::RationalFunction value);

template Add<DdType::Sylvan, double> Bdd<DdType::Sylvan>::toAdd() const;
template Add<DdType::Sylvan, uint_fast64_t> Bdd<DdType::Sylvan>::toAdd() const;

template Add<DdType::Sylvan, storm::RationalNumber> Bdd<DdType::Sylvan>::toAdd() const;
template Add<DdType::Sylvan, storm::RationalFunction> Bdd<DdType::Sylvan>::toAdd() const;

template std::vector<double> Bdd<DdType::Sylvan>::filterExplicitVector(Odd const& odd, std::vector<double> const& values) const;
template std::vector<uint_fast64_t> Bdd<DdType::Sylvan>::filterExplicitVector(Odd const& odd, std::vector<uint_fast64_t> const& values) const;

template std::vector<storm::RationalNumber> Bdd<DdType::Sylvan>::filterExplicitVector(Odd const& odd, std::vector<storm::RationalNumber> const& values) const;
template std::vector<storm::RationalFunction> Bdd<DdType::Sylvan>::filterExplicitVector(Odd const& odd,
                                                                                        std::vector<storm::RationalFunction> const& values) const;

template Add<DdType::Sylvan, double> Bdd<DdType::Sylvan>::ite(Add<DdType::Sylvan, double> const& thenAdd, Add<DdType::Sylvan, double> const& elseAdd) const;
template Add<DdType::Sylvan, uint_fast64_t> Bdd<DdType::Sylvan>::ite(Add<DdType::Sylvan, uint_fast64_t> const& thenAdd,
                                                                     Add<DdType::Sylvan, uint_fast64_t> const& elseAdd) const;

template Add<DdType::Sylvan, storm::RationalNumber> Bdd<DdType::Sylvan>::ite(Add<DdType::Sylvan, storm::RationalNumber> const& thenAdd,
                                                                             Add<DdType::Sylvan, storm::RationalNumber> const& elseAdd) const;
template Add<DdType::Sylvan, storm::RationalFunction> Bdd<DdType::Sylvan>::ite(Add<DdType::Sylvan, storm::RationalFunction> const& thenAdd,
                                                                               Add<DdType::Sylvan, storm::RationalFunction> const& elseAdd) const;
}  // namespace dd
}  // namespace storm
