#pragma once

#include <filesystem>
#include <memory>

#include "storm/io/umb/export/ExportOptions.h"
#include "storm/languages/umb/UmbModelForward.h"

namespace storm::umb {

void toArchive(storm::umb::UmbModel const& umbModel, std::filesystem::path const& archivePath, ExportOptions const& options = {});

}  // namespace storm::umb