#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CounterexampleGeneratorSettings.h"
#include "test/storm_gtest.h"

int main(int argc, char **argv) {
    storm::settings::initializeAll("Storm-counterexamples (Functional) Testing Suite", "test-counterexamples");
    storm::settings::addModule<storm::settings::modules::CounterexampleGeneratorSettings>();
    ::testing::InitGoogleTest(&argc, argv);
    storm::test::initialize(&argc, argv);
    return RUN_ALL_TESTS();
}
