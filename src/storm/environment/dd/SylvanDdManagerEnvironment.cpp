#include "storm/environment/dd/SylvanDdManagerEnvironment.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/SylvanSettings.h"
#include "storm/utility/OsDetection.h"
#include "storm/utility/macros.h"
#include "storm/utility/threads.h"

namespace storm {

SylvanDdManagerEnvironment::SylvanDdManagerEnvironment() : maximalMemory(4096), numberOfThreads(0) {
    if (storm::settings::hasModule<storm::settings::modules::SylvanSettings>()) {
        auto const& sylvanSettings = storm::settings::getModule<storm::settings::modules::SylvanSettings>();
        maximalMemory = sylvanSettings.getMaximalMemory();
        numberOfThreads = sylvanSettings.getNumberOfThreads();
    }
}

SylvanDdManagerEnvironment::~SylvanDdManagerEnvironment() {
    // Intentionally left empty
}

uint64_t SylvanDdManagerEnvironment::getMaximalMemory() const {
    return maximalMemory;
}

void SylvanDdManagerEnvironment::setMaximalMemory(uint64_t value) {
    maximalMemory = value;
}

uint64_t SylvanDdManagerEnvironment::getNumberOfThreads() const {
    if (numberOfThreads == 0) {
        // Automatic detection
#ifdef ARM
        // Prevents issues with multi-threaded execution on ARM architecture
        return 1ul;
#else
        return std::max(UINT64_C(1), storm::utility::getNumberOfThreads());
#endif
    }
    return numberOfThreads;
}

void SylvanDdManagerEnvironment::setNumberOfThreads(uint64_t value) {
    numberOfThreads = value;
}

}  // namespace storm
