#pragma once

#include <utility>

#include "storm-dft/storage/DFT.h"
#include "storm-gspn/storage/gspn/GSPN.h"
#include "storm/storage/jani/Model.h"

namespace storm::dft {
namespace api {

/*!
 * Transform DFT to GSPN.
 *
 * @param dft DFT.
 * @return Pair of GSPN and id of failed place corresponding to the top level element.
 */
template<typename ValueType>
std::pair<std::shared_ptr<storm::gspn::GSPN>, uint64_t> transformToGSPN(storm::dft::storage::DFT<ValueType> const& dft);

/*!
 * Transform GSPN to Jani model.
 *
 * @param gspn GSPN.
 * @param toplevelFailedPlace Id of the failed place in the GSPN for the top level element in the DFT.
 * @return JANI model.
 */
std::shared_ptr<storm::jani::Model> transformToJani(storm::gspn::GSPN const& gspn, uint64_t toplevelFailedPlace);

}  // namespace api
}  // namespace storm::dft
