#include "DftJsonExporter.h"

#include <algorithm>
#include <string>

#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/io/file.h"

namespace storm::dft {
namespace storage {

template<typename ValueType>
void DftJsonExporter<ValueType>::toFile(storm::dft::storage::DFT<ValueType> const& dft, std::string const& filepath) {
    std::ofstream stream;
    storm::io::openFile(filepath, stream);
    toStream(dft, stream);
    storm::io::closeFile(stream);
}

template<typename ValueType>
void DftJsonExporter<ValueType>::toStream(storm::dft::storage::DFT<ValueType> const& dft, std::ostream& os) {
    os << storm::dumpJson(translate(dft)) << '\n';
}

template<typename ValueType>
typename DftJsonExporter<ValueType>::Json DftJsonExporter<ValueType>::translate(storm::dft::storage::DFT<ValueType> const& dft) {
    Json jsonDft;
    // Top level element
    jsonDft["toplevel"] = std::to_string(dft.getTopLevelIndex());
    // Parameters
    Json jsonParameters = translateParameters(dft);
    if (!jsonParameters.empty()) {
        jsonDft["parameters"] = jsonParameters;
    }
    // Nodes
    Json jsonNodes;
    for (size_t i = 0; i < dft.nrElements(); ++i) {
        jsonNodes.push_back(translateElement(dft.getElement(i)));
    }
    jsonDft["nodes"] = jsonNodes;
    return jsonDft;
}

template<>
typename DftJsonExporter<storm::RationalFunction>::Json DftJsonExporter<storm::RationalFunction>::translateParameters(
    storm::dft::storage::DFT<storm::RationalFunction> const& dft) {
    Json jsonParameters;
    for (auto const& parameter : storm::dft::storage::getParameters(dft)) {
        std::stringstream stream;
        stream << parameter;
        jsonParameters.push_back(stream.str());
    }
    return jsonParameters;
}

template<typename ValueType>
typename DftJsonExporter<ValueType>::Json DftJsonExporter<ValueType>::translateParameters(storm::dft::storage::DFT<ValueType> const& dft) {
    // No parameters for non-parametric models
    return Json::array();
}

template<typename ValueType>
typename DftJsonExporter<ValueType>::Json DftJsonExporter<ValueType>::translateElement(DFTElementCPointer element) {
    Json nodeData;
    nodeData["id"] = std::to_string(element->id());
    nodeData["name"] = element->name();
    std::string type = storm::dft::storage::elements::toString(element->type());
    // Make lower case
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);
    nodeData["type"] = type;
    if (element->isRelevant()) {
        nodeData["relevant"] = true;
    }

    if (element->isGate() || element->isRestriction()) {
        // Set children for gate/restriction
        auto elemWithChildren = std::static_pointer_cast<storm::dft::storage::elements::DFTChildren<ValueType> const>(element);
        std::vector<std::string> children;
        for (auto const& child : elemWithChildren->children()) {
            children.push_back(std::to_string(child->id()));
        }
        nodeData["children"] = children;

        // Set additional parameters
        switch (element->type()) {
            case storm::dft::storage::elements::DFTElementType::VOT:
                nodeData["voting"] = std::static_pointer_cast<storm::dft::storage::elements::DFTVot<ValueType> const>(element)->threshold();
                break;
            case storm::dft::storage::elements::DFTElementType::PAND:
                nodeData["inclusive"] = std::static_pointer_cast<storm::dft::storage::elements::DFTPand<ValueType> const>(element)->isInclusive();
                break;
            case storm::dft::storage::elements::DFTElementType::POR:
                nodeData["inclusive"] = std::static_pointer_cast<storm::dft::storage::elements::DFTPor<ValueType> const>(element)->isInclusive();
                break;
            case storm::dft::storage::elements::DFTElementType::AND:
            case storm::dft::storage::elements::DFTElementType::OR:
            case storm::dft::storage::elements::DFTElementType::SPARE:
            case storm::dft::storage::elements::DFTElementType::SEQ:
            case storm::dft::storage::elements::DFTElementType::MUTEX:
                // No additional parameters
                break;
            default:
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                                "Element '" << element->name() << "' of type '" << element->type() << "' is not supported.");
        }
    } else if (element->isDependency()) {
        // Set children for dependency
        auto dependency = std::static_pointer_cast<storm::dft::storage::elements::DFTDependency<ValueType> const>(element);
        std::vector<std::string> children;
        children.push_back(std::to_string(dependency->triggerEvent()->id()));
        for (auto const& child : dependency->dependentEvents()) {
            children.push_back(std::to_string(child->id()));
        }
        nodeData["children"] = children;
        if (storm::utility::isOne<ValueType>(dependency->probability())) {
            nodeData["type"] = "fdep";
        } else {
            std::stringstream stream;
            stream << dependency->probability();
            nodeData["probability"] = stream.str();
        }
    } else if (element->isBasicElement()) {
        DFTBECPointer be = std::static_pointer_cast<storm::dft::storage::elements::DFTBE<ValueType> const>(element);
        nodeData = translateBE(be, nodeData);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                        "Element '" << element->name() << "' of type '" << element->type() << "' is not supported.");
    }

    Json jsonNode;
    jsonNode["data"] = nodeData;
    jsonNode["group"] = "nodes";
    jsonNode["classes"] = type;
    return jsonNode;
}

template<typename ValueType>
typename DftJsonExporter<ValueType>::Json DftJsonExporter<ValueType>::translateBE(DFTBECPointer be, Json nodeData) {
    std::string distributionType = storm::dft::storage::elements::toString(be->beType());
    // Make lower case
    std::transform(distributionType.begin(), distributionType.end(), distributionType.begin(), ::tolower);
    nodeData["distribution"] = distributionType;

    // Add distribution specific information
    switch (be->beType()) {
        case storm::dft::storage::elements::BEType::CONSTANT: {
            auto beConst = std::static_pointer_cast<storm::dft::storage::elements::BEConst<ValueType> const>(be);
            nodeData["failed"] = beConst->failed();
            return nodeData;
        }
        case storm::dft::storage::elements::BEType::PROBABILITY: {
            auto beProb = std::static_pointer_cast<storm::dft::storage::elements::BEProbability<ValueType> const>(be);
            std::stringstream stream;
            stream << beProb->activeFailureProbability();
            nodeData["prob"] = stream.str();
            stream.str(std::string());  // Clear stringstream
            stream << beProb->dormancyFactor();
            nodeData["dorm"] = stream.str();
            return nodeData;
        }
        case storm::dft::storage::elements::BEType::EXPONENTIAL: {
            auto beExp = std::static_pointer_cast<storm::dft::storage::elements::BEExponential<ValueType> const>(be);
            std::stringstream stream;
            stream << beExp->activeFailureRate();
            nodeData["rate"] = stream.str();
            stream.str(std::string());  // Clear stringstream
            stream << beExp->dormancyFactor();
            nodeData["dorm"] = stream.str();
            nodeData["transient"] = beExp->isTransient();
            return nodeData;
        }
        case storm::dft::storage::elements::BEType::ERLANG: {
            auto beErlang = std::static_pointer_cast<storm::dft::storage::elements::BEErlang<ValueType> const>(be);
            std::stringstream stream;
            stream << beErlang->activeFailureRate();
            nodeData["rate"] = stream.str();
            nodeData["phases"] = beErlang->phases();
            stream.str(std::string());  // Clear stringstream
            stream << beErlang->dormancyFactor();
            nodeData["dorm"] = stream.str();
            return nodeData;
        }
        case storm::dft::storage::elements::BEType::WEIBULL: {
            auto beWeibull = std::static_pointer_cast<storm::dft::storage::elements::BEWeibull<ValueType> const>(be);
            std::stringstream stream;
            stream << beWeibull->shape();
            nodeData["shape"] = stream.str();
            stream.str(std::string());  // Clear stringstream
            stream << beWeibull->rate();
            nodeData["rate"] = stream.str();
            return nodeData;
        }
        case storm::dft::storage::elements::BEType::LOGNORMAL: {
            auto beLogNormal = std::static_pointer_cast<storm::dft::storage::elements::BELogNormal<ValueType> const>(be);
            std::stringstream stream;
            stream << beLogNormal->mean();
            nodeData["mean"] = stream.str();
            stream.str(std::string());  // Clear stringstream
            stream << beLogNormal->standardDeviation();
            nodeData["stddev"] = stream.str();
            return nodeData;
        }
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "BE of type '" << be->beType() << "' is not known.");
            return nodeData;
    }
}

// Explicitly instantiate the class.
template class DftJsonExporter<double>;
template class DftJsonExporter<storm::RationalFunction>;

}  // namespace storage
}  // namespace storm::dft
