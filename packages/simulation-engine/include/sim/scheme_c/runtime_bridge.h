#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "sim/scheme_c/types.h"

namespace sim::scheme_c {

class Recorder;
class VirtualAxisGroup;
class VirtualIo;

struct RuntimeAxisBinding {
    std::int32_t logical_axis_index{0};
    std::string axis_name;
};

struct RuntimeIoBinding {
    std::int32_t channel{0};
    std::string signal_name;
    bool output{true};
};

struct RuntimeBridgeBindings {
    std::vector<RuntimeAxisBinding> axis_bindings{};
    std::vector<RuntimeIoBinding> io_bindings{};
};

enum class ReplayTriggerSpace {
    AxisPosition,
    PathProgress
};

struct ReplayIoDelaySpec {
    std::string channel;
    Duration delay{Duration::zero()};
};

struct ReplayTriggerSpec {
    std::string channel;
    double position_mm{0.0};
    bool state{false};
    std::int32_t logical_axis_index{0};
    ReplayTriggerSpace space{ReplayTriggerSpace::PathProgress};
    std::uint32_t pulse_width_us{0};
};

struct ReplayValveSpec {
    std::string channel{"DO_VALVE"};
    Duration open_delay{Duration::zero()};
    Duration close_delay{Duration::zero()};
};

struct DeterministicReplayPlan {
    std::vector<ReplayIoDelaySpec> io_delays{};
    std::vector<ReplayTriggerSpec> triggers{};
    std::optional<ReplayValveSpec> valve{};
};

enum class RuntimeBridgeCommandKind {
    Home,
    Move,
    ExecutePath,
    Stop
};

enum class RuntimeBridgeCommandState {
    Pending,
    Active,
    Completed,
    Failed,
    Cancelled
};

struct RuntimeBridgeHomeCommand {
    std::vector<std::int32_t> logical_axis_indices{};
    bool home_all_axes{true};
    bool force_rehome{false};
    std::int32_t timeout_ms{30000};
};

struct RuntimeBridgeMoveCommand {
    double target_x_mm{0.0};
    double target_y_mm{0.0};
    double velocity_mm_per_s{50.0};
};

enum class RuntimePathSegmentType {
    Line,
    ArcClockwise,
    ArcCounterClockwise
};

struct RuntimeBridgePathSegment {
    RuntimePathSegmentType type{RuntimePathSegmentType::Line};
    double end_x_mm{0.0};
    double end_y_mm{0.0};
    double center_x_mm{0.0};
    double center_y_mm{0.0};
};

struct RuntimeBridgePathCommand {
    std::vector<RuntimeBridgePathSegment> segments{};
    double max_velocity_mm_per_s{50.0};
    double max_acceleration_mm_per_s2{500.0};
    double sample_dt_s{0.001};
};

struct RuntimeBridgeStopCommand {
    bool immediate{false};
    bool clear_pending_commands{true};
};

struct RuntimeBridgeCommandStatus {
    std::uint64_t command_id{0};
    RuntimeBridgeCommandKind kind{RuntimeBridgeCommandKind::Home};
    RuntimeBridgeCommandState state{RuntimeBridgeCommandState::Pending};
    TickIndex submitted_tick{0};
    std::optional<TickIndex> started_tick{};
    std::optional<TickIndex> completed_tick{};
    std::string detail;
};

enum class RuntimeBridgeMode {
    SeamStub,
    ProcessRuntimeCore
};

struct RuntimeBridgeMetadata {
    RuntimeBridgeMode mode{RuntimeBridgeMode::SeamStub};
    std::string owner{"packages/simulation-engine/runtime-bridge"};
    std::string next_integration_point{
        "Attach process-runtime-core motion use cases and ports behind this seam"};
    bool process_runtime_core_linked{false};
    std::vector<std::string> follow_up_seams{};
};

class RuntimeBridge {
public:
    virtual ~RuntimeBridge() = default;

    virtual RuntimeBridgeMetadata metadata() const = 0;
    virtual const RuntimeBridgeBindings& bindings() const = 0;
    virtual void attach(VirtualAxisGroup& axis_group, VirtualIo& io, Recorder* recorder) = 0;
    virtual void initialize(const TickInfo& tick) = 0;
    virtual bool advance(const TickInfo& tick) = 0;
    virtual void requestStop() = 0;
};

class RuntimeBridgeControl {
public:
    virtual ~RuntimeBridgeControl() = default;

    virtual std::uint64_t submitHome(const RuntimeBridgeHomeCommand& command) = 0;
    virtual std::uint64_t submitMove(const RuntimeBridgeMoveCommand& command) = 0;
    virtual std::uint64_t submitPath(const RuntimeBridgePathCommand& command) = 0;
    virtual std::uint64_t submitStop(const RuntimeBridgeStopCommand& command) = 0;

    virtual std::optional<RuntimeBridgeCommandStatus> commandStatus(std::uint64_t command_id) const = 0;
    virtual std::vector<RuntimeBridgeCommandStatus> commandHistory() const = 0;
    virtual bool hasPendingCommands() const = 0;
    virtual bool hasCommandFailure() const = 0;
};

RuntimeBridgeControl* runtimeBridgeControl(RuntimeBridge& bridge) noexcept;
const RuntimeBridgeControl* runtimeBridgeControl(const RuntimeBridge& bridge) noexcept;

RuntimeBridgeBindings makeDefaultRuntimeBridgeBindings(
    const std::vector<std::string>& axis_names,
    const std::vector<std::string>& io_channels);

std::unique_ptr<RuntimeBridge> createRuntimeBridge(
    const RuntimeBridgeBindings& bindings = {},
    const DeterministicReplayPlan& replay_plan = {});
std::unique_ptr<RuntimeBridge> createRuntimeBridgeSeam(
    const RuntimeBridgeBindings& bindings = {});

}  // namespace sim::scheme_c
