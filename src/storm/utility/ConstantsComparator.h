#pragma once

namespace storm::utility {

template<typename ValueType>
class ConstantsComparator {
   public:
    ConstantsComparator(ValueType const& precision, bool relative = false);
    bool isOne(ValueType const& value) const;
    bool isZero(ValueType const& value) const;
    bool isEqual(ValueType const& value1, ValueType const& value2) const;
    bool isLess(ValueType const& value1, ValueType const& value2) const;

   private:
    ValueType precision;
    bool relative;
};
}  // namespace storm::utility
