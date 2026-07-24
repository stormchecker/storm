#pragma once

#include <set>
#include <string>

#include "storm/storage/prism/ParallelComposition.h"

namespace storm {
namespace prism {
class RestrictedParallelComposition : public ParallelComposition {
   public:
    RestrictedParallelComposition(std::shared_ptr<Composition> const& left, std::set<std::string> const& synchronizingActions,
                                  std::shared_ptr<Composition> const& right);

    virtual boost::any accept(CompositionVisitor& visitor, boost::any const& data) const override;

    std::set<std::string> const& getSynchronizingActions() const;

   protected:
    virtual void writeToStream(std::ostream& stream) const override;

   private:
    std::set<std::string> synchronizingActions;
};
}  // namespace prism
}  // namespace storm
