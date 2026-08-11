#include "storm/storage/jani/Edge.h"

#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/utility/macros.h"

#include "storm/storage/jani/TemplateEdge.h"

namespace storm {
namespace jani {

Edge::Edge(uint64_t sourceLocationIndex, uint64_t actionIndex, boost::optional<storm::expressions::Expression> const& rate,
           std::shared_ptr<TemplateEdge> const& templateEdge,
           std::vector<std::pair<uint64_t, storm::expressions::Expression>> const& destinationTargetLocationsAndProbabilities)
    : sourceLocationIndex(sourceLocationIndex), actionIndex(actionIndex), rate(rate), templateEdge(templateEdge) {
    // Create the concrete destinations from the template edge.
    STORM_LOG_THROW(templateEdge->getNumberOfDestinations() == destinationTargetLocationsAndProbabilities.size(), storm::exceptions::InvalidArgumentException,
                    "Sizes of template edge destinations and target locations mismatch.");
    for (uint64_t i = 0; i < templateEdge->getNumberOfDestinations(); ++i) {
        auto const& templateDestination = templateEdge->getDestination(i);
        destinations.emplace_back(destinationTargetLocationsAndProbabilities[i].first, destinationTargetLocationsAndProbabilities[i].second,
                                  templateDestination);
    }
}

Edge::Edge(uint64_t sourceLocationIndex, uint64_t actionIndex, boost::optional<storm::expressions::Expression> const& rate,
           std::shared_ptr<TemplateEdge> const& templateEdge, std::vector<uint64_t> const& destinationLocations,
           std::vector<storm::expressions::Expression> const& destinationProbabilities)
    : sourceLocationIndex(sourceLocationIndex), actionIndex(actionIndex), rate(rate), templateEdge(templateEdge) {
    // Create the concrete destinations from the template edge.
    STORM_LOG_THROW(templateEdge->getNumberOfDestinations() == destinationLocations.size() && destinationLocations.size() == destinationProbabilities.size(),
                    storm::exceptions::InvalidArgumentException, "Sizes of template edge destinations and target locations mismatch.");
    for (uint64_t i = 0; i < templateEdge->getNumberOfDestinations(); ++i) {
        auto const& templateDestination = templateEdge->getDestination(i);
        destinations.emplace_back(destinationLocations[i], destinationProbabilities[i], templateDestination);
    }
}

uint64_t Edge::getSourceLocationIndex() const {
    return sourceLocationIndex;
}

uint64_t Edge::getActionIndex() const {
    return actionIndex;
}

bool Edge::hasRate() const {
    return static_cast<bool>(rate);
}

storm::expressions::Expression const& Edge::getRate() const {
    return rate.get();
}

boost::optional<storm::expressions::Expression> const& Edge::getOptionalRate() const {
    return rate;
}

void Edge::setRate(storm::expressions::Expression const& rate) {
    this->rate = rate;
}

storm::expressions::Expression const& Edge::getGuard() const {
    return templateEdge->getGuard();
}

void Edge::setGuard(const expressions::Expression& guard) {
    templateEdge->setGuard(guard);
}

EdgeDestination const& Edge::getDestination(uint64_t index) const {
    return destinations[index];
}

std::vector<EdgeDestination> const& Edge::getDestinations() const {
    return destinations;
}

std::vector<EdgeDestination>& Edge::getDestinations() {
    return destinations;
}

std::size_t Edge::getNumberOfDestinations() const {
    return destinations.size();
}

OrderedAssignments const& Edge::getAssignments() const {
    return templateEdge->getAssignments();
}

void Edge::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution, bool const substituteTranscendentalNumbers) {
    if (this->hasRate()) {
        this->setRate(substituteJaniExpression(this->getRate(), substitution, substituteTranscendentalNumbers));
    }
    for (auto& destination : destinations) {
        destination.substitute(substitution, substituteTranscendentalNumbers);
    }
}

bool Edge::hasSilentAction() const {
    return actionIndex == Model::SILENT_ACTION_INDEX;
}

uint64_t Edge::getColor() const {
    return this->color;
}

void Edge::setColor(uint64_t newColor) {
    this->color = newColor;
}

storm::storage::FlatSet<storm::expressions::Variable> const& Edge::getWrittenGlobalVariables() const {
    return templateEdge->getWrittenGlobalVariables();
}

bool Edge::usesVariablesInNonTransientAssignments(std::set<storm::expressions::Variable> const& variables) const {
    return templateEdge->usesVariablesInNonTransientAssignments(variables);
}

bool Edge::hasTransientEdgeDestinationAssignments() const {
    return templateEdge->hasTransientEdgeDestinationAssignments();
}

bool Edge::usesAssignmentLevels(bool onlyTransient) const {
    return templateEdge->usesAssignmentLevels(onlyTransient);
}

void Edge::simplifyIndexedAssignments(VariableSet const& localVars) {
    if (usesAssignmentLevels()) {
        templateEdge = std::make_shared<TemplateEdge>(templateEdge->simplifyIndexedAssignments(!hasSilentAction(), localVars));
        std::vector<EdgeDestination> newdestinations;
        STORM_LOG_ASSERT(templateEdge->getNumberOfDestinations() == destinations.size(), "Destination count mismatch.");
        for (uint64_t i = 0; i < templateEdge->getNumberOfDestinations(); ++i) {
            auto const& templateDestination = templateEdge->getDestination(i);
            newdestinations.emplace_back(destinations[i].getLocationIndex(), destinations[i].getProbability(), templateDestination);
        }
        destinations = newdestinations;
    }
}

int64_t const& Edge::getLowestAssignmentLevel() const {
    return templateEdge->getLowestAssignmentLevel();
}

int64_t const& Edge::getHighestAssignmentLevel() const {
    return templateEdge->getHighestAssignmentLevel();
}

void Edge::setTemplateEdge(std::shared_ptr<TemplateEdge> const& newTe) {
    templateEdge = newTe;
    uint64_t i = 0;
    std::vector<EdgeDestination> newdestinations;

    STORM_LOG_ASSERT(destinations.size() == newTe->getNumberOfDestinations(), "Destination count mismatch.");
    for (auto& destination : destinations) {
        newdestinations.emplace_back(destination.getLocationIndex(), destination.getProbability(), newTe->getDestination(i));
        // destination.updateTemplateEdgeDestination(newTe->getDestination(i));
        ++i;
    }
    destinations = newdestinations;
}

std::string Edge::toString() const {
    std::stringstream ss;
    ss << *this;
    return ss.str();
}

std::shared_ptr<TemplateEdge> const& Edge::getTemplateEdge() {
    return templateEdge;
}

std::ostream& operator<<(std::ostream& stream, Edge const& edge) {
    stream << "[" << (edge.hasSilentAction() ? "" : ("action_id: " + std::to_string(edge.getActionIndex()))) << "]";
    stream << "guard: '" << edge.getGuard() << "'\t from_location_id: " << edge.getSourceLocationIndex();
    if (edge.hasRate()) {
        stream << " with rate '" << edge.getRate() << "'";
    }
    if (edge.getDestinations().empty()) {
        stream << "without any destination";
    } else {
        stream << " to ... [\n";
        for (auto const& dest : edge.getDestinations()) {
            stream << "\tlocation_id: " << dest.getLocationIndex() << " with probability '" << dest.getProbability() << "' and updates: ";
            if (dest.getOrderedAssignments().empty()) {
                stream << "none\n";
            }
            bool first = true;
            for (auto const& a : dest.getOrderedAssignments()) {
                if (first) {
                    first = false;
                    stream << a;
                }
                stream << ", " << a;
            }
        }
        stream << "]";
    }
    if (edge.getColor() != 0) {
        stream << " color: " << edge.getColor();
    }
    return stream;
}
}  // namespace jani
}  // namespace storm
