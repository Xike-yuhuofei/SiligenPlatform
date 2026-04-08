#include "domain/dispensing/domain-services/ValveCoordinationService.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Domain::Dispensing::DomainServices::ValveCoordinationService;
using Siligen::Domain::Dispensing::Ports::DispenserValveParams;
using Siligen::Domain::Dispensing::Ports::DispenserValveState;
using Siligen::Domain::Dispensing::Ports::DispenserValveStatus;
using Siligen::Domain::Dispensing::Ports::IValvePort;
using Siligen::Domain::Dispensing::Ports::PositionTriggeredDispenserParams;
using Siligen::Domain::Dispensing::Ports::SupplyValveState;
using Siligen::Domain::Dispensing::Ports::SupplyValveStatusDetail;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

class FakeValvePort final : public IValvePort {
   public:
    Result<DispenserValveState> StartDispenser(const DispenserValveParams&) noexcept override {
        return Result<DispenserValveState>::Success(dispenser_state_);
    }

    Result<DispenserValveState> OpenDispenser() noexcept override {
        return Result<DispenserValveState>::Success(dispenser_state_);
    }

    Result<void> CloseDispenser() noexcept override {
        return Result<void>::Success();
    }

    Result<DispenserValveState> StartPositionTriggeredDispenser(const PositionTriggeredDispenserParams&) noexcept override {
        return Result<DispenserValveState>::Success(dispenser_state_);
    }

    Result<void> StopDispenser() noexcept override {
        return Result<void>::Success();
    }

    Result<void> PauseDispenser() noexcept override {
        return Result<void>::Success();
    }

    Result<void> ResumeDispenser() noexcept override {
        return Result<void>::Success();
    }

    Result<DispenserValveState> GetDispenserStatus() noexcept override {
        if (dispenser_status_should_fail_) {
            return Result<DispenserValveState>::Failure(
                Error(ErrorCode::HARDWARE_ERROR, "dispenser status read failure", "FakeValvePort"));
        }
        return Result<DispenserValveState>::Success(dispenser_state_);
    }

    Result<SupplyValveState> OpenSupply() noexcept override {
        supply_status_detail_.state = SupplyValveState::Open;
        return Result<SupplyValveState>::Success(SupplyValveState::Open);
    }

    Result<SupplyValveState> CloseSupply() noexcept override {
        supply_status_detail_.state = SupplyValveState::Closed;
        return Result<SupplyValveState>::Success(SupplyValveState::Closed);
    }

    Result<SupplyValveStatusDetail> GetSupplyStatus() noexcept override {
        if (supply_status_should_fail_) {
            return Result<SupplyValveStatusDetail>::Failure(
                Error(ErrorCode::HARDWARE_ERROR, "supply status read failure", "FakeValvePort"));
        }
        return Result<SupplyValveStatusDetail>::Success(supply_status_detail_);
    }

    DispenserValveState dispenser_state_{};
    SupplyValveStatusDetail supply_status_detail_{};
    bool dispenser_status_should_fail_{false};
    bool supply_status_should_fail_{false};
};

DispenserValveParams BuildValidDispenserParams() {
    DispenserValveParams params;
    params.count = 1;
    params.intervalMs = 1000;
    params.durationMs = 100;
    return params;
}

}  // namespace

TEST(ValveCoordinationServiceTest, StartDispenserRejectsWhenSupplyValveNotOpen) {
    auto valve_port = std::make_shared<FakeValvePort>();
    valve_port->supply_status_detail_.state = SupplyValveState::Closed;
    valve_port->dispenser_state_.status = DispenserValveStatus::Idle;

    ValveCoordinationService service(valve_port);
    auto result = service.StartDispenser(BuildValidDispenserParams());

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(result.GetError().GetMessage().find("failure_stage=check_valve_safety_supply_open"), std::string::npos);
}

TEST(ValveCoordinationServiceTest, OpenSupplyValveRejectsWhenDispenserStatusHasError) {
    auto valve_port = std::make_shared<FakeValvePort>();
    valve_port->dispenser_state_.status = DispenserValveStatus::Error;
    valve_port->dispenser_state_.errorMessage = "dispenser_error";
    valve_port->supply_status_detail_.state = SupplyValveState::Closed;

    ValveCoordinationService service(valve_port);
    auto result = service.OpenSupplyValve();

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::HARDWARE_ERROR);
    EXPECT_NE(result.GetError().GetMessage().find("failure_stage=check_valve_safety_dispenser_status"), std::string::npos);
}

TEST(ValveCoordinationServiceTest, StartDispenserRejectsWhenValvePortUnavailable) {
    ValveCoordinationService service(nullptr);
    auto result = service.StartDispenser(BuildValidDispenserParams());

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::PORT_NOT_INITIALIZED);
}
