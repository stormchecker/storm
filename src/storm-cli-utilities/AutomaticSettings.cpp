#include "AutomaticSettings.h"

#include <sstream>

#include "storm/logic/FormulaInformation.h"
#include "storm/storage/SymbolicModelDescription.h"
#include "storm/storage/jani/Property.h"
#include "storm/storage/jani/traverser/InformationCollector.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace utility {

namespace pfinternal {
enum class PropertyType { Bounded, Unbounded, LongRun };

PropertyType getPropertyType(storm::jani::Property const& property) {
    auto formulaInfo = property.getRawFormula()->info();
    if (formulaInfo.containsBoundedUntilFormula() || formulaInfo.containsNextFormula() || formulaInfo.containsCumulativeRewardFormula()) {
        STORM_LOG_INFO("Assuming step or time bounded property:" << property);
        return PropertyType::Bounded;
    } else if (formulaInfo.containsLongRunFormula()) {
        STORM_LOG_INFO("Assuming Long Run property:" << property);
        return PropertyType::LongRun;
    } else {
        STORM_LOG_INFO("Assuming Unbounded property:" << property);
        return PropertyType::Unbounded;
    }
}

struct Features {
    Features(storm::jani::Model const& model, storm::jani::Property const& property) {
        continuousTime = !model.isDiscreteTimeModel();
        nondeterminism = !model.isDeterministicModel();
        propertyType = getPropertyType(property);
        auto modelInfo = model.getModelInformation();
        numVariables = modelInfo.nrVariables;
        numEdges = modelInfo.nrEdges;
        numAutomata = modelInfo.nrAutomata;
        stateDomainSize = modelInfo.stateDomainSize;
        avgDomainSize = modelInfo.avgVarDomainSize;
        stateEstimate = 0;
    }

    std::string toString() const {
        std::stringstream str;
        str << std::boolalpha << "continuous-time=" << continuousTime << "\tnondeterminism=" << nondeterminism << "\tpropertytype=";
        switch (propertyType) {
            case PropertyType::Unbounded:
                str << "unbounded";
                break;
            case PropertyType::Bounded:
                str << "bounded";
                break;
            case PropertyType::LongRun:
                str << "longrun";
                break;
        }
        str << "\tnumVariables=" << numVariables;
        str << "\tnumEdges=" << numEdges;
        str << "\tnumAutomata=" << numAutomata;
        if (stateDomainSize > 0) {
            str << "\tstateDomainSize=" << stateDomainSize;
        }
        if (avgDomainSize > 0.0) {
            str << "\tavgDomainSize=" << avgDomainSize;
        }
        if (stateEstimate > 0) {
            str << "\tstateEstimate=" << stateEstimate;
        }
        return str.str();
    }

    bool continuousTime;
    bool nondeterminism;
    PropertyType propertyType;
    uint64_t numVariables;
    uint64_t numAutomata;
    uint64_t numEdges;
    uint64_t stateDomainSize;
    uint64_t stateEstimate;
    double avgDomainSize;
};
}  // namespace pfinternal

AutomaticSettings::AutomaticSettings() : engine(storm::utility::Engine::Unknown), useBisimulation(false), useExact(false) {
    // Intentionally left empty
}

void AutomaticSettings::predict(storm::jani::Model const& model, storm::jani::Property const& property) {
    typedef pfinternal::PropertyType PropertyType;
    auto f = pfinternal::Features(model, property);
    STORM_LOG_INFO("Automatic engine using features " << f.toString() << ".");

    if (f.numVariables <= 12) {
        if (f.avgDomainSize <= 323.25) {
            if (f.stateDomainSize <= 381703) {
                if (f.numAutomata <= 1) {
                    exact();
                } else {  // f.numAutomata > 1
                    if (f.numEdges <= 34) {
                        if (!f.nondeterminism) {
                            sparse();
                        } else {  // f.nondeterminism
                            if (f.stateDomainSize <= 1764) {
                                ddbisim();
                            } else {  // f.stateDomainSize > 1764
                                sparse();
                            }
                        }
                    } else {  // f.numEdges > 34
                        sparse();
                    }
                }
            } else {  // f.stateDomainSize > 381703
                if (!f.continuousTime) {
                    if (f.numEdges <= 6002) {
                        if (f.propertyType == PropertyType::Bounded) {
                            if (f.numEdges <= 68) {
                                hybrid();
                            } else {  // f.numEdges > 68
                                ddbisim();
                            }
                        } else {  // !(f.propertyType == PropertyType::Bounded)
                            if (f.avgDomainSize <= 26.4375) {
                                hybrid();
                            } else {  // f.avgDomainSize > 26.4375
                                if (f.stateDomainSize <= 208000000) {
                                    sparse();
                                } else {  // f.stateDomainSize > 208000000
                                    hybrid();
                                }
                            }
                        }
                    } else {  // f.numEdges > 6002
                        sparse();
                    }
                } else {  // f.continuousTime
                    if (f.numAutomata <= 16) {
                        sparse();
                    } else {  // f.numAutomata > 16
                        if (f.propertyType == PropertyType::Bounded) {
                            sparse();
                        } else {  // !(f.propertyType == PropertyType::Bounded)
                            hybrid();
                        }
                    }
                }
            }
        } else {  // f.avgDomainSize > 323.25
            sparse();
        }
    } else {  // f.numVariables > 12
        if (f.stateDomainSize <= 47006507008) {
            if (f.avgDomainSize <= 4.538461446762085) {
                if (f.numVariables <= 82) {
                    if (!f.continuousTime) {
                        if (f.numAutomata <= 8) {
                            sparse();
                        } else {  // f.numAutomata > 8
                            hybrid();
                        }
                    } else {  // f.continuousTime
                        hybrid();
                    }
                } else {  // f.numVariables > 82
                    if (f.numVariables <= 114) {
                        ddbisim();
                    } else {  // f.numVariables > 114
                        hybrid();
                    }
                }
            } else {  // f.avgDomainSize > 4.538461446762085
                ddbisim();
            }
        } else {  // f.stateDomainSize > 47006507008
            if (f.numVariables <= 19) {
                if (f.avgDomainSize <= 20.430288314819336) {
                    hybrid();
                } else {  // f.avgDomainSize > 20.430288314819336
                    if (f.stateDomainSize <= 209736679590723584) {
                        ddbisim();
                    } else {  // f.stateDomainSize > 209736679590723584
                        if (f.propertyType == PropertyType::Bounded) {
                            hybrid();
                        } else {  // !(f.propertyType == PropertyType::Bounded)
                            sparse();
                        }
                    }
                }
            } else {  // f.numVariables > 19
                if (!f.nondeterminism) {
                    if (f.numVariables <= 27) {
                        if (f.propertyType == PropertyType::Bounded) {
                            hybrid();
                        } else {  // !(f.propertyType == PropertyType::Bounded)
                            sparse();
                        }
                    } else {  // f.numVariables > 27
                        ddbisim();
                    }
                } else {  // f.nondeterminism
                    if (f.numAutomata <= 8) {
                        sparse();
                    } else {  // f.numAutomata > 8
                        hybrid();
                    }
                }
            }
        }
    }
}

void AutomaticSettings::predict(storm::jani::Model const& model, storm::jani::Property const& property, uint64_t) {
    // typedef pfinternal::PropertyType PropertyType;
    // auto f = pfinternal::Features(model, property);
    // f.stateEstimate = stateEstimate;
    // STORM_LOG_INFO("Automatic engine using features " << f.toString() << ".");

    // Right now, we do not make use of the state estimate, so we just ask the 'default' decision tree.
    predict(model, property);
}

storm::utility::Engine AutomaticSettings::getEngine() const {
    STORM_LOG_THROW(engine != storm::utility::Engine::Unknown, storm::exceptions::InvalidOperationException,
                    "Tried to get the engine but apparently no prediction was done before.");
    return engine;
}

bool AutomaticSettings::enableBisimulation() const {
    return useBisimulation;
}

bool AutomaticSettings::enableExact() const {
    return useExact;
}

void AutomaticSettings::sparse() {
    engine = storm::utility::Engine::Sparse;
    useBisimulation = false;
    useExact = false;
}

void AutomaticSettings::hybrid() {
    engine = storm::utility::Engine::Hybrid;
    useBisimulation = false;
    useExact = false;
}

void AutomaticSettings::dd() {
    engine = storm::utility::Engine::Dd;
    useBisimulation = false;
    useExact = false;
}

void AutomaticSettings::exact() {
    engine = storm::utility::Engine::Sparse;
    useBisimulation = false;
    useExact = true;
}

void AutomaticSettings::ddbisim() {
    engine = storm::utility::Engine::DdSparse;
    useBisimulation = true;
    useExact = false;
}

}  // namespace utility
}  // namespace storm