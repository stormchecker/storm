#include "CompositionToJaniVisitor.h"

#include "storm/storage/jani/Compositions.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/visitor/CompositionInformationVisitor.h"
#include "storm/storage/prism/Compositions.h"

namespace storm {
namespace prism {

std::shared_ptr<storm::jani::Composition> CompositionToJaniVisitor::toJani(Composition const& composition, storm::jani::Model const& model) {
    auto result = boost::any_cast<std::shared_ptr<storm::jani::Composition>>(composition.accept(*this, model));
    return result;
}

boost::any CompositionToJaniVisitor::visit(ModuleComposition const& composition, boost::any const&) {
    std::shared_ptr<storm::jani::Composition> result = std::make_shared<storm::jani::AutomatonComposition>(composition.getModuleName());
    return result;
}

boost::any CompositionToJaniVisitor::visit(RenamingComposition const& composition, boost::any const& data) {
    std::vector<storm::jani::SynchronizationVector> synchronizationVectors;
    for (auto const& renamingPair : composition.getActionRenaming()) {
        synchronizationVectors.push_back(storm::jani::SynchronizationVector({renamingPair.first}, renamingPair.second));
    }
    std::shared_ptr<storm::jani::Composition> result = std::make_shared<storm::jani::ParallelComposition>(
        boost::any_cast<std::shared_ptr<storm::jani::Composition>>(composition.getSubcomposition().accept(*this, data)), synchronizationVectors);
    return result;
}

boost::any CompositionToJaniVisitor::visit(HidingComposition const& composition, boost::any const& data) {
    std::vector<storm::jani::SynchronizationVector> synchronizationVectors;
    for (auto const& action : composition.getActionsToHide()) {
        synchronizationVectors.push_back(storm::jani::SynchronizationVector({action}, storm::jani::Model::SILENT_ACTION_NAME));
    }
    std::shared_ptr<storm::jani::Composition> result = std::make_shared<storm::jani::ParallelComposition>(
        boost::any_cast<std::shared_ptr<storm::jani::Composition>>(composition.getSubcomposition().accept(*this, data)), synchronizationVectors);
    return result;
}

boost::any CompositionToJaniVisitor::visit(SynchronizingParallelComposition const& composition, boost::any const& data) {
    auto leftSubcomposition = boost::any_cast<std::shared_ptr<storm::jani::Composition>>(composition.getLeftSubcomposition().accept(*this, data));
    auto rightSubcomposition = boost::any_cast<std::shared_ptr<storm::jani::Composition>>(composition.getRightSubcomposition().accept(*this, data));

    storm::jani::Model const& model = boost::any_cast<storm::jani::Model const&>(data);
    storm::jani::CompositionInformation leftActionInformation = storm::jani::CompositionInformationVisitor(model, *leftSubcomposition).getInformation();
    storm::jani::CompositionInformation rightActionInformation = storm::jani::CompositionInformationVisitor(model, *rightSubcomposition).getInformation();

    std::set<std::string> leftActions;
    for (auto const& actionIndex : leftActionInformation.getNonSilentActionIndices()) {
        leftActions.insert(leftActionInformation.getActionName(actionIndex));
    }
    std::set<std::string> rightActions;
    for (auto const& actionIndex : rightActionInformation.getNonSilentActionIndices()) {
        rightActions.insert(rightActionInformation.getActionName(actionIndex));
    }

    std::set<std::string> commonActions;
    std::set_intersection(leftActions.begin(), leftActions.end(), rightActions.begin(), rightActions.end(),
                          std::inserter(commonActions, commonActions.begin()));

    std::shared_ptr<storm::jani::Composition> result =
        std::make_shared<storm::jani::ParallelComposition>(leftSubcomposition, rightSubcomposition, commonActions);
    return result;
}

boost::any CompositionToJaniVisitor::visit(InterleavingParallelComposition const& composition, boost::any const& data) {
    auto leftSubcomposition = boost::any_cast<std::shared_ptr<storm::jani::Composition>>(composition.getLeftSubcomposition().accept(*this, data));
    auto rightSubcomposition = boost::any_cast<std::shared_ptr<storm::jani::Composition>>(composition.getRightSubcomposition().accept(*this, data));
    std::shared_ptr<storm::jani::Composition> result =
        std::make_shared<storm::jani::ParallelComposition>(leftSubcomposition, rightSubcomposition, std::set<std::string>());
    return result;
}

boost::any CompositionToJaniVisitor::visit(RestrictedParallelComposition const& composition, boost::any const& data) {
    auto leftSubcomposition = boost::any_cast<std::shared_ptr<storm::jani::Composition>>(composition.getLeftSubcomposition().accept(*this, data));
    auto rightSubcomposition = boost::any_cast<std::shared_ptr<storm::jani::Composition>>(composition.getRightSubcomposition().accept(*this, data));
    std::shared_ptr<storm::jani::Composition> result =
        std::make_shared<storm::jani::ParallelComposition>(leftSubcomposition, rightSubcomposition, composition.getSynchronizingActions());
    return result;
}
}  // namespace prism
}  // namespace storm
