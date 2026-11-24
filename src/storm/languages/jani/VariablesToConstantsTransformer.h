#pragma once

namespace storm {
namespace expressions {
class Expression;
}

namespace jani {
class Model;

class VariablesToConstantsTransformer {
   public:
    VariablesToConstantsTransformer() = default;

    /*!
     * Replaces each variable to which we never assign a value with a constant.
     */
    void transform(Model& model);
};
}  // namespace jani
}  // namespace storm
