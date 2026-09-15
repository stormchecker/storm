#pragma once

#include <utility>
#include <vector>

#include "storm-dft/storage/DFT.h"
#include "storm-gspn/storage/gspn/GSPN.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/Property.h"

namespace storm::dft {
namespace api {

/*!
 * Transform DFT to GSPN.
 *
 * @param dft DFT.
 * @param disableDC Whether Don't Care propagation is disabled.
 * @param extendPriorities Whether priorities should be extended.
 * @param smartTransformation Whether the smart transformation is used.
 * @param mergeDCFailed Whether Don't Care and failed places are merged.
 * @return Pair of GSPN and id of failed place corresponding to the top level element.
 */
template<typename ValueType>
std::pair<std::shared_ptr<storm::gspn::GSPN>, uint64_t> transformToGSPN(storm::dft::storage::DFT<ValueType> const& dft, bool disableDC, bool extendPriorities,
                                                                        bool smartTransformation, bool mergeDCFailed);

/*!
 * Transform GSPN to Jani model.
 *
 * @param gspn GSPN.
 * @param toplevelFailedPlace Id of the failed place in the GSPN for the top level element in the DFT.
 * @return Pair of JANI model and the standard properties built for it.
 */
std::pair<std::shared_ptr<storm::jani::Model>, std::vector<storm::jani::Property>> transformToJani(storm::gspn::GSPN const& gspn, uint64_t toplevelFailedPlace);

}  // namespace api
}  // namespace storm::dft
