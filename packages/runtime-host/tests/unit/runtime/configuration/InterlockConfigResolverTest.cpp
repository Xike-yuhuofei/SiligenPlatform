#include "runtime/configuration/InterlockConfigResolver.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

using Siligen::Infrastructure::Configuration::InterlockConfigResolution;
using Siligen::Infrastructure::Configuration::ResolveInterlockConfigFromIni;

std::filesystem::path WriteTempIni(const std::string& suffix, const std::string& content) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() /
                ("siligen_interlock_config_" + suffix + "_" + std::to_string(now) + ".ini");
    std::ofstream out(path, std::ios::binary);
    out << content;
    out.close();
    return path;
}

}  // namespace

TEST(InterlockConfigResolverTest, UsesExplicitInterlockSectionWhenPresent) {
    const auto ini_path = WriteTempIni(
        "explicit",
        "[Safety]\n"
        "emergency_stop_enabled=false\n"
        "\n"
        "[Interlock]\n"
        "enabled=true\n"
        "emergency_stop_input=4\n"
        "safety_door_input=7\n"
        "poll_interval_ms=25\n");

    const InterlockConfigResolution resolution = ResolveInterlockConfigFromIni(ini_path.string());

    EXPECT_TRUE(resolution.explicit_interlock_section);
    EXPECT_FALSE(resolution.used_compat_fallback);
    EXPECT_TRUE(resolution.warnings.empty());
    EXPECT_TRUE(resolution.config.enabled);
    EXPECT_EQ(resolution.config.emergency_stop_input, 4);
    EXPECT_TRUE(resolution.config.emergency_stop_active_low);
    EXPECT_EQ(resolution.config.safety_door_input, 7);
    EXPECT_EQ(resolution.config.poll_interval_ms, 25);

    std::error_code ec;
    std::filesystem::remove(ini_path, ec);
}

TEST(InterlockConfigResolverTest, AllowsDoorInputToBeExplicitlyDisabled) {
    const auto ini_path = WriteTempIni(
        "explicit_disabled_door",
        "[Safety]\n"
        "emergency_stop_enabled=true\n"
        "\n"
        "[Interlock]\n"
        "enabled=true\n"
        "emergency_stop_input=7\n"
        "safety_door_input=-1\n"
        "poll_interval_ms=50\n");

    const InterlockConfigResolution resolution = ResolveInterlockConfigFromIni(ini_path.string());

    EXPECT_TRUE(resolution.explicit_interlock_section);
    EXPECT_FALSE(resolution.used_compat_fallback);
    EXPECT_TRUE(resolution.warnings.empty());
    EXPECT_TRUE(resolution.config.enabled);
    EXPECT_EQ(resolution.config.emergency_stop_input, 7);
    EXPECT_TRUE(resolution.config.emergency_stop_active_low);
    EXPECT_EQ(resolution.config.safety_door_input, -1);
    EXPECT_EQ(resolution.config.poll_interval_ms, 50);

    std::error_code ec;
    std::filesystem::remove(ini_path, ec);
}

TEST(InterlockConfigResolverTest, FallsBackToSafetyAndDefaultBitsWhenInterlockSectionMissing) {
    const auto ini_path = WriteTempIni(
        "fallback_missing_section",
        "[Safety]\n"
        "emergency_stop_enabled=true\n");

    const InterlockConfigResolution resolution = ResolveInterlockConfigFromIni(ini_path.string());

    EXPECT_FALSE(resolution.explicit_interlock_section);
    EXPECT_TRUE(resolution.used_compat_fallback);
    EXPECT_TRUE(resolution.config.enabled);
    EXPECT_EQ(resolution.config.emergency_stop_input, 0);
    EXPECT_TRUE(resolution.config.emergency_stop_active_low);
    EXPECT_EQ(resolution.config.safety_door_input, 1);
    ASSERT_FALSE(resolution.warnings.empty());
    EXPECT_NE(resolution.warnings.front().find("compatibility fallback"), std::string::npos);

    std::error_code ec;
    std::filesystem::remove(ini_path, ec);
}

TEST(InterlockConfigResolverTest, FallsBackWhenRequiredInterlockKeysAreInvalid) {
    const auto ini_path = WriteTempIni(
        "fallback_invalid_required",
        "[Safety]\n"
        "emergency_stop_enabled=true\n"
        "\n"
        "[Interlock]\n"
        "enabled=true\n"
        "emergency_stop_input=invalid\n");

    const InterlockConfigResolution resolution = ResolveInterlockConfigFromIni(ini_path.string());

    EXPECT_TRUE(resolution.explicit_interlock_section);
    EXPECT_TRUE(resolution.used_compat_fallback);
    EXPECT_TRUE(resolution.config.enabled);
    EXPECT_EQ(resolution.config.emergency_stop_input, 0);
    EXPECT_TRUE(resolution.config.emergency_stop_active_low);
    EXPECT_EQ(resolution.config.safety_door_input, 1);
    ASSERT_FALSE(resolution.warnings.empty());
    EXPECT_NE(resolution.warnings.front().find("required keys"), std::string::npos);

    std::error_code ec;
    std::filesystem::remove(ini_path, ec);
}

TEST(InterlockConfigResolverTest, AllowsEmergencyStopPolarityToBeConfigured) {
    const auto ini_path = WriteTempIni(
        "explicit_estop_polarity",
        "[Safety]\n"
        "emergency_stop_enabled=true\n"
        "\n"
        "[Interlock]\n"
        "enabled=true\n"
        "emergency_stop_input=7\n"
        "emergency_stop_active_low=false\n"
        "safety_door_input=-1\n");

    const InterlockConfigResolution resolution = ResolveInterlockConfigFromIni(ini_path.string());

    EXPECT_TRUE(resolution.explicit_interlock_section);
    EXPECT_FALSE(resolution.used_compat_fallback);
    EXPECT_TRUE(resolution.warnings.empty());
    EXPECT_TRUE(resolution.config.enabled);
    EXPECT_EQ(resolution.config.emergency_stop_input, 7);
    EXPECT_FALSE(resolution.config.emergency_stop_active_low);
    EXPECT_EQ(resolution.config.safety_door_input, -1);

    std::error_code ec;
    std::filesystem::remove(ini_path, ec);
}
