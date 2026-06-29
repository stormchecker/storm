#pragma once

namespace storm::umb {
struct ImportOptions {
    /*!
     * The type that is used for all kinds of values.
     * Default picks the value type of the input model.
     * If the input model is an interval model, the value type will be the correspdonding interval type.
     * @note UMB allows to use different value types for e.g. probabilities, rates, and rewards.
     *       We don't support that for now in favour of a cleaner option interface.
     */
    enum class ValueType { Default, Rational, Double } valueType{ValueType::Default};

    /*!
     * Controls building of choice labelings.
     * A choice labelling will only be built if this is set to true *and* the umb model contains appropriate information.
     */
    bool buildChoiceLabeling{true};

    /*!
     * Controls building of state valuations.
     * State valuations will only be built if this is set to true *and* the umb model contains appropriate information.
     */
    bool buildStateValuations{true};

    /*!
     * Controls building of observation valuations.
     * Observation valuations will only be built if this is set to true *and* the umb model contains appropriate information.
     */
    bool buildObservationValuations{true};
};
}  // namespace storm::umb
