#include "CommandLineParser.h"

#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <vector>

namespace {

using Siligen::Application::CommandLineConfig;
using Siligen::Application::CommandLineParser;
using Siligen::Application::CommandType;

CommandLineConfig ParseArgs(std::initializer_list<const char*> args) {
    std::vector<std::string> storage;
    storage.reserve(args.size());
    for (const char* arg : args) {
        storage.emplace_back(arg);
    }

    std::vector<char*> argv;
    argv.reserve(storage.size());
    for (auto& arg : storage) {
        argv.push_back(arg.data());
    }

    return CommandLineParser::Parse(static_cast<int>(argv.size()), argv.data());
}

}  // namespace

TEST(CommandLineParserTest, DxfPreviewSnapshotEnablesInterpolationPlannerByDefault) {
    const auto config = ParseArgs({
        "siligen_planner_cli",
        "dxf-preview-snapshot",
        "--file",
        "sample.dxf",
    });

    EXPECT_EQ(config.command, CommandType::DXF_PREVIEW_SNAPSHOT);
    EXPECT_TRUE(config.use_interpolation_planner);
}

TEST(CommandLineParserTest, DxfPreviewSnapshotAllowsExplicitInterpolationDisable) {
    const auto config = ParseArgs({
        "siligen_planner_cli",
        "dxf-preview-snapshot",
        "--file",
        "sample.dxf",
        "--no-interpolation-planner",
    });

    EXPECT_EQ(config.command, CommandType::DXF_PREVIEW_SNAPSHOT);
    EXPECT_FALSE(config.use_interpolation_planner);
}

TEST(CommandLineParserTest, DxfPlanKeepsInterpolationPlannerDisabledByDefault) {
    const auto config = ParseArgs({
        "siligen_planner_cli",
        "dxf-plan",
        "--file",
        "sample.dxf",
    });

    EXPECT_EQ(config.command, CommandType::DXF_PLAN);
    EXPECT_FALSE(config.use_interpolation_planner);
}
