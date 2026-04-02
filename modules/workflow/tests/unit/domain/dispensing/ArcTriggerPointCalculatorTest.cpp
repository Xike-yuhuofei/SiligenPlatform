#include "domain/dispensing/domain-services/ArcTriggerPointCalculator.h"
#include "shared/types/VisualizationTypes.h"

#include <gtest/gtest.h>

namespace {
bool IsMonotonic(const std::vector<long>& values) {
    if (values.size() < 2) {
        return true;
    }

    int direction = 0;
    for (size_t i = 1; i < values.size(); ++i) {
        long diff = values[i] - values[i - 1];
        if (diff == 0) {
            continue;
        }
        int current = diff > 0 ? 1 : -1;
        if (direction == 0) {
            direction = current;
        } else if (direction != current) {
            return false;
        }
    }
    return true;
}
}  // namespace

TEST(ArcTriggerPointCalculatorTest, ReturnsMonotonicTriggerPositions) {
    Siligen::Shared::Types::DXFSegment segment;
    segment.type = Siligen::Shared::Types::DXFSegmentType::ARC;
    segment.center_point = {0.0f, 0.0f};
    segment.radius = 10.0f;
    segment.start_angle = 0.0f;
    segment.end_angle = 90.0f;
    segment.clockwise = false;
    segment.start_point = {10.0f, 0.0f};
    segment.end_point = {0.0f, 10.0f};

    auto result = Siligen::Domain::Dispensing::DomainServices::ArcTriggerPointCalculator::Calculate(
        segment, 1.0f, 100.0f);

    ASSERT_TRUE(result.IsSuccess());
    const auto& positions = result.Value().trigger_positions;
    EXPECT_FALSE(positions.empty());
    EXPECT_TRUE(IsMonotonic(positions));
}