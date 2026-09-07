#pragma once

#include <boost/optional.hpp>
#include <cstdint>
#include <map>
#include <set>

#include "storm/storage/jani/visitor/CompositionVisitor.h"

namespace storm {
namespace jani {

class Model;

class CompositionInformation {
   public:
    CompositionInformation();

    void increaseAutomatonMultiplicity(std::string const& automatonName, uint64_t count = 1);
    std::map<std::string, uint64_t> const& getAutomatonToMultiplicityMap() const;
    static std::map<std::string, uint64_t> joinMultiplicityMaps(std::map<std::string, uint64_t> const& first, std::map<std::string, uint64_t> const& second);
    void addMultiplicityMap(std::map<std::string, uint64_t> const& multiplicityMap);

    void setContainsNonStandardParallelComposition(bool value);
    bool containsNonStandardParallelComposition() const;

    void setContainsNestedParallelComposition(bool value);
    bool containsNestedParallelComposition() const;

    void setContainsParallelComposition(bool value);
    bool containsParallelComposition() const;

    std::string const& getActionName(uint64_t index) const;
    uint64_t getActionIndex(std::string const& name) const;

    void addNonSilentActionIndex(uint64_t index);
    void addNonSilentActionIndices(std::set<uint64_t> const& indices);
    bool hasNonSilentActionIndex(uint64_t index);
    void addInputEnabledActionIndex(uint64_t index);

    std::set<uint64_t> const& getNonSilentActionIndices() const;
    std::set<uint64_t> const& getInputEnabledActionIndices() const;

    void setMappings(std::map<uint64_t, std::string> const& indexToNameMap, std::map<std::string, uint64_t> const& nameToIndexMap);

   private:
    /// The indices of the non-silent actions appearing in the topmost element of the composition.
    std::set<uint64_t> nonSilentActionIndices;

    /// The set of indices of actions for which the topmost element of the composition is input-enabled.
    std::set<uint64_t> inputEnabledActionIndices;

    /// A mapping from action indices to names. Since the composition may introduce new action names, this may
    /// extend the one from the underlying model.
    std::map<uint64_t, std::string> indexToNameMap;

    /// A mapping from action names to their indices.
    std::map<std::string, uint64_t> nameToIndexMap;

    /// A mapping from the automata's names to the amount of times they occur in the composition.
    std::map<std::string, uint64_t> automatonNameToMultiplicity;

    /// A flag indicating whether the composition contains any non-standard parallel composition.
    bool nonStandardParallelComposition;

    /// A flag indicating whether the composition contains nested parallel compositions;
    bool nestedParallelComposition;

    /// A flag indicating whether the composition contains a parallel composition;
    bool parallelComposition;
};

class CompositionInformationVisitor : public CompositionVisitor {
   public:
    CompositionInformationVisitor(Model const& model, Composition const& composition);
    CompositionInformation getInformation();

    virtual boost::any visit(AutomatonComposition const& composition, boost::any const& data) override;
    virtual boost::any visit(ParallelComposition const& composition, boost::any const& data) override;

   private:
    uint64_t addOrGetActionIndex(std::string const& name);

    storm::jani::Model const& model;
    Composition const& composition;
    uint64_t nextFreeActionIndex;
    std::map<std::string, uint64_t> nameToIndexMap;
    std::map<uint64_t, std::string> indexToNameMap;
};

}  // namespace jani
}  // namespace storm
