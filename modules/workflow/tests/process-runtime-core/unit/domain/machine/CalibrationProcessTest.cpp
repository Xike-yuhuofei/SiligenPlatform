#include "domain/machine/domain-services/CalibrationProcess.h"
#include "shared/types/Error.h"

#include <gtest/gtest.h>

namespace {

class FakeCalibrationDevicePort final : public Siligen::Domain::Machine::Ports::ICalibrationDevicePort {
   public:
    Siligen::Shared::Types::Result<void> Prepare(
        const Siligen::Domain::Machine::ValueObjects::CalibrationRequest&) noexcept override {
        ++prepare_calls_;
        return prepare_result_;
    }

    Siligen::Shared::Types::Result<void> Execute(
        const Siligen::Domain::Machine::ValueObjects::CalibrationRequest&) noexcept override {
        ++execute_calls_;
        return execute_result_;
    }

    Siligen::Shared::Types::Result<void> Verify(
        const Siligen::Domain::Machine::ValueObjects::CalibrationRequest&) noexcept override {
        ++verify_calls_;
        return verify_result_;
    }

    Siligen::Shared::Types::Result<void> Stop() noexcept override {
        ++stop_calls_;
        return stop_result_;
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Machine::Ports::CalibrationDeviceStatus> GetStatus()
        const noexcept override {
        return Siligen::Shared::Types::Result<Siligen::Domain::Machine::Ports::CalibrationDeviceStatus>::Success(
            status_);
    }

    void SetPrepareResult(const Siligen::Shared::Types::Result<void>& result) { prepare_result_ = result; }
    void SetExecuteResult(const Siligen::Shared::Types::Result<void>& result) { execute_result_ = result; }
    void SetVerifyResult(const Siligen::Shared::Types::Result<void>& result) { verify_result_ = result; }
    void SetStopResult(const Siligen::Shared::Types::Result<void>& result) { stop_result_ = result; }

    int PrepareCalls() const { return prepare_calls_; }
    int ExecuteCalls() const { return execute_calls_; }
    int VerifyCalls() const { return verify_calls_; }
    int StopCalls() const { return stop_calls_; }

   private:
    int prepare_calls_ = 0;
    int execute_calls_ = 0;
    int verify_calls_ = 0;
    int stop_calls_ = 0;

    Siligen::Shared::Types::Result<void> prepare_result_{Siligen::Shared::Types::Result<void>::Success()};
    Siligen::Shared::Types::Result<void> execute_result_{Siligen::Shared::Types::Result<void>::Success()};
    Siligen::Shared::Types::Result<void> verify_result_{Siligen::Shared::Types::Result<void>::Success()};
    Siligen::Shared::Types::Result<void> stop_result_{Siligen::Shared::Types::Result<void>::Success()};

    Siligen::Domain::Machine::Ports::CalibrationDeviceStatus status_{};
};

class FakeCalibrationResultPort final : public Siligen::Domain::Machine::Ports::ICalibrationResultPort {
   public:
    Siligen::Shared::Types::Result<void> SaveResult(
        const Siligen::Domain::Machine::ValueObjects::CalibrationResult& result) noexcept override {
        ++save_calls_;
        last_result_ = result;
        return save_result_;
    }

    void SetSaveResult(const Siligen::Shared::Types::Result<void>& result) { save_result_ = result; }

    int SaveCalls() const { return save_calls_; }
    const Siligen::Domain::Machine::ValueObjects::CalibrationResult& LastResult() const { return last_result_; }

   private:
    int save_calls_ = 0;
    Siligen::Domain::Machine::ValueObjects::CalibrationResult last_result_{};
    Siligen::Shared::Types::Result<void> save_result_{Siligen::Shared::Types::Result<void>::Success()};
};

}  // namespace

TEST(CalibrationProcessTest, RunsThroughStatesAndPersistsResult) {
    auto device_port = std::make_shared<FakeCalibrationDevicePort>();
    auto result_port = std::make_shared<FakeCalibrationResultPort>();
    Siligen::Domain::Machine::DomainServices::CalibrationProcess process(device_port, result_port);

    Siligen::Domain::Machine::ValueObjects::CalibrationRequest request;
    request.calibration_id = "cal-001";
    request.timeout_ms = 1000;

    auto start_result = process.Start(request);
    EXPECT_TRUE(start_result.IsSuccess());
    EXPECT_EQ(process.GetState(), Siligen::Domain::Machine::ValueObjects::CalibrationState::Validating);

    EXPECT_TRUE(process.ProcessNextStep().IsSuccess());
    EXPECT_EQ(process.GetState(), Siligen::Domain::Machine::ValueObjects::CalibrationState::Executing);

    EXPECT_TRUE(process.ProcessNextStep().IsSuccess());
    EXPECT_EQ(process.GetState(), Siligen::Domain::Machine::ValueObjects::CalibrationState::Verifying);

    EXPECT_TRUE(process.ProcessNextStep().IsSuccess());
    EXPECT_EQ(process.GetState(), Siligen::Domain::Machine::ValueObjects::CalibrationState::Completed);

    EXPECT_EQ(device_port->PrepareCalls(), 1);
    EXPECT_EQ(device_port->ExecuteCalls(), 1);
    EXPECT_EQ(device_port->VerifyCalls(), 1);
    EXPECT_EQ(result_port->SaveCalls(), 1);
    EXPECT_TRUE(result_port->LastResult().success);
}

TEST(CalibrationProcessTest, RejectsInvalidRequest) {
    auto device_port = std::make_shared<FakeCalibrationDevicePort>();
    auto result_port = std::make_shared<FakeCalibrationResultPort>();
    Siligen::Domain::Machine::DomainServices::CalibrationProcess process(device_port, result_port);

    Siligen::Domain::Machine::ValueObjects::CalibrationRequest request;
    request.timeout_ms = 0;

    auto start_result = process.Start(request);
    EXPECT_TRUE(start_result.IsError());
    EXPECT_EQ(start_result.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER);
}
