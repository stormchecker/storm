This version of [nlohmann/json](https://github.com/nlohmann/json) has been adapted by the Storm developers so that we
are able to parse floating point numbers using exact arithmetic.

For documentation on how to update the library version see [here](/doc/3rdparty_dependencies.md).

The major changes are:

 * `json_value` now stores the `number_float` as a pointer, if `number_float_t` is not trivial.
 * Conversion operations between number representations are done using storm's utility functions.
 * Type traits like `std::is_scalar`, `std::is_trivial`, and `std::is_arithmetic_v` are replaced by [custom definitions](include/nlohmann/storm_utility.hpp).
