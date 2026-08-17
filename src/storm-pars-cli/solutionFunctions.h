#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/models/sparse/ModelForward.h"
#include "storm/storage/dd/DdType.h"

namespace storm::jani {
class Property;
}
namespace storm::logic {
class Formula;
}
namespace storm::modelchecker {
class CheckResult;
}
namespace storm::models::symbolic {
template<storm::dd::DdType DdType, typename ValueType>
class Model;
}
namespace storm::cli {
struct SymbolicInput;
}

namespace storm::pars {

template<typename ValueType>
void verifyProperties(
    std::vector<storm::jani::Property> const& properties,
    std::function<std::unique_ptr<storm::modelchecker::CheckResult>(std::shared_ptr<storm::logic::Formula const> const& formula)> const& verificationCallback,
    std::function<void(std::unique_ptr<storm::modelchecker::CheckResult> const&)> const& postprocessingCallback);

template<typename ValueType>
void computeSolutionFunctionsWithSparseEngine(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, storm::cli::SymbolicInput const& input);

template<storm::dd::DdType DdType, typename ValueType>
void computeSolutionFunctionsWithSymbolicEngine(std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const& model,
                                                storm::cli::SymbolicInput const& input);

extern template void verifyProperties<storm::RationalFunction>(
    std::vector<storm::jani::Property> const&,
    std::function<std::unique_ptr<storm::modelchecker::CheckResult>(std::shared_ptr<storm::logic::Formula const> const&)> const&,
    std::function<void(std::unique_ptr<storm::modelchecker::CheckResult> const&)> const&);

extern template void computeSolutionFunctionsWithSparseEngine<storm::RationalFunction>(
    std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> const&, storm::cli::SymbolicInput const&);

extern template void computeSolutionFunctionsWithSymbolicEngine<storm::dd::DdType::Sylvan, storm::RationalFunction>(
    std::shared_ptr<storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalFunction>> const&, storm::cli::SymbolicInput const&);

}  // namespace storm::pars
