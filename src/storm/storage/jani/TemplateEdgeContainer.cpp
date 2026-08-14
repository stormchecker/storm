#include "storm/storage/jani/TemplateEdgeContainer.h"
#include "storm/storage/jani/TemplateEdge.h"

namespace storm {
namespace jani {
TemplateEdgeContainer::TemplateEdgeContainer(TemplateEdgeContainer const& other) : std::unordered_set<std::shared_ptr<TemplateEdge>>(other) {
    this->clear();
    for (auto const& te : other) {
        this->insert(std::make_shared<TemplateEdge>(*te));
    }
}

TemplateEdgeContainer& TemplateEdgeContainer::operator=(const TemplateEdgeContainer& other) {
    this->clear();
    for (auto const& te : other) {
        this->insert(std::make_shared<TemplateEdge>(*te));
    }
    return *this;
}
}  // namespace jani
}  // namespace storm
