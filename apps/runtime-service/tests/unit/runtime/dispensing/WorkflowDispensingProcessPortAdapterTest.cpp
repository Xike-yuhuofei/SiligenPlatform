#include "runtime/dispensing/WorkflowDispensingProcessPortAdapter.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace {

using IWorkflowDispensingProcessOperations =
    Siligen::Runtime::Service::Dispensing::IWorkflowDispensingProcessOperations;
using WorkflowDispensingProcessPortAdapter =
    Siligen::Runtime::Service::Dispensing::WorkflowDispensingProcessPortAdapter;
using DispensingExecutionOptions = Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionOptions;
using DispensingExecutionPlan = Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan;
using DispensingExecutionReport = Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionReport;
using DispensingRuntimeOverrides = Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides;
using DispensingRuntimeParams = Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams;
using Error = Siligen::Shared::Types::Error;
using ErrorCode = Siligen::Shared::Types::ErrorCode;
using ResultVoid = Siligen::Shared::Types::Result<void>;

Error BuildTestError(const std::string& message) {
    return Error(ErrorCode::NOT_IMPLEMENTED, message, "WorkflowDispensingProcessPortAdapterTest");
}

class FakeWorkflowDispensingProcessOperations final : public IWorkflowDispensingProcessOperations {
   public:
    ResultVoid validate_result = ResultVoid::Success();
    Siligen::Shared::Types::Result<DispensingRuntimeParams> runtime_params_result =
        Siligen::Shared::Types::Result<DispensingRuntimeParams>::Success(DispensingRuntimeParams{});
    Siligen::Shared::Types::Result<DispensingExecutionReport> execute_result =
        Siligen::Shared::Types::Result<DispensingExecutionReport>::Success(DispensingExecutionReport{});

    int validate_calls = 0;
    int build_runtime_params_calls = 0;
    int execute_calls = 0;
    int stop_calls = 0;

    DispensingRuntimeOverrides last_overrides{};
    DispensingExecutionPlan last_plan{};
    DispensingRuntimeParams last_params{};
    DispensingExecutionOptions last_options{};
    std::atomic<bool>* last_stop_flag = nullptr;
    std::atomic<bool>* last_pause_flag = nullptr;
    std::atomic<bool>* last_pause_applied_flag = nullptr;
    Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver* last_observer = nullptr;

    ResultVoid ValidateHardwareConnection() noexcept override {
        ++validate_calls;
        return validate_result;
    }

    Siligen::Shared::Types::Result<DispensingRuntimeParams> BuildRuntimeParams(
        const DispensingRuntimeOverrides& overrides) noexcept override {
        ++build_runtime_params_calls;
        last_overrides = overrides;
        return runtime_params_result;
    }

    Siligen::Shared::Types::Result<DispensingExecutionReport> ExecuteProcess(
        const DispensingExecutionPlan& plan,
        const DispensingRuntimeParams& params,
        const DispensingExecutionOptions& options,
        std::atomic<bool>* stop_flag,
        std::atomic<bool>* pause_flag,
        std::atomic<bool>* pause_applied_flag,
        Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver* observer) noexcept override {
        ++execute_calls;
        last_plan = plan;
        last_params = params;
        last_options = options;
        last_stop_flag = stop_flag;
        last_pause_flag = pause_flag;
        last_pause_applied_flag = pause_applied_flag;
        last_observer = observer;
        return execute_result;
    }

    void StopExecution(std::atomic<bool>* stop_flag, std::atomic<bool>* pause_flag) noexcept override {
        ++stop_calls;
        last_stop_flag = stop_flag;
        last_pause_flag = pause_flag;
    }
};

TEST(WorkflowDispensingProcessPortAdapterTest, ValidateHardwareConnectionDelegatesToInternalOperations) {
    auto operations = std::make_shared<FakeWorkflowDispensingProcessOperations>();
    WorkflowDispensingProcessPortAdapter adapter(operations);

    auto result = adapter.ValidateHardwareConnection();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(operations->validate_calls, 1);
}

TEST(WorkflowDispensingProcessPortAdapterTest, BuildRuntimeParamsForwardsOverridesAndResult) {
    auto operations = std::make_shared<FakeWorkflowDispensingProcessOperations>();
    DispensingRuntimeParams expected_params;
    expected_params.dispensing_velocity = 123.0f;
    operations->runtime_params_result =
        Siligen::Shared::Types::Result<DispensingRuntimeParams>::Success(expected_params);
    WorkflowDispensingProcessPortAdapter adapter(operations);

    DispensingRuntimeOverrides overrides;
    overrides.dry_run = true;
    overrides.dispensing_speed_mm_s = 12.5f;

    auto result = adapter.BuildRuntimeParams(overrides);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(operations->build_runtime_params_calls, 1);
    EXPECT_TRUE(operations->last_overrides.dry_run);
    ASSERT_TRUE(operations->last_overrides.dispensing_speed_mm_s.has_value());
    EXPECT_FLOAT_EQ(operations->last_overrides.dispensing_speed_mm_s.value(), 12.5f);
    EXPECT_FLOAT_EQ(result.Value().dispensing_velocity, 123.0f);
}

TEST(WorkflowDispensingProcessPortAdapterTest, ExecuteProcessForwardsAllArgumentsAndResult) {
    auto operations = std::make_shared<FakeWorkflowDispensingProcessOperations>();
    DispensingExecutionReport expected_report;
    expected_report.executed_segments = 7;
    expected_report.total_distance = 45.6f;
    operations->execute_result =
        Siligen::Shared::Types::Result<DispensingExecutionReport>::Success(expected_report);
    WorkflowDispensingProcessPortAdapter adapter(operations);

    DispensingExecutionPlan plan;
    plan.trigger_interval_ms = 20;
    DispensingRuntimeParams params;
    params.dispensing_velocity = 22.0f;
    DispensingExecutionOptions options;
    options.dispense_enabled = true;

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{true};
    std::atomic<bool> pause_applied_flag{false};

    auto result = adapter.ExecuteProcess(
        plan,
        params,
        options,
        &stop_flag,
        &pause_flag,
        &pause_applied_flag,
        nullptr);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(operations->execute_calls, 1);
    EXPECT_EQ(operations->last_plan.trigger_interval_ms, 20U);
    EXPECT_FLOAT_EQ(operations->last_params.dispensing_velocity, 22.0f);
    EXPECT_TRUE(operations->last_options.dispense_enabled);
    EXPECT_EQ(operations->last_stop_flag, &stop_flag);
    EXPECT_EQ(operations->last_pause_flag, &pause_flag);
    EXPECT_EQ(operations->last_pause_applied_flag, &pause_applied_flag);
    EXPECT_EQ(operations->last_observer, nullptr);
    EXPECT_EQ(result.Value().executed_segments, 7U);
    EXPECT_FLOAT_EQ(result.Value().total_distance, 45.6f);
}

TEST(WorkflowDispensingProcessPortAdapterTest, StopExecutionDelegatesFlagsToInternalOperations) {
    auto operations = std::make_shared<FakeWorkflowDispensingProcessOperations>();
    WorkflowDispensingProcessPortAdapter adapter(operations);

    std::atomic<bool> stop_flag{false};
    std::atomic<bool> pause_flag{false};
    adapter.StopExecution(&stop_flag, &pause_flag);

    EXPECT_EQ(operations->stop_calls, 1);
    EXPECT_EQ(operations->last_stop_flag, &stop_flag);
    EXPECT_EQ(operations->last_pause_flag, &pause_flag);
}

TEST(WorkflowDispensingProcessPortAdapterTest, ErrorsFromInternalOperationsArePropagated) {
    auto operations = std::make_shared<FakeWorkflowDispensingProcessOperations>();
    operations->validate_result = ResultVoid::Failure(BuildTestError("validate failed"));
    operations->runtime_params_result =
        Siligen::Shared::Types::Result<DispensingRuntimeParams>::Failure(BuildTestError("params failed"));
    operations->execute_result =
        Siligen::Shared::Types::Result<DispensingExecutionReport>::Failure(BuildTestError("execute failed"));
    WorkflowDispensingProcessPortAdapter adapter(operations);

    auto validate_result = adapter.ValidateHardwareConnection();
    auto params_result = adapter.BuildRuntimeParams(DispensingRuntimeOverrides{});
    auto execute_result = adapter.ExecuteProcess(
        DispensingExecutionPlan{},
        DispensingRuntimeParams{},
        DispensingExecutionOptions{},
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    ASSERT_TRUE(validate_result.IsError());
    EXPECT_EQ(validate_result.GetError().GetMessage(), "validate failed");
    ASSERT_TRUE(params_result.IsError());
    EXPECT_EQ(params_result.GetError().GetMessage(), "params failed");
    ASSERT_TRUE(execute_result.IsError());
    EXPECT_EQ(execute_result.GetError().GetMessage(), "execute failed");
}

TEST(WorkflowDispensingProcessPortAdapterTest, FactoryCreatesOperationsWithoutDirectHostOwnershipOfProcessService) {
    const auto operations =
        Siligen::Runtime::Service::Dispensing::CreateWorkflowDispensingProcessOperations(nullptr, nullptr, nullptr, nullptr, nullptr);

    ASSERT_NE(operations, nullptr);
    const auto result = operations->ValidateHardwareConnection();
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::PORT_NOT_INITIALIZED);
}

}  // namespace
