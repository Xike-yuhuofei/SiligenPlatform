#include "adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace {

using Siligen::Infrastructure::Adapters::Planning::Geometry::ContourAugmentConfig;

std::filesystem::path GoldenPath() {
    return std::filesystem::path(__FILE__).parent_path() / "contour_augment_config.defaults.txt";
}

std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in.is_open()) << "missing golden file: " << path.string();
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::string SerializeConfig(const ContourAugmentConfig& config) {
    std::ostringstream out;
    out << std::boolalpha << std::fixed << std::setprecision(6);
    out << "cup_offset_far=" << config.cup_offset_far << "\n";
    out << "cup_offset_mid=" << config.cup_offset_mid << "\n";
    out << "cup_offset_near=" << config.cup_offset_near << "\n";
    out << "mid_path_cut_ratio=" << config.mid_path_cut_ratio << "\n";
    out << "grid_spacing=" << config.grid_spacing << "\n";
    out << "grid_angle_deg=" << config.grid_angle_deg << "\n";
    out << "grid_u_offset=" << config.grid_u_offset << "\n";
    out << "grid_v_offset=" << config.grid_v_offset << "\n";
    out << "grid_min_distance_to_cups=" << config.grid_min_distance_to_cups << "\n";
    out << "grid_border_tolerance=" << config.grid_border_tolerance << "\n";
    out << "merge_epsilon=" << config.merge_epsilon << "\n";
    out << "intersect_epsilon=" << config.intersect_epsilon << "\n";
    out << "miter_limit=" << config.miter_limit << "\n";
    out << "offset_approx_epsilon=" << config.offset_approx_epsilon << "\n";
    out << "contour_layer=" << config.contour_layer << "\n";
    out << "glue_layer=" << config.glue_layer << "\n";
    out << "point_layer=" << config.point_layer << "\n";
    out << "output_r12=" << config.output_r12 << "\n";
    return out.str();
}

TEST(ContourAugmentConfigGoldenTest, FrozenDefaultsMatchGoldenSnapshot) {
    const auto golden_path = GoldenPath();
    const auto actual = SerializeConfig(ContourAugmentConfig());
    const auto expected = ReadTextFile(golden_path);

    EXPECT_EQ(actual, expected);
}

}  // namespace
