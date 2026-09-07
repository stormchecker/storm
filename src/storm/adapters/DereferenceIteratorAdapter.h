#pragma once

#include <boost/iterator/transform_iterator.hpp>
#include <type_traits>

namespace storm {
namespace adapters {

template<typename T>
struct Dereferencer {
    decltype((*std::declval<T>())) operator()(T const& t) const {
        return *t;
    }
};

template<typename ContainerType>
class DereferenceIteratorAdapter {
   public:
    typedef typename ContainerType::value_type value_type;
    typedef typename std::conditional<std::is_const<ContainerType>::value, typename ContainerType::const_iterator, typename ContainerType::iterator>::type
        input_iterator;
    typedef typename boost::transform_iterator<Dereferencer<value_type>, input_iterator> iterator;

    DereferenceIteratorAdapter(input_iterator it, input_iterator ite) : it(it), ite(ite) {
        // Intentionally left empty.
    }

    iterator begin() const {
        return boost::make_transform_iterator(it, Dereferencer<value_type>());
    }

    iterator end() const {
        return boost::make_transform_iterator(ite, Dereferencer<value_type>());
    }

    static iterator make_iterator(input_iterator it) {
        return boost::make_transform_iterator(it, Dereferencer<value_type>());
    }

    bool empty() const {
        return it == ite;
    }

    decltype((*std::declval<value_type>())) front() const {
        return **it;
    }

    decltype((*std::declval<value_type>())) back() const {
        return **(ite - 1);
    }

    std::size_t size() const {
        return std::distance(it, ite);
    }

   private:
    input_iterator it;
    input_iterator ite;
};

}  // namespace adapters
}  // namespace storm
