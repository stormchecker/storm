#include "storm/storage/jani/EdgeContainer.h"
#include "storm/storage/jani/Edge.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/TemplateEdge.h"
#include "storm/storage/jani/Variable.h"

namespace storm {
namespace jani {
namespace detail {
Edges::Edges(iterator it, iterator ite) : it(it), ite(ite) {
    // Intentionally left empty.
}

Edges::iterator Edges::begin() const {
    return it;
}

Edges::iterator Edges::end() const {
    return ite;
}

bool Edges::empty() const {
    return it == ite;
}

std::size_t Edges::size() const {
    return std::distance(it, ite);
}

ConstEdges::ConstEdges(const_iterator it, const_iterator ite) : it(it), ite(ite) {
    // Intentionally left empty.
}

ConstEdges::const_iterator ConstEdges::begin() const {
    return it;
}

ConstEdges::const_iterator ConstEdges::end() const {
    return ite;
}

bool ConstEdges::empty() const {
    return it == ite;
}

std::size_t ConstEdges::size() const {
    return std::distance(it, ite);
}
}  // namespace detail

EdgeContainer::EdgeContainer(EdgeContainer const& other) {
    edges = other.getConcreteEdges();
    // templates = other.templates;
    std::map<std::shared_ptr<TemplateEdge>, std::shared_ptr<TemplateEdge>> map;
    for (auto const& te : other.templates) {
        auto newTe = std::make_shared<TemplateEdge>(*te);
        this->templates.insert(newTe);
        map[te] = newTe;
    }

    for (auto& e : edges) {
        if (map.count(e.getTemplateEdge()) == 0) {
            e.setTemplateEdge(std::make_shared<TemplateEdge>(*(e.getTemplateEdge())));
        } else {
            e.setTemplateEdge(map[e.getTemplateEdge()]);
        }
    }
}

EdgeContainer& EdgeContainer::operator=(EdgeContainer const& other) {
    EdgeContainer otherCpy(other);
    this->templates = std::move(otherCpy.templates);
    this->edges = std::move(otherCpy.edges);
    return *this;
}

void EdgeContainer::finalize(Model const& containingModel) {
    templates.clear();
    for (auto& edge : edges) {
        templates.insert(edge.getTemplateEdge());
    }
    for (auto& templateEdge : templates) {
        templateEdge->finalize(containingModel);
    }
}

void EdgeContainer::clearConcreteEdges() {
    edges.clear();
}

void EdgeContainer::liftTransientDestinationAssignments(int64_t maxLevel) {
    for (auto& templateEdge : templates) {
        templateEdge->liftTransientDestinationAssignments(maxLevel);
    }
}

void EdgeContainer::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution,
                               bool const substituteTranscendentalNumbers) {
    for (auto& templateEdge : templates) {
        templateEdge->substitute(substitution, substituteTranscendentalNumbers);
    }
    for (auto& edge : edges) {
        edge.substitute(substitution, substituteTranscendentalNumbers);
    }
}

bool EdgeContainer::isLinear() const {
    for (auto const& templateEdge : templates) {
        if (!templateEdge->isLinear()) {
            return false;
        }
    }
    return true;
}

bool EdgeContainer::usesAssignmentLevels(bool onlyTransient) const {
    for (auto const& edge : edges) {
        if (edge.usesAssignmentLevels(onlyTransient)) {
            return true;
        }
    }
    return false;
}

std::vector<Edge>& EdgeContainer::getConcreteEdges() {
    return edges;
}

std::vector<Edge> const& EdgeContainer::getConcreteEdges() const {
    return edges;
}

TemplateEdgeContainer const& EdgeContainer::getTemplateEdges() const {
    return templates;
}

std::set<uint64_t> EdgeContainer::getActionIndices() const {
    std::set<uint64_t> result;
    for (auto const& edge : edges) {
        result.insert(edge.getActionIndex());
    }
    return result;
}

/**
 * Insert an edge, then sort the range between locstart and locend according to the action index.
 * @param e
 * @param locStart index where to start
 * @param locEnd index where to end
 */
void EdgeContainer::insertEdge(Edge const& e, uint64_t locStart, uint64_t locEnd) {
    STORM_LOG_ASSERT(locStart <= locEnd, "Invalid edge location range.");
    // Find the right position for the edge and insert it properly.
    auto posIt = edges.begin();
    std::advance(posIt, locEnd);
    edges.insert(posIt, e);

    // Sort all edges form the source location of the newly introduced edge by their action indices.
    auto it = edges.begin();
    std::advance(it, locStart);
    auto ite = edges.begin();
    std::advance(ite, locEnd + 1);
    std::sort(it, ite, [](Edge const& a, Edge const& b) { return a.getActionIndex() < b.getActionIndex(); });
}

void EdgeContainer::insertTemplateEdge(std::shared_ptr<TemplateEdge> const& te) {
    templates.insert(te);
}

void EdgeContainer::pushAssignmentsToDestinations() {
    for (auto& templateEdge : templates) {
        templateEdge->pushAssignmentsToDestinations();
    }
}

void EdgeContainer::changeAssignmentVariables(std::map<Variable const*, std::reference_wrapper<Variable const>> const& remapping) {
    for (auto& templateEdge : templates) {
        templateEdge->changeAssignmentVariables(remapping);
    }
}

size_t EdgeContainer::size() const {
    return edges.size();
}

EdgeContainer::iterator EdgeContainer::begin() {
    return edges.begin();
}
EdgeContainer::iterator EdgeContainer::end() {
    return edges.end();
}
EdgeContainer::const_iterator EdgeContainer::begin() const {
    return edges.begin();
}
EdgeContainer::const_iterator EdgeContainer::end() const {
    return edges.end();
}

}  // namespace jani
}  // namespace storm