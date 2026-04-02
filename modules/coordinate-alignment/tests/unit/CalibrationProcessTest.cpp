#include "domain/machine/domain-services/CalibrationProcess.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>

namespace {

using Siligen::Domain::Machine::DomainServices::CalibrationProcess;
using Siligen::Domain::Machine::Ports::CalibrationDeviceStatus;
using Siligen::Domain::Machine::Ports::ICalibrationDevicePort;
using Siligen::Domain::Machine::Ports::ICalibrationResultPort;
using Siligen::Domain::Machine::ValueObjects::CalibrationRequest;
using Siligen::Domain::Machine::ValueObjects::CalibrationResult;
using Siligen::Domain::Machine::ValueObjects::CalibrationState;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

class FakeCalibrationDevicePort final : public ICalibrationDevicePort {
   public:
    int prepare_calls = 0;
    int execute_calls = 0;
    int verify_calls = 0;
    int stop_calls = 0;

    Result<void> Prepare(const CalibrationRequest&) noexcept override {
        ++prepare_calls;
        return Result<void>::Success();
    }

    Result<void> Execute(const CalibrationRequest&) noexcept override {
        ++execute_calls;
        return Result<void>::Success();
    }

    Result<void> Verify(const CalibrationRequest&) noexcept override {
        ++verify_calls;
        return Result<void>::Success();
    }

    Result<void> Stop() noexcept override {
        ++stop_calls;
        return Result<void>::Success();
    }

    Result<CalibrationDeviceStatus> GetStatus() const noexcept override {
        return Result<CalibrationDeviceStatus>::Success(CalibrationDeviceStatus{});
    }
};

class FakeCalibrationResultPort final : public ICalibrationResultPort {
   public:
    int save_calls = 0;
    std::optional<CalibrationResult> last_result;

    Result<void> SaveResult(const CalibrationResult& result) noexcept override {
        ++save_calls;
        last_result = result;
        return Result<void>::Success();
    }
};

TEST(CalibrationProcessTest, RunsHappyPathAndPersistsCompletedResult) {
    auto device_port = std::make_shared<FakeCalibrationDevicePort>();
    auto result_port = std::make_shared<FakeCalibrationResultPort>();

    CalibrationProcess process(device_port, result_port);
    CalibrationRequest request;
    request.calibration_id = "alignment-happy-path";

    ASSERT_TRUE(process.Start(request).IsSuccess());
    EXPECT_EQ(process.GetState(), CalibrationState::Validating);

    ASSERT_TRUE(process.ProcessNextStep().IsSuccess());
    EXPECT_EQ(process.GetState(), CalibrationState::Executing);

    ASSERT_TRUE(process.ProcessNextStep().IsSuccess());
    EXPECT_EQ(process.GetState(), CalibrationState::Verifying);

    ASSERT_TRUE(process.ProcessNextStep().IsSuccess());
    EXPECT_EQ(process.GetState(), CalibrationState::Completed);

    EXPECT_EQ(device_port->prepare_calls, 1);
    EXPECT_EQ(device_port->execute_calls, 1);
    EXPECT_EQ(device_port->verify_calls, 1);

    ASSERT_EQ(result_port->save_calls, 1);
    ASSERT_TRUE(result_port->last_result.has_value());
    EXPECT_EQ(result_port->last_result->calibration_id, "alignment-happy-path");
    EXPECT_EQ(result_port->last_result->state, CalibrationState::Completed);
    EXPECT_TRUE(result_port->last_result->success);
}

TEST(CalibrationProcessTest, FailsWhenDevicePortIsMissingDuringValidation) {
    auto result_port = std::make_shared<FakeCalibrationResultPort>();

    CalibrationProcess process(std::shared_ptr<ICalibrationDevicePort>{}, result_port);
    CalibrationRequest request;
    request.calibration_id = "alignment-missing-device";

    ASSERT_TRUE(process.Start(request).IsSuccess());
    ASSERT_EQ(process.GetState(), CalibrationState::Validating);

    const auto step_result = process.ProcessNextStep();

    ASSERT_TRUE(step_result.IsError());
    EXPECT_EQ(step_result.GetError().GetCode(), ErrorCode::PORT_NOT_INITIALIZED);
    EXPECT_EQ(process.GetState(), CalibrationState::Failed);

    ASSERT_EQ(result_port->save_calls, 1);
    ASSERT_TRUE(result_port->last_result.has_value());
    EXPECT_EQ(result_port->last_result->state, CalibrationState::Failed);
    EXPECT_FALSE(result_port->last_result->success);
}

}  // namespace
