#pragma once

#include <utility>
#include <vector>

#include "storm-dft/storage/DFT.h"
#include "storm-gspn/storage/gspn/GSPN.h"

// Forward declarations
namespace storm {
namespace jani {
class Model;
class Property;
}  // namespace jani
}  // namespace storm

namespace storm::dft {
namespace api {

/*!
 * Transform DFT to GSPN.
 *
 * @param dft DFT.
 * @param enableDC Whether Don't Care propagation is enabled.
 * @param extendPriorities Whether calculation of transition priorities is enabled.
 * @param enableSmartTransformation Whether the smart transformation is enabled.
 * @param mergeDCFailed Whether Don't Care and Failed places are merged into a combined place.
 * @return Pair of GSPN and id of failed place corresponding to the top level element.
 */
template<typename ValueType>
std::pair<std::shared_ptr<storm::gspn::GSPN>, uint64_t> transformToGSPN(storm::dft::storage::DFT<ValueType> const& dft, bool enableDC, bool extendPriorities,
                                                                        bool enableSmartTransformation, bool mergeDCFailed);

/*!
 * Transform GSPN to Jani model.
 *
 * @param gspn GSPN.
 * @param toplevelFailedPlace Id of the failed place in the GSPN for the top level element in the DFT.
 * @param addDeadlockProperties Whether deadlock properties are added to the returned properties.
 * @return Pair of JANI model and its properties.
 */
std::pair<std::shared_ptr<storm::jani::Model>, std::vector<storm::jani::Property>> transformToJani(storm::gspn::GSPN const& gspn, uint64_t toplevelFailedPlace,
                                                                                                   bool addDeadlockProperties = false);

}  // namespace api
}  // namespace storm::dft
