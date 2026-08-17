#include "storm-dft/transformations/DftInstantiator.h"

#include "storm-dft/builder/DFTBuilder.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotImplementedException.h"

namespace storm::dft {
namespace transformations {

template<typename ParametricType, typename ConstantType>
DftInstantiator<ParametricType, ConstantType>::DftInstantiator(storm::dft::storage::DFT<ParametricType> const &dft) : dft(dft) {}

template<typename ParametricType, typename ConstantType>
std::shared_ptr<storm::dft::storage::DFT<ConstantType>> DftInstantiator<ParametricType, ConstantType>::instantiate(
    storm::utility::parametric::Valuation<ParametricType> const &valuation) {
    storm::dft::builder::DFTBuilder<ConstantType> builder;
    for (size_t i = 0; i < dft.nrElements(); ++i) {
        std::shared_ptr<storm::dft::storage::elements::DFTElement<ParametricType> const> element = dft.getElement(i);
        switch (element->type()) {
            case storm::dft::storage::elements::DFTElementType::BE: {
                // Instantiate probability distributions
                auto be = std::static_pointer_cast<storm::dft::storage::elements::DFTBE<ParametricType> const>(element);
                switch (be->beType()) {
                    case storm::dft::storage::elements::BEType::CONSTANT: {
                        auto beConst = std::static_pointer_cast<storm::dft::storage::elements::BEConst<ParametricType> const>(element);
                        builder.addBasicElementConst(beConst->name(), beConst->canFail());
                        break;
                    }
                    case storm::dft::storage::elements::BEType::PROBABILITY: {
                        auto beProb = std::static_pointer_cast<storm::dft::storage::elements::BEProbability<ParametricType> const>(element);
                        ConstantType activeFailureProbability = instantiate_helper(beProb->activeFailureProbability(), valuation);
                        ConstantType dormancyFactor = instantiate_helper(beProb->dormancyFactor(), valuation);
                        builder.addBasicElementProbability(beProb->name(), activeFailureProbability, dormancyFactor);
                        break;
                    }
                    case storm::dft::storage::elements::BEType::EXPONENTIAL: {
                        auto beExp = std::static_pointer_cast<storm::dft::storage::elements::BEExponential<ParametricType> const>(element);
                        ConstantType activeFailureRate = instantiate_helper(beExp->activeFailureRate(), valuation);
                        ConstantType dormancyFactor = instantiate_helper(beExp->dormancyFactor(), valuation);
                        builder.addBasicElementExponential(beExp->name(), activeFailureRate, dormancyFactor, beExp->isTransient());
                        break;
                    }
                    case storm::dft::storage::elements::BEType::ERLANG: {
                        auto beErlang = std::static_pointer_cast<storm::dft::storage::elements::BEErlang<ParametricType> const>(element);
                        ConstantType activeFailureRate = instantiate_helper(beErlang->activeFailureRate(), valuation);
                        ConstantType dormancyFactor = instantiate_helper(beErlang->dormancyFactor(), valuation);
                        builder.addBasicElementErlang(beErlang->name(), activeFailureRate, beErlang->phases(), dormancyFactor);
                        break;
                    }
                    case storm::dft::storage::elements::BEType::WEIBULL: {
                        auto beWeibull = std::static_pointer_cast<storm::dft::storage::elements::BEWeibull<ParametricType> const>(element);
                        ConstantType shape = instantiate_helper(beWeibull->shape(), valuation);
                        ConstantType rate = instantiate_helper(beWeibull->rate(), valuation);
                        builder.addBasicElementWeibull(beWeibull->name(), shape, rate);
                        break;
                    }
                    case storm::dft::storage::elements::BEType::LOGNORMAL: {
                        auto beLogNormal = std::static_pointer_cast<storm::dft::storage::elements::BELogNormal<ParametricType> const>(element);
                        ConstantType mean = instantiate_helper(beLogNormal->mean(), valuation);
                        ConstantType stddev = instantiate_helper(beLogNormal->standardDeviation(), valuation);
                        builder.addBasicElementLogNormal(beLogNormal->name(), mean, stddev);
                        break;
                    }
                    case storm::dft::storage::elements::BEType::SAMPLES: {
                        auto beSamples = std::static_pointer_cast<storm::dft::storage::elements::BESamples<ParametricType> const>(element);
                        std::map<ConstantType, ConstantType> activeSamples{};
                        for (auto &[time, prob] : beSamples->activeSamples()) {
                            ConstantType timeInst = instantiate_helper(time, valuation);
                            ConstantType probInst = instantiate_helper(prob, valuation);
                            activeSamples[timeInst] = probInst;
                        }
                        builder.addBasicElementSamples(beSamples->name(), activeSamples);
                        break;
                    }
                    default:
                        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "BE type '" << be->beType() << "' not known.");
                        break;
                }
                break;
            }
            // We cannot use cloneElement() as ValueType differs between the original element and the clone.
            case storm::dft::storage::elements::DFTElementType::AND:
                builder.addAndGate(element->name(), getChildrenVector(element));
                break;
            case storm::dft::storage::elements::DFTElementType::OR:
                builder.addOrGate(element->name(), getChildrenVector(element));
                break;
            case storm::dft::storage::elements::DFTElementType::VOT: {
                auto vot = std::static_pointer_cast<storm::dft::storage::elements::DFTVot<ParametricType> const>(element);
                builder.addVotingGate(vot->name(), vot->threshold(), getChildrenVector(vot));
                break;
            }
            case storm::dft::storage::elements::DFTElementType::PAND: {
                auto pand = std::static_pointer_cast<storm::dft::storage::elements::DFTPand<ParametricType> const>(element);
                builder.addPandGate(pand->name(), getChildrenVector(pand), pand->isInclusive());
                break;
            }
            case storm::dft::storage::elements::DFTElementType::POR: {
                auto por = std::static_pointer_cast<storm::dft::storage::elements::DFTPor<ParametricType> const>(element);
                builder.addPorGate(por->name(), getChildrenVector(por), por->isInclusive());
                break;
            }
            case storm::dft::storage::elements::DFTElementType::SPARE:
                builder.addSpareGate(element->name(), getChildrenVector(element));
                break;
            case storm::dft::storage::elements::DFTElementType::SEQ:
                builder.addSequenceEnforcer(element->name(), getChildrenVector(element));
                break;
            case storm::dft::storage::elements::DFTElementType::MUTEX:
                builder.addMutex(element->name(), getChildrenVector(element));
                break;
            case storm::dft::storage::elements::DFTElementType::PDEP: {
                auto dependency = std::static_pointer_cast<storm::dft::storage::elements::DFTDependency<ParametricType> const>(element);
                // Instantiate probability
                ConstantType probability = instantiate_helper(dependency->probability(), valuation);
                std::vector<std::string> children = {dependency->triggerEvent()->name()};
                for (auto const &depEvent : dependency->dependentEvents()) {
                    children.push_back(depEvent->name());
                }
                builder.addPdep(dependency->name(), children, probability);
                break;
            }
            default:
                STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "DFT type '" << element->type() << "' not known.");
                break;
        }
    }

    builder.setTopLevel(dft.getTopLevelElement()->name());
    return std::make_shared<storm::dft::storage::DFT<ConstantType>>(builder.build());
}

template<typename ParametricType, typename ConstantType>
std::vector<std::string> DftInstantiator<ParametricType, ConstantType>::getChildrenVector(
    std::shared_ptr<storm::dft::storage::elements::DFTElement<ParametricType> const> element) {
    std::vector<std::string> children;
    auto elementWithChildren = std::static_pointer_cast<storm::dft::storage::elements::DFTChildren<ParametricType> const>(element);
    for (auto const &child : elementWithChildren->children()) {
        children.push_back(child->name());
    }
    return children;
}

template<typename ParametricType, typename ConstantType>
void DftInstantiator<ParametricType, ConstantType>::checkValid() const {
    // TODO write some checks
}

// Explicitly instantiate the class.
template class DftInstantiator<storm::RationalFunction, double>;

}  // namespace transformations
}  // namespace storm::dft
