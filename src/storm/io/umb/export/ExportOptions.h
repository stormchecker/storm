#pragma once

#include <optional>
#include <set>
#include <string>

#include "storm/io/CompressionMode.h"

namespace storm::umb {
struct ExportOptions {
    /*!
     * The type that is used for all kinds of values. Default picks the value type of the input model.
     * @note UMB allows to use different value types for e.g. probabilities, rates, and rewards.
     *       We don't support that for now in favour of a cleaner option interface.
     */
    enum class ValueType { Default, Rational, Double, DoubleInterval, RationalInterval } valueType{ValueType::Default};

    /*!
     * Whether export of choice origins is enabled.
     * @note If both, choice origins and choice labeling are present and enabled, choice origins will be used.
     */
    bool allowChoiceOriginsAsActions{false};

    /*!
     * Whether export of choice origins is enabled.
     * @note If both, choice origins and choice labelings are present and enabled, choice origins will be used.
     */
    bool allowChoiceLabelingAsActions{true};

    /*!
     * The type of compression used for the exported UMB model.
     */
    storm::io::CompressionMode compression{storm::io::CompressionMode::Default};

    /*!
     * Whether to canonicize POMDPs before export.
     */
    bool canonicizePomdp{true};
};
}  // namespace storm::umb
