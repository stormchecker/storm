#pragma once

#include "storm/expressions/Expression.h"

namespace storm {
namespace logic {

class TimeBound {
   public:
    TimeBound(bool strict, storm::expressions::Expression const& bound);

    storm::expressions::Expression const& getBound() const;
    bool isStrict() const;

   private:
    bool strict;
    storm::expressions::Expression bound;
};

}  // namespace logic
}  // namespace storm
