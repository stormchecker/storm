#include "storm-gspn/storage/gspn/GSPN.h"

#include <unordered_map>

#include <boost/lexical_cast.hpp>

#include "storm-gspn/storage/gspn/GspnJsonExporter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace gspn {
uint64_t GSPN::timedTransitionIdToTransitionId(uint64_t ttId) {
    return ttId | (1ull << ((sizeof(ttId) * CHAR_BIT) - 1));
}

uint64_t GSPN::immediateTransitionIdToTransitionId(uint64_t itId) {
    return itId;
}

uint64_t GSPN::transitionIdToTimedTransitionId(uint64_t tId) {
    return (tId << 1) >> 1;
}

uint64_t GSPN::transitionIdToImmediateTransitionId(uint64_t tId) {
    return tId;
}

GSPN::GSPN(std::string const& name, std::vector<Place> const& places, std::vector<ImmediateTransition<WeightType>> const& itransitions,
           std::vector<TimedTransition<RateType>> const& ttransitions, std::vector<TransitionPartition> const& partitions,
           std::shared_ptr<storm::expressions::ExpressionManager> const& exprManager,
           std::map<storm::expressions::Variable, storm::expressions::Expression> const& constantsSubstitution)
    : name(name),
      places(places),
      immediateTransitions(itransitions),
      timedTransitions(ttransitions),
      partitions(partitions),
      exprManager(exprManager),
      constantsSubstitution(constantsSubstitution) {}

uint64_t GSPN::getNumberOfPlaces() const {
    return places.size();
}

uint64_t GSPN::getNumberOfImmediateTransitions() const {
    return immediateTransitions.size();
}

uint64_t GSPN::getNumberOfTimedTransitions() const {
    return timedTransitions.size();
}

std::vector<storm::gspn::TimedTransition<GSPN::RateType>> const& GSPN::getTimedTransitions() const {
    return this->timedTransitions;
}

std::vector<storm::gspn::ImmediateTransition<GSPN::WeightType>> const& GSPN::getImmediateTransitions() const {
    return this->immediateTransitions;
}

std::vector<storm::gspn::Place> const& GSPN::getPlaces() const {
    return places;
}

std::shared_ptr<storm::gspn::Marking> GSPN::getInitialMarking(std::map<uint64_t, uint64_t>& numberOfBits, uint64_t const& numberOfTotalBits) const {
    auto m = std::make_shared<storm::gspn::Marking>(getNumberOfPlaces(), numberOfBits, numberOfTotalBits);
    for (auto& place : getPlaces()) {
        m->setNumberOfTokensAt(place.getID(), place.getNumberOfInitialTokens());
    }
    return m;
}

std::vector<TransitionPartition> const& GSPN::getPartitions() const {
    return partitions;
}

storm::gspn::Place const* GSPN::getPlace(uint64_t id) const {
    if (id < places.size()) {
        STORM_LOG_ASSERT(places.at(id).getID() == id, "Place ID mismatch.");
        return &places.at(id);
    }
    return nullptr;
}

storm::gspn::Place* GSPN::getPlace(uint64_t id) {
    if (id < places.size()) {
        STORM_LOG_ASSERT(places.at(id).getID() == id, "Place ID mismatch.");
        return &places.at(id);
    }
    return nullptr;
}

storm::gspn::Place const* GSPN::getPlace(std::string const& name) const {
    for (auto& place : places) {
        if (place.getName() == name) {
            return &place;
        }
    }
    return nullptr;
}

storm::gspn::Place* GSPN::getPlace(std::string const& name) {
    for (auto& place : places) {
        if (place.getName() == name) {
            return &place;
        }
    }
    return nullptr;
}

storm::gspn::TimedTransition<GSPN::RateType> const* GSPN::getTimedTransition(std::string const& name) const {
    for (auto& trans : timedTransitions) {
        if (name == trans.getName()) {
            return &trans;
        }
    }
    return nullptr;
}

storm::gspn::ImmediateTransition<GSPN::WeightType> const* GSPN::getImmediateTransition(std::string const& name) const {
    for (auto& trans : immediateTransitions) {
        if (name == trans.getName()) {
            return &trans;
        }
    }
    return nullptr;
}

storm::gspn::Transition const* GSPN::getTransition(std::string const& id) const {
    auto trans = getTimedTransition(id);
    if (trans != nullptr) {
        return trans;
    }

    return getImmediateTransition(id);
}

std::shared_ptr<storm::expressions::ExpressionManager> const& GSPN::getExpressionManager() const {
    return exprManager;
}

std::map<storm::expressions::Variable, storm::expressions::Expression> const& GSPN::getConstantsSubstitution() const {
    return constantsSubstitution;
}

void GSPN::setCapacities(std::unordered_map<std::string, uint64_t> const& mapping) {
    for (auto const& entry : mapping) {
        storm::gspn::Place* place = getPlace(entry.first);
        STORM_LOG_THROW(place != nullptr, storm::exceptions::InvalidArgumentException, "No place with name " << entry.first << ".");
        place->setCapacity(entry.second);
    }
}

void GSPN::writeDotToStream(std::ostream& outStream) const {
    outStream << "digraph " << this->getName() << " {\n";

    // print places with initial marking (not printed is the capacity)
    outStream << "\t" << "node [shape=ellipse]\n";
    for (auto& place : this->getPlaces()) {
        outStream << "\t" << place.getName() << " [label=\"" << place.getName() << "(" << place.getNumberOfInitialTokens();
        outStream << ")\"];\n";
    }

    // print transitions with weight/rate
    outStream << "\t" << "node [shape=box]\n";

    for (auto& trans : this->getImmediateTransitions()) {
        outStream << "\t" << trans.getName() << " [fontcolor=white, style=filled, fillcolor=black, label=<" << trans.getName()
                  << "<br/><FONT POINT-SIZE=\"10\"> π = " + std::to_string(trans.getPriority()) << "</FONT>>];\n";
    }

    for (auto& trans : this->getTimedTransitions()) {
        outStream << "\t" << trans.getName() << " [label=\"" << trans.getName();
        outStream << "(" << trans.getRate() << ")\"];\n";
        STORM_LOG_WARN_COND(trans.hasSingleServerSemantics(), "Unable to export non-trivial transition semantics");  // TODO
    }

    // print arcs
    for (auto& trans : this->getImmediateTransitions()) {
        for (auto const& inEntry : trans.getInputPlaces()) {
            if (trans.getOutputPlaces().count(inEntry.first) == 0) {
                outStream << "\t" << places.at(inEntry.first).getName() << " -> " << trans.getName() << "[label=\""
                          << (inEntry.second > 1 ? std::to_string(inEntry.second) : "") << "\"];\n";
            }
        }

        for (auto const& inhEntry : trans.getInhibitionPlaces()) {
            if (trans.getOutputPlaces().count(inhEntry.first) == 0) {
                outStream << "\t" << places.at(inhEntry.first).getName() << " -> " << trans.getName() << "[arrowhead=\"dot\", label=\""
                          << (inhEntry.second > 1 ? std::to_string(inhEntry.second) : "") << "\"];\n";
            }
        }

        for (auto const& outEntry : trans.getOutputPlaces()) {
            if (trans.getInhibitionPlaces().count(outEntry.first) == 1) {
                outStream << "\t" << trans.getName() << " -> " << places.at(outEntry.first).getName() << "[arrowtail=\"dot\", label=\""
                          << (outEntry.second > 1 ? std::to_string(outEntry.second) : "") << "\", dir=both];\n";
            } else if (trans.getInputPlaces().count(outEntry.first) == 1) {
                outStream << "\t" << trans.getName() << " -> " << places.at(outEntry.first).getName() << "[label=\""
                          << (outEntry.second > 1 ? std::to_string(outEntry.second) : "") << "\", dir=both];\n";
            } else {
                outStream << "\t" << trans.getName() << " -> " << places.at(outEntry.first).getName() << "[label=\""
                          << (outEntry.second > 1 ? std::to_string(outEntry.second) : "") << "\"];\n";
            }
        }
    }

    for (auto& trans : this->getTimedTransitions()) {
        for (auto const& inEntry : trans.getInputPlaces()) {
            if (trans.getOutputPlaces().count(inEntry.first) == 0) {
                outStream << "\t" << places.at(inEntry.first).getName() << " -> " << trans.getName() << "[label=\""
                          << (inEntry.second > 1 ? std::to_string(inEntry.second) : "") << "\"];\n";
            }
        }

        for (auto const& inhEntry : trans.getInhibitionPlaces()) {
            if (trans.getOutputPlaces().count(inhEntry.first) == 0) {
                outStream << "\t" << places.at(inhEntry.first).getName() << " -> " << trans.getName() << "[arrowhead=\"dot\", label=\""
                          << (inhEntry.second > 1 ? std::to_string(inhEntry.second) : "") << "\"];\n";
            }
        }

        for (auto const& outEntry : trans.getOutputPlaces()) {
            if (trans.getInhibitionPlaces().count(outEntry.first) == 1) {
                outStream << "\t" << trans.getName() << " -> " << places.at(outEntry.first).getName() << "[arrowtail=\"dot\", label=\""
                          << (outEntry.second > 1 ? std::to_string(outEntry.second) : "") << "\", dir=both];\n";
            } else if (trans.getInputPlaces().count(outEntry.first) == 1) {
                outStream << "\t" << trans.getName() << " -> " << places.at(outEntry.first).getName() << "[label=\""
                          << (outEntry.second > 1 ? std::to_string(outEntry.second) : "") << "\", dir=both];\n";
            } else {
                outStream << "\t" << trans.getName() << " -> " << places.at(outEntry.first).getName() << "[label=\""
                          << (outEntry.second > 1 ? std::to_string(outEntry.second) : "") << "\"];\n";
            }
        }
    }

    outStream << "}\n";
}

void GSPN::setName(std::string const& name) {
    this->name = name;
}

std::string const& GSPN::getName() const {
    return this->name;
}

bool GSPN::isValid() const {
    bool result = true;
    result |= testPlaces();
    result |= testTransitions();

    return result;
}

bool GSPN::testPlaces() const {
    std::vector<std::string> namesOfPlaces;
    std::vector<uint64_t> idsOfPlaces;
    bool result = true;

    for (auto const& place : this->getPlaces()) {
        if (std::find(namesOfPlaces.begin(), namesOfPlaces.end(), place.getName()) != namesOfPlaces.end()) {
            STORM_LOG_WARN("duplicates states with the name \"" + place.getName() + "\"\n");
            result = false;
        }

        if (std::find(idsOfPlaces.begin(), idsOfPlaces.end(), place.getID()) != idsOfPlaces.end()) {
            STORM_LOG_WARN("duplicates states with the id \"" + boost::lexical_cast<std::string>(place.getID()) + "\"\n");
            result = false;
        }

        if (place.getNumberOfInitialTokens() > place.getCapacity()) {
            STORM_LOG_WARN("number of initial tokens is greater than the capacity for place \"" + place.getName() + "\"\n");
            result = false;
        }
    }

    return result;
}

bool GSPN::testTransitions() const {
    bool result = true;

    //            for (auto const& transition : this->getImmediateTransitions()) {
    //                if (transition.getInputPlaces().empty() &&
    //                    transition.getInhibitionPlaces().empty()) {
    //                    STORM_PRINT_AND_LOG("transition \"" + transition.getName() + "\" has no input or inhibition place\n")
    //                    result = false;
    //                }
    //
    //                if (transition.getOutputPlaces().empty()) {
    //                    STORM_PRINT_AND_LOG("transition \"" + transition.getName() + "\" has no output place\n")
    //                    result = false;
    //                }
    //            }
    //
    //            for (auto const& transition : this->getTimedTransitions()) {
    //                if (transition.getInputPlaces().empty() &&
    //                    transition.getInputPlaces().empty()) {
    //                    STORM_PRINT_AND_LOG("transition \"" + transition.getName() + "\" has no input or inhibition place\n")
    //                    result = false;
    //                }
    //
    //                if (transition.getOutputPlaces().empty()) {
    //                    STORM_PRINT_AND_LOG("transition \"" + transition.getName() + "\" has no output place\n")
    //                    result = false;
    //                }
    //            }
    //
    //            //test if places exists in the gspn
    //            for (auto const& transition : this->getImmediateTransitions()) {
    //                for (auto &placePtr : transition.getInputPlaces()) {
    //                    bool foundPlace = false;
    //                    for (auto const& place : places) {
    //                        if (place.getName() == placePtr->getName()) {
    //                            foundPlace = true;
    //                        }
    //                    }
    //                    if (!foundPlace) {
    //                        STORM_PRINT_AND_LOG("input place \"" + placePtr->getName() + "\" of transition \"" + transition.getName() + "\" was not found \n")
    //                        result = false;
    //                    }
    //                }
    //
    //                for (auto &placePtr : transition.getInhibitionPlaces()) {
    //                    bool foundPlace = false;
    //                    for (auto const& place : places) {
    //                        if (place.getName() == placePtr->getName()) {
    //                            foundPlace = true;
    //                        }
    //                    }
    //                    if (!foundPlace) {
    //                        STORM_PRINT_AND_LOG("inhibition place \"" + placePtr->getName() + "\" of transition \"" + transition.getName() + "\" was not found
    //                        \n") result = false;
    //                    }
    //                }
    //
    //                for (auto &placePtr : transition.getOutputPlaces()) {
    //                    bool foundPlace = false;
    //                    for (auto const& place : places) {
    //                        if (place.getName() == placePtr->getName()) {
    //                            foundPlace = true;
    //                        }
    //                    }
    //                    if (!foundPlace) {
    //                        STORM_PRINT_AND_LOG("output place \"" + placePtr->getName() + "\" of transition \"" + transition.getName() + "\" was not found
    //                        \n") result = false;
    //                    }
    //                }
    //            }
    //
    //            for (auto const& transition : this->getTimedTransitions()) {
    //                for (auto &placePtr : transition.getInputPlaces()) {
    //                    bool foundPlace = false;
    //                    for (auto const& place : places) {
    //                        if (place.getName() == placePtr->getName()) {
    //                            foundPlace = true;
    //                        }
    //                    }
    //                    if (!foundPlace) {
    //                        STORM_PRINT_AND_LOG("input place \"" + placePtr->getName() + "\" of transition \"" + transition.getName() + "\" was not found \n")
    //                        result = false;
    //                    }
    //                }
    //
    //                for (auto &placePtr : transition.getInhibitionPlaces()) {
    //                    bool foundPlace = false;
    //                    for (auto const& place : places) {
    //                        if (place.getName() == placePtr->getName()) {
    //                            foundPlace = true;
    //                        }
    //                    }
    //                    if (!foundPlace) {
    //                        STORM_PRINT_AND_LOG("inhibition place \"" + placePtr->getName() + "\" of transition \"" + transition.getName() + "\" was not found
    //                        \n") result = false;
    //                    }
    //                }
    //
    //                for (auto &placePtr : transition.getOutputPlaces()) {
    //                    bool foundPlace = false;
    //                    for (auto const& place : places) {
    //                        if (place.getName() == placePtr->getName()) {
    //                            foundPlace = true;
    //                        }
    //                    }
    //                    if (!foundPlace) {
    //                        STORM_PRINT_AND_LOG("output place \"" + placePtr->getName() + "\" of transition \"" + transition.getName() + "\" was not found
    //                        \n") result = false;
    //                    }
    //                }
    //            }

    return result;
}

void GSPN::setPlaceLayoutInfo(uint64_t placeId, LayoutInfo const& layout) const {
    placeLayout[placeId] = layout;
}
void GSPN::setTransitionLayoutInfo(uint64_t transitionId, LayoutInfo const& layout) const {
    transitionLayout[transitionId] = layout;
}

void GSPN::setPlaceLayoutInfo(std::map<uint64_t, LayoutInfo> const& placeLayout) const {
    this->placeLayout = placeLayout;
}
void GSPN::setTransitionLayoutInfo(std::map<uint64_t, LayoutInfo> const& transitionLayout) const {
    this->transitionLayout = transitionLayout;
}

std::map<uint64_t, LayoutInfo> const& GSPN::getPlaceLayoutInfos() const {
    return this->placeLayout;
}

std::map<uint64_t, LayoutInfo> const& GSPN::getTransitionLayoutInfos() const {
    return this->transitionLayout;
}

void GSPN::toPnpro(std::ostream& stream) const {
    auto space = "  ";
    auto space2 = "    ";
    auto space3 = "      ";
    auto projectName = "storm-export";  // TODO add to args
    stream << "<project name=\"" << projectName << "\" version=\"121\">\n";
    stream << space << "<gspn name=\"" << getName() << "\" >\n";

    uint64_t x = 1;
    stream << space2 << "<nodes>\n";
    for (auto& place : places) {
        stream << space3 << "<place marking=\"" << place.getNumberOfInitialTokens() << "\" ";
        stream << "name =\"" << place.getName() << "\" ";
        if (placeLayout.count(place.getID()) > 0) {
            stream << "x=\"" << placeLayout.at(place.getID()).x << "\" ";
            stream << "y=\"" << placeLayout.at(place.getID()).y << "\" ";
        } else {
            stream << "x=\"" << x << "\" ";
            stream << "y=\"1\" ";
        }
        stream << "/>\n";
        x = x + 3;
    }
    x = 1;
    for (auto& trans : timedTransitions) {
        stream << space3 << "<transition name=\"" << trans.getName() << "\" ";
        stream << "type=\"EXP\" ";
        // stream << "nservers-x=\"" << trans.getRate() << "\" ";
        // Use single-server semantics for GSPNs:
        // timed rates are independent of number of tokens in input places
        stream << "nservers=\"1\" ";
        // Note: The rate is translated to a number showing the decimal figures so GreatSPN can process it
        stream << "delay=\"" << std::showpoint << trans.getRate() << "\" ";
        if (transitionLayout.count(trans.getID()) > 0) {
            stream << "x=\"" << transitionLayout.at(trans.getID()).x << "\" ";
            stream << "y=\"" << transitionLayout.at(trans.getID()).y << "\" ";
        } else {
            stream << "x=\"" << x << "\" ";
            stream << "y=\"4\" ";
        }
        stream << "/>\n";
        x = x + 3;
    }
    for (auto& trans : immediateTransitions) {
        stream << space3 << "<transition name=\"" << trans.getName() << "\" ";
        stream << "type=\"IMM\" ";
        stream << "priority=\"" << trans.getPriority() << "\" ";
        stream << "weight=\"" << trans.getWeight() << "\" ";
        if (transitionLayout.count(trans.getID()) > 0) {
            stream << "x=\"" << transitionLayout.at(trans.getID()).x << "\" ";
            stream << "y=\"" << transitionLayout.at(trans.getID()).y << "\" ";
        } else {
            stream << "x=\"" << x << "\" ";
            stream << "y=\"4\" ";
        }
        stream << "/>\n";
        x = x + 3;
    }
    stream << space2 << "</nodes>\n";

    stream << space2 << "<edges>\n";
    for (auto& trans : timedTransitions) {
        for (auto const& inEntry : trans.getInputPlaces()) {
            stream << space3 << "<arc ";
            stream << "head=\"" << trans.getName() << "\" ";
            stream << "tail=\"" << places.at(inEntry.first).getName() << "\" ";
            stream << "kind=\"INPUT\" ";
            stream << "mult=\"" << inEntry.second << "\" ";
            stream << "/>\n";
        }
        for (auto const& inhEntry : trans.getInhibitionPlaces()) {
            stream << space3 << "<arc ";
            stream << "head=\"" << trans.getName() << "\" ";
            stream << "tail=\"" << places.at(inhEntry.first).getName() << "\" ";
            stream << "kind=\"INHIBITOR\" ";
            stream << "mult=\"" << inhEntry.second << "\" ";
            stream << "/>\n";
        }
        for (auto const& outEntry : trans.getOutputPlaces()) {
            stream << space3 << "<arc ";
            stream << "head=\"" << places.at(outEntry.first).getName() << "\" ";
            stream << "tail=\"" << trans.getName() << "\" ";
            stream << "kind=\"OUTPUT\" ";
            stream << "mult=\"" << outEntry.second << "\" ";
            stream << "/>\n";
        }
    }
    for (auto& trans : immediateTransitions) {
        for (auto const& inEntry : trans.getInputPlaces()) {
            stream << space3 << "<arc ";
            stream << "head=\"" << trans.getName() << "\" ";
            stream << "tail=\"" << places.at(inEntry.first).getName() << "\" ";
            stream << "kind=\"INPUT\" ";
            stream << "mult=\"" << inEntry.second << "\" ";
            stream << "/>\n";
        }
        for (auto const& inhEntry : trans.getInhibitionPlaces()) {
            stream << space3 << "<arc ";
            stream << "head=\"" << trans.getName() << "\" ";
            stream << "tail=\"" << places.at(inhEntry.first).getName() << "\" ";
            stream << "kind=\"INHIBITOR\" ";
            stream << "mult=\"" << inhEntry.second << "\" ";
            stream << "/>\n";
        }
        for (auto const& outEntry : trans.getOutputPlaces()) {
            stream << space3 << "<arc ";
            stream << "head=\"" << places.at(outEntry.first).getName() << "\" ";
            stream << "tail=\"" << trans.getName() << "\" ";
            stream << "kind=\"OUTPUT\" ";
            stream << "mult=\"" << outEntry.second << "\" ";
            stream << "/>\n";
        }
    }
    stream << space2 << "</edges>\n";
    stream << space << "</gspn>\n";
    stream << "</project>\n";
}

void GSPN::toPnml(std::ostream& stream) const {
    std::string space = "  ";
    std::string space2 = "    ";
    std::string space3 = "      ";
    std::string space4 = "        ";

    stream << "<pnml>\n";
    stream << space << "<net id=\"" << getName() << "\">\n";

    // add places
    for (const auto& place : places) {
        stream << space2 << "<place id=\"" << place.getName() << "\">\n";
        stream << space3 << "<initialMarking>\n";
        stream << space4 << "<value>Default," << place.getNumberOfInitialTokens() << "</value>\n";
        stream << space3 << "</initialMarking>\n";
        if (place.hasRestrictedCapacity()) {
            stream << space3 << "<capacity>\n";
            stream << space4 << "<value>Default," << place.getCapacity() << "</value>\n";
            stream << space3 << "</capacity>\n";
        }
        stream << space2 << "</place>\n";
    }

    // add immediate transitions
    for (const auto& trans : immediateTransitions) {
        stream << space2 << "<transition id=\"" << trans.getName() << "\">\n";
        stream << space3 << "<rate>\n";
        stream << space4 << "<value>" << trans.getWeight() << "</value>\n";
        stream << space3 << "</rate>\n";
        stream << space3 << "<timed>\n";
        stream << space4 << "<value>false</value>\n";
        stream << space3 << "</timed>\n";
        stream << space2 << "</transition>\n";
    }

    // add timed transitions
    for (const auto& trans : timedTransitions) {
        STORM_LOG_WARN_COND(trans.hasInfiniteServerSemantics(), "Unable to export non-trivial transition semantics");  // TODO
        stream << space2 << "<transition id=\"" << trans.getName() << "\">\n";
        stream << space3 << "<rate>\n";
        stream << space4 << "<value>" << trans.getRate() << "</value>\n";
        stream << space3 << "</rate>\n";
        stream << space3 << "<timed>\n";
        stream << space4 << "<value>true</value>\n";
        stream << space3 << "</timed>\n";
        stream << space2 << "</transition>\n";
    }

    uint64_t i = 0;
    // add arcs for immediate transitions
    for (const auto& trans : immediateTransitions) {
        // add input arcs
        for (auto const& inEntry : trans.getInputPlaces()) {
            stream << space2 << "<arc ";
            stream << "id=\"arc" << i++ << "\" ";
            stream << "source=\"" << places.at(inEntry.first).getName() << "\" ";
            stream << "target=\"" << trans.getName() << "\" ";
            stream << ">\n";

            stream << space3 << "<inscription>\n";
            stream << space4 << "<value>Default," << inEntry.second << "</value>\n";
            stream << space3 << "</inscription>\n";

            stream << space3 << "<type value=\"normal\" />\n";

            stream << space2 << "</arc>\n";
        }

        // add inhibition arcs
        for (auto const& inhEntry : trans.getInhibitionPlaces()) {
            stream << space2 << "<arc ";
            stream << "id=\"arc" << i++ << "\" ";
            stream << "source=\"" << places.at(inhEntry.first).getName() << "\" ";
            stream << "target=\"" << trans.getName() << "\" ";
            stream << ">\n";

            stream << space3 << "<inscription>\n";
            stream << space4 << "<value>Default," << inhEntry.second << "</value>\n";
            stream << space3 << "</inscription>\n";

            stream << space3 << "<type value=\"inhibition\" />\n";

            stream << space2 << "</arc>\n";
        }

        // add output arcs
        for (auto const& outEntry : trans.getOutputPlaces()) {
            stream << space2 << "<arc ";
            stream << "id=\"arc" << i++ << "\" ";
            stream << "source=\"" << trans.getName() << "\" ";
            stream << "target=\"" << places.at(outEntry.first).getName() << "\" ";
            stream << ">\n";

            stream << space3 << "<inscription>\n";
            stream << space4 << "<value>Default," << outEntry.second << "</value>\n";
            stream << space3 << "</inscription>\n";

            stream << space3 << "<type value=\"normal\" />\n";

            stream << space2 << "</arc>\n";
        }
    }

    // add arcs for immediate transitions
    for (const auto& trans : timedTransitions) {
        // add input arcs
        for (auto const& inEntry : trans.getInputPlaces()) {
            stream << space2 << "<arc ";
            stream << "id=\"arc" << i++ << "\" ";
            stream << "source=\"" << places.at(inEntry.first).getName() << "\" ";
            stream << "target=\"" << trans.getName() << "\" ";
            stream << ">\n";

            stream << space3 << "<inscription>\n";
            stream << space4 << "<value>Default," << inEntry.second << "</value>\n";
            stream << space3 << "</inscription>\n";

            stream << space3 << "<type value=\"normal\" />\n";

            stream << space2 << "</arc>\n";
        }

        // add inhibition arcs
        for (auto const& inhEntry : trans.getInhibitionPlaces()) {
            stream << space2 << "<arc ";
            stream << "id=\"arc" << i++ << "\" ";
            stream << "source=\"" << places.at(inhEntry.first).getName() << "\" ";
            stream << "target=\"" << trans.getName() << "\" ";
            stream << ">\n";

            stream << space3 << "<inscription>\n";
            stream << space4 << "<value>Default," << inhEntry.second << "</value>\n";
            stream << space3 << "</inscription>\n";

            stream << space3 << "<type value=\"inhibition\" />\n";

            stream << space2 << "</arc>\n";
        }

        // add output arcs
        for (auto const& outEntry : trans.getOutputPlaces()) {
            stream << space2 << "<arc ";
            stream << "id=\"arc" << i++ << "\" ";
            stream << "source=\"" << trans.getName() << "\" ";
            stream << "target=\"" << places.at(outEntry.first).getName() << "\" ";
            stream << ">\n";

            stream << space3 << "<inscription>\n";
            stream << space4 << "<value>Default," << outEntry.second << "</value>\n";
            stream << space3 << "</inscription>\n";

            stream << space3 << "<type value=\"normal\" />\n";

            stream << space2 << "</arc>\n";
        }
    }

    stream << space << "</net>\n";
    stream << "</pnml>\n";
}

void GSPN::toJson(std::ostream& stream) const {
    return storm::gspn::GspnJsonExporter::toStream(*this, stream);
}

void GSPN::writeStatsToStream(std::ostream& stream) const {
    stream << "Number of places: " << getNumberOfPlaces() << '\n';
    stream << "Number of timed transitions: " << getNumberOfTimedTransitions() << '\n';
    stream << "Number of immediate transitions: " << getNumberOfImmediateTransitions() << '\n';
}
}  // namespace gspn
}  // namespace storm
