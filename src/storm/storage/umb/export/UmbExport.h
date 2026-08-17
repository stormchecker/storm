#pragma once

#include <filesystem>
#include <memory>

#include "storm/storage/umb/export/ExportOptions.h"
#include "storm/storage/umb/model/UmbModelForward.h"

namespace storm::umb {

void toArchive(storm::umb::UmbModel const& umbModel, std::filesystem::path const& archivePath, ExportOptions const& options = {});

}  // namespace storm::umb