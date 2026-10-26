#pragma once

#include <memory>

#include "storm/languages/prism/CompositionVisitor.h"

namespace storm {
namespace jani {
class Composition;
class Model;
}  // namespace jani

namespace prism {
class Composition;

class CompositionToJaniVisitor : public CompositionVisitor {
   public:
    std::shared_ptr<storm::jani::Composition> toJani(Composition const& composition, storm::jani::Model const& model);

    virtual boost::any visit(ModuleComposition const& composition, boost::any const& data) override;
    virtual boost::any visit(RenamingComposition const& composition, boost::any const& data) override;
    virtual boost::any visit(HidingComposition const& composition, boost::any const& data) override;
    virtual boost::any visit(SynchronizingParallelComposition const& composition, boost::any const& data) override;
    virtual boost::any visit(InterleavingParallelComposition const& composition, boost::any const& data) override;
    virtual boost::any visit(RestrictedParallelComposition const& composition, boost::any const& data) override;
};

}  // namespace prism
}  // namespace storm
