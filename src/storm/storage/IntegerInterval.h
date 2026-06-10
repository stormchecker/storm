#pragma once

#include <boost/optional.hpp>
#include <cstdint>

namespace storm {
namespace storage {
class IntegerInterval {
   public:
    explicit IntegerInterval(int64_t v) : leftBound(v), rightBound(v) {}

    IntegerInterval(int64_t lb, int64_t rb) : leftBound(lb), rightBound(rb) {}

    bool hasLeftBound() const {
        return leftBound != boost::none;
    }

    bool hasRightBound() const {
        return rightBound != boost::none;
    }

    bool contains(int64_t val) const {
        if (hasLeftBound()) {
            if (val < leftBound.get()) {
                return false;
            }
        }
        if (hasRightBound()) {
            if (val > rightBound.get()) {
                return false;
            }
        }
        return true;
    }

    bool contains(IntegerInterval const& i) const {
        if (hasLeftBound()) {
            if (!i.hasLeftBound()) {
                return false;
            }
            if (leftBound.get() > i.getLeftBound().get()) {
                return false;
            }
        }
        if (hasRightBound()) {
            if (!i.hasRightBound()) {
                return false;
            }
            if (rightBound.get() < i.getRightBound().get()) {
                return false;
            }
        }
        return true;
    }

    void extend(int64_t val) {
        if (hasLeftBound()) {
            if (val < leftBound.get()) {
                leftBound = val;
                return;
            }
        }
        if (hasRightBound()) {
            if (val > rightBound.get()) {
                rightBound = val;
            }
        }
    }

    void extend(IntegerInterval const& i) {
        if (i.hasLeftBound()) {
            extend(i.getLeftBound().get());
        }
        if (i.hasRightBound()) {
            extend(i.getRightBound().get());
        }
    }

    boost::optional<int64_t> getLeftBound() const {
        return leftBound;
    }

    boost::optional<int64_t> getRightBound() const {
        return rightBound;
    }

   private:
    boost::optional<int64_t> leftBound;
    boost::optional<int64_t> rightBound;
};

std::ostream& operator<<(std::ostream& os, IntegerInterval const& i);
IntegerInterval parseIntegerInterval(std::string const& stringRepr);
}  // namespace storage
}  // namespace storm
