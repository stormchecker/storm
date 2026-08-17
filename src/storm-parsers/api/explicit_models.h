#pragma once

#include <boost/optional.hpp>
#include <memory>
#include <string>
#include <type_traits>

#include "storm-parsers/parser/AutoParser.h"
#include "storm-parsers/parser/DirectEncodingParser.h"
#include "storm-parsers/parser/ImcaMarkovAutomatonParser.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/umb/Umb.h"
#include "storm/utility/macros.h"

namespace storm::api {

template<typename ValueType>
inline std::shared_ptr<storm::models::sparse::Model<ValueType>> buildExplicitModel(
    std::string const& transitionsFile, std::string const& labelingFile, boost::optional<std::string> const& stateRewardsFile,
    boost::optional<std::string> const& transitionRewardsFile, boost::optional<std::string> const& choiceLabelingFile,
    storm::parser::ExplicitModelParserOptions const& options = storm::parser::ExplicitModelParserOptions()) {
    if constexpr (std::is_same_v<ValueType, double>) {
        return storm::parser::AutoParser<ValueType, ValueType>::parseModel(transitionsFile, labelingFile, stateRewardsFile ? stateRewardsFile.get() : "",
                                                                           transitionRewardsFile ? transitionRewardsFile.get() : "",
                                                                           choiceLabelingFile ? choiceLabelingFile.get() : "", options);
    }
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Exact or parametric models with explicit input are not supported.");
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> buildExplicitDRNModel(
    std::string const& drnFile, storm::parser::DirectEncodingParserOptions const& options = storm::parser::DirectEncodingParserOptions()) {
    return storm::parser::parseDirectEncodingModel<ValueType>(drnFile, options);
}

inline std::shared_ptr<storm::models::ModelBase> buildExplicitDRNModel(
    std::string const& drnFile, storm::parser::DirectEncodingValueType valueType = storm::parser::DirectEncodingValueType::Default,
    storm::parser::DirectEncodingParserOptions const& options = storm::parser::DirectEncodingParserOptions()) {
    return storm::parser::parseDirectEncodingModel(drnFile, valueType, options);
}

inline std::shared_ptr<storm::models::ModelBase> buildExplicitUmbModel(std::string const& umbLocation, storm::umb::ImportOptions const& options = {}) {
    return storm::umb::buildModelFromUmb(umbLocation, options);
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> buildExplicitIMCAModel(
    std::string const& imcaFile, storm::parser::ExplicitModelParserOptions const& options = storm::parser::ExplicitModelParserOptions()) {
    if constexpr (std::is_same_v<ValueType, double>) {
        return storm::parser::ImcaMarkovAutomatonParser<ValueType>::parseImcaFile(imcaFile, options);
    }
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Exact models with direct encoding are not supported.");
}

}  // namespace storm::api
