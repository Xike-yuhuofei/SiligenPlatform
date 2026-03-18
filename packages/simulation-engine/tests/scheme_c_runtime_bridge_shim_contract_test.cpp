#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

#include "domain/motion/ports/IHomingPort.h"
#include "domain/motion/ports/IInterpolationPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Error.h"
#include "sim/scheme_c/process_runtime_shim.h"
#include "sim/scheme_c/runtime_bridge.h"
#include "sim/scheme_c/virtual_axis_group.h"
#include "sim/scheme_c/virtual_io.h"

namespace {

using LogicalAxisId = Siligen::Shared::Types::LogicalAxisId;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testDefaultBindingsAreDeterministic() {
    const auto bindings = sim::scheme_c::makeDefaultRuntimeBridgeBindings(
        {"X", "Y", "Z"},
        {"DO_VALVE", "DO_CAMERA"});

    require(bindings.axis_bindings.size() == 3U, "bindings: axis binding count mismatch");
    require(bindings.axis_bindings[0].logical_axis_index == 0, "bindings: X logical axis mismatch");
    require(bindings.axis_bindings[1].logical_axis_index == 1, "bindings: Y logical axis mismatch");
    require(bindings.io_bindings.size() == 2U, "bindings: io binding count mismatch");
    require(bindings.io_bindings[0].channel == 0, "bindings: first io channel mismatch");
    require(bindings.io_bindings[1].signal_name == "DO_CAMERA", "bindings: second io name mismatch");
}

void testShimBundleReflectsAxisAndIoState() {
    auto axis_group = sim::scheme_c::createInMemoryVirtualAxisGroup({"X", "Y"});
    auto io = sim::scheme_c::createInMemoryVirtualIo({"DO_VALVE"});

    sim::scheme_c::RuntimeBridgeBindings bindings;
    bindings.axis_bindings = {
        {0, "X"},
        {1, "Y"}
    };
    bindings.io_bindings = {
        {3, "DO_VALVE", true}
    };

    auto shims = sim::scheme_c::createProcessRuntimeCoreShimBundle(*axis_group, *io, bindings);

    sim::scheme_c::AxisState x_state = axis_group->readAxis("X");
    x_state.position_mm = 4.0;
    x_state.configured_velocity_mm_per_s = 2.0;
    axis_group->writeAxisState(x_state);

    require(shims.axis_control->EnableAxis(LogicalAxisId::X).IsSuccess(), "shim: enable axis failed");
    require(shims.homing->HomeAxis(LogicalAxisId::X).IsSuccess(), "shim: home axis failed");
    require(shims.io_control->WriteDigitalOutput(3, true).IsSuccess(), "shim: write digital output failed");

    const auto axis_enabled = shims.axis_control->IsAxisEnabled(LogicalAxisId::X);
    require(axis_enabled.IsSuccess() && axis_enabled.Value(), "shim: axis should be enabled");

    axis_group->advance(sim::scheme_c::TickInfo{0, std::chrono::seconds(0), std::chrono::seconds(1)});
    io->advance(sim::scheme_c::TickInfo{0, std::chrono::seconds(0), std::chrono::seconds(1)});

    const auto homing_in_progress = shims.homing->IsHomingInProgress(LogicalAxisId::X);
    require(homing_in_progress.IsSuccess() && homing_in_progress.Value(), "shim: homing should be in progress after first tick");

    const auto homing_status = shims.homing->GetHomingStatus(LogicalAxisId::X);
    require(homing_status.IsSuccess(), "shim: homing status failed");
    require(homing_status.Value().state == Siligen::Domain::Motion::Ports::HomingState::HOMING,
            "shim: homing state should reflect tick-driven search");

    axis_group->advance(sim::scheme_c::TickInfo{1, std::chrono::seconds(1), std::chrono::seconds(1)});

    const auto homed = shims.homing->IsAxisHomed(LogicalAxisId::X);
    require(homed.IsSuccess() && homed.Value(), "shim: axis should be homed after second tick");

    const auto axis_status = shims.motion_state->GetAxisStatus(LogicalAxisId::X);
    require(axis_status.IsSuccess(), "shim: axis status failed");
    require(axis_status.Value().enabled, "shim: motion state should reflect enabled axis");
    require(axis_status.Value().state == Siligen::Domain::Motion::Ports::MotionState::HOMED,
            "shim: motion state should reflect homed axis");

    const auto io_status = shims.io_control->ReadDigitalOutput(3);
    require(io_status.IsSuccess(), "shim: read digital output failed");
    require(io_status.Value().signal_active, "shim: digital output should be active");

    sim::scheme_c::AxisState limited_state = axis_group->readAxis("X");
    limited_state.positive_soft_limit = true;
    limited_state.servo_alarm = true;
    axis_group->writeAxisState(limited_state);

    const auto positive_limit = shims.io_control->ReadLimitStatus(LogicalAxisId::X, true);
    require(positive_limit.IsSuccess() && positive_limit.Value(), "shim: positive limit should be active");

    const auto servo_alarm = shims.io_control->ReadServoAlarm(LogicalAxisId::X);
    require(servo_alarm.IsSuccess() && servo_alarm.Value(), "shim: servo alarm should be active");
}

void testInterpolationShimCommandsVirtualAxes() {
    auto axis_group = sim::scheme_c::createInMemoryVirtualAxisGroup({"X", "Y"});
    auto io = sim::scheme_c::createInMemoryVirtualIo({"DO_VALVE"});
    auto shims = sim::scheme_c::createProcessRuntimeCoreShimBundle(*axis_group, *io);

    require(shims.axis_control->EnableAxis(LogicalAxisId::X).IsSuccess(), "interp: enable X failed");
    require(shims.axis_control->EnableAxis(LogicalAxisId::Y).IsSuccess(), "interp: enable Y failed");

    Siligen::Domain::Motion::Ports::CoordinateSystemConfig config;
    config.axis_map = {LogicalAxisId::X, LogicalAxisId::Y};
    config.max_velocity = 120.0f;
    config.max_acceleration = 240.0f;

    Siligen::Domain::Motion::Ports::InterpolationData data;
    data.positions = {12.5f, 25.0f};
    data.velocity = 6.25f;
    data.acceleration = 120.0f;

    require(shims.interpolation->ConfigureCoordinateSystem(1, config).IsSuccess(), "interp: configure failed");
    require(shims.interpolation->AddInterpolationData(1, data).IsSuccess(), "interp: add data failed");
    require(shims.interpolation->StartCoordinateSystemMotion(1U).IsSuccess(), "interp: start failed");

    axis_group->advance(sim::scheme_c::TickInfo{0, std::chrono::seconds(0), std::chrono::seconds(1)});

    const auto moving_x = shims.motion_state->GetAxisVelocity(LogicalAxisId::X);
    require(moving_x.IsSuccess() && moving_x.Value() == 6.25f, "interp: X velocity should reflect active motion");

    const auto moving = shims.motion_state->IsAxisMoving(LogicalAxisId::X);
    require(moving.IsSuccess() && moving.Value(), "interp: X should be moving after first tick");

    const auto position = shims.motion_state->GetCurrentPosition();
    require(position.IsSuccess(), "interp: current position failed");
    require(position.Value().x == 6.25f, "interp: X position should advance by one tick");
    require(position.Value().y == 6.25f, "interp: Y position should advance by one tick");

    axis_group->advance(sim::scheme_c::TickInfo{1, std::chrono::seconds(1), std::chrono::seconds(1)});
    axis_group->advance(sim::scheme_c::TickInfo{2, std::chrono::seconds(2), std::chrono::seconds(1)});
    axis_group->advance(sim::scheme_c::TickInfo{3, std::chrono::seconds(3), std::chrono::seconds(1)});

    const auto coord_status = shims.interpolation->GetCoordinateSystemStatus(1);
    require(coord_status.IsSuccess(), "interp: coord status failed");
    require(coord_status.Value().remaining_segments == 0U, "interp: remaining segments mismatch");

    const auto final_position = shims.motion_state->GetCurrentPosition();
    require(final_position.IsSuccess(), "interp: final current position failed");
    require(final_position.Value().x == 12.5f, "interp: X final position mismatch");
    require(final_position.Value().y == 25.0f, "interp: Y final position mismatch");
}

void testMissingBindingFailsDeterministically() {
    auto axis_group = sim::scheme_c::createInMemoryVirtualAxisGroup({"X"});
    auto io = sim::scheme_c::createInMemoryVirtualIo({"DO_VALVE"});

    sim::scheme_c::RuntimeBridgeBindings bindings;
    bindings.axis_bindings = {{0, "X"}};
    bindings.io_bindings = {{0, "DO_VALVE", true}};

    auto shims = sim::scheme_c::createProcessRuntimeCoreShimBundle(*axis_group, *io, bindings);

    const auto axis_status = shims.motion_state->GetAxisStatus(LogicalAxisId::Y);
    require(axis_status.IsError(), "missing binding: expected axis status failure");
    require(axis_status.GetError().GetCode() == Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
            "missing binding: expected invalid parameter for missing axis");

    const auto io_status = shims.io_control->ReadDigitalOutput(9);
    require(io_status.IsError(), "missing binding: expected io status failure");
    require(io_status.GetError().GetCode() == Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
            "missing binding: expected invalid parameter for missing io binding");
}

}  // namespace

int main() {
    try {
        testDefaultBindingsAreDeterministic();
        testShimBundleReflectsAxisAndIoState();
        testInterpolationShimCommandsVirtualAxes();
        testMissingBindingFailsDeterministically();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
