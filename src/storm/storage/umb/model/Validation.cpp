#include "storm/storage/umb/model/Validation.h"

#include <sstream>
#include <string_view>

#include "storm/storage/umb/model/UmbModel.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/UnexpectedException.h"
#include "storm/exceptions/WrongFormatException.h"

namespace storm::umb {

namespace validation {
bool validateCsr(auto const& csr, std::string_view const name, uint64_t numMappedElements, std::optional<uint64_t> expectedlastEntry, std::ostream& err) {
    std::stringstream err_reason;
    if (csr) {
        // Check if csr has expected length and the form {0, ..., expectedlastEntry}
        if (csr->size() != numMappedElements + 1) {
            err_reason << "CSR has unexpected size: " << csr->size() << " != " << (numMappedElements + 1) << ".";
        }
        if (csr.value()[0] != 0) {
            err_reason << "CSR has unexpected first entry: " << csr.value()[0] << " != 0" << ".";
        }
        if (expectedlastEntry.has_value() && csr.value()[numMappedElements] != expectedlastEntry.value()) {
            err_reason << "CSR has unexpected last entry: " << csr.value()[numMappedElements] << " != " << expectedlastEntry.value() << ".";
        }
    } else if (expectedlastEntry.has_value() && numMappedElements != expectedlastEntry.value()) {  // we assume a 1:1 mapping
        err_reason << "CSR is not given and the default 1:1 mapping {0, ... ," << numMappedElements << "} does not match. Expected the mapping to end with '"
                   << expectedlastEntry.value() << "'" << ".";
    }
    if (!err_reason.view().empty()) {
        err << "Validation error in CSR mapping '" << name << "':\n\t" << err_reason.str() << "\n";
        return false;
    }
    return true;
}

bool validateTypeDeclaration(storm::umb::SizedType const& type, bool requireStandardSize, std::ostream& err) {
    using enum storm::umb::Type;
    uint64_t const size = type.bitSize();
    if (size == 0) {
        err << "Type declaration " << type.toString() << " has size 0.\n";
        return false;
    }
    uint64_t const defaultSize = defaultBitSize(type.type);
    bool sizeError = false;
    switch (type.type) {
        case Double:
        case DoubleInterval:
        case String:
            // types that always must be their default size
            sizeError = size != defaultSize;
            break;
        case Bool:
        case Int:
        case Uint:
        case IntInterval:
        case UintInterval:
            // types that occasionally must be their default size
            sizeError = requireStandardSize && (size != defaultSize);
            break;
            // types that occasionally must be a multiple of their default size
        case Rational:
        case RationalInterval:
            // types that occasionally must be their default size
            sizeError = requireStandardSize && ((size % defaultSize) != 0);
    }
    if (isIntervalType(type.type)) {
        // interval type sizes must be multiples of four or two
        sizeError = sizeError || (size % (type.type == RationalInterval ? 4 : 2) != 0);
    }
    if (sizeError) {
        err << "Type declaration " << type.toString() << " has invalid bit size: " << size << " (default size is " << defaultSize << ").\n";
        return false;
    }
    return true;
}

bool vectorMatchesType(storm::umb::GenericVector const& vector, storm::umb::SizedType const& type) {
    if (!vector.hasValue()) {
        return true;  // no values are given so nothing wrong with the vector.
    }

    using enum storm::umb::Type;
    switch (type.type) {
        case Bool:
            return vector.isType<bool>();
        case Int:
        case IntInterval:
            return vector.isType<int64_t>();
        case Uint:
        case UintInterval:
            return vector.isType<uint64_t>();
        case Double:
            return vector.isType<double>();
        case DoubleInterval:
            return vector.isType<double>() || vector.isType<storm::Interval>();
        case Rational:
            return vector.isType<storm::RationalNumber>() || vector.isType<uint64_t>();  // rationals might be encoded as uint64_t
        case RationalInterval:
            return vector.isType<storm::RationalInterval>() || vector.isType<uint64_t>();  // rationals might be encoded as uint64_t
        case String:
            return vector.isType<uint64_t>();  // strings are encoded by their indices
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unhandled type.");
}
}  // namespace validation

bool validate(storm::umb::UmbModel const& umbModel, std::ostream& err) {
    ////////////
    // Index
    ////////////
    auto const& index = umbModel.index;
    auto const& tsIndex = index.transitionSystem;
    bool isValid = true;

    // validate counts
    auto checkNum = [&err](uint64_t num, auto&& name, uint64_t lowerBound = 0) {
        if (num == storm::umb::ModelIndex::TransitionSystem::InvalidNumber) {
            err << "Number of " << name << " is not set.\n";
            return false;
        } else if (num < lowerBound) {
            err << "Number of " << name << " is " << num << " which is below lower bound " << lowerBound << ".\n";
            return false;
        }
        return true;
    };
    isValid &= checkNum(tsIndex.numPlayers, "players");
    isValid &= checkNum(tsIndex.numStates, "states", 1u);
    isValid &= checkNum(tsIndex.numInitialStates, "initial-states");
    isValid &= checkNum(tsIndex.numChoices, "choices");
    isValid &= checkNum(tsIndex.numChoiceActions, "choice-actions");
    isValid &= checkNum(tsIndex.numBranches, "branches");
    isValid &= checkNum(tsIndex.numBranchActions, "branch-actions");
    isValid &= checkNum(tsIndex.numObservations, "observations");

    // validate types
    if (tsIndex.branchProbabilityType) {
        isValid &= validation::validateTypeDeclaration(tsIndex.branchProbabilityType.value(), true, err);
        if (!isContinuousNumericType(tsIndex.branchProbabilityType->type)) {
            err << "Branch probability type must be a continuous numeric type.\n";
            isValid = false;
        }
    }
    if (tsIndex.exitRateType) {
        isValid &= validation::validateTypeDeclaration(tsIndex.exitRateType.value(), true, err);
        if (!isContinuousNumericType(tsIndex.exitRateType->type)) {
            err << "Exit rate type must be a continuous numeric type.\n";
            isValid = false;
        }
    }
    if (tsIndex.observationProbabilityType) {
        isValid &= validation::validateTypeDeclaration(tsIndex.observationProbabilityType.value(), true, err);
        if (!isContinuousNumericType(tsIndex.observationProbabilityType->type)) {
            err << "Observation probability type must be a continuous numeric type.\n";
            isValid = false;
        }
    }

    if (bool const hasObservations = tsIndex.numObservations > 0; hasObservations != tsIndex.observationsApplyTo.has_value()) {
        err << "observations-apply-to is " << (tsIndex.observationsApplyTo.has_value() ? "set" : "not set") << " although the number of observations is "
            << tsIndex.numObservations << ".\n";
        isValid = false;
    }

    if (index.annotations) {
        for (auto const& [annotationType, annotationMap] : index.annotations.value()) {
            for (auto const& [name, annotation] : annotationMap) {
                isValid &= validation::validateTypeDeclaration(annotation.type, true, err);
                if (annotation.probabilityType) {
                    isValid &= validation::validateTypeDeclaration(annotation.probabilityType.value(), true, err);
                    if (!isContinuousNumericType(annotation.probabilityType->type)) {
                        err << "Probability type for annotation '" << name << "' must be a continuous numeric type.\n";
                        isValid = false;
                    }
                }
                if (annotationType == "aps") {
                    if (!isBooleanType(annotation.type.type)) {
                        err << "Atomic proposition annotation '" << name << "' must be of boolean type.\n";
                        isValid = false;
                    }
                } else if (annotationType == "rewards") {
                    if (!isNumericType(annotation.type.type)) {
                        err << "Reward annotation '" << name << "' must have numeric type.\n";
                        isValid = false;
                    }
                }
            }
        }
    }

    if (index.valuations) {
        boost::pfr::for_each_field(index.valuations.value(), [&isValid, &err](auto const& description) {
            if (!description.has_value()) {
                return;
            }
            if (description->classes.empty()) {
                err << "A valuation description has no classes.\n";
                isValid = false;
            }
            for (auto const& descr : description->classes) {
                for (auto const& var : descr.variables) {
                    if (std::holds_alternative<ValuationClassDescription::Variable>(var)) {
                        auto const& variable = std::get<ValuationClassDescription::Variable>(var);
                        if (variable.name.empty()) {
                            err << "A valuation description has a variable with an empty name.\n";
                            isValid = false;
                        }
                        isValid &= validation::validateTypeDeclaration(variable.type, false, err);
                    }
                }
                if (descr.sizeInBits() % 8 != 0) {
                    err << "A valuation description has size " << descr.sizeInBits() << " bits which is not a multiple of 8.\n";
                    isValid = false;
                }
            }
        });
    }

    ////////////
    // Files
    ////////////

    // Validate the expected size of TO1<..> and SEQ<..> vectors.
    auto isExpectedBoolVectorSize = [](uint64_t const actual, uint64_t const expected) {
        return actual == expected || actual == ((expected + 63) / 64) * 64;  // size might be rounded up to the nearest multiple of 64
    };
    auto isExpectedTypedVectorSize = [](uint64_t const actual, storm::umb::SizedType const& type, uint64_t const expected) {
        // Either the size matches exactly or we have a type-dependend encoding where other sizes are possible, too.
        if (actual == expected) {
            return true;
        } else {
            using enum storm::umb::Type;
            switch (type.type) {
                case Bool:
                    // size might be rounded up to the nearest multiple of 64
                    return actual == ((expected + 63) / 64) * 64;
                case IntInterval:
                case UintInterval:
                case DoubleInterval:
                    // vector might be encoded by storing lower and upper separately
                    return actual == 2 * expected;
                case Rational:
                    // might be encoded as uint64
                    return actual == expected * type.bitSize() / 64;
                case RationalInterval:
                    // might be encoded as uint64
                    return actual == (expected * type.bitSize() / 64);
                default:
                    // for all other types, the size must match exactly which we have checked above already
                    return false;
            }
        }
    };

    // States
    isValid &= validation::validateCsr(umbModel.stateToChoices, "state-to-choice", tsIndex.numStates, tsIndex.numChoices, err);
    if (umbModel.stateToPlayer.has_value()) {
        if (tsIndex.numPlayers == 0) {
            err << "state-to-player mapping is given but the model has no players.\n";
            isValid = false;
        } else if (umbModel.stateToPlayer->size() != tsIndex.numStates) {
            err << "state-to-player mapping has invalid size: " << umbModel.stateToPlayer->size() << " != #states=" << tsIndex.numStates << ".\n";
            isValid = false;
        }
    }
    if (umbModel.stateIsInitial.has_value() && !isExpectedBoolVectorSize(umbModel.stateIsInitial->size(), tsIndex.numStates)) {
        err << "state-is-initial has invalid size: " << umbModel.stateIsInitial->size() << " != #states=" << tsIndex.numStates << ".\n";
        isValid = false;
    }
    if (umbModel.stateIsMarkovian.has_value()) {
        if (tsIndex.time != ModelIndex::TransitionSystem::Time::UrgentStochastic) {
            err << "state-is-markovian is given but the model does not have urgent-stochastic time.\n";
            isValid = false;
        } else if (!isExpectedBoolVectorSize(umbModel.stateIsMarkovian->size(), tsIndex.numStates)) {
            err << "state-is-markovian has invalid size: " << umbModel.stateIsMarkovian->size() << " != #states=" << tsIndex.numStates << ".\n";
            isValid = false;
        }
    }
    if (umbModel.stateToExitRate.hasValue()) {
        if (tsIndex.time == ModelIndex::TransitionSystem::Time::Discrete) {
            err << "state-to-exit-rate mapping is given but the model has discrete time.\n";
            isValid = false;
        }
        if (!tsIndex.exitRateType.has_value()) {
            err << "state-to-exit-rate mapping is given but exit rate type is not declared.\n";
            isValid = false;
        } else if (!validation::vectorMatchesType(umbModel.stateToExitRate, tsIndex.exitRateType.value())) {
            err << "state-to-exit-rate mapping has values that do not match the declared exit rate type " << tsIndex.exitRateType->toString() << ".\n";
            isValid = false;
        } else if (!isExpectedTypedVectorSize(umbModel.stateToExitRate.size(), tsIndex.exitRateType.value(), tsIndex.numStates)) {
            err << "state-to-exit-rate mapping has invalid size: " << umbModel.stateToExitRate.size() << " != #states=" << tsIndex.numStates << ".\n";
            isValid = false;
        }
    }

    // Choices
    isValid &= validation::validateCsr(umbModel.choiceToBranches, "choice-to-branch", tsIndex.numChoices, tsIndex.numBranches, err);

    // Branches
    if (umbModel.branchToTarget.has_value() && umbModel.branchToTarget->size() != tsIndex.numBranches) {
        err << "branch-to-target mapping has invalid size: " << umbModel.branchToTarget->size() << " != #branches=" << tsIndex.numBranches << ".\n";
        isValid = false;
    }
    if (umbModel.branchToProbability.hasValue()) {
        if (!tsIndex.branchProbabilityType.has_value()) {
            err << "branch-to-probability mapping is given but branch probability type is not declared.\n";
            isValid = false;
        } else if (!validation::vectorMatchesType(umbModel.branchToProbability, tsIndex.branchProbabilityType.value())) {
            err << "branch-to-probability mapping has values that do not match the declared branch probability type "
                << tsIndex.branchProbabilityType->toString() << ".\n";
            isValid = false;
        } else if (!isExpectedTypedVectorSize(umbModel.branchToProbability.size(), tsIndex.branchProbabilityType.value(), tsIndex.numBranches)) {
            err << "branch-to-probability mapping has invalid size: " << umbModel.branchToProbability.size() << " != #branches=" << tsIndex.numBranches
                << ".\n";
            isValid = false;
        }
    }

    // Action labels
    auto validateActionLabels = [&isValid, &err](storm::umb::UmbModel::ActionLabels const& al, auto const& entityName, uint64_t const numEntity,
                                                 uint64_t const numEntityActions) {
        if (al.values.has_value()) {
            if (numEntityActions == 0) {
                err << "actions/" << entityName << "/values given but the number of " << entityName << "-actions is zero.\n";
                isValid = false;
            } else if (al.values->size() != numEntity) {
                err << "actions/" << entityName << "/values has invalid size: " << al.values->size() << " != #" << entityName << "=" << numEntity << ".\n";
                isValid = false;
            }
        }
        if (al.stringMapping.has_value() && numEntityActions == 0) {
            err << "actions/" << entityName << "/string-mapping given but the number of " << entityName << "-actions is zero.\n";
            isValid = false;
        }
        if (al.stringMapping.has_value() != al.strings.has_value()) {
            err << "actions/" << entityName << "/string-mapping is " << (al.stringMapping.has_value() ? "set" : "not set") << " although actions/" << entityName
                << "/strings is " << (al.strings.has_value() ? "set" : "not set") << ".\n";
            isValid = false;
        }
        if (al.stringMapping.has_value()) {
            isValid &=
                validation::validateCsr(al.stringMapping, std::string("actions/") + entityName + "/string-mapping", numEntityActions, al.strings->size(), err);
        }
    };
    if (umbModel.choiceActions.has_value()) {
        validateActionLabels(umbModel.choiceActions.value(), "choices", tsIndex.numChoices, tsIndex.numChoiceActions);
    }
    if (umbModel.branchActions.has_value()) {
        validateActionLabels(umbModel.branchActions.value(), "branches", tsIndex.numBranches, tsIndex.numBranchActions);
    }

    // Observations
    auto validateObservations = [&isValid, &err, &isExpectedTypedVectorSize](storm::umb::UmbModel::Observations const& obs, auto const& entityName,
                                                                             uint64_t const numEntity, uint64_t const numObservations,
                                                                             std::optional<storm::umb::SizedType> const& obsProbType) {
        if (obs.values.has_value()) {
            if (numObservations == 0) {
                err << "observations/" << entityName << "/values given but the number of observations is zero.\n";
                isValid = false;
            } else if (obs.probabilities.hasValue() || obs.values->size() != numEntity) {
                err << "observations/" << entityName << "/values has invalid size: " << obs.values->size() << " != #" << entityName
                    << "-observation-values=" << numEntity << ".\n";
                isValid = false;
            }
            isValid &= validation::validateCsr(obs.distributionMapping, std::string("observations/") + entityName + "/distribution-mapping", numEntity,
                                               obs.values->size(), err);
            if (obs.probabilities.hasValue()) {
                if (!obsProbType.has_value()) {
                    err << "observations/" << entityName << "/probabilities given but observation probability type is not declared.\n";
                    isValid = false;
                } else if (!validation::vectorMatchesType(obs.probabilities, obsProbType.value())) {
                    err << "observations/" << entityName << "/probabilities has values that do not match the declared observation probability type "
                        << obsProbType->toString() << ".\n";
                    isValid = false;
                } else if (!isExpectedTypedVectorSize(obs.probabilities.size(), obsProbType.value(), numObservations)) {
                    err << "observations/" << entityName << "/probabilities has invalid size: " << obs.probabilities.size()
                        << " != #observations=" << numObservations << ".\n";
                    isValid = false;
                }
            }
        }
    };
    if (umbModel.stateObservations.has_value()) {
        validateObservations(umbModel.stateObservations.value(), "states", tsIndex.numStates, tsIndex.numObservations, tsIndex.observationProbabilityType);
    }
    if (umbModel.branchObservations.has_value()) {
        validateObservations(umbModel.branchObservations.value(), "branches", tsIndex.numBranches, tsIndex.numObservations, tsIndex.observationProbabilityType);
    }

    // Annotations
    auto validateAnnotationValues = [&isValid, &err, &isExpectedTypedVectorSize](storm::umb::UmbModel::AnnotationValues const& av, auto const& group,
                                                                                 auto const& id, auto const& entityName, uint64_t const numEntity,
                                                                                 storm::umb::ModelIndex::Annotation const& ai) {
        auto const context = std::string("annotations/") + group + "/" + id + "/" + entityName;
        uint64_t const numAnnotationValues = ai.numProbabilities.value_or(numEntity);
        if (av.values.hasValue()) {
            if (!validation::vectorMatchesType(av.values, ai.type)) {
                err << context << "/values has values that do not match the declared annotation type " << ai.type.toString() << ".\n";
                isValid = false;
            } else if (!isExpectedTypedVectorSize(av.values.size(), ai.type, numAnnotationValues)) {
                err << context << "/values has invalid size: " << av.values.size() << " != #" << entityName << "-annotation-values=" << numAnnotationValues;
                isValid = false;
            }
        }
        if (isStringType(ai.type.type) != av.stringMapping.has_value()) {
            err << context << "/string-mapping is " << (av.stringMapping.has_value() ? "set" : "not set") << " although the annotation type is "
                << ai.type.toString() << ".\n";
            isValid = false;
        }
        if (av.stringMapping.has_value() && ai.numStrings.value_or(0) == 0) {
            err << context << "/string-mapping given but the number of strings is zero.\n";
            isValid = false;
        }
        if (av.stringMapping.has_value() != av.strings.has_value()) {
            err << context << "/string-mapping is " << (av.stringMapping.has_value() ? "set" : "not set") << " although " << context << "/strings is "
                << (av.strings.has_value() ? "set" : "not set") << ".\n";
            isValid = false;
        }
        if (av.stringMapping.has_value()) {
            isValid &= validation::validateCsr(av.stringMapping, context + "/string-mapping", ai.numStrings.value(), av.strings->size(), err);
        }
        isValid &= validation::validateCsr(av.distributionMapping, context + "/distribution-mapping", numEntity, numAnnotationValues, err);
        if (av.probabilities.hasValue()) {
            if (!ai.probabilityType.has_value()) {
                err << context << "/probabilities given but annotation probability type is not declared.\n";
                isValid = false;
            } else if (!validation::vectorMatchesType(av.probabilities, ai.probabilityType.value())) {
                err << context << "/probabilities has values that do not match the declared annotation probability type " << ai.probabilityType->toString()
                    << ".\n";
                isValid = false;
            } else if (!isExpectedTypedVectorSize(av.probabilities.size(), ai.probabilityType.value(), numAnnotationValues)) {
                err << context << "/probabilities has invalid size: " << av.probabilities.size() << " != #" << entityName
                    << "-annotation-values=" << numAnnotationValues << ".\n";
                isValid = false;
            }
        }
    };
    for (auto const& [annotationType, annotationMap] : umbModel.annotations) {
        if (!umbModel.index.annotations.has_value() || !umbModel.index.annotations->contains(annotationType)) {
            err << "Annotation '" << annotationType << "' is given but not declared in the index.\n";
            isValid = false;
            continue;
        }
        for (auto const& [annotationId, annotationValues] : annotationMap) {
            if (!umbModel.index.annotations->at(annotationType).contains(annotationId)) {
                err << "Annotation '" << annotationId << "' of type '" << annotationType << "' is given but not declared in the index.\n";
                isValid = false;
                continue;
            }
            auto const& annotationIndex = index.annotations->at(annotationType).at(annotationId);
            if (annotationValues.states.has_value()) {
                if (!annotationIndex.appliesToStates()) {
                    err << "Annotation '" << annotationId << "' of type '" << annotationType
                        << "' has states values but does not apply to states according to the index.\n";
                    isValid = false;
                }
                validateAnnotationValues(annotationValues.states.value(), annotationType, annotationId, "states", tsIndex.numStates, annotationIndex);
            }
            if (annotationValues.choices.has_value()) {
                if (!annotationIndex.appliesToChoices()) {
                    err << "Annotation '" << annotationId << "' of type '" << annotationType
                        << "' has choices values but does not apply to choices according to the index.\n";
                    isValid = false;
                }
                validateAnnotationValues(annotationValues.choices.value(), annotationType, annotationId, "choices", tsIndex.numChoices, annotationIndex);
            }
            if (annotationValues.branches.has_value()) {
                if (!annotationIndex.appliesToBranches()) {
                    err << "Annotation '" << annotationId << "' of type '" << annotationType
                        << "' has branches values but does not apply to branches according to the index.\n";
                    isValid = false;
                }
                validateAnnotationValues(annotationValues.branches.value(), annotationType, annotationId, "branches", tsIndex.numBranches, annotationIndex);
            }
            if (annotationValues.observations.has_value()) {
                if (!annotationIndex.appliesToObservations()) {
                    err << "Annotation '" << annotationId << "' of type '" << annotationType
                        << "' has observations values but does not apply to observations according to the index.\n";
                    isValid = false;
                }
                validateAnnotationValues(annotationValues.observations.value(), annotationType, annotationId, "observations", tsIndex.numObservations,
                                         annotationIndex);
            }
            if (annotationValues.players.has_value()) {
                if (!annotationIndex.appliesToPlayers()) {
                    err << "Annotation '" << annotationId << "' of type '" << annotationType
                        << "' has players values but does not apply to players according to the index.\n";
                    isValid = false;
                }
                validateAnnotationValues(annotationValues.players.value(), annotationType, annotationId, "players", tsIndex.numPlayers, annotationIndex);
            }
        }
    }

    // Valuations
    auto validateValuation = [&isValid, &err](storm::umb::UmbModel::Valuation const& v, auto const& entityName, uint64_t const numEntity,
                                              storm::umb::ValuationDescription const& descr) {
        auto const context = std::string("valuations/") + entityName;
        if (v.valuationToClass.has_value() && v.valuationToClass->size() != numEntity) {
            err << context << "/valuation-to-class has invalid size: " << v.valuationToClass->size() << " != #" << entityName << "=" << numEntity << ".\n";
            isValid = false;
        }
        if ((!v.valuationToClass.has_value() && !descr.classes.empty()) || descr.classes.size() == 1) {
            // common case: all entities have the same valuation class
            auto const& classDescr = descr.classes.front();
            if (v.valuations.has_value() && v.valuations->size() * 8 < classDescr.sizeInBits() * numEntity) {
                err << context << "/valuations has invalid size: " << v.valuations->size() << " != size of one valuation class (" << classDescr.sizeInBits()
                    << " bits) * 8 * #entities=" << (classDescr.sizeInBits() * 8 * numEntity) << ".\n";
                isValid = false;
            }
        }
        if (v.stringMapping.has_value() && descr.numStrings.value_or(0) == 0) {
            err << context << "/string-mapping given but the number of strings is zero.\n";
            isValid = false;
        }
        if (v.stringMapping.has_value() != v.strings.has_value()) {
            err << context << "/string-mapping is " << (v.stringMapping.has_value() ? "set" : "not set") << " although " << context << "/strings is "
                << (v.strings.has_value() ? "set" : "not set") << ".\n";
            isValid = false;
        }
        if (v.stringMapping.has_value()) {
            isValid &= validation::validateCsr(v.stringMapping, context + "/string-mapping", descr.numStrings.value(), v.strings->size(), err);
        }
    };
    if (umbModel.valuations.states.has_value()) {
        if (!umbModel.index.valuations.has_value() || !umbModel.index.valuations->states.has_value()) {
            err << "State valuations are given but no valuation descriptions are declared in the index.\n";
            isValid = false;
        } else {
            validateValuation(umbModel.valuations.states.value(), "states", tsIndex.numStates, umbModel.index.valuations->states.value());
        }
    }
    if (umbModel.valuations.choices.has_value()) {
        if (!umbModel.index.valuations.has_value() || !umbModel.index.valuations->choices.has_value()) {
            err << "Choice valuations are given but no valuation descriptions are declared in the index.\n";
            isValid = false;
        } else {
            validateValuation(umbModel.valuations.choices.value(), "choices", tsIndex.numChoices, umbModel.index.valuations->choices.value());
        }
    }
    if (umbModel.valuations.branches.has_value()) {
        if (!umbModel.index.valuations.has_value() || !umbModel.index.valuations->branches.has_value()) {
            err << "Branch valuations are given but no valuation descriptions are declared in the index.\n";
            isValid = false;
        } else {
            validateValuation(umbModel.valuations.branches.value(), "branches", tsIndex.numBranches, umbModel.index.valuations->branches.value());
        }
    }
    if (umbModel.valuations.observations.has_value()) {
        if (!umbModel.index.valuations.has_value() || !umbModel.index.valuations->observations.has_value()) {
            err << "Observation valuations are given but no valuation descriptions are declared in the index.\n";
            isValid = false;
        } else {
            validateValuation(umbModel.valuations.observations.value(), "observations", tsIndex.numObservations,
                              umbModel.index.valuations->observations.value());
        }
    }
    if (umbModel.valuations.players.has_value()) {
        if (!umbModel.index.valuations.has_value() || !umbModel.index.valuations->players.has_value()) {
            err << "Player valuations are given but no valuation descriptions are declared in the index.\n";
            isValid = false;
        } else {
            validateValuation(umbModel.valuations.players.value(), "players", tsIndex.numPlayers, umbModel.index.valuations->players.value());
        }
    }

    return isValid;
}

void validateOrThrow(storm::umb::UmbModel const& umbModel) {
    std::stringstream errors;
    if (!validate(umbModel, errors)) {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                        "UMB model " << umbModel.getShortModelInformation() << " is invalid:\n"
                                     << errors.str());
    }
}

}  // namespace storm::umb
