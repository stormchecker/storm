#include "Valuation.h"

#include "storm/storage/expressions/ExpressionManager.h"

namespace storm {
namespace expressions {
Valuation::Valuation(std::shared_ptr<ExpressionManager const> const& manager) : manager(manager) {
    // Intentionally left empty.
}

Valuation::~Valuation() {
    // Intentionally left empty.
}

ExpressionManager const& Valuation::getManager() const {
    return *manager;
}

std::shared_ptr<ExpressionManager const> const& Valuation::getManagerAsSharedPtr() const {
    return manager;
}

void Valuation::setManager(std::shared_ptr<ExpressionManager const> const& manager) {
    this->manager = manager;
}

}  // namespace expressions
}  // namespace storm
