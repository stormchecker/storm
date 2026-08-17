#pragma once

#include "storm/storage/prism/ParallelComposition.h"

namespace storm {
namespace prism {
class InterleavingParallelComposition : public ParallelComposition {
   public:
    InterleavingParallelComposition(std::shared_ptr<Composition> const& left, std::shared_ptr<Composition> const& right);

    virtual boost::any accept(CompositionVisitor& visitor, boost::any const& data) const override;

   protected:
    virtual void writeToStream(std::ostream& stream) const override;
};
}  // namespace prism
}  // namespace storm
