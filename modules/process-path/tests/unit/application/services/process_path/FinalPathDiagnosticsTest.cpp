#include "application/services/process_path/ProcessPathFacade.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
using Siligen::ProcessPath::Contracts::PathGenerationStatus;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::Shared::Types::Point2D;

int CountRapidSegments(const Siligen::ProcessPath::Contracts::ProcessPath& path) {
    int count = 0;
    for (const auto& segment : path.segments) {
        if (!segment.dispense_on) {
            count += 1;
        }
    }
    return count;
}

int CountDispenseFragments(const Siligen::ProcessPath::Contracts::ProcessPath& path) {
    int count = 0;
    bool previous_dispense = false;
    bool has_previous = false;
    for (const auto& segment : path.segments) {
        if (segment.dispense_on && (!has_previous || !previous_dispense)) {
            count += 1;
        }
        previous_dispense = segment.dispense_on;
        has_previous = true;
    }
    return count;
}

TEST(FinalPathDiagnosticsTest, FinalDiagnosticsTrackShapedPathAndRetainPreShapeCounts) {
    ProcessPathBuildRequest request;
    request.normalization.continuity_tolerance = 0.1f;
    request.process.approach_dist = 1.0f;
    request.process.lead_off_dist = 1.0f;
    request.process.rapid_gap_threshold = 0.1f;
    request.primitives = {
        Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
        Primitive::MakeLine(Point2D(11.0f, 0.0f), Point2D(21.0f, 0.0f)),
    };

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    ASSERT_EQ(result.status, PathGenerationStatus::Success);
    EXPECT_EQ(result.topology_diagnostics.pre_shape_rapid_segment_count, CountRapidSegments(result.process_path));
    EXPECT_EQ(result.topology_diagnostics.pre_shape_dispense_fragment_count, CountDispenseFragments(result.process_path));
    EXPECT_EQ(result.topology_diagnostics.rapid_segment_count, CountRapidSegments(result.shaped_path));
    EXPECT_EQ(result.topology_diagnostics.dispense_fragment_count, CountDispenseFragments(result.shaped_path));
    EXPECT_EQ(result.topology_diagnostics.pre_shape_rapid_segment_count, 3);
    EXPECT_EQ(result.topology_diagnostics.rapid_segment_count, 3);
    EXPECT_EQ(result.topology_diagnostics.pre_shape_dispense_fragment_count, 2);
    EXPECT_EQ(result.topology_diagnostics.dispense_fragment_count, 2);
}

}  // namespace
