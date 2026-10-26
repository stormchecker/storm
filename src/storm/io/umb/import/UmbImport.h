#pragma once

#include <filesystem>
#include <memory>

#include "storm/io/umb/import/ImportOptions.h"
#include "storm/languages/umb/UmbModelForward.h"

namespace storm::umb {

storm::umb::UmbModel importUmb(std::filesystem::path const& umbLocation, ImportOptions const& options = {});

}  // namespace storm::umb