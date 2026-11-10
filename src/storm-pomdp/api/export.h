#pragma once

#include <string>

namespace storm::pomdp {
namespace policy {
template<typename ValueType>
class ObservationBasedFiniteStateController;
}

namespace api {

/**
 * Exports an observation-based finite-state controller to a JSON file.
 *
 * @param policy The controller to export.
 * @param filename The output filename. An existing file is overwritten.
 * @throws storm::exceptions::FileIoException If the output file cannot be opened.
 */
template<typename ValueType>
void exportPolicyAsJson(storm::pomdp::policy::ObservationBasedFiniteStateController<ValueType> const& policy, std::string const& filename);

/**
 * Exports an observation-based finite-state controller to a Graphviz DOT file.
 *
 * @param policy The controller to export.
 * @param filename The output filename. An existing file is overwritten.
 * @throws storm::exceptions::FileIoException If the output file cannot be opened.
 */
template<typename ValueType>
void exportPolicyAsDot(storm::pomdp::policy::ObservationBasedFiniteStateController<ValueType> const& policy, std::string const& filename);

}  // namespace api
}  // namespace storm::pomdp
