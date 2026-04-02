#include "adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.h"
#include "shared/types/Error.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>

namespace {

using Siligen::Infrastructure::Adapters::Planning::Geometry::ContourAugmenterAdapter;
using Siligen::Shared::Types::ErrorCode;

std::filesystem::path MakeTempDir() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / ("siligen_topology_feature_" + std::to_string(now));
    std::filesystem::create_directories(path);
    return path;
}

TEST(ContourAugmenterAdapterIntegrationTest, MissingInputProducesFrozenErrorSurface) {
    const auto temp_dir = MakeTempDir();
    const auto input_path = temp_dir / "missing_input.dxf";
    const auto output_path = temp_dir / "output.dxf";

    ContourAugmenterAdapter adapter;
    auto result = adapter.ConvertFile(input_path.string(), output_path.string());

    ASSERT_TRUE(result.IsError());
    const auto code = result.GetError().GetCode();
    EXPECT_TRUE(code == ErrorCode::NOT_IMPLEMENTED || code == ErrorCode::FILE_IO_ERROR)
        << "unexpected error code: " << static_cast<int>(code);
    EXPECT_FALSE(std::filesystem::exists(output_path));

    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);
}

}  // namespace
