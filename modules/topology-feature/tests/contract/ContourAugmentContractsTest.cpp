#include "topology_feature/contracts/ContourAugmentContracts.h"

#include <gtest/gtest.h>

namespace {

using CanonicalConfig = Siligen::Infrastructure::Adapters::Planning::Geometry::ContourAugmentConfig;
using CanonicalAdapter = Siligen::Infrastructure::Adapters::Planning::Geometry::ContourAugmenterAdapter;

TEST(ContourAugmentContractsTest, ContractHeaderExposesCanonicalAdapterTypes) {
    CanonicalConfig config;
    CanonicalAdapter adapter;

    EXPECT_EQ(config.contour_layer, "layer0");
    EXPECT_EQ(config.glue_layer, "0");
    EXPECT_NO_THROW(static_cast<void>(adapter));
}

}  // namespace
