#pragma once

#include <string>

#include "storm/storage/prism/Composition.h"

namespace storm {
namespace prism {
class ModuleComposition : public Composition {
   public:
    ModuleComposition(std::string const& moduleName);

    virtual boost::any accept(CompositionVisitor& visitor, boost::any const& data) const override;

    std::string const& getModuleName() const;

   protected:
    virtual void writeToStream(std::ostream& stream) const override;

   private:
    // The name of the module to compose.
    std::string moduleName;
};
}  // namespace prism
}  // namespace storm
