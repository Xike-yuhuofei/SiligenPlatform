#include <chrono>
#include <cmath>
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
constexpr std::int32_t kBufferedStatusBit = 1 << 1;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool nearlyEqual(double lhs, double rhs, double epsilon = 1e-6) {
    return std::abs(lhs - rhs) <= epsilon;
}

sim::scheme_c::TickInfo secondsTick(sim::scheme_c::TickIndex tick_index, int seconds) {
    return sim::scheme_c::TickInfo{
        tick_index,
        std::chrono::seconds(seconds),
        std::chrono::seconds(1)
    };
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

void testShimReportsFollowingErrorWhenMotionRealismEnabled() {
    sim::scheme_c::MotionRealismConfig realism;
    realism.enabled = true;
    realism.servo_lag_seconds = 5.0;
    realism.encoder_quantization_mm = 1.0;

    auto axis_group = sim::scheme_c::createInMemoryVirtualAxisGroup({"X", "Y"}, realism);
    auto io = sim::scheme_c::createInMemoryVirtualIo({"DO_VALVE"});
    auto shims = sim::scheme_c::createProcessRuntimeCoreShimBundle(*axis_group, *io);

    sim::scheme_c::AxisState x_state = axis_group->readAxis("X");
    x_state.enabled = true;
    x_state.configured_velocity_mm_per_s = 10.0;
    axis_group->writeAxisState(x_state);

    axis_group->applyCommands({sim::scheme_c::AxisCommand{"X", 10.0, 10.0, false, false}});
    axis_group->advance(secondsTick(0, 1));

    const auto axis_position = shims.motion_state->GetAxisPosition(LogicalAxisId::X);
    require(axis_position.IsSuccess(), "shim realism: axis position read failed");
    require(nearlyEqual(axis_position.Value(), 2.0), "shim realism: quantized feedback position mismatch");

    const auto axis_velocity = shims.motion_state->GetAxisVelocity(LogicalAxisId::X);
    require(axis_velocity.IsSuccess(), "shim realism: axis velocity read failed");
    require(nearlyEqual(axis_velocity.Value(), 2.0), "shim realism: quantized feedback velocity mismatch");

    const auto axis_status = shims.motion_state->GetAxisStatus(LogicalAxisId::X);
    require(axis_status.IsSuccess(), "shim realism: axis status failed");
    require(axis_status.Value().following_error, "shim realism: following_error flag should surface through shim");
    require(!axis_status.Value().in_position, "shim realism: settling axis should not be in position");
    require(nearlyEqual(axis_status.Value().position.x, 2.0), "shim realism: current position should use feedback");
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
    require(shims.interpolation->FlushInterpolationData(1).IsSuccess(), "interp: flush failed");
    require(shims.interpolation->StartCoordinateSystemMotion(1U).IsSuccess(), "interp: start failed");

    axis_group->advance(secondsTick(0, 0));

    const auto moving_x = shims.motion_state->GetAxisVelocity(LogicalAxisId::X);
    require(moving_x.IsSuccess() && moving_x.Value() == 6.25f, "interp: X velocity should reflect active motion");

    const auto moving = shims.motion_state->IsAxisMoving(LogicalAxisId::X);
    require(moving.IsSuccess() && moving.Value(), "interp: X should be moving after first tick");

    const auto position = shims.motion_state->GetCurrentPosition();
    require(position.IsSuccess(), "interp: current position failed");
    require(position.Value().x == 6.25f, "interp: X position should advance by one tick");
    require(position.Value().y == 6.25f, "interp: Y position should advance by one tick");

    axis_group->advance(secondsTick(1, 1));
    shims.interpolation->StartCoordinateSystemMotion(1U);
    axis_group->advance(secondsTick(2, 2));
    shims.interpolation->StartCoordinateSystemMotion(1U);
    axis_group->advance(secondsTick(3, 3));
    shims.interpolation->StartCoordinateSystemMotion(1U);

    const auto coord_status = shims.interpolation->GetCoordinateSystemStatus(1);
    require(coord_status.IsSuccess(), "interp: coord status failed");
    require(coord_status.Value().remaining_segments == 0U, "interp: remaining segments mismatch");

    const auto final_position = shims.motion_state->GetCurrentPosition();
    require(final_position.IsSuccess(), "interp: final current position failed");
    require(final_position.Value().x == 12.5f, "interp: X final position mismatch");
    require(final_position.Value().y == 25.0f, "interp: Y final position mismatch");
}

void testInterpolationShimBuffersSegmentsAndReportsStatus() {
    auto axis_group = sim::scheme_c::createInMemoryVirtualAxisGroup({"X", "Y"});
    auto io = sim::scheme_c::createInMemoryVirtualIo({"DO_VALVE"});
    auto shims = sim::scheme_c::createProcessRuntimeCoreShimBundle(*axis_group, *io);

    require(shims.axis_control->EnableAxis(LogicalAxisId::X).IsSuccess(), "buffered: enable X failed");
    require(shims.axis_control->EnableAxis(LogicalAxisId::Y).IsSuccess(), "buffered: enable Y failed");

    Siligen::Domain::Motion::Ports::CoordinateSystemConfig config;
    config.axis_map = {LogicalAxisId::X, LogicalAxisId::Y};
    config.max_velocity = 120.0f;
    config.max_acceleration = 240.0f;
    config.look_ahead_segments = 2;

    require(shims.interpolation->ConfigureCoordinateSystem(1, config).IsSuccess(), "buffered: configure failed");

    Siligen::Domain::Motion::Ports::InterpolationData line_1;
    line_1.type = Siligen::Domain::Motion::Ports::InterpolationType::LINEAR;
    line_1.positions = {5.0f, 0.0f};
    line_1.velocity = 5.0f;
    line_1.acceleration = 50.0f;

    auto line_2 = line_1;
    line_2.positions = {10.0f, 0.0f};

    auto arc = line_1;
    arc.type = Siligen::Domain::Motion::Ports::InterpolationType::CIRCULAR_CCW;
    arc.positions = {10.0f, 5.0f};
    arc.center_x = 7.5f;
    arc.center_y = 2.5f;

    const auto buffer_space_before = shims.interpolation->GetInterpolationBufferSpace(1);
    require(buffer_space_before.IsSuccess() && buffer_space_before.Value() == 16U,
            "buffered: initial interpolation buffer space mismatch");
    const auto lookahead_before = shims.interpolation->GetLookAheadBufferSpace(1);
    require(lookahead_before.IsSuccess() && lookahead_before.Value() == 2U,
            "buffered: initial lookahead space mismatch");

    require(shims.interpolation->AddInterpolationData(1, line_1).IsSuccess(), "buffered: add line_1 failed");
    require(shims.interpolation->AddInterpolationData(1, line_2).IsSuccess(), "buffered: add line_2 failed");
    require(shims.interpolation->AddInterpolationData(1, arc).IsSuccess(), "buffered: add arc failed");

    const auto buffer_space_staged = shims.interpolation->GetInterpolationBufferSpace(1);
    require(buffer_space_staged.IsSuccess() && buffer_space_staged.Value() == 13U,
            "buffered: staged buffer space mismatch");

    require(shims.interpolation->FlushInterpolationData(1).IsSuccess(), "buffered: flush failed");

    const auto buffered_status = shims.interpolation->GetCoordinateSystemStatus(1);
    require(buffered_status.IsSuccess(), "buffered: status after flush failed");
    require(!buffered_status.Value().is_moving, "buffered: flushed buffer should be idle before start");
    require(buffered_status.Value().remaining_segments == 3U, "buffered: remaining segments before start mismatch");
    require((buffered_status.Value().raw_status_word & kBufferedStatusBit) != 0,
            "buffered: raw status should expose buffered state");

    const auto lookahead_full = shims.interpolation->GetLookAheadBufferSpace(1);
    require(lookahead_full.IsSuccess() && lookahead_full.Value() == 0U,
            "buffered: lookahead should be full after flush");

    require(shims.interpolation->StartCoordinateSystemMotion(1U).IsSuccess(), "buffered: start failed");
    auto running_status = shims.interpolation->GetCoordinateSystemStatus(1);
    require(running_status.IsSuccess() && running_status.Value().is_moving,
            "buffered: coordinate system should report moving after start");
    require(running_status.Value().remaining_segments == 3U,
            "buffered: active + queued segments should be reported after start");

    axis_group->advance(secondsTick(0, 1));
    require(shims.interpolation->StartCoordinateSystemMotion(1U).IsSuccess(), "buffered: service first completion failed");
    running_status = shims.interpolation->GetCoordinateSystemStatus(1);
    require(running_status.IsSuccess() && running_status.Value().remaining_segments == 2U,
            "buffered: remaining segments after first completion mismatch");

    axis_group->advance(secondsTick(1, 2));
    require(shims.interpolation->StartCoordinateSystemMotion(1U).IsSuccess(), "buffered: service second completion failed");
    running_status = shims.interpolation->GetCoordinateSystemStatus(1);
    require(running_status.IsSuccess() && running_status.Value().remaining_segments == 1U,
            "buffered: remaining segments after second completion mismatch");

    axis_group->advance(secondsTick(2, 3));
    require(shims.interpolation->StartCoordinateSystemMotion(1U).IsSuccess(), "buffered: service final completion failed");

    const auto final_status = shims.interpolation->GetCoordinateSystemStatus(1);
    require(final_status.IsSuccess(), "buffered: final status failed");
    require(!final_status.Value().is_moving, "buffered: coordinate system should be idle after drain");
    require(final_status.Value().remaining_segments == 0U, "buffered: all segments should be drained");

    const auto final_position = shims.motion_state->GetCurrentPosition();
    require(final_position.IsSuccess(), "buffered: final position read failed");
    require(final_position.Value().x == 10.0f, "buffered: final X mismatch");
    require(final_position.Value().y == 5.0f, "buffered: final Y mismatch");

    const auto buffer_space_after = shims.interpolation->GetInterpolationBufferSpace(1);
    require(buffer_space_after.IsSuccess() && buffer_space_after.Value() == 16U,
            "buffered: interpolation buffer space should recover after drain");
}

void testInterpolationShimStopClearsQueuedSegments() {
    auto axis_group = sim::scheme_c::createInMemoryVirtualAxisGroup({"X", "Y"});
    auto io = sim::scheme_c::createInMemoryVirtualIo({"DO_VALVE"});
    auto shims = sim::scheme_c::createProcessRuntimeCoreShimBundle(*axis_group, *io);

    require(shims.axis_control->EnableAxis(LogicalAxisId::X).IsSuccess(), "stop: enable X failed");
    require(shims.axis_control->EnableAxis(LogicalAxisId::Y).IsSuccess(), "stop: enable Y failed");

    Siligen::Domain::Motion::Ports::CoordinateSystemConfig config;
    config.axis_map = {LogicalAxisId::X, LogicalAxisId::Y};
    config.max_velocity = 120.0f;
    config.max_acceleration = 240.0f;
    config.look_ahead_segments = 2;

    require(shims.interpolation->ConfigureCoordinateSystem(1, config).IsSuccess(), "stop: configure failed");

    Siligen::Domain::Motion::Ports::InterpolationData line_1;
    line_1.type = Siligen::Domain::Motion::Ports::InterpolationType::LINEAR;
    line_1.positions = {20.0f, 0.0f};
    line_1.velocity = 5.0f;
    line_1.acceleration = 50.0f;

    auto line_2 = line_1;
    line_2.positions = {20.0f, 10.0f};

    require(shims.interpolation->AddInterpolationData(1, line_1).IsSuccess(), "stop: add line_1 failed");
    require(shims.interpolation->AddInterpolationData(1, line_2).IsSuccess(), "stop: add line_2 failed");
    require(shims.interpolation->FlushInterpolationData(1).IsSuccess(), "stop: flush failed");
    require(shims.interpolation->StartCoordinateSystemMotion(1U).IsSuccess(), "stop: start failed");

    axis_group->advance(secondsTick(0, 1));

    const auto before_stop = shims.interpolation->GetCoordinateSystemStatus(1);
    require(before_stop.IsSuccess() && before_stop.Value().remaining_segments == 2U,
            "stop: expected active and queued segments before stop");

    require(shims.interpolation->StopCoordinateSystemMotion(1U).IsSuccess(), "stop: stop failed");
    axis_group->advance(secondsTick(1, 2));

    const auto after_stop = shims.interpolation->GetCoordinateSystemStatus(1);
    require(after_stop.IsSuccess(), "stop: status after stop failed");
    require(after_stop.Value().remaining_segments == 0U, "stop: stop should clear queued segments");
    require(!after_stop.Value().is_moving, "stop: axes should be stopped after stop tick");

    const auto final_position = shims.motion_state->GetCurrentPosition();
    require(final_position.IsSuccess(), "stop: final position read failed");
    require(final_position.Value().x < 20.0f, "stop: stop should prevent reaching first target");

    const auto buffer_space_after = shims.interpolation->GetInterpolationBufferSpace(1);
    require(buffer_space_after.IsSuccess() && buffer_space_after.Value() == 16U,
            "stop: interpolation buffer should be fully available after stop");
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
        testShimReportsFollowingErrorWhenMotionRealismEnabled();
        testInterpolationShimCommandsVirtualAxes();
        testInterpolationShimBuffersSegmentsAndReportsStatus();
        testInterpolationShimStopClearsQueuedSegments();
        testMissingBindingFailsDeterministically();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
