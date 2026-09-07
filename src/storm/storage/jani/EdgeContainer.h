#pragma once

#include <map>
#include <set>
#include <vector>

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/jani/TemplateEdgeContainer.h"

namespace storm {
namespace jani {

class Edge;
class TemplateEdge;
class Variable;
class Model;

namespace detail {
class Edges {
   public:
    typedef std::vector<Edge>::iterator iterator;
    typedef std::vector<Edge>::const_iterator const_iterator;

    Edges(iterator it, iterator ite);

    /*!
     * Retrieves an iterator to the edges.
     */
    iterator begin() const;

    /*!
     * Retrieves an end iterator to the edges.
     */
    iterator end() const;

    /*!
     * Determines whether this set of edges is empty.
     */
    bool empty() const;

    /*!
     * Retrieves the number of edges.
     */
    std::size_t size() const;

   private:
    iterator it;
    iterator ite;
};

class ConstEdges {
   public:
    typedef std::vector<Edge>::iterator iterator;
    typedef std::vector<Edge>::const_iterator const_iterator;

    ConstEdges(const_iterator it, const_iterator ite);

    /*!
     * Retrieves an iterator to the edges.
     */
    const_iterator begin() const;

    /*!
     * Retrieves an end iterator to the edges.
     */
    const_iterator end() const;

    /*!
     * Determines whether this set of edges is empty.
     */
    bool empty() const;

    /*!
     * Retrieves the number of edges.
     */
    std::size_t size() const;

   private:
    const_iterator it;
    const_iterator ite;
};
}  // namespace detail

class EdgeContainer {
   public:
    typedef std::vector<Edge>::iterator iterator;
    typedef std::vector<Edge>::const_iterator const_iterator;

    EdgeContainer() = default;
    EdgeContainer(EdgeContainer const& other);
    EdgeContainer& operator=(EdgeContainer const& other);

    void clearConcreteEdges();
    std::vector<Edge> const& getConcreteEdges() const;
    std::vector<Edge>& getConcreteEdges();
    TemplateEdgeContainer const& getTemplateEdges() const;

    size_t size() const;

    iterator begin();
    const_iterator begin() const;
    iterator end();
    const_iterator end() const;

    std::set<uint64_t> getActionIndices() const;

    void substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution, bool const substituteTranscendentalNumbers);
    void liftTransientDestinationAssignments(int64_t maxLevel = 0);
    void pushAssignmentsToDestinations();
    void insertEdge(Edge const& e, uint64_t locStart, uint64_t locEnd);
    void insertTemplateEdge(std::shared_ptr<TemplateEdge> const& te);
    bool isLinear() const;
    bool usesAssignmentLevels(bool onlyTransient = false) const;
    void finalize(Model const& containingModel);

    void changeAssignmentVariables(std::map<Variable const*, std::reference_wrapper<Variable const>> const& remapping);

   private:
    std::vector<Edge> edges;
    TemplateEdgeContainer templates;
};
}  // namespace jani
}  // namespace storm
