#pragma once

#include <iosfwd>

namespace storm {
namespace logic {

enum class FormulaContext { Undefined, Probability, Reward, LongRunAverage, Time };
std::ostream& operator<<(std::ostream& out, FormulaContext const& formulaContext);
}  // namespace logic
}  // namespace storm
