#include "test/storm_gtest.h"

#include "storm/environment/Environment.h"
#include "storm/environment/dd/CuddDdManagerEnvironment.h"
#include "storm/environment/dd/DdEnvironment.h"
#include "storm/environment/modelchecker/ModelCheckerEnvironment.h"
#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"
#include "storm/settings/EnvironmentBuilder.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CuddSettings.h"
#include "storm/settings/modules/MultiObjectiveSettings.h"

// storm-pars does not register every settings module; building an environment must keep the defaults for the missing ones.
TEST(EnvironmentBuilderTest, MissingModulesKeepDefaults) {
    ASSERT_FALSE(storm::settings::hasModule<storm::settings::modules::MultiObjectiveSettings>());
    ASSERT_FALSE(storm::settings::hasModule<storm::settings::modules::CuddSettings>());

    storm::Environment const defaultEnv;
    storm::Environment builtEnv;
    ASSERT_NO_THROW(builtEnv = storm::settings::EnvironmentBuilder::buildEnvironment());
    EXPECT_EQ(defaultEnv.modelchecker().multi().getMethod(), builtEnv.modelchecker().multi().getMethod());
    EXPECT_EQ(defaultEnv.modelchecker().multi().getPrecision(), builtEnv.modelchecker().multi().getPrecision());
}
