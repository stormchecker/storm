#pragma once

#include <filesystem>
#include <memory>

#include "storm/storage/umb/import/ImportOptions.h"
#include "storm/storage/umb/model/UmbModelForward.h"

namespace storm::umb {

storm::umb::UmbModel importUmb(std::filesystem::path const& umbLocation, ImportOptions const& options = {});

}  // namespace storm::umb