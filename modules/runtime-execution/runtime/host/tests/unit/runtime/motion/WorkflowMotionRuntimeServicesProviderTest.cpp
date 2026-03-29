#include "runtime/motion/WorkflowMotionRuntimeServicesProvider.h"

#include "domain/motion/ports/IHomingPort.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCard.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCardWrapper.h"
#include "siligen/device/adapters/motion/MotionRuntimeFacade.h"

#include <gtest/gtest.h>

#include <memory>

namespace {

using WorkflowMotionRuntimeServicesProvider =
    Siligen::RuntimeExecution::Host::Motion::WorkflowMotionRuntimeServicesProvider;
using MotionRuntimeFacade = Siligen::Infrastructure::Adapters::Motion::MotionRuntimeFacade;
using MultiCardMotionAdapter = Siligen::Infrastructure::Adapters::MultiCardMotionAdapter;
using MockMultiCard = Siligen::Infrastructure::Hardware::MockMultiCard;
using MockMultiCardWrapper = Siligen::Infrastructure::Hardware::MockMultiCardWrapper;
using IHomingPort = Siligen::Domain::Motion::Ports::IHomingPort;
using IMotionRuntimePort = Siligen::Domain::Motion::Ports::IMotionRuntimePort;
using LogicalAxisId = Siligen::Shared::Types::LogicalAxisId;
using HomingStatus = Siligen::Domain::Motion::Ports::HomingStatus;
using ResultVoid = Siligen::Shared::Types::Result<void>;
using int32 = Siligen::Shared::Types::int32;

class NoOpHomingPort final : public IHomingPort {
   public:
    ResultVoid HomeAxis(LogicalAxisId) override { return ResultVoid::Success(); }
    ResultVoid StopHoming(LogicalAxisId) override { return ResultVoid::Success(); }
    ResultVoid ResetHomingState(LogicalAxisId) override { return ResultVoid::Success(); }

    Siligen::Shared::Types::Result<HomingStatus> GetHomingStatus(LogicalAxisId axis) const override {
        HomingStatus status;
        status.axis = axis;
        return Siligen::Shared::Types::Result<HomingStatus>::Success(status);
    }

    Siligen::Shared::Types::Result<bool> IsAxisHomed(LogicalAxisId) const override {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    Siligen::Shared::Types::Result<bool> IsHomingInProgress(LogicalAxisId) const override {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    ResultVoid WaitForHomingComplete(LogicalAxisId, int32) override { return ResultVoid::Success(); }
};

std::shared_ptr<IMotionRuntimePort> CreateRuntimePort() {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    auto motion_adapter_result = MultiCardMotionAdapter::Create(wrapper);
    EXPECT_TRUE(motion_adapter_result.IsSuccess()) << motion_adapter_result.GetError().GetMessage();
    if (motion_adapter_result.IsError()) {
        return nullptr;
    }

    return std::make_shared<MotionRuntimeFacade>(
        motion_adapter_result.Value(),
        std::make_shared<NoOpHomingPort>());
}

TEST(WorkflowMotionRuntimeServicesProviderTest, ReturnsEmptyBundleWhenPortIsMissing) {
    WorkflowMotionRuntimeServicesProvider provider;

    const auto bundle = provider.CreateServices(nullptr);

    EXPECT_EQ(bundle.motion_control_service, nullptr);
    EXPECT_EQ(bundle.motion_status_service, nullptr);
    EXPECT_EQ(bundle.motion_validation_service, nullptr);
}

TEST(WorkflowMotionRuntimeServicesProviderTest, CreatesMotionServicesFromRuntimePort) {
    WorkflowMotionRuntimeServicesProvider provider;
    auto runtime_port = CreateRuntimePort();
    ASSERT_NE(runtime_port, nullptr);

    const auto bundle = provider.CreateServices(runtime_port);

    EXPECT_NE(bundle.motion_control_service, nullptr);
    EXPECT_NE(bundle.motion_status_service, nullptr);
    EXPECT_EQ(bundle.motion_validation_service, nullptr);
}

}  // namespace
