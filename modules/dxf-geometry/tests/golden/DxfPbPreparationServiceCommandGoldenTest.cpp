#include "support/DxfPbPreparationServiceTestSupport.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

namespace {

using Siligen::DxfGeometry::Tests::Support::ArgsPathFor;
using Siligen::DxfGeometry::Tests::Support::DxfPbPreparationService;
using Siligen::DxfGeometry::Tests::Support::FakeConfigurationPort;
using Siligen::DxfGeometry::Tests::Support::MinimalDxf;
using Siligen::DxfGeometry::Tests::Support::PbPathFor;
using Siligen::DxfGeometry::Tests::Support::ScopedEnvVar;
using Siligen::DxfGeometry::Tests::Support::ScopedTempDir;
using Siligen::DxfGeometry::Tests::Support::WriteTextFile;

std::filesystem::path GoldenPath() {
    return std::filesystem::path(__FILE__).parent_path() / "dxf_to_pb.args.summary.txt";
}

std::string ReadGoldenFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in.is_open()) << "missing golden file: " << path.string();
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::string ReplaceAll(std::string value, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
        value.replace(pos, from.size(), to);
        pos += to.size();
    }
    return value;
}

std::string CanonicalizeSnapshot(std::string value) {
    value = ReplaceAll(std::move(value), "\r\n", "\n");
    if (!value.empty() && value.back() != '\n') {
        value.push_back('\n');
    }
    return value;
}

std::string NormalizeSnapshot(std::string value,
                              const std::filesystem::path& dxf_path,
                              const std::filesystem::path& pb_path) {
    value = ReplaceAll(std::move(value), dxf_path.string(), "<dxf>");
    value = ReplaceAll(std::move(value), pb_path.string(), "<pb>");
    return CanonicalizeSnapshot(std::move(value));
}

std::string CaptureArgsScript() {
    return "import pathlib\n"
           "import sys\n"
           "args = sys.argv[1:]\n"
           "output = pathlib.Path(args[args.index('--output') + 1])\n"
           "output.write_bytes(b'golden-pb')\n"
           "output.with_suffix('.args.txt').write_text('\\n'.join(f'arg[{i}]={arg}' for i, arg in enumerate(args)), encoding='utf-8')\n";
}

TEST(DxfPbPreparationServiceCommandGoldenTest, NonDefaultPreprocessConfigMatchesGoldenSnapshot) {
    ScopedTempDir workspace("dxf_pb_golden");
    const auto dxf_path = workspace.Path() / "sample.dxf";
    const auto script_path = workspace.Path() / "capture_args.py";
    const auto pb_path = PbPathFor(dxf_path);
    const auto args_path = ArgsPathFor(pb_path);

    WriteTextFile(dxf_path, MinimalDxf());
    WriteTextFile(script_path, CaptureArgsScript());

    auto config = std::make_shared<FakeConfigurationPort>();
    config->preprocess_config.normalize_units = true;
    config->preprocess_config.approx_splines = true;
    config->preprocess_config.snap_enabled = true;
    config->preprocess_config.densify_enabled = false;
    config->preprocess_config.min_seg_enabled = true;
    config->preprocess_config.spline_samples = 12;
    config->preprocess_config.spline_max_step = 0.25f;
    config->preprocess_config.chordal = 0.01f;
    config->preprocess_config.max_seg = 1.5f;
    config->preprocess_config.snap = 0.2f;
    config->preprocess_config.angular = 0.3f;
    config->preprocess_config.min_seg = 0.4f;

    const ScopedEnvVar script_override("SILIGEN_DXF_PB_SCRIPT", script_path.string());
    DxfPbPreparationService service(config);

    auto result = service.EnsurePbReady(dxf_path.string());
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    ASSERT_TRUE(std::filesystem::exists(args_path));

    const auto actual = NormalizeSnapshot(
        Siligen::DxfGeometry::Tests::Support::ReadTextFile(args_path), dxf_path, pb_path);
    const auto expected = CanonicalizeSnapshot(ReadGoldenFile(GoldenPath()));
    EXPECT_EQ(actual, expected);
}

}  // namespace
