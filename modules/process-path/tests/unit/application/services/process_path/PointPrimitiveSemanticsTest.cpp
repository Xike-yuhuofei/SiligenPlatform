#include "application/services/process_path/ProcessPathFacade.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
using Siligen::ProcessPath::Contracts::PathGenerationStage;
using Siligen::ProcessPath::Contracts::PathGenerationStatus;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::ProcessPath::Contracts::TopologyRepairPolicy;
using Siligen::Shared::Types::Point2D;

TEST(PointPrimitiveSemanticsTest, MixedPointInputIsRejectedWhenRepairPolicyIsOff) {
    ProcessPathBuildRequest request;
    request.topology_repair.policy = TopologyRepairPolicy::Off;
    request.primitives = {
        Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
        Primitive::MakePoint(Point2D(5.0f, 5.0f)),
    };

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    EXPECT_EQ(result.status, PathGenerationStatus::InvalidInput);
    EXPECT_EQ(result.failed_stage, PathGenerationStage::InputValidation);
    EXPECT_EQ(result.error_message, "point primitive is not a supported live process-path input");
}

TEST(PointPrimitiveSemanticsTest, MixedPointInputIsRejectedWhenRepairPolicyIsAuto) {
    ProcessPathBuildRequest request;
    request.topology_repair.policy = TopologyRepairPolicy::Auto;
    request.primitives = {
        Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
        Primitive::MakePoint(Point2D(5.0f, 5.0f)),
    };

    ProcessPathFacade facade;
    const auto result = facade.Build(request);

    EXPECT_EQ(result.status, PathGenerationStatus::InvalidInput);
    EXPECT_EQ(result.failed_stage, PathGenerationStage::InputValidation);
    EXPECT_EQ(result.error_message, "point primitive is not a supported live process-path input");
}

}  // namespace
