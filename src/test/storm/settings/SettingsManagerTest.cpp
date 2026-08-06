#include "test/storm_gtest.h"

#include "storm/exceptions/OptionParserException.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/ModuleSettings.h"

namespace {

class SettingsParsingTestModule : public storm::settings::modules::ModuleSettings {
   public:
    static const std::string moduleName;

    SettingsParsingTestModule() : ModuleSettings(moduleName) {
        this->addOption(storm::settings::OptionBuilder("settingsparsing", "optiona", false, "Test option a.")
                            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("value", "The value.").build())
                            .build());
        this->addOption(storm::settings::OptionBuilder("settingsparsing", "optionb", false, "Test option b.")
                            .setShortName("ob")
                            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("value", "The value.").build())
                            .build());
        this->addOption(storm::settings::OptionBuilder("settingsparsing", "optionc", false, "Test option c.")
                            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("value", "The value.").build())
                            .build());
        this->addOption(storm::settings::OptionBuilder("settingsparsing", "optiond", false, "Test option d.")
                            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("value", "The value.").build())
                            .build());
    }

    std::string getOptionString(std::string const& optionName) const {
        return this->getOption(optionName).getArgumentByName("value").getValueAsString();
    }
};

const std::string SettingsParsingTestModule::moduleName = "settingsparsing";

bool ensureSettingsParsingTestModuleRegistered() {
    if (!storm::settings::manager().hasModule("settingsparsing")) {
        storm::settings::addModule<SettingsParsingTestModule>();
    }
    return true;
}

bool settingsParsingTestModuleRegistered = ensureSettingsParsingTestModuleRegistered();

}  // namespace

TEST(SettingsManagerTest, ValueWithLeadingDashLongOption) {
    storm::settings::mutableManager().setFromExplodedString({"--optiona", "-1 <= x <= 1"});
    EXPECT_EQ("-1 <= x <= 1", storm::settings::getModule<SettingsParsingTestModule>().getOptionString("optiona"));
}

TEST(SettingsManagerTest, ValueWithLeadingDashShortOption) {
    storm::settings::mutableManager().setFromExplodedString({"-ob", "-1<=x<=1"});
    EXPECT_EQ("-1<=x<=1", storm::settings::getModule<SettingsParsingTestModule>().getOptionString("optionb"));
}

TEST(SettingsManagerTest, ValueWithLeadingDashFollowedByKnownOption) {
    storm::settings::mutableManager().setFromExplodedString({"--optionc", "-1 <= x <= 1", "--verbose"});
    EXPECT_EQ("-1 <= x <= 1", storm::settings::getModule<SettingsParsingTestModule>().getOptionString("optionc"));
}

TEST(SettingsManagerTest, KnownOptionNotSwallowedAsValue) {
    EXPECT_THROW({ storm::settings::mutableManager().setFromExplodedString({"--optiond", "--verbose"}); }, storm::exceptions::OptionParserException);
}
