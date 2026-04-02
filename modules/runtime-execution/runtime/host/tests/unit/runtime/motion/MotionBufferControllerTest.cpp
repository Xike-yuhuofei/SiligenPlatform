#include "domain/motion/domain-services/MotionBufferController.h"

#include <gtest/gtest.h>

#include <memory>

namespace {

using Siligen::Domain::Motion::Ports::CoordinateSystemStatus;
using Siligen::Domain::Motion::Ports::IInterpolationPort;
using Siligen::Domain::Motion::Ports::InterpolationData;
using Siligen::Domain::Motion::Ports::CoordinateSystemConfig;
using Siligen::Domain::Motion::MotionBufferController;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;

Result<void> NotImplementedVoid(const char* method) {
    return Result<void>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, method, "MotionBufferControllerTest"));
}

template <typename T>
Result<T> NotImplemented(const char* method) {
    return Result<T>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, method, "MotionBufferControllerTest"));
}

class FakeInterpolationPort final : public IInterpolationPort {
   public:
    Result<void> ConfigureCoordinateSystem(int16 /*coord_sys*/, const CoordinateSystemConfig& /*config*/) override {
        return NotImplementedVoid("ConfigureCoordinateSystem");
    }

    Result<void> AddInterpolationData(int16 /*coord_sys*/, const InterpolationData& /*data*/) override {
        return NotImplementedVoid("AddInterpolationData");
    }

    Result<void> ClearInterpolationBuffer(int16 coord_sys) override {
        last_cleared_coord_sys = coord_sys;
        return clear_result;
    }

    Result<void> FlushInterpolationData(int16 /*coord_sys*/) override {
        return NotImplementedVoid("FlushInterpolationData");
    }

    Result<void> StartCoordinateSystemMotion(uint32 /*coord_sys_mask*/) override {
        return NotImplementedVoid("StartCoordinateSystemMotion");
    }

    Result<void> StopCoordinateSystemMotion(uint32 /*coord_sys_mask*/) override {
        return NotImplementedVoid("StopCoordinateSystemMotion");
    }

    Result<void> SetCoordinateSystemVelocityOverride(int16 /*coord_sys*/, float32 /*override_percent*/) override {
        return NotImplementedVoid("SetCoordinateSystemVelocityOverride");
    }

    Result<void> EnableCoordinateSystemSCurve(int16 /*coord_sys*/, float32 /*jerk*/) override {
        return NotImplementedVoid("EnableCoordinateSystemSCurve");
    }

    Result<void> DisableCoordinateSystemSCurve(int16 /*coord_sys*/) override {
        return NotImplementedVoid("DisableCoordinateSystemSCurve");
    }

    Result<void> SetConstLinearVelocityMode(int16 /*coord_sys*/, bool /*enabled*/, uint32 /*rotate_axis_mask*/) override {
        return NotImplementedVoid("SetConstLinearVelocityMode");
    }

    Result<uint32> GetInterpolationBufferSpace(int16 coord_sys) const override {
        last_space_coord_sys = coord_sys;
        return space_result;
    }

    Result<uint32> GetLookAheadBufferSpace(int16 /*coord_sys*/) const override {
        return Result<uint32>::Success(0);
    }

    Result<CoordinateSystemStatus> GetCoordinateSystemStatus(int16 /*coord_sys*/) const override {
        return Result<CoordinateSystemStatus>::Success(CoordinateSystemStatus{});
    }

    mutable int16 last_space_coord_sys = 0;
    int16 last_cleared_coord_sys = 0;
    Result<uint32> space_result = Result<uint32>::Success(0);
    Result<void> clear_result = Result<void>::Success();
};

TEST(MotionBufferControllerTest, CreateRejectsNullInterpolationPort) {
    const auto result = MotionBufferController::Create(nullptr);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);
}

TEST(MotionBufferControllerTest, UsesInterpolationPortForBufferQueriesAndClear) {
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->space_result = Result<uint32>::Success(42);

    auto controller_result = MotionBufferController::Create(interpolation_port);
    ASSERT_TRUE(controller_result.IsSuccess());
    auto controller = controller_result.Value();

    EXPECT_EQ(controller->GetBufferSpace(3), 42);
    EXPECT_EQ(interpolation_port->last_space_coord_sys, 3);

    EXPECT_TRUE(controller->ClearBuffer(3));
    EXPECT_EQ(interpolation_port->last_cleared_coord_sys, 3);
}

}  // namespace
