#pragma once

#include <set>

#include "Composition.h"

namespace storm {
namespace jani {

class AutomatonComposition : public Composition {
   public:
    /*!
     * Creates a reference to an automaton to be used in a composition.
     */
    AutomatonComposition(std::string const& name, std::set<std::string> const& inputEnabledActions = {});

    /*!
     * Retrieves the name of the automaton this composition element refers to.
     */
    std::string const& getAutomatonName() const;

    virtual boost::any accept(CompositionVisitor& visitor, boost::any const& data) const override;

    virtual void write(std::ostream& stream) const override;

    std::set<std::string> const& getInputEnabledActions() const;

    virtual bool isAutomatonComposition() const override;

   private:
    /// The name of the automaton this composition element refers to.
    std::string name;

    /// The names of the actions for which to make this automaton input-enabled.
    std::set<std::string> inputEnabledActions;
};

}  // namespace jani
}  // namespace storm
