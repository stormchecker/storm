#include "storm/storage/umb/model/UmbModel.h"

#include <boost/algorithm/string/join.hpp>
#include <sstream>

#include "storm/storage/umb/model/Validation.h"
#include "storm/storage/umb/model/ValueEncoding.h"

namespace storm::umb {

std::string UmbModel::getShortModelInformation() const {
    std::stringstream info;
    if (index.modelData) {
        info << index.modelData->name.value_or("<unnamed_model>");
        if (index.modelData->version) {
            info << " v" << index.modelData->version.value();
        }
    } else {
        info << "<unnamed_model>";
    }
    return info.str();
}

std::string UmbModel::getModelInformation() const {
    std::stringstream info;
    info << "UMB Model " << getShortModelInformation() << "\n";
    auto optionalPrint = [&info](std::string const& label, auto const& value) {
        if (value) {
            info << "\t* " << label << ": " << value.value() << "\n";
        }
    };
    if (index.modelData) {
        auto const& md = index.modelData.value();
        if (md.authors && !md.authors->empty()) {
            info << "\t* Authors: " << boost::join(md.authors.value(), "; ") << "\n";
        }
        optionalPrint("Description", md.description);
        optionalPrint("Comment", md.comment);
        optionalPrint("DOI", md.doi);
        optionalPrint("URL", md.url);
    }
    if (index.fileData) {
        auto const& fd = index.fileData.value();
        if (fd.tool) {
            info << "\t* Created with tool: " << fd.tool.value();
            if (fd.toolVersion) {
                info << " v" << fd.toolVersion.value();
            }
            info << "\n";
        }
        if (fd.creationDate) {
            info << "\t* Creation date: " << fd.creationDateAsString() << "\n";
        }
        if (fd.parameters) {
            info << "\t* Parameters:\n" << storm::dumpJson(fd.parameters.value()) << "\n";
        }
    }
    return info.str();
}

storm::OptionalRef<UmbModel::Annotation> UmbModel::annotation(std::string const& annotationType, bool createIfMissing) {
    if (createIfMissing) {
        return annotations[annotationType];
    }
    auto it = annotations.find(annotationType);
    if (it != annotations.end()) {
        return it->second;
    }
    return {};
}

storm::OptionalRef<UmbModel::Annotation const> UmbModel::annotation(std::string const& annotationType) const {
    auto it = annotations.find(annotationType);
    if (it != annotations.end()) {
        return it->second;
    }
    return {};
}

storm::OptionalRef<UmbModel::Annotation> UmbModel::aps(bool createIfMissing) {
    return annotation("aps", createIfMissing);
}

storm::OptionalRef<UmbModel::Annotation const> UmbModel::aps() const {
    return annotation("aps");
}

storm::OptionalRef<UmbModel::Annotation> UmbModel::rewards(bool createIfMissing) {
    return annotation("rewards", createIfMissing);
}

storm::OptionalRef<UmbModel::Annotation const> UmbModel::rewards() const {
    return annotation("rewards");
}

bool UmbModel::validate(std::ostream& errors) const {
    return storm::umb::validate(*this, errors);
}

void UmbModel::validateOrThrow() const {
    storm::umb::validateOrThrow(*this);
}

void UmbModel::encodeRationals() {
    auto getSize = [](storm::umb::GenericVector& v) -> uint64_t {
        if (v.isType<storm::RationalNumber>()) {
            return ValueEncoding::getMinimalRationalSize(v.template get<storm::RationalNumber>(), true);
        } else if (v.isType<storm::RationalInterval>()) {
            return ValueEncoding::getMinimalRationalIntervalSize(v.template get<storm::RationalInterval>(), true);
        }
        return 0ull;
    };
    auto encodeForSize = [](storm::umb::GenericVector& v, uint64_t const size) {
        if (v.isType<storm::RationalNumber>()) {
            auto values = ValueEncoding::createUint64FromRationalRange(v.template get<storm::RationalNumber>(), size);
            v.template set<uint64_t>(std::move(values));
        } else if (v.isType<storm::RationalInterval>()) {
            auto values = ValueEncoding::createUint64FromRationalIntervalRange(v.template get<storm::RationalInterval>(), size);
            v.template set<uint64_t>(std::move(values));
        }
    };
    auto encode = [&getSize, &encodeForSize](storm::umb::GenericVector& v, std::optional<uint64_t>& size) {
        if (v.isType<storm::RationalNumber>() || v.isType<storm::RationalInterval>()) {
            size = getSize(v);
            encodeForSize(v, size.value());
        }
    };

    if (index.transitionSystem.branchProbabilityType.has_value()) {
        encode(branchToProbability, index.transitionSystem.branchProbabilityType->size);
    }
    if (index.transitionSystem.exitRateType.has_value()) {
        encode(stateToExitRate, index.transitionSystem.exitRateType->size);
    }
    if (index.transitionSystem.observationProbabilityType.has_value()) {
        if (stateObservations.has_value()) {
            encode(stateObservations->probabilities, index.transitionSystem.observationProbabilityType->size);
        }
        if (branchObservations.has_value()) {
            encode(branchObservations->probabilities, index.transitionSystem.observationProbabilityType->size);
        }
    }
    for (auto& [annotationType, annotation] : annotations) {
        for (auto& [annotationName, annotationValues] : annotation) {
            // Get encoding bit sizes. Note that if the annotation applies to multiple entities, we need to use the maximum size.
            uint64_t valueSize{0}, probSize{0};
            boost::pfr::for_each_field(annotationValues, [&valueSize, &probSize, &getSize](auto& entity) {
                if (entity.has_value()) {
                    valueSize = std::max(valueSize, getSize(entity->values));
                    probSize = std::max(probSize, getSize(entity->probabilities));
                }
            });
            // encode
            if (valueSize > 0 || probSize > 0) {
                STORM_LOG_ASSERT(index.annotation(annotationType).has_value() && index.annotation(annotationType)->contains(annotationName),
                                 "There are annotation files stored for annotations/" << annotationType << "/" << annotationName
                                                                                      << ", but no corresponding entry in the index.");
                auto& indexAnnotation = index.annotation(annotationType)->at(annotationName);
                if (valueSize > 0) {
                    indexAnnotation.type.size = valueSize;
                    boost::pfr::for_each_field(annotationValues, [&valueSize, &encodeForSize](auto& entity) {
                        if (entity.has_value()) {
                            encodeForSize(entity->values, valueSize);
                        }
                    });
                }
                if (probSize > 0) {
                    STORM_LOG_ASSERT(indexAnnotation.probabilityType.has_value(), "Found annotation probabilities but no type in index.");
                    indexAnnotation.probabilityType.value().size = probSize;
                    boost::pfr::for_each_field(annotationValues, [&probSize, &encodeForSize](auto& entity) {
                        if (entity.has_value()) {
                            encodeForSize(entity->probabilities, probSize);
                        }
                    });
                }
            }
        }
    }
}

void UmbModel::decodeRationals() {
    auto decode = [](storm::umb::GenericVector& v, std::optional<SizedType> const type) {
        if (type.has_value() && type->type == Type::Rational && v.isType<uint64_t>()) {
            auto valueView = ValueEncoding::uint64ToRationalRangeView(v.template get<uint64_t>(), type->bitSize());
            std::vector<storm::RationalNumber> values(valueView.begin(), valueView.end());
            v.template set<storm::RationalNumber>(std::move(values));
        } else if (type.has_value() && type->type == Type::RationalInterval && v.isType<uint64_t>()) {
            auto valueView = ValueEncoding::uint64ToRationalIntervalRangeView(v.template get<uint64_t>(), type->bitSize());
            std::vector<storm::RationalInterval> values(valueView.begin(), valueView.end());
            v.template set<storm::RationalInterval>(std::move(values));
        }
    };

    if (index.transitionSystem.branchProbabilityType.has_value()) {
        decode(branchToProbability, index.transitionSystem.branchProbabilityType.value());
    }
    if (index.transitionSystem.exitRateType.has_value()) {
        decode(stateToExitRate, index.transitionSystem.exitRateType.value());
    }
    if (index.transitionSystem.observationProbabilityType.has_value()) {
        if (stateObservations.has_value()) {
            decode(stateObservations->probabilities, index.transitionSystem.observationProbabilityType.value());
        }
        if (branchObservations.has_value()) {
            decode(branchObservations->probabilities, index.transitionSystem.observationProbabilityType.value());
        }
    }

    for (auto& [annotationType, annotation] : annotations) {
        for (auto& [annotationName, annotationValues] : annotation) {
            STORM_LOG_ASSERT(index.annotation(annotationType).has_value() && index.annotation(annotationType)->contains(annotationName),
                             "There are annotation files stored for annotations/" << annotationType << "/" << annotationName
                                                                                  << ", but no corresponding entry in the index.");
            auto const& annotationIndex = index.annotation(annotationType)->at(annotationName);
            boost::pfr::for_each_field(annotationValues, [&annotationIndex, &decode](auto& entity) {
                if (entity.has_value()) {
                    decode(entity->values, annotationIndex.type);
                    decode(entity->probabilities, annotationIndex.probabilityType.value());
                }
            });
        }
    }
}

}  // namespace storm::umb
