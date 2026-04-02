#include "adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Infrastructure::Adapters::Planning::Geometry::ContourAugmentConfig;

TEST(ContourAugmentConfigDefaultsTest, DefaultsRemainFrozen) {
    const ContourAugmentConfig config;

    EXPECT_FLOAT_EQ(config.cup_offset_far, 20.0f);
    EXPECT_FLOAT_EQ(config.cup_offset_mid, 17.5f);
    EXPECT_FLOAT_EQ(config.cup_offset_near, 2.5f);
    EXPECT_FLOAT_EQ(config.mid_path_cut_ratio, 0.34f);
    EXPECT_FLOAT_EQ(config.grid_spacing, 6.0f);
    EXPECT_FLOAT_EQ(config.grid_angle_deg, 55.0f);
    EXPECT_FLOAT_EQ(config.grid_u_offset, 5.813678f);
    EXPECT_FLOAT_EQ(config.grid_v_offset, 0.699722f);
    EXPECT_FLOAT_EQ(config.grid_min_distance_to_cups, 20.0f);
    EXPECT_FLOAT_EQ(config.grid_border_tolerance, 0.8f);
    EXPECT_FLOAT_EQ(config.merge_epsilon, 1e-4f);
    EXPECT_FLOAT_EQ(config.intersect_epsilon, 1e-6f);
    EXPECT_FLOAT_EQ(config.miter_limit, 5.0f);
    EXPECT_FLOAT_EQ(config.offset_approx_epsilon, 0.1f);
    EXPECT_EQ(config.contour_layer, "layer0");
    EXPECT_EQ(config.glue_layer, "0");
    EXPECT_EQ(config.point_layer, "0");
    EXPECT_TRUE(config.output_r12);
}

}  // namespace
