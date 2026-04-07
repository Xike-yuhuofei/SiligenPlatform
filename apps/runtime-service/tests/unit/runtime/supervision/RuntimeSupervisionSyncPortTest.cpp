#include "runtime/supervision/RuntimeSupervisionSyncPort.h"

#include "runtime/supervision/RuntimeExecutionSupervisionBackend.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace {

using DispensingExecutionUseCase = Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase;
using JobID = Siligen::Application::UseCases::Dispensing::JobID;
using RuntimeJobStatusResponse = Siligen::Application::UseCases::Dispensing::RuntimeJobStatusResponse;
using RuntimeSupervisionPort = Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort;
using RuntimeSupervisionSnapshot = Siligen::RuntimeExecution::Contracts::System::RuntimeSupervisionSnapshot;
using TaskSync = Siligen::Runtime::Service::Supervision::IRuntimeJobTerminalSync;
using RuntimeSupervisionSyncPort = Siligen::Runtime::Service::Supervision::RuntimeSupervisionSyncPort;

class FakeRuntimeSupervisionPort final : public RuntimeSupervisionPort {
   public:
    RuntimeSupervisionSnapshot next_snapshot{};

    Siligen::Shared::Types::Result<RuntimeSupervisionSnapshot> ReadSnapshot() const override {
        return Siligen::Shared::Types::Result<RuntimeSupervisionSnapshot>::Success(next_snapshot);
    }
};

class FakeTerminalSync final : public TaskSync {
   public:
    mutable int sync_calls = 0;
    mutable JobID last_job_id;

    void OnTerminalJobObserved(const std::string& job_id) const override {
        ++sync_calls;
        last_job_id = job_id;
    }
};

std::shared_ptr<DispensingExecutionUseCase> BuildExecutionUseCase() {
    return std::make_shared<DispensingExecutionUseCase>(
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr);
}

RuntimeJobStatusResponse MakeJobStatus(const std::string& job_id, const std::string& state) {
    RuntimeJobStatusResponse status;
    status.job_id = job_id;
    status.plan_id = "plan-1";
    status.plan_fingerprint = "fp-1";
    status.state = state;
    status.target_count = 1;
    status.completed_count = state == "completed" ? 1U : 0U;
    status.current_cycle = 1;
    status.total_segments = 10;
    status.cycle_progress_percent = state == "completed" ? 100U : 50U;
    status.overall_progress_percent = status.cycle_progress_percent;
    return status;
}

TEST(RuntimeSupervisionSyncPortTest, MissingInnerPortReturnsNotInitializedError) {
    auto port = RuntimeSupervisionSyncPort(nullptr, BuildExecutionUseCase(), std::make_shared<FakeTerminalSync>());

    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::PORT_NOT_INITIALIZED);
}

TEST(RuntimeSupervisionSyncPortTest, SyncsTerminalJobOnceAfterObservedJobClears) {
    auto inner_port = std::make_shared<FakeRuntimeSupervisionPort>();
    inner_port->next_snapshot.active_job_id = "job-terminal";
    inner_port->next_snapshot.active_job_state = "running";

    auto execution_use_case = BuildExecutionUseCase();
    execution_use_case->SeedJobStateForTesting(MakeJobStatus("job-terminal", "running"));
    auto terminal_sync = std::make_shared<FakeTerminalSync>();

    auto port = RuntimeSupervisionSyncPort(inner_port, execution_use_case, terminal_sync);

    auto running_result = port.ReadSnapshot();
    ASSERT_TRUE(running_result.IsSuccess()) << running_result.GetError().GetMessage();
    EXPECT_EQ(terminal_sync->sync_calls, 0);

    execution_use_case->SeedJobStateForTesting(MakeJobStatus("job-terminal", "completed"));
    inner_port->next_snapshot.active_job_id.clear();
    inner_port->next_snapshot.active_job_state.clear();

    auto terminal_result = port.ReadSnapshot();
    ASSERT_TRUE(terminal_result.IsSuccess()) << terminal_result.GetError().GetMessage();
    EXPECT_EQ(terminal_sync->sync_calls, 1);
    EXPECT_EQ(terminal_sync->last_job_id, "job-terminal");

    auto repeated_result = port.ReadSnapshot();
    ASSERT_TRUE(repeated_result.IsSuccess()) << repeated_result.GetError().GetMessage();
    EXPECT_EQ(terminal_sync->sync_calls, 1);
}

TEST(RuntimeSupervisionSyncPortTest, MissingExecutionUseCaseDropsStaleObservedJob) {
    auto inner_port = std::make_shared<FakeRuntimeSupervisionPort>();
    inner_port->next_snapshot.active_job_id = "job-terminal";

    auto terminal_sync = std::make_shared<FakeTerminalSync>();
    auto port = RuntimeSupervisionSyncPort(inner_port, nullptr, terminal_sync);

    auto running_result = port.ReadSnapshot();
    ASSERT_TRUE(running_result.IsSuccess()) << running_result.GetError().GetMessage();

    inner_port->next_snapshot.active_job_id.clear();
    auto terminal_result = port.ReadSnapshot();
    ASSERT_TRUE(terminal_result.IsSuccess()) << terminal_result.GetError().GetMessage();
    EXPECT_EQ(terminal_sync->sync_calls, 0);

    auto repeated_result = port.ReadSnapshot();
    ASSERT_TRUE(repeated_result.IsSuccess()) << repeated_result.GetError().GetMessage();
    EXPECT_EQ(terminal_sync->sync_calls, 0);
}

}  // namespace
