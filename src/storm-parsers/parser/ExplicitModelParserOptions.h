#pragma once

namespace storm {
namespace parser {

struct ExplicitModelParserOptions {
    bool fixDeadlocks = true;
    bool buildChoiceLabels = false;
};

}  // namespace parser
}  // namespace storm
