#include <gtest/gtest.h>

#include <atomic>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "modules/dispense-packaging/domain/dispensing/domain-services/DispensingProcessService.h"

#include "domain/motion/ports/IInterpolationPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "shared/types/Error.h"

namespace {

using Siligen::Domain::Dispensing::DomainServices::DispensingProcessService;
using Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver;
using Siligen::Domain::Motion::Ports::CoordinateSystemConfig;
using Siligen::Domain::Motion::Ports::CoordinateSystemState;
using Siligen::Domain::Motion::Ports::CoordinateSystemStatus;
using Siligen::Domain::Motion::Ports::IInterpolationPort;
using Siligen::Domain::Motion::Ports::IMotionStatePort;
using Siligen::Domain::Motion::Ports::InterpolationData;
using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using int16 = std::int16_t;
using int32 = std::int32_t;
using uint32 = std::uint32_t;

template <typename Tag, typename Tag::type Member>
struct PrivateMemberAccessor {
    friend typename Tag::type GetPrivateMember(Tag) {
        return Member;
    }
};

struct WaitForMotionCompleteTag {
    using type = Result<void> (DispensingProcessService::*)(int32,
                                                            std::atomic<bool>*,
                                                            std::atomic<bool>*,
                                                            std::atomic<bool>*,
                                                            const Point2D*,
                                                            float32,
                                                            uint32,
                                                            bool,
                                                            IDispensingExecutionObserver*) noexcept;
    friend type GetPrivateMember(WaitForMotionCompleteTag);
};

template struct PrivateMemberAccessor<
    WaitForMotionCompleteTag,
    &DispensingProcessService::WaitForMotionComplete>;

template <typename T>
Result<T> NotImplemented(const char* method) {
    return Result<T>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, method, "DispensingProcessServiceWaitTest"));
}

Result<void> NotImplementedVoid(const char* method) {
    return Result<void>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, method, "DispensingProcessServiceWaitTest"));
}

class FakeInterpolationPort final : public IInterpolationPort {
   public:
    Result<void> ConfigureCoordinateSystem(int16, const CoordinateSystemConfig&) override {
        return Result<void>::Success();
    }

    Result<void> AddInterpolationData(int16, const InterpolationData&) override {
        return Result<void>::Success();
    }

    Result<void> ClearInterpolationBuffer(int16) override {
        return Result<void>::Success();
    }

    Result<void> FlushInterpolationData(int16) override {
        return Result<void>::Success();
    }

    Result<void> StartCoordinateSystemMotion(uint32) override {
        return Result<void>::Success();
    }

    Result<void> StopCoordinateSystemMotion(uint32) override {
        return Result<void>::Success();
    }

    Result<void> SetCoordinateSystemVelocityOverride(int16, float32) override {
        return Result<void>::Success();
    }

    Result<void> EnableCoordinateSystemSCurve(int16, float32) override {
        return NotImplementedVoid("EnableCoordinateSystemSCurve");
    }

    Result<void> DisableCoordinateSystemSCurve(int16) override {
        return NotImplementedVoid("DisableCoordinateSystemSCurve");
    }

    Result<void> SetConstLinearVelocityMode(int16, bool, uint32) override {
        return NotImplementedVoid("SetConstLinearVelocityMode");
    }

    Result<uint32> GetInterpolationBufferSpace(int16) const override {
        return Result<uint32>::Success(0U);
    }

    Result<uint32> GetLookAheadBufferSpace(int16) const override {
        return Result<uint32>::Success(0U);
    }

    Result<CoordinateSystemStatus> GetCoordinateSystemStatus(int16) const override {
        return Result<CoordinateSystemStatus>::Success(status);
    }

    CoordinateSystemStatus status{
        CoordinateSystemState::MOVING,
        true,
        1U,
        0.0f,
        0,
        1,
        0};
};

class SequencedMotionStatePort final : public IMotionStatePort {
   public:
    explicit SequencedMotionStatePort(std::vector<Point2D> positions_in) : positions(std::move(positions_in)) {}

    Result<Point2D> GetCurrentPosition() const override {
        if (positions.empty()) {
            return Result<Point2D>::Success(Point2D{});
        }
        const auto index = current_index < positions.size() ? current_index : positions.size() - 1;
        const auto value = positions[index];
        if (current_index + 1 < positions.size()) {
            ++current_index;
        }
        last_position = value;
        return Result<Point2D>::Success(value);
    }

    Result<float32> GetAxisPosition(LogicalAxisId axis) const override {
        if (axis == LogicalAxisId::X) {
            return Result<float32>::Success(last_position.x);
        }
        if (axis == LogicalAxisId::Y) {
            return Result<float32>::Success(last_position.y);
        }
        return Result<float32>::Success(0.0f);
    }

    Result<float32> GetAxisVelocity(LogicalAxisId) const override {
        return Result<float32>::Success(0.0f);
    }

    Result<MotionStatus> GetAxisStatus(LogicalAxisId) const override {
        MotionStatus status;
        status.state = MotionState::IDLE;
        status.position = last_position;
        status.velocity = 0.0f;
        return Result<MotionStatus>::Success(status);
    }

    Result<bool> IsAxisMoving(LogicalAxisId) const override {
        return Result<bool>::Success(false);
    }

    Result<bool> IsAxisInPosition(LogicalAxisId) const override {
        return Result<bool>::Success(true);
    }

    Result<std::vector<MotionStatus>> GetAllAxesStatus() const override {
        return Result<std::vector<MotionStatus>>::Success({});
    }

    mutable std::size_t current_index = 0;
    mutable Point2D last_position{};
    std::vector<Point2D> positions{};
};

TEST(DispensingProcessServiceWaitForMotionCompleteTest, StablePositionAwayFromTargetTimesOut) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f}, Point2D{92.305f, 0.0f}});
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    Point2D final_target{100.0f, 100.0f};
    const auto wait_for_motion_complete = GetPrivateMember(WaitForMotionCompleteTag{});

    auto result = (service.*wait_for_motion_complete)(
        180,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        &final_target,
        1.0f,
        9U,
        false,
        nullptr);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::MOTION_TIMEOUT);
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest, NearTargetLowVelocityFallsBackToSuccess) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status.state = CoordinateSystemState::IDLE;
    interpolation_port->status.is_moving = false;
    interpolation_port->status.remaining_segments = 0U;
    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f}, Point2D{99.7f, 99.8f}, Point2D{99.7f, 99.8f}, Point2D{99.7f, 99.8f},
                             Point2D{99.7f, 99.8f}, Point2D{99.7f, 99.8f}, Point2D{99.7f, 99.8f}});
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    Point2D final_target{100.0f, 100.0f};
    const auto wait_for_motion_complete = GetPrivateMember(WaitForMotionCompleteTag{});

    auto result = (service.*wait_for_motion_complete)(
        800,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        &final_target,
        1.0f,
        9U,
        false,
        nullptr);

    ASSERT_TRUE(result.IsSuccess());
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest, FifoFinishedWithLatchedRunBitFallsBackToSuccess) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status.state = CoordinateSystemState::MOVING;
    interpolation_port->status.is_moving = true;
    interpolation_port->status.remaining_segments = std::numeric_limits<uint32>::max();
    interpolation_port->status.current_velocity = 0.0f;
    interpolation_port->status.raw_status_word = 0x11;
    interpolation_port->status.raw_segment = -858993460;
    interpolation_port->status.mc_status_ret = 0;

    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f}, Point2D{100.0f, 100.0f}, Point2D{100.0f, 100.0f},
                             Point2D{100.0f, 100.0f}, Point2D{100.0f, 100.0f}, Point2D{100.0f, 100.0f},
                             Point2D{100.0f, 100.0f}});
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    Point2D final_target{100.0f, 100.0f};
    const auto wait_for_motion_complete = GetPrivateMember(WaitForMotionCompleteTag{});

    auto result = (service.*wait_for_motion_complete)(
        800,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        &final_target,
        1.0f,
        9U,
        false,
        nullptr);

    ASSERT_TRUE(result.IsSuccess());
}

TEST(DispensingProcessServiceWaitForMotionCompleteTest, IdleCoordinateSystemAwayFromTargetStillTimesOut) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status.state = CoordinateSystemState::IDLE;
    interpolation_port->status.is_moving = false;
    interpolation_port->status.remaining_segments = 0U;

    auto motion_state_port = std::make_shared<SequencedMotionStatePort>(
        std::vector<Point2D>{Point2D{0.0f, 0.0f}, Point2D{92.305f, 0.0f}, Point2D{92.305f, 0.0f},
                             Point2D{92.305f, 0.0f}, Point2D{92.305f, 0.0f}, Point2D{92.305f, 0.0f}});
    DispensingProcessService service(nullptr,
                                     interpolation_port,
                                     motion_state_port,
                                     nullptr,
                                     nullptr);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    std::atomic<bool> pause_applied_flag{false};
    Point2D final_target{100.0f, 100.0f};
    const auto wait_for_motion_complete = GetPrivateMember(WaitForMotionCompleteTag{});

    auto result = (service.*wait_for_motion_complete)(
        180,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        &final_target,
        1.0f,
        9U,
        false,
        nullptr);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::MOTION_TIMEOUT);
}

}  // namespace
