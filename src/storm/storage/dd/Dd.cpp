#include "storm/storage/dd/Dd.h"

#include <algorithm>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {
template<DdType LibraryType>
Dd<LibraryType>::Dd(DdManager<LibraryType> const& ddManager, std::set<storm::expressions::Variable> const& containedMetaVariables)
    : ddManager(const_cast<DdManager<LibraryType>*>(&ddManager)), containedMetaVariables(containedMetaVariables) {
    // Intentionally left empty.
}

template<DdType LibraryType>
bool Dd<LibraryType>::containsMetaVariable(storm::expressions::Variable const& metaVariable) const {
    return containedMetaVariables.contains(metaVariable);
}

template<DdType LibraryType>
bool Dd<LibraryType>::containsMetaVariables(std::set<storm::expressions::Variable> const& metaVariables) const {
    return std::includes(containedMetaVariables.begin(), containedMetaVariables.end(), metaVariables.begin(), metaVariables.end());
}

template<DdType LibraryType>
std::set<storm::expressions::Variable> const& Dd<LibraryType>::getContainedMetaVariables() const {
    return this->containedMetaVariables;
}

template<DdType LibraryType>
std::set<storm::expressions::Variable>& Dd<LibraryType>::getContainedMetaVariables() {
    return this->containedMetaVariables;
}

template<DdType LibraryType>
DdManager<LibraryType>& Dd<LibraryType>::getDdManager() const {
    return *this->ddManager;
}

template<DdType LibraryType>
void Dd<LibraryType>::addMetaVariables(std::set<storm::expressions::Variable> const& metaVariables) {
    std::set<storm::expressions::Variable> result;
    std::set_union(containedMetaVariables.begin(), containedMetaVariables.end(), metaVariables.begin(), metaVariables.end(),
                   std::inserter(result, result.begin()));
    containedMetaVariables = std::move(result);
}

template<DdType LibraryType>
void Dd<LibraryType>::addMetaVariable(storm::expressions::Variable const& metaVariable) {
    this->getContainedMetaVariables().insert(metaVariable);
}

template<DdType LibraryType>
void Dd<LibraryType>::removeMetaVariable(storm::expressions::Variable const& metaVariable) {
    this->getContainedMetaVariables().erase(metaVariable);
}

template<DdType LibraryType>
void Dd<LibraryType>::removeMetaVariables(std::set<storm::expressions::Variable> const& metaVariables) {
    std::set<storm::expressions::Variable> result;
    std::set_difference(containedMetaVariables.begin(), containedMetaVariables.end(), metaVariables.begin(), metaVariables.end(),
                        std::inserter(result, result.begin()));
    containedMetaVariables = std::move(result);
}

template<DdType LibraryType>
std::vector<uint_fast64_t> Dd<LibraryType>::getSortedVariableIndices() const {
    return this->getDdManager().getSortedVariableIndices(this->getContainedMetaVariables());
}

template<DdType LibraryType>
std::set<storm::expressions::Variable> Dd<LibraryType>::joinMetaVariables(storm::dd::Dd<LibraryType> const& first, storm::dd::Dd<LibraryType> const& second) {
    std::set<storm::expressions::Variable> metaVariables;
    std::set_union(first.getContainedMetaVariables().begin(), first.getContainedMetaVariables().end(), second.getContainedMetaVariables().begin(),
                   second.getContainedMetaVariables().end(), std::inserter(metaVariables, metaVariables.begin()));
    return metaVariables;
}

template<DdType LibraryType>
std::set<storm::expressions::Variable> Dd<LibraryType>::subtractMetaVariables(storm::dd::Dd<LibraryType> const& first,
                                                                              storm::dd::Dd<LibraryType> const& second) {
    bool includesAllMetaVariables = std::includes(first.getContainedMetaVariables().begin(), first.getContainedMetaVariables().end(),
                                                  second.getContainedMetaVariables().begin(), second.getContainedMetaVariables().end());
    STORM_LOG_THROW(includesAllMetaVariables, storm::exceptions::InvalidArgumentException, "Cannot subtract meta variables that are not contained in the DD.");
    std::set<storm::expressions::Variable> metaVariables;
    std::set_difference(first.getContainedMetaVariables().begin(), first.getContainedMetaVariables().end(), second.getContainedMetaVariables().begin(),
                        second.getContainedMetaVariables().end(), std::inserter(metaVariables, metaVariables.begin()));
    return metaVariables;
}

template class Dd<storm::dd::DdType::CUDD>;
template class Dd<storm::dd::DdType::Sylvan>;
}  // namespace dd
}  // namespace storm
