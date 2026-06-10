#include "storm/io/DirectEncodingExporter.h"

#include <sstream>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/io/ArchiveWriter.h"
#include "storm/io/file.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace io {

template<typename ValueType>
void explicitExportSparseModel(std::filesystem::path const& filename, std::shared_ptr<storm::models::sparse::Model<ValueType>> sparseModel,
                               std::vector<std::string> const& parameters, DirectEncodingExporterOptions const& options) {
    auto const compression = (options.compression == CompressionMode::Default) ? getCompressionModeFromFileExtension(filename) : options.compression;
    if (compression == CompressionMode::None) {
        // No compression
        std::ofstream stream;
        storm::io::openFile(filename, stream);
        storm::io::explicitExportSparseModel(stream, sparseModel, parameters, options);
        storm::io::closeFile(stream);
    } else {
        // TODO: write directly to compressed file instead of having the entire file in memory first
        std::stringstream stream;
        storm::io::explicitExportSparseModel(stream, sparseModel, parameters, options);
        auto nameInArchive = filename.filename();
        if (getCompressionModeFromFileExtension(filename) != CompressionMode::None) {
            // remove compression extension from filename (e.g. model.drn.gz -> model.drn)
            nameInArchive.replace_extension();
        }
        storm::io::ArchiveWriter archiveWriter(filename, compression);
        archiveWriter.addTextFile(nameInArchive, stream.str());
    }
}

template<typename ValueType>
void explicitExportSparseModel(std::ostream& os, std::shared_ptr<storm::models::sparse::Model<ValueType>> sparseModel,
                               std::vector<std::string> const& parameters, DirectEncodingExporterOptions const& options) {
    // Handle output precision of doubles
    if (options.outputPrecision) {
        os.precision(*options.outputPrecision);
    }

    // Notice that for CTMCs we write the rate matrix instead of probabilities

    // Initialize
    std::vector<ValueType> exitRates;  // Only for CTMCs and MAs.
    if (sparseModel->getType() == storm::models::ModelType::Ctmc) {
        exitRates = sparseModel->template as<storm::models::sparse::Ctmc<ValueType>>()->getExitRateVector();
    } else if (sparseModel->getType() == storm::models::ModelType::MarkovAutomaton) {
        exitRates = sparseModel->template as<storm::models::sparse::MarkovAutomaton<ValueType>>()->getExitRates();
    }

    // Write header
    os << "// Exported by storm\n";
    os << "// Original model type: " << sparseModel->getType() << '\n';
    os << "@type: " << sparseModel->getType() << '\n';
    os << "@value_type: ";
    if constexpr (std::is_same_v<ValueType, double>) {
        os << "double";
    } else if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
        os << "rational";
    } else if constexpr (std::is_same_v<ValueType, storm::Interval>) {
        os << "double-interval";
    } else if constexpr (std::is_same_v<ValueType, storm::RationalInterval>) {
        os << "rational-interval";
    } else if constexpr (std::is_same_v<ValueType, storm::RationalFunction>) {
        os << "parametric";
    } else {
        STORM_LOG_ASSERT(false, "Unhandled value type");
    }
    os << '\n';
    os << "@parameters\n";
    if (parameters.empty()) {
        for (std::string const& parameter : getParameters(sparseModel)) {
            os << parameter << " ";
        }
    } else {
        for (std::string const& parameter : parameters) {
            os << parameter << " ";
        }
    }
    os << '\n';

    // Optionally write placeholders which only need to be parsed once
    // This is used to reduce the parsing effort for rational functions
    // Placeholders begin with the dollar symbol $
    std::unordered_map<ValueType, std::string> placeholders;
    if (options.allowPlaceholders) {
        placeholders = generatePlaceholders(sparseModel, exitRates);
    }
    if (!placeholders.empty()) {
        os << "@placeholders\n";
        for (auto const& entry : placeholders) {
            os << "$" << entry.second << " : " << entry.first << '\n';
        }
    }

    os << "@reward_models\n";
    for (auto const& rewardModel : sparseModel->getRewardModels()) {
        os << rewardModel.first << " ";
    }
    os << '\n';
    os << "@nr_states\n" << sparseModel->getNumberOfStates() << '\n';
    os << "@nr_choices\n" << sparseModel->getNumberOfChoices() << '\n';
    os << "@model\n";

    storm::storage::SparseMatrix<ValueType> const& matrix = sparseModel->getTransitionMatrix();

    // Iterate over states and export state information and outgoing transitions
    for (typename storm::storage::SparseMatrix<ValueType>::index_type group = 0; group < matrix.getRowGroupCount(); ++group) {
        os << "state " << group;

        // Write exit rates for CTMCs and MAs
        if (!exitRates.empty()) {
            os << " !";
            writeValue(os, exitRates.at(group), placeholders);
        }

        if (sparseModel->getType() == storm::models::ModelType::Pomdp) {
            os << " {" << sparseModel->template as<storm::models::sparse::Pomdp<ValueType>>()->getObservation(group) << "}";
        }

        // Write state rewards
        bool first = true;
        for (auto const& rewardModelEntry : sparseModel->getRewardModels()) {
            if (first) {
                os << " [";
                first = false;
            } else {
                os << ", ";
            }

            if (rewardModelEntry.second.hasStateRewards()) {
                writeValue(os, rewardModelEntry.second.getStateRewardVector().at(group), placeholders);
            } else {
                os << "0";
            }
        }

        if (!first) {
            os << "]";
        }

        // Write labels. Only labels with a whitespace are put in (double) quotation marks.
        for (auto const& label : sparseModel->getStateLabeling().getLabelsOfState(group)) {
            STORM_LOG_THROW(std::count(label.begin(), label.end(), '\"') == 0, storm::exceptions::NotSupportedException,
                            "Labels with quotation marks are not supported in the DRN format and therefore may not be exported.");
            // TODO consider escaping the quotation marks. Not sure whether that is a good idea.
            if (std::count_if(label.begin(), label.end(), isspace) > 0) {
                os << " \"" << label << "\"";
            } else {
                os << " " << label;
            }
        }
        os << '\n';
        // Write state valuations as comments
        if (sparseModel->hasStateValuations()) {
            os << "//" << sparseModel->getStateValuations().getStateInfo(group) << '\n';
        }

        // Write probabilities
        typename storm::storage::SparseMatrix<ValueType>::index_type start = matrix.hasTrivialRowGrouping() ? group : matrix.getRowGroupIndices()[group];
        typename storm::storage::SparseMatrix<ValueType>::index_type end = matrix.hasTrivialRowGrouping() ? group + 1 : matrix.getRowGroupIndices()[group + 1];

        // Iterate over all actions
        for (typename storm::storage::SparseMatrix<ValueType>::index_type row = start; row < end; ++row) {
            // Write choice
            if (sparseModel->hasChoiceLabeling()) {
                os << "\taction ";
                bool lfirst = true;
                if (sparseModel->getChoiceLabeling().getLabelsOfChoice(row).empty()) {
                    os << "__NOLABEL__";
                }
                for (auto const& label : sparseModel->getChoiceLabeling().getLabelsOfChoice(row)) {
                    if (!lfirst) {
                        os << "_";
                        lfirst = false;
                    }
                    os << label;
                }
            } else {
                os << "\taction " << row - start;
            }

            // Write action rewards
            bool first = true;
            for (auto const& rewardModelEntry : sparseModel->getRewardModels()) {
                if (first) {
                    os << " [";
                    first = false;
                } else {
                    os << ", ";
                }

                if (rewardModelEntry.second.hasStateActionRewards()) {
                    writeValue(os, rewardModelEntry.second.getStateActionRewardVector().at(row), placeholders);
                } else {
                    os << "0";
                }
            }
            if (!first) {
                os << "]";
            }
            os << '\n';

            // Write transitions
            for (auto it = matrix.begin(row); it != matrix.end(row); ++it) {
                ValueType prob = it->getValue();
                os << "\t\t" << it->getColumn() << " : ";
                writeValue(os, prob, placeholders);
                os << '\n';
            }
        }
    }  // end state iteration
}

template<typename ValueType>
std::vector<std::string> getParameters(std::shared_ptr<storm::models::sparse::Model<ValueType>>) {
    return {};
}

template<>
std::vector<std::string> getParameters(std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> sparseModel) {
    std::vector<std::string> parameters;
    std::set<storm::RationalFunctionVariable> parametersProb = storm::models::sparse::getProbabilityParameters(*sparseModel);
    std::set<storm::RationalFunctionVariable> parametersReward = storm::models::sparse::getRewardParameters(*sparseModel);
    parametersProb.insert(parametersReward.begin(), parametersReward.end());
    for (auto const& parameter : parametersProb) {
        std::stringstream stream;
        stream << parameter;
        parameters.push_back(stream.str());
    }
    return parameters;
}

template<typename ValueType>
std::unordered_map<ValueType, std::string> generatePlaceholders(std::shared_ptr<storm::models::sparse::Model<ValueType>>, std::vector<ValueType>) {
    return {};
}

/*!
 * Helper function to create a possible placeholder.
 * A new placeholder is inserted if the rational function is not constant and the function does not exist yet.
 * @param placeholders Existing placeholders.
 * @param value Value.
 * @param i Counter to enumerate placeholders.
 */

void createPlaceholder(std::unordered_map<storm::RationalFunction, std::string>& placeholders, storm::RationalFunction const& value, size_t& i) {
    if (!storm::utility::isConstant(value)) {
        auto ret = placeholders.insert(std::make_pair(value, std::to_string(i)));
        if (ret.second) {
            // New element was inserted
            ++i;
        }
    }
}

template<>
std::unordered_map<storm::RationalFunction, std::string> generatePlaceholders(
    std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> sparseModel, std::vector<storm::RationalFunction> exitRates) {
    std::unordered_map<storm::RationalFunction, std::string> placeholders;
    size_t i = 0;

    // Exit rates
    for (auto const& exitRate : exitRates) {
        createPlaceholder(placeholders, exitRate, i);
    }

    // Rewards
    for (auto const& rewardModelEntry : sparseModel->getRewardModels()) {
        if (rewardModelEntry.second.hasStateRewards()) {
            for (auto const& reward : rewardModelEntry.second.getStateRewardVector()) {
                createPlaceholder(placeholders, reward, i);
            }
        }
        if (rewardModelEntry.second.hasStateActionRewards()) {
            for (auto const& reward : rewardModelEntry.second.getStateActionRewardVector()) {
                createPlaceholder(placeholders, reward, i);
            }
        }
    }

    // Transition probabilities
    for (auto const& entry : sparseModel->getTransitionMatrix()) {
        createPlaceholder(placeholders, entry.getValue(), i);
    }

    return placeholders;
}

template<typename ValueType>
void writeValue(std::ostream& os, ValueType value, std::unordered_map<ValueType, std::string> const& placeholders) {
    if (storm::utility::isConstant(value)) {
        os << value;
        return;
    }

    // Try to use placeholder
    auto it = placeholders.find(value);
    if (it != placeholders.end()) {
        // Use placeholder
        os << "$" << it->second;
    } else {
        os << value;
    }
}

// Template instantiations
template void explicitExportSparseModel<double>(std::filesystem::path const& os, std::shared_ptr<storm::models::sparse::Model<double>> sparseModel,
                                                std::vector<std::string> const& parameters, DirectEncodingExporterOptions const& options);
template void explicitExportSparseModel<storm::RationalNumber>(std::filesystem::path const& os,
                                                               std::shared_ptr<storm::models::sparse::Model<storm::RationalNumber>> sparseModel,
                                                               std::vector<std::string> const& parameters, DirectEncodingExporterOptions const& options);
template void explicitExportSparseModel<storm::RationalFunction>(std::filesystem::path const& os,
                                                                 std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> sparseModel,
                                                                 std::vector<std::string> const& parameters, DirectEncodingExporterOptions const& options);
template void explicitExportSparseModel<storm::Interval>(std::filesystem::path const& os,
                                                         std::shared_ptr<storm::models::sparse::Model<storm::Interval>> sparseModel,
                                                         std::vector<std::string> const& parameters, DirectEncodingExporterOptions const& options);
template void explicitExportSparseModel<storm::RationalInterval>(std::filesystem::path const& os,
                                                                 std::shared_ptr<storm::models::sparse::Model<storm::RationalInterval>> sparseModel,
                                                                 std::vector<std::string> const& parameters, DirectEncodingExporterOptions const& options);

template void explicitExportSparseModel<double>(std::ostream& os, std::shared_ptr<storm::models::sparse::Model<double>> sparseModel,
                                                std::vector<std::string> const& parameters, DirectEncodingExporterOptions const& options);
template void explicitExportSparseModel<storm::RationalNumber>(std::ostream& os,
                                                               std::shared_ptr<storm::models::sparse::Model<storm::RationalNumber>> sparseModel,
                                                               std::vector<std::string> const& parameters, DirectEncodingExporterOptions const& options);
template void explicitExportSparseModel<storm::RationalFunction>(std::ostream& os,
                                                                 std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> sparseModel,
                                                                 std::vector<std::string> const& parameters, DirectEncodingExporterOptions const& options);
template void explicitExportSparseModel<storm::Interval>(std::ostream& os, std::shared_ptr<storm::models::sparse::Model<storm::Interval>> sparseModel,
                                                         std::vector<std::string> const& parameters, DirectEncodingExporterOptions const& options);
template void explicitExportSparseModel<storm::RationalInterval>(std::ostream& os,
                                                                 std::shared_ptr<storm::models::sparse::Model<storm::RationalInterval>> sparseModel,
                                                                 std::vector<std::string> const& parameters, DirectEncodingExporterOptions const& options);
}  // namespace io
}  // namespace storm
