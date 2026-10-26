#pragma once

#include <memory>
#include <unordered_set>

namespace storm {
namespace jani {

class TemplateEdge;

struct TemplateEdgeContainer : public std::unordered_set<std::shared_ptr<TemplateEdge>> {
    TemplateEdgeContainer() = default;
    TemplateEdgeContainer(TemplateEdgeContainer const& other);
    TemplateEdgeContainer& operator=(TemplateEdgeContainer const& other);
};
}  // namespace jani
}  // namespace storm