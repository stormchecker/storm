#pragma once

#include <boost/variant.hpp>

#include "storm/expressions/Variable.h"
#include "storm/languages/jani/Variable.h"

namespace storm {
namespace expressions {
class Expression;
}

namespace jani {
class Model;
class Property;

struct ArrayEliminatorData {
    class Replacement {
       public:
        Replacement(storm::jani::Variable const& variable);
        Replacement(std::vector<Replacement>&& replacements = {});
        bool isVariable() const;
        storm::jani::Variable const& getVariable() const;
        std::vector<Replacement> const& getReplacements() const;
        Replacement const& at(std::size_t const& index) const;                 /// assumes this is not a variable
        Replacement& at(std::size_t const& index);                             /// assumes this is not a variable
        Replacement const& at(std::vector<std::size_t> const& indices) const;  /// equivalent to .at(i_1).at(i_2). ... .at(i_n) if indices = {i_1,i_2, ... i_n}
        Replacement& at(std::vector<std::size_t> const& indices);              /// equivalent to .at(i_1).at(i_2). ... .at(i_n) if indices = {i_1,i_2, ... i_n}
        std::size_t size() const;                                              /// assumes this is not a variable
        void grow(std::size_t const& minimumSize);                             /// assumes this is not a variable

       private:
        boost::variant<storm::jani::Variable const*, std::vector<Replacement>> data;
    };
    std::vector<std::shared_ptr<Variable>> eliminatedArrayVariables;
    std::unordered_map<storm::expressions::Variable, Replacement> replacements;

    // Transforms the given expression (which might contain array expressions) to an equivalent expression without array variables.
    storm::expressions::Expression transformExpression(storm::expressions::Expression const& arrayExpression) const;
    // Transforms the given property (which might contain array expressions) to an equivalent property without array variables.
    void transformProperty(storm::jani::Property& property) const;
};

class ArrayEliminator {
   public:
    ArrayEliminator() = default;

    /*!
     * Eliminates all array references in the given model by replacing them with basic variables.
     */

    ArrayEliminatorData eliminate(Model& model, bool keepNonTrivialArrayAccess = false);
};
}  // namespace jani
}  // namespace storm
