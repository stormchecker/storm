#pragma once

#include <ostream>

#include "storm/languages/jani/visitor/CompositionVisitor.h"

namespace storm {
namespace jani {

class Composition {
   public:
    virtual ~Composition() = default;

    virtual boost::any accept(CompositionVisitor& visitor, boost::any const& data) const = 0;

    virtual void write(std::ostream& stream) const = 0;

    virtual bool isAutomatonComposition() const;
    AutomatonComposition const& asAutomatonComposition() const;

    virtual bool isParallelComposition() const;
    ParallelComposition const& asParallelComposition() const;

    friend std::ostream& operator<<(std::ostream& stream, Composition const& composition);
};

}  // namespace jani
}  // namespace storm
