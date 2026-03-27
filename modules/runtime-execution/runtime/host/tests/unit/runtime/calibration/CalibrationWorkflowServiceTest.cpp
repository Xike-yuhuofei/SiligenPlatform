#include "runtime_execution/application/services/calibration/CalibrationWorkflowService.h"

#include <gtest/gtest.h>

namespace {

using Siligen::RuntimeExecution::Application::Services::Calibration::CalibrationWorkflowService;
using Siligen::RuntimeExecution::Contracts::System::CalibrationExecutionRequest;
using Siligen::RuntimeExecution::Contracts::System::CalibrationExecutionResult;
using Siligen::RuntimeExecution::Contracts::System::CalibrationExecutionState;
using Siligen::RuntimeExecution::Contracts::System::CalibrationExecutionStatus;
using Siligen::RuntimeExecution::Contracts::System::ICalibrationExecutionPort;
using Siligen::RuntimeExecution::Contracts::System::ICalibrationResultSink;

class FakeCalibrationExecutionPort final : public ICalibrationExecutionPort {
   public:
    Siligen::Shared::Types::Result<void> Prepare(const CalibrationExecutionRequest&) noexcept override {
        ++prepare_calls;
        return prepare_result;
    }

    Siligen::Shared::Types::Result<void> Execute(const CalibrationExecutionRequest&) noexcept override {
        ++execute_calls;
        return execute_result;
    }

    Siligen::Shared::Types::Result<void> Verify(const CalibrationExecutionRequest&) noexcept override {
        ++verify_calls;
        return verify_result;
    }

    Siligen::Shared::Types::Result<void> Stop() noexcept override {
        ++stop_calls;
        return stop_result;
    }

    Siligen::Shared::Types::Result<CalibrationExecutionStatus> GetStatus() const noexcept override {
        return Siligen::Shared::Types::Result<CalibrationExecutionStatus>::Success(status);
    }

    int prepare_calls = 0;
    int execute_calls = 0;
    int verify_calls = 0;
    int stop_calls = 0;
    CalibrationExecutionStatus status{};
    Siligen::Shared::Types::Result<void> prepare_result{Siligen::Shared::Types::Result<void>::Success()};
    Siligen::Shared::Types::Result<void> execute_result{Siligen::Shared::Types::Result<void>::Success()};
    Siligen::Shared::Types::Result<void> verify_result{Siligen::Shared::Types::Result<void>::Success()};
    Siligen::Shared::Types::Result<void> stop_result{Siligen::Shared::Types::Result<void>::Success()};
};

class FakeCalibrationResultSink final : public ICalibrationResultSink {
   public:
    Siligen::Shared::Types::Result<void> SaveResult(const CalibrationExecutionResult& result) noexcept override {
        ++save_calls;
        last_result = result;
        return save_result;
    }

    int save_calls = 0;
    CalibrationExecutionResult last_result{};
    Siligen::Shared::Types::Result<void> save_result{Siligen::Shared::Types::Result<void>::Success()};
};

}  // namespace

TEST(CalibrationWorkflowServiceTest, RunsThroughStatesAndPersistsResult) {
    auto execution_port = std::make_shared<FakeCalibrationExecutionPort>();
    auto result_sink = std::make_shared<FakeCalibrationResultSink>();
    CalibrationWorkflowService service(execution_port, result_sink);

    CalibrationExecutionRequest request;
    request.calibration_id = "cal-rtx-001";
    request.timeout_ms = 1000;

    ASSERT_TRUE(service.Start(request).IsSuccess());
    EXPECT_EQ(service.GetState(), CalibrationExecutionState::Validating);

    EXPECT_TRUE(service.ProcessNextStep().IsSuccess());
    EXPECT_EQ(service.GetState(), CalibrationExecutionState::Executing);

    EXPECT_TRUE(service.ProcessNextStep().IsSuccess());
    EXPECT_EQ(service.GetState(), CalibrationExecutionState::Verifying);

    EXPECT_TRUE(service.ProcessNextStep().IsSuccess());
    EXPECT_EQ(service.GetState(), CalibrationExecutionState::Completed);

    EXPECT_EQ(execution_port->prepare_calls, 1);
    EXPECT_EQ(execution_port->execute_calls, 1);
    EXPECT_EQ(execution_port->verify_calls, 1);
    EXPECT_EQ(result_sink->save_calls, 1);
    EXPECT_TRUE(result_sink->last_result.success);
}

TEST(CalibrationWorkflowServiceTest, RejectsInvalidRequest) {
    auto execution_port = std::make_shared<FakeCalibrationExecutionPort>();
    auto result_sink = std::make_shared<FakeCalibrationResultSink>();
    CalibrationWorkflowService service(execution_port, result_sink);

    CalibrationExecutionRequest request;
    request.timeout_ms = 0;

    auto start_result = service.Start(request);
    EXPECT_TRUE(start_result.IsError());
    EXPECT_EQ(start_result.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER);
}
