#include "coordinate_alignment/contracts/CoordinateTransformSet.h"

#include <gtest/gtest.h>

#include <type_traits>

namespace {

using Siligen::CoordinateAlignment::Contracts::CoordinateTransform;
using Siligen::CoordinateAlignment::Contracts::CoordinateTransformSet;

static_assert(std::is_default_constructible_v<CoordinateTransform>);
static_assert(std::is_default_constructible_v<CoordinateTransformSet>);

TEST(CoordinateTransformContractsTest, DefaultContractStateRemainsFrozen) {
    const CoordinateTransformSet transform_set;

    EXPECT_TRUE(transform_set.alignment_id.empty());
    EXPECT_TRUE(transform_set.plan_id.empty());
    EXPECT_FALSE(transform_set.valid);
    EXPECT_TRUE(transform_set.transforms.empty());
    EXPECT_EQ(transform_set.owner_module, "M5");
}

TEST(CoordinateTransformContractsTest, ContractCarriesOriginOffsetAndRotationTransforms) {
    CoordinateTransformSet transform_set;
    transform_set.alignment_id = "baseline-alignment";
    transform_set.plan_id = "plan-origin-rotation";
    transform_set.valid = true;
    transform_set.transforms = {
        {"origin-offset", "machine-origin", "alignment-origin"},
        {"rotation-z", "alignment-origin", "process-frame"},
    };

    ASSERT_EQ(transform_set.transforms.size(), 2u);
    EXPECT_EQ(transform_set.transforms[0].transform_id, "origin-offset");
    EXPECT_EQ(transform_set.transforms[0].source_frame, "machine-origin");
    EXPECT_EQ(transform_set.transforms[0].target_frame, "alignment-origin");
    EXPECT_EQ(transform_set.transforms[1].transform_id, "rotation-z");
    EXPECT_EQ(transform_set.transforms[1].source_frame, "alignment-origin");
    EXPECT_EQ(transform_set.transforms[1].target_frame, "process-frame");
}

}  // namespace
