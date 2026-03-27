#include "application/services/dispensing/DispensePlanningFacade.h"

#include <gtest/gtest.h>

namespace {

class LinePathSourceStub : public Siligen::Domain::Trajectory::Ports::IPathSourcePort {
   public:
    Siligen::Shared::Types::Result<Siligen::Domain::Trajectory::Ports::PathSourceResult> LoadFromFile(
        const std::string& filepath) override {
        (void)filepath;

        using Siligen::Domain::Trajectory::Ports::PathPrimitiveMeta;
        using Siligen::Domain::Trajectory::Ports::PathSourceResult;
        using Siligen::Domain::Trajectory::ValueObjects::Primitive;
        using Siligen::Shared::Types::Point2D;
        using Siligen::Shared::Types::Result;

        PathSourceResult result;
        result.success = true;
        result.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(20.0f, 0.0f)));
        result.metadata.push_back(PathPrimitiveMeta{});
        return Result<PathSourceResult>::Success(result);
    }
};

}  // namespace

TEST(DispensePlanningFacadeTest, BuildsDispensingPlan) {
    using Siligen::Application::Services::Dispensing::DispensePlanningFacade;
    using Siligen::Domain::Dispensing::DomainServices::DispensingPlanRequest;

    auto path_source = std::make_shared<LinePathSourceStub>();
    DispensePlanningFacade facade(path_source);

    DispensingPlanRequest request;
    request.dxf_filepath = "line-test.dxf";
    request.dispensing_velocity = 20.0f;
    request.acceleration = 100.0f;
    request.max_jerk = 500.0f;
    request.sample_dt = 0.01f;
    request.trigger_spatial_interval_mm = 5.0f;

    auto result = facade.Plan(request);
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_FALSE(result.Value().motion_trajectory.points.empty());
}
