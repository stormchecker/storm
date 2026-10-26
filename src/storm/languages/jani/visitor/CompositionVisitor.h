#pragma once

#include <boost/any.hpp>

namespace storm {
namespace jani {

class Composition;
class AutomatonComposition;
class ParallelComposition;

class CompositionVisitor {
   public:
    virtual ~CompositionVisitor() = default;

    virtual boost::any visit(AutomatonComposition const& composition, boost::any const& data) = 0;
    virtual boost::any visit(ParallelComposition const& composition, boost::any const& data) = 0;
};

}  // namespace jani
}  // namespace storm
