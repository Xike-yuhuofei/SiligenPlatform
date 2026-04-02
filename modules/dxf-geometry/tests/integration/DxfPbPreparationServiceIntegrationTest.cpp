#include "support/DxfPbPreparationServiceTestSupport.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

namespace {

using Siligen::DxfGeometry::Tests::Support::ArgsPathFor;
using Siligen::DxfGeometry::Tests::Support::DxfPbPreparationService;
using Siligen::DxfGeometry::Tests::Support::FakeConfigurationPort;
using Siligen::DxfGeometry::Tests::Support::MinimalDxf;
using Siligen::DxfGeometry::Tests::Support::PbPathFor;
using Siligen::DxfGeometry::Tests::Support::ReadTextFile;
using Siligen::DxfGeometry::Tests::Support::ScopedCurrentPath;
using Siligen::DxfGeometry::Tests::Support::ScopedTempDir;
using Siligen::DxfGeometry::Tests::Support::ScopedUnsetEnvVar;
using Siligen::DxfGeometry::Tests::Support::WriteTextFile;

std::string CaptureArgsScript() {
    return "import pathlib\n"
           "import sys\n"
           "args = sys.argv[1:]\n"
           "output = pathlib.Path(args[args.index('--output') + 1])\n"
           "output.write_bytes(b'workspace-pb')\n"
           "output.with_suffix('.args.txt').write_text('\\n'.join(f'arg[{i}]={arg}' for i, arg in enumerate(args)), encoding='utf-8')\n";
}

TEST(DxfPbPreparationServiceIntegrationTest, ResolvesWorkspaceScriptAndAppliesConfigurationFlags) {
    ScopedTempDir workspace("dxf_pb_workspace_integration");
    const auto workspace_root = workspace.Path() / "workspace";
    const auto nested_cwd = workspace_root / "build" / "bin" / "Debug";
    const auto uploads_dir = workspace_root / "uploads";
    const auto script_path = workspace_root / "scripts" / "engineering-data" / "dxf_to_pb.py";
    const auto dxf_path = uploads_dir / "sample.dxf";
    const auto pb_path = PbPathFor(dxf_path);
    const auto args_path = ArgsPathFor(pb_path);

    std::filesystem::create_directories(nested_cwd);
    std::filesystem::create_directories(uploads_dir);
    std::filesystem::create_directories(script_path.parent_path());
    WriteTextFile(dxf_path, MinimalDxf());
    WriteTextFile(script_path, CaptureArgsScript());

    auto config = std::make_shared<FakeConfigurationPort>();
    config->preprocess_config.normalize_units = false;
    config->preprocess_config.strict_r12 = true;
    config->preprocess_config.densify_enabled = true;
    config->preprocess_config.spline_samples = 8;

    const ScopedUnsetEnvVar unset_script("SILIGEN_DXF_PB_SCRIPT");
    const ScopedUnsetEnvVar unset_command("SILIGEN_DXF_PB_COMMAND");
    const ScopedUnsetEnvVar unset_root("SILIGEN_DXF_PROJECT_ROOT");
    const ScopedCurrentPath current_path(nested_cwd);

    DxfPbPreparationService service(config);
    auto result = service.EnsurePbReady(dxf_path.string());
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    ASSERT_TRUE(std::filesystem::exists(pb_path));
    ASSERT_TRUE(std::filesystem::exists(args_path));
    EXPECT_EQ(ReadTextFile(pb_path), "workspace-pb");

    const auto args_snapshot = ReadTextFile(args_path);
    EXPECT_NE(args_snapshot.find("--no-normalize-units"), std::string::npos);
    EXPECT_NE(args_snapshot.find("--strict-r12"), std::string::npos);
    EXPECT_NE(args_snapshot.find("--densify-enabled"), std::string::npos);
    EXPECT_NE(args_snapshot.find("arg[11]=8"), std::string::npos);
}

}  // namespace
