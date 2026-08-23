#pragma once

#include <string>

#include "storm-dft/storage/DFT.h"

namespace storm::dft {
namespace api {

/*!
 * Load DFT from Galileo file.
 *
 * @param file File containing DFT description in Galileo format.
 * @return DFT.
 */
template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> loadDFTGalileoFile(std::string const& file);

/*!
 * Load DFT from JSON string.
 *
 * @param jsonString String containing DFT description in JSON format.
 * @return DFT.
 */
template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> loadDFTJsonString(std::string const& jsonString);

/*!
 * Load DFT from JSON file.
 *
 * @param file File containing DFT description in JSON format.
 * @return DFT.
 */
template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> loadDFTJsonFile(std::string const& file);

/*!
 * Export DFT to JSON file.
 *
 * @param dft DFT.
 * @param file File.
 */
template<typename ValueType>
void exportDFTToJsonFile(storm::dft::storage::DFT<ValueType> const& dft, std::string const& file);

/*!
 * Export DFT to JSON string.
 *
 * @param dft DFT.
 * @return DFT in JSON format.
 */
template<typename ValueType>
std::string exportDFTToJsonString(storm::dft::storage::DFT<ValueType> const& dft);

/*!
 * Export DFT to SMT encoding.
 *
 * @param dft DFT.
 * @param file File.
 */
template<typename ValueType>
void exportDFTToSMT(storm::dft::storage::DFT<ValueType> const& dft, std::string const& file);

}  // namespace api
}  // namespace storm::dft
