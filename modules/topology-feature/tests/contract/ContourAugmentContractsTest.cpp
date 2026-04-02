#include "topology_feature/contracts/ContourAugmentContracts.h"
#include "infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.h"

#include <gtest/gtest.h>

#include <type_traits>

namespace {

using CanonicalConfig = Siligen::Infrastructure::Adapters::Planning::Geometry::ContourAugmentConfig;
using CanonicalAdapter = Siligen::Infrastructure::Adapters::Planning::Geometry::ContourAugmenterAdapter;
using LegacyBridgeConfig = Siligen::Infrastructure::Adapters::Planning::Geometry::ContourAugmentConfig;
using LegacyBridgeAdapter = Siligen::Infrastructure::Adapters::Planning::Geometry::ContourAugmenterAdapter;

static_assert(std::is_same_v<CanonicalConfig, LegacyBridgeConfig>);
static_assert(std::is_same_v<CanonicalAdapter, LegacyBridgeAdapter>);

TEST(ContourAugmentContractsTest, ContractHeaderExposesCanonicalAdapterTypes) {
    CanonicalConfig config;
    CanonicalAdapter adapter;

    EXPECT_EQ(config.contour_layer, "layer0");
    EXPECT_EQ(config.glue_layer, "0");
    EXPECT_NO_THROW(static_cast<void>(adapter));
}

}  // namespace
