#pragma once

#include <boost/optional.hpp>
#include <map>
#include <memory>
#include <string>

#include "storm/storage/prism/Composition.h"

namespace storm {
namespace prism {
class RenamingComposition : public Composition {
   public:
    RenamingComposition(std::shared_ptr<Composition> const& sub, std::map<std::string, std::string> const& actionRenaming);

    virtual boost::any accept(CompositionVisitor& visitor, boost::any const& data) const override;

    Composition const& getSubcomposition() const;

    std::map<std::string, std::string> const& getActionRenaming() const;

   protected:
    virtual void writeToStream(std::ostream& stream) const override;

   private:
    std::shared_ptr<Composition> sub;

    // The renaming of action indices to apply. If the target name is none, the action is hidden.
    std::map<std::string, std::string> actionRenaming;
};
}  // namespace prism
}  // namespace storm
