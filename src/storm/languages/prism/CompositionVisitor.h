#pragma once

#include <boost/any.hpp>

namespace storm {
namespace prism {

class ModuleComposition;
class RenamingComposition;
class HidingComposition;
class SynchronizingParallelComposition;
class InterleavingParallelComposition;
class RestrictedParallelComposition;

class CompositionVisitor {
   public:
    virtual ~CompositionVisitor() = default;

    virtual boost::any visit(ModuleComposition const& composition, boost::any const& data) = 0;
    virtual boost::any visit(RenamingComposition const& composition, boost::any const& data) = 0;
    virtual boost::any visit(HidingComposition const& composition, boost::any const& data) = 0;
    virtual boost::any visit(SynchronizingParallelComposition const& composition, boost::any const& data) = 0;
    virtual boost::any visit(InterleavingParallelComposition const& composition, boost::any const& data) = 0;
    virtual boost::any visit(RestrictedParallelComposition const& composition, boost::any const& data) = 0;
};
}  // namespace prism
}  // namespace storm
