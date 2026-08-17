#pragma once

#include <filesystem>
#include <iosfwd>
#include <map>
#include <memory>
#include <string>

#include "storm/storage/BitVector.h"
#include "storm/storage/umb/model/FileTypes.h"
#include "storm/storage/umb/model/GenericVector.h"
#include "storm/storage/umb/model/ModelIndex.h"
#include "storm/utility/OptionalRef.h"

namespace storm::umb {
/*!
 * Represents a model in the UMB format. The data is stored in a way that closely reflects the file structure of the UMB format.
 * @see https://pmc-tools.github.io/umb/spec
 * @note We use reflexion to associate the data with the expected file names.
 */
class UmbModel {
   public:
    // index
    ModelIndex index;
    // states
    CSR stateToChoices;
    TO1<uint32_t> stateToPlayer;
    TO1<bool> stateIsInitial;
    TO1<bool> stateIsMarkovian;
    TO1<AnyValueType> stateToExitRate;
    // choices
    CSR choiceToBranches;
    // branches
    TO1<uint64_t> branchToTarget;
    TO1<AnyValueType> branchToProbability;
    // actions
    struct ActionLabels {
        TO1<uint32_t> values;
        CSR stringMapping;
        SEQ<char> strings;
        auto static constexpr FileNames = {"values.bin", "string-mapping.bin", "strings.bin"};
    };
    std::optional<ActionLabels> choiceActions, branchActions;
    // observations
    struct Observations {
        TO1<uint64_t> values;
        CSR distributionMapping;
        TO1<AnyValueType> probabilities;
        auto static constexpr FileNames = {"values.bin", "distribution-mapping.bin", "probabilities.bin"};
    };
    std::optional<Observations> stateObservations, branchObservations;
    // annotations
    template<typename Values>
    struct AppliesToEntity {
        std::optional<Values> states, choices, branches, observations, players;
        auto static constexpr FileNames = {"states/", "choices/", "branches/", "observations/", "players/"};
    };
    struct AnnotationValues {
        TO1<AnyValueType> values;
        CSR stringMapping;
        SEQ<char> strings;
        CSR distributionMapping;
        TO1<AnyValueType> probabilities;
        auto static constexpr FileNames = {"values.bin", "string-mapping.bin", "strings.bin", "distribution-mapping.bin", "probabilities.bin"};
    };
    using Annotation = std::map<std::string, AppliesToEntity<AnnotationValues>>;
    std::map<std::string, Annotation> annotations;
    // valuations
    struct Valuation {
        TO1<uint32_t> valuationToClass;
        SEQ<char> valuations;
        CSR stringMapping;
        SEQ<char> strings;
        auto static constexpr FileNames = {"valuation-to-class.bin", "valuations.bin", "string-mapping.bin", "strings.bin"};
    };
    using Valuations = AppliesToEntity<Valuation>;
    Valuations valuations;

    // Collects non-standard files (for custom extensions)
    // Note: As this field matches any file path, we need to make sure that it is the last field so that our importer tries the other fields first.
    std::map<std::filesystem::path, std::vector<char>> nonStandardFiles;

    auto static constexpr FileNames = {"index.json",
                                       "state-to-choices.bin",
                                       "state-to-player.bin",
                                       "state-is-initial.bin",
                                       "state-is-markovian.bin",
                                       "state-to-exit-rate.bin",
                                       "choice-to-branches.bin",
                                       "branch-to-target.bin",
                                       "branch-to-probability.bin",
                                       "actions/choices/",
                                       "actions/branches/",
                                       "observations/states/",
                                       "observations/branches/",
                                       "annotations/",
                                       "valuations/",
                                       ""};

    /*!
     * Retrieves a short string that can be used to refer to the model in user output.
     */
    std::string getShortModelInformation() const;

    /*!
     * Retrieves a string that describes the model in more detail.
     */
    std::string getModelInformation() const;

    /*!
     * Gets the annotation data with the given annotation type
     */
    storm::OptionalRef<Annotation> annotation(std::string const& annotationType, bool createIfMissing = false);
    storm::OptionalRef<Annotation const> annotation(std::string const& annotationType) const;
    storm::OptionalRef<Annotation> aps(bool createIfMissing = false);
    storm::OptionalRef<Annotation const> aps() const;
    storm::OptionalRef<Annotation> rewards(bool createIfMissing = false);
    storm::OptionalRef<Annotation const> rewards() const;

    /*!
     * Validates the given UMB model and writes potential errors to the given output stream.
     * @return true if the UMB model is valid.
     */
    bool validate(std::ostream& errors) const;

    /*!
     * Validates the UmbModel. If it is invalid, an exception is thrown.
     */
    void validateOrThrow() const;

    /*!
     * Encodes all rational values stored in the model into their UMB bit representation.
     */
    void encodeRationals();

    /*!
     * Decodes all rational values stored in the model from their UMB bit representation.
     */
    void decodeRationals();
};

}  // namespace storm::umb