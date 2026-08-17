#include "GspnJsonExporter.h"

#include <algorithm>
#include <string>

#include "storm/adapters/JsonAdapter.h"
#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/NotImplementedException.h"

namespace storm {
namespace gspn {

// Prevent some magic constants
static constexpr uint64_t scaleFactor = 50;

void GspnJsonExporter::toStream(storm::gspn::GSPN const& gspn, std::ostream& os) {
    os << storm::dumpJson(translate(gspn)) << '\n';
}

typename GspnJsonExporter::Json GspnJsonExporter::translate(storm::gspn::GSPN const& gspn) {
    Json jsonGspn;

    // Layouts
    std::map<uint64_t, LayoutInfo> placeLayout = gspn.getPlaceLayoutInfos();
    std::map<uint64_t, LayoutInfo> transitionLayout = gspn.getTransitionLayoutInfos();
    double tmpX = 0;
    double tmpY = 10;

    // Export places
    for (const auto& place : gspn.getPlaces()) {
        double x = tmpX;
        double y = tmpY;
        if (placeLayout.count(place.getID()) > 0) {
            x = placeLayout.at(place.getID()).x;
            y = placeLayout.at(place.getID()).y;
        }
        tmpX += 3;
        Json jsonPlace = translatePlace(place, x, y);
        jsonGspn.push_back(jsonPlace);
    }

    // Export immediate transitions
    for (const auto& transition : gspn.getImmediateTransitions()) {
        double x = tmpX;
        double y = tmpY;
        if (transitionLayout.count(transition.getID()) > 0) {
            x = transitionLayout.at(transition.getID()).x;
            y = transitionLayout.at(transition.getID()).y;
        }
        tmpX += 3;
        Json jsonImmediateTransition = translateImmediateTransition(transition, x, y);
        jsonGspn.push_back(jsonImmediateTransition);
    }

    // Export timed transitions
    for (const auto& transition : gspn.getTimedTransitions()) {
        double x = tmpX;
        double y = tmpY;
        if (transitionLayout.count(transition.getID()) > 0) {
            x = transitionLayout.at(transition.getID()).x;
            y = transitionLayout.at(transition.getID()).y;
        }
        tmpX += 3;
        Json jsonTimedTransition = translateTimedTransition(transition, x, y);
        jsonGspn.push_back(jsonTimedTransition);
    }

    // Export arcs
    std::vector<storm::gspn::Place> places = gspn.getPlaces();
    // Export arcs for immediate transitions
    for (const auto& transition : gspn.getImmediateTransitions()) {
        // Export input arcs
        for (auto const& entry : transition.getInputPlaces()) {
            storm::gspn::Place place = places.at(entry.first);
            Json jsonInputArc = translateArc(transition, place, entry.second, true, ArcType::INPUT);
            jsonGspn.push_back(jsonInputArc);
        }

        // Export inhibitor arcs
        for (auto const& entry : transition.getInhibitionPlaces()) {
            storm::gspn::Place place = places.at(entry.first);
            Json jsonInputArc = translateArc(transition, place, entry.second, true, ArcType::INHIBITOR);
            jsonGspn.push_back(jsonInputArc);
        }

        // Export output arcs
        for (auto const& entry : transition.getOutputPlaces()) {
            storm::gspn::Place place = places.at(entry.first);
            Json jsonInputArc = translateArc(transition, place, entry.second, true, ArcType::OUTPUT);
            jsonGspn.push_back(jsonInputArc);
        }
    }
    // Export arcs for timed transitions
    for (const auto& transition : gspn.getTimedTransitions()) {
        // Export input arcs
        for (auto const& entry : transition.getInputPlaces()) {
            storm::gspn::Place place = places.at(entry.first);
            Json jsonInputArc = translateArc(transition, place, entry.second, false, ArcType::INPUT);
            jsonGspn.push_back(jsonInputArc);
        }

        // Export inhibitor arcs
        for (auto const& entry : transition.getInhibitionPlaces()) {
            storm::gspn::Place place = places.at(entry.first);
            Json jsonInputArc = translateArc(transition, place, entry.second, false, ArcType::INHIBITOR);
            jsonGspn.push_back(jsonInputArc);
        }

        // Export output arcs
        for (auto const& entry : transition.getOutputPlaces()) {
            storm::gspn::Place place = places.at(entry.first);
            Json jsonInputArc = translateArc(transition, place, entry.second, false, ArcType::OUTPUT);
            jsonGspn.push_back(jsonInputArc);
        }
    }
    return jsonGspn;
}

typename GspnJsonExporter::Json GspnJsonExporter::translatePlace(storm::gspn::Place const& place, double x, double y) {
    Json data;
    data["id"] = toJsonString(place);
    data["name"] = place.getName();
    data["marking"] = place.getNumberOfInitialTokens();

    Json position;
    position["x"] = x * scaleFactor;
    position["y"] = y * scaleFactor;

    Json jsonPlace;
    jsonPlace["data"] = data;
    jsonPlace["position"] = position;
    jsonPlace["group"] = "nodes";
    jsonPlace["classes"] = "place";
    return jsonPlace;
}

typename GspnJsonExporter::Json GspnJsonExporter::translateImmediateTransition(storm::gspn::ImmediateTransition<double> const& transition, double x, double y) {
    Json data;
    data["id"] = toJsonString(transition, true);
    data["name"] = transition.getName();
    data["priority"] = transition.getPriority();
    data["weight"] = transition.getWeight();

    Json position;
    position["x"] = x * scaleFactor;
    position["y"] = y * scaleFactor;

    Json jsonTrans;
    jsonTrans["data"] = data;
    jsonTrans["position"] = position;
    jsonTrans["group"] = "nodes";
    jsonTrans["classes"] = "trans_im";
    return jsonTrans;
}

typename GspnJsonExporter::Json GspnJsonExporter::translateTimedTransition(storm::gspn::TimedTransition<double> const& transition, double x, double y) {
    Json data;
    data["id"] = toJsonString(transition, false);
    data["name"] = transition.getName();
    data["rate"] = transition.getRate();
    data["priority"] = transition.getPriority();
    if (!transition.hasSingleServerSemantics()) {
        if (transition.hasInfiniteServerSemantics()) {
            data["server-semantics"] = "infinite";
        } else if (transition.hasKServerSemantics()) {
            data["server-semantics"] = transition.getNumberOfServers();
        } else {
            STORM_LOG_WARN("Unable to export transition semantics.");
        }
    }

    Json position;
    position["x"] = x * scaleFactor;
    position["y"] = y * scaleFactor;

    Json jsonTrans;
    jsonTrans["data"] = data;
    jsonTrans["position"] = position;
    jsonTrans["group"] = "nodes";
    jsonTrans["classes"] = "trans_time";
    return jsonTrans;
}

typename GspnJsonExporter::Json GspnJsonExporter::translateArc(storm::gspn::Transition const& transition, storm::gspn::Place const& place,
                                                               uint64_t multiplicity, bool immediate, ArcType arctype) {
    Json data;
    data["id"] = toJsonString(transition, place, arctype);
    data["source"] = toJsonString(place);
    data["target"] = toJsonString(transition, immediate);
    data["mult"] = multiplicity;

    Json jsonArc;
    jsonArc["data"] = data;
    // jsonTrans["group"] = "nodes";
    switch (arctype) {
        case INPUT:
            jsonArc["classes"] = "input";
            break;
        case OUTPUT:
            jsonArc["classes"] = "output";
            break;
        case INHIBITOR:
            jsonArc["classes"] = "inhibit";
            break;
        default:
            STORM_LOG_ASSERT(false, "Unknown type " << arctype << " used.");
    }
    return jsonArc;
}

}  // namespace gspn
}  // namespace storm
