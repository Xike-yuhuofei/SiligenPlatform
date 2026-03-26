#include "application/usecases/motion/safety/MotionSafetyUseCase.h"

#include "gtest/gtest.h"

namespace {

using Siligen::Application::UseCases::Motion::Safety::MotionSafetyUseCase;
using Siligen::Domain::Motion::Ports::IPositionControlPort;
using Siligen::Domain::Motion::Ports::MotionCommand;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;

class FakePositionControlPort final : public IPositionControlPort {
   public:
    Result<void> MoveToPosition(const Point2D& /*position*/, float32 /*velocity*/) override {
        return Result<void>::Success();
    }
    Result<void> MoveAxisToPosition(LogicalAxisId /*axis*/, float32 /*position*/, float32 /*velocity*/) override {
        return Result<void>::Success();
    }
    Result<void> RelativeMove(LogicalAxisId /*axis*/, float32 /*distance*/, float32 /*velocity*/) override {
        return Result<void>::Success();
    }
    Result<void> SynchronizedMove(const std::vector<MotionCommand>& /*commands*/) override {
        return Result<void>::Success();
    }
    Result<void> StopAxis(LogicalAxisId /*axis*/, bool /*immediate*/ = false) override {
        return Result<void>::Success();
    }
    Result<void> StopAllAxes(bool immediate = false) override {
        ++stop_all_calls;
        last_stop_all_immediate = immediate;
        return Result<void>::Success();
    }
    Result<void> EmergencyStop() override {
        ++emergency_stop_calls;
        return Result<void>::Success();
    }
    Result<void> WaitForMotionComplete(LogicalAxisId /*axis*/, int32 /*timeout_ms*/ = 60000) override {
        return Result<void>::Success();
    }

    int emergency_stop_calls = 0;
    int stop_all_calls = 0;
    bool last_stop_all_immediate = false;
};

TEST(MotionSafetyUseCaseTest, UsesUnifiedPositionControlPortForEmergencyStop) {
    auto position_port = std::make_shared<FakePositionControlPort>();
    MotionSafetyUseCase use_case(position_port, nullptr);

    auto result = use_case.EmergencyStop();

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(position_port->emergency_stop_calls, 1);
}

TEST(MotionSafetyUseCaseTest, StopsAllAxesViaUnifiedPositionControlPort) {
    auto position_port = std::make_shared<FakePositionControlPort>();
    MotionSafetyUseCase use_case(position_port, nullptr);

    auto result = use_case.StopAllAxes(true);

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(position_port->stop_all_calls, 1);
    EXPECT_TRUE(position_port->last_stop_all_immediate);
}

}  // namespace
