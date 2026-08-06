#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/JsonSerializationAdapter.h"
#include "storm/storage/umb/model/Type.h"
#include "storm/storage/valuations/ValuationDescription.h"
#include "storm/utility/OptionalRef.h"

namespace storm::umb {

struct ModelIndex {
    uint64_t formatVersion{1}, formatRevision{0};

    struct ModelData {
        std::optional<std::string> name, version;
        std::optional<std::vector<std::string>> authors;
        std::optional<std::string> description, comment, doi, url;
        static auto constexpr JsonKeys = {"name", "version", "authors", "description", "comment", "doi", "url"};
        using JsonSerialization = storm::JsonSerialization;
    };
    std::optional<ModelData> modelData;

    struct FileData {
        std::optional<std::string> tool, toolVersion;
        std::optional<uint64_t> creationDate;
        std::optional<storm::json<storm::RationalNumber>> parameters;
        auto static constexpr JsonKeys = {"tool", "tool-version", "creation-date", "parameters"};
        using JsonSerialization = storm::JsonSerialization;

        std::string creationDateAsString() const;
        void setCreationDateToNow();
    };
    std::optional<FileData> fileData;

    struct TransitionSystem {
        enum class Time { Discrete, Stochastic, UrgentStochastic };
        struct TimeDeclaration {
            using Values = Time;
            auto static constexpr Keys = {"discrete", "stochastic", "urgent-stochastic"};
        };
        storm::SerializedEnum<TimeDeclaration> time;

        auto static constexpr InvalidNumber = std::numeric_limits<uint64_t>::max();  // initial value for counts
        uint64_t numPlayers{InvalidNumber}, numStates{InvalidNumber}, numInitialStates{InvalidNumber}, numChoices{InvalidNumber},
            numChoiceActions{InvalidNumber}, numBranches{InvalidNumber}, numBranchActions{InvalidNumber}, numObservations{InvalidNumber};

        enum class ObservationsApplyTo { States, Branches };
        struct ObservationsApplyToDeclaration {
            using Values = ObservationsApplyTo;
            auto static constexpr Keys = {"states", "branches"};
        };
        std::optional<storm::SerializedEnum<ObservationsApplyToDeclaration>> observationsApplyTo;

        std::optional<SizedType> branchProbabilityType, exitRateType, observationProbabilityType;

        std::optional<std::vector<std::string>> playerNames;

        auto static constexpr JsonKeys = {"time",
                                          "#players",
                                          "#states",
                                          "#initial-states",
                                          "#choices",
                                          "#choice-actions",
                                          "#branches",
                                          "#branch-actions",
                                          "#observations",
                                          "observations-apply-to",
                                          "branch-probability-type",
                                          "exit-rate-type",
                                          "observation-probability-type",
                                          "player-names"};
        using JsonSerialization = storm::JsonSerialization;
    } transitionSystem;

    struct Annotation {
        std::optional<std::string> alias, description;
        enum class AppliesTo { States, Choices, Branches, Observations, Players };
        struct AppliesToDeclaration {
            using Values = AppliesTo;
            auto static constexpr Keys = {"states", "choices", "branches", "observations", "players"};
        };
        std::vector<storm::SerializedEnum<AppliesToDeclaration>> appliesTo;
        SizedType type;
        std::optional<int64_t> lower, upper;
        std::optional<uint64_t> numStrings;
        std::optional<SizedType> probabilityType;
        std::optional<uint64_t> numProbabilities;
        auto static constexpr JsonKeys = {"alias", "description", "applies-to", "type", "lower", "upper", "#strings", "probability-type", "#probabilities"};
        using JsonSerialization = storm::JsonSerialization;

        /*!
         * Takes an alias (which can be an arbitrary string) and converts it to a valid identifier in [0-9a-z_-]+
         * @param alias
         * @return
         */
        static std::string getValidIdentifierFromAlias(std::string const& alias);

        bool appliesToStates() const;
        bool appliesToChoices() const;
        bool appliesToBranches() const;
        bool appliesToObservations() const;
        bool appliesToPlayers() const;
    };
    using AnnotationMap = std::map<std::string, Annotation>;
    std::optional<std::map<std::string, AnnotationMap>> annotations;

    struct Valuations {
        std::optional<storm::storage::sparse::ValuationDescription> states, choices, branches, observations, players;
        auto static constexpr JsonKeys = {"states", "choices", "branches", "observations", "players"};
        using JsonSerialization = storm::JsonSerialization;
    };
    std::optional<Valuations> valuations;

    auto static constexpr JsonKeys = {"format-version", "format-revision", "model-data", "file-data", "transition-system", "annotations", "valuations"};
    using JsonSerialization = storm::JsonSerialization;

    // conveniently access aps, rewards, or a custom annotation (return a NullRef iff the annotation is not present)
    storm::OptionalRef<AnnotationMap> aps(bool createIfMissing = false);
    storm::OptionalRef<AnnotationMap const> aps() const;
    storm::OptionalRef<AnnotationMap> rewards(bool createIfMissing = false);
    storm::OptionalRef<AnnotationMap const> rewards() const;
    storm::OptionalRef<AnnotationMap> annotation(std::string const& annotationsType, bool createIfMissing = false);
    storm::OptionalRef<AnnotationMap const> annotation(std::string const& annotationsType) const;

    /*!
     *  Finds a name of the annotation (i.e. key in the AnnotationMap) by
     *  - first searching if there is an alias matching the given id
     *  - and if no alias exist checks if the id is a key in the AnnotationMap
     */
    std::optional<std::string> findAPName(std::string const& id) const;
    std::optional<std::string> findRewardName(std::string const& id) const;
    std::optional<std::string> findAnnotationName(std::string const& annotationsType, std::string const& id) const;
};

}  // namespace storm::umb