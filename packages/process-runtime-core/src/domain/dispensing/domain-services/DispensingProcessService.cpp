#define MODULE_NAME "DispensingProcessService"

#include "DispensingProcessService.h"

#include "domain/dispensing/domain-services/DispenseCompensationService.h"
#include "domain/dispensing/domain-services/SupplyStabilizationPolicy.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>
#include <utility>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int16;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::uint32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Domain::Machine::Ports::HardwareConnectionState;

namespace {
constexpr float32 kDefaultAcceleration = 100.0f;
constexpr float32 kDefaultPulsePerMm = 200.0f;
constexpr uint32 kDefaultDispenserIntervalMs = 100;
constexpr uint32 kDefaultDispenserDurationMs = 100;
constexpr float32 kDefaultTriggerSpatialIntervalMm = 3.0f;
constexpr uint32 kMotionCompletionToleranceMs = 100;
constexpr uint32 kStatusPollIntervalMs = 50;
constexpr int16 kCoordinateSystem = 1;
constexpr uint32 kCoordinateSystemMask = 0x01;

struct SupplyValveGuard {
    std::shared_ptr<IValvePort> port;
    bool opened = false;

    explicit SupplyValveGuard(std::shared_ptr<IValvePort> p)
        : port(std::move(p)) {}

    Result<void> Open() {
        if (!port) {
            return Result<void>::Failure(Error(ErrorCode::PORT_NOT_INITIALIZED, "供胶阀端口未初始化"));
        }
        auto result = port->OpenSupply();
        if (!result.IsSuccess()) {
            return Result<void>::Failure(result.GetError());
        }
        opened = true;
        return Result<void>::Success();
    }

    void Close() {
        if (!port || !opened) {
            return;
        }
        auto result = port->CloseSupply();
        if (!result.IsSuccess()) {
            SILIGEN_LOG_WARNING("关闭供胶阀失败: " + result.GetError().GetMessage());
        }
        opened = false;
    }

    ~SupplyValveGuard() {
        Close();
    }

    SupplyValveGuard(const SupplyValveGuard&) = delete;
    SupplyValveGuard& operator=(const SupplyValveGuard&) = delete;
};

class DispenserOperationGuard {
   public:
    explicit DispenserOperationGuard(std::shared_ptr<IValvePort> port)
        : port_(std::move(port)), started_(false) {}

    Result<void> Start(const Ports::PositionTriggeredDispenserParams& params) {
        auto result = port_->StartPositionTriggeredDispenser(params);
        if (result.IsSuccess()) {
            started_ = true;
            return Result<void>::Success();
        }
        return Result<void>::Failure(result.GetError());
    }

    ~DispenserOperationGuard() {
        if (started_) {
            SILIGEN_LOG_DEBUG("[DispenserGuard] 段执行完成,调用 StopDispenser 关闭点胶阀");
            auto stop_result = port_->StopDispenser();
            if (!stop_result.IsSuccess()) {
                SILIGEN_LOG_WARNING("[DispenserGuard] StopDispenser 失败: " + stop_result.GetError().GetMessage());
            } else {
                SILIGEN_LOG_DEBUG("[DispenserGuard] StopDispenser 成功,阀门已关闭");
            }
        }
    }

    DispenserOperationGuard(const DispenserOperationGuard&) = delete;
    DispenserOperationGuard& operator=(const DispenserOperationGuard&) = delete;

   private:
    std::shared_ptr<IValvePort> port_;
    bool started_;
};

class CoordinateSystemStopGuard {
   public:
    CoordinateSystemStopGuard(std::shared_ptr<IInterpolationPort> port, uint32 mask)
        : port_(std::move(port)), mask_(mask) {}

    void Arm() { armed_ = true; }

    ~CoordinateSystemStopGuard() {
        if (!armed_ || !port_) {
            return;
        }
        auto result = port_->StopCoordinateSystemMotion(mask_);
        if (result.IsError()) {
            SILIGEN_LOG_WARNING("坐标系停止失败: " + result.GetError().GetMessage());
        }
    }

    CoordinateSystemStopGuard(const CoordinateSystemStopGuard&) = delete;
    CoordinateSystemStopGuard& operator=(const CoordinateSystemStopGuard&) = delete;

   private:
    std::shared_ptr<IInterpolationPort> port_;
    uint32 mask_;
    bool armed_ = false;
};

class MotionObserverGuard {
   public:
    explicit MotionObserverGuard(IDispensingExecutionObserver* observer)
        : observer_(observer) {}

    void Start() noexcept {
        if (!observer_) {
            return;
        }
        observer_->OnMotionStart();
        started_ = true;
    }

    ~MotionObserverGuard() {
        if (!observer_ || !started_) {
            return;
        }
        observer_->OnMotionStop();
    }

    MotionObserverGuard(const MotionObserverGuard&) = delete;
    MotionObserverGuard& operator=(const MotionObserverGuard&) = delete;

   private:
    IDispensingExecutionObserver* observer_;
    bool started_ = false;
};

}  // namespace

DispensingProcessService::DispensingProcessService(std::shared_ptr<IValvePort> valve_port,
                                                   std::shared_ptr<IInterpolationPort> interpolation_port,
                                                   std::shared_ptr<IMotionStatePort> motion_state_port,
                                                   std::shared_ptr<IHardwareConnectionPort> connection_port,
                                                   std::shared_ptr<IConfigurationPort> config_port) noexcept
    : valve_port_(std::move(valve_port)),
      interpolation_port_(std::move(interpolation_port)),
      motion_state_port_(std::move(motion_state_port)),
      connection_port_(std::move(connection_port)),
      config_port_(std::move(config_port)) {}

Result<void> DispensingProcessService::ValidateHardwareConnection() noexcept {
    if (!connection_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "硬件连接端口未初始化", "DispensingProcessService"));
    }
    auto conn_info = connection_port_->GetConnectionInfo();
    if (conn_info.state != HardwareConnectionState::Connected) {
        return Result<void>::Failure(Error(ErrorCode::HARDWARE_NOT_CONNECTED, "硬件未连接"));
    }
    return Result<void>::Success();
}

Result<DispensingRuntimeParams> DispensingProcessService::BuildRuntimeParams(
    const DispensingRuntimeOverrides& overrides) noexcept {
    auto override_validation = overrides.Validate();
    if (override_validation.IsError()) {
        return Result<DispensingRuntimeParams>::Failure(override_validation.GetError());
    }

    DispensingRuntimeParams params{};
    float32 safety_limit_velocity = 0.0f;
    float32 default_velocity = 0.0f;

    if (config_port_) {
        auto config_result = config_port_->LoadConfiguration();
        if (config_result.IsSuccess()) {
            const auto& config = config_result.Value();

            if (config.machine.max_speed > 0.0f) {
                safety_limit_velocity = config.machine.max_speed;
            }
            if (config.machine.max_acceleration > 0.0f) {
                params.acceleration = config.machine.max_acceleration;
            }
            if (config.machine.pulse_per_mm > 0.0f) {
                params.pulse_per_mm = config.machine.pulse_per_mm;
            }

            if (config.dispensing.dot_diameter_target_mm > 0.0f) {
                float32 interval = config.dispensing.dot_diameter_target_mm;
                if (config.dispensing.dot_edge_gap_mm > 0.0f) {
                    interval += config.dispensing.dot_edge_gap_mm;
                }
                params.trigger_spatial_interval_mm = interval;
            }

            params.compensation_profile.open_comp_ms = config.dispensing.open_comp_ms;
            params.compensation_profile.close_comp_ms = config.dispensing.close_comp_ms;
            params.compensation_profile.retract_enabled = config.dispensing.retract_enabled;
            params.compensation_profile.corner_pulse_scale = config.dispensing.corner_pulse_scale;
            params.compensation_profile.curvature_speed_factor = config.dispensing.curvature_speed_factor;
            params.sample_dt = config.dispensing.trajectory_sample_dt;
            params.sample_ds = config.dispensing.trajectory_sample_ds;
        } else {
            SILIGEN_LOG_WARNING("读取系统配置失败: " + config_result.GetError().GetMessage());
        }

        auto valve_coord_result = config_port_->GetValveCoordinationConfig();
        if (valve_coord_result.IsSuccess()) {
            const auto& valve_cfg = valve_coord_result.Value();
            if (valve_cfg.dispensing_interval_ms > 0) {
                params.dispenser_interval_ms = static_cast<uint32>(valve_cfg.dispensing_interval_ms);
            }
            if (valve_cfg.dispensing_duration_ms > 0) {
                params.dispenser_duration_ms = static_cast<uint32>(valve_cfg.dispensing_duration_ms);
            }
            if (valve_cfg.valve_response_ms >= 0) {
                params.valve_response_ms = static_cast<float32>(valve_cfg.valve_response_ms);
            }
            if (valve_cfg.visual_margin_ms >= 0) {
                params.safety_margin_ms = static_cast<float32>(valve_cfg.visual_margin_ms);
            }
            if (valve_cfg.min_interval_ms >= 0) {
                params.min_interval_ms = static_cast<float32>(valve_cfg.min_interval_ms);
            }
        }
    }

    if (overrides.acceleration_mm_s2.has_value() && *overrides.acceleration_mm_s2 > 0.0f) {
        params.acceleration = *overrides.acceleration_mm_s2;
    }
    if (params.acceleration <= 0.0f) {
        params.acceleration = kDefaultAcceleration;
    }

    float32 velocity = default_velocity;
    if (overrides.dry_run) {
        if (overrides.dry_run_speed_mm_s.has_value() && *overrides.dry_run_speed_mm_s > 0.0f) {
            velocity = *overrides.dry_run_speed_mm_s;
        } else if (overrides.dispensing_speed_mm_s.has_value() && *overrides.dispensing_speed_mm_s > 0.0f) {
            velocity = *overrides.dispensing_speed_mm_s;
        }
    } else {
        if (overrides.dispensing_speed_mm_s.has_value() && *overrides.dispensing_speed_mm_s > 0.0f) {
            velocity = *overrides.dispensing_speed_mm_s;
        }
    }

    params.dispensing_velocity = velocity;

    if (overrides.rapid_speed_mm_s.has_value() && *overrides.rapid_speed_mm_s > 0.0f) {
        params.rapid_velocity = *overrides.rapid_speed_mm_s;
    } else {
        params.rapid_velocity = params.dispensing_velocity;
    }

    if (safety_limit_velocity > 0.0f) {
        params.dispensing_velocity = std::min(params.dispensing_velocity, safety_limit_velocity);
        params.rapid_velocity = std::min(params.rapid_velocity, safety_limit_velocity);
    }

    if (params.pulse_per_mm <= 0.0f) {
        params.pulse_per_mm = kDefaultPulsePerMm;
    }
    if (params.dispenser_interval_ms == 0) {
        params.dispenser_interval_ms = kDefaultDispenserIntervalMs;
    }
    if (params.dispenser_duration_ms == 0) {
        params.dispenser_duration_ms = kDefaultDispenserDurationMs;
    }
    if (params.trigger_spatial_interval_mm <= 0.0f) {
        params.trigger_spatial_interval_mm = kDefaultTriggerSpatialIntervalMm;
    }

    if (params.trigger_spatial_interval_mm <= 0.0f && params.dispensing_velocity > 0.0f) {
        params.trigger_spatial_interval_mm =
            params.dispensing_velocity * (static_cast<float32>(params.dispenser_interval_ms) / 1000.0f);
    }

    params.velocity_guard_enabled = overrides.velocity_guard_enabled;
    params.velocity_guard_ratio = overrides.velocity_guard_ratio;
    params.velocity_guard_abs_mm_s = overrides.velocity_guard_abs_mm_s;
    params.velocity_guard_min_expected_mm_s = overrides.velocity_guard_min_expected_mm_s;
    params.velocity_guard_grace_ms = overrides.velocity_guard_grace_ms;
    params.velocity_guard_interval_ms = overrides.velocity_guard_interval_ms;
    params.velocity_guard_max_consecutive = overrides.velocity_guard_max_consecutive;
    params.velocity_guard_stop_on_violation = overrides.velocity_guard_stop_on_violation;

    if (params.dispensing_velocity <= 0.0f) {
        return Result<DispensingRuntimeParams>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "未指定有效点胶速度", "DispensingProcessService"));
    }

    SILIGEN_LOG_INFO_FMT_HELPER(
        "DXF运行参数: dispensing_speed_mm_s=%.2f, rapid_speed_mm_s=%.2f, acceleration_mm_s2=%.2f, pulse_per_mm=%.1f",
        params.dispensing_velocity,
        params.rapid_velocity,
        params.acceleration,
        params.pulse_per_mm);
    if (params.velocity_guard_enabled) {
        SILIGEN_LOG_INFO_FMT_HELPER(
            "速度保护参数: ratio=%.2f, abs=%.2f, min_expected=%.2f, grace_ms=%d, interval_ms=%d, max_consecutive=%d, stop=%d",
            params.velocity_guard_ratio,
            params.velocity_guard_abs_mm_s,
            params.velocity_guard_min_expected_mm_s,
            params.velocity_guard_grace_ms,
            params.velocity_guard_interval_ms,
            params.velocity_guard_max_consecutive,
            params.velocity_guard_stop_on_violation ? 1 : 0);
    }

    return Result<DispensingRuntimeParams>::Success(params);
}

Result<void> DispensingProcessService::ConfigureCoordinateSystem(const DispensingRuntimeParams& params) noexcept {
    if (!interpolation_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "插补端口未初始化", "DispensingProcessService"));
    }

    Motion::Ports::CoordinateSystemConfig config;
    config.dimension = 2;
    config.axis_map = {LogicalAxisId::X, LogicalAxisId::Y};
    config.max_velocity = params.dispensing_velocity;
    config.max_acceleration = params.acceleration;
    config.look_ahead_enabled = false;

    SILIGEN_LOG_INFO_FMT_HELPER(
        "坐标系配置: max_velocity=%.2fmm/s, acc=%.2fmm/s2",
        config.max_velocity,
        config.max_acceleration);

    return interpolation_port_->ConfigureCoordinateSystem(kCoordinateSystem, config);
}

Result<DispensingExecutionReport> DispensingProcessService::ExecuteProcess(
    const DispensingExecutionPlan& plan,
    const DispensingRuntimeParams& params,
    const DispensingExecutionOptions& options,
    std::atomic<bool>* stop_flag,
    std::atomic<bool>* pause_flag,
    std::atomic<bool>* pause_applied_flag,
    IDispensingExecutionObserver* observer) noexcept {
    auto conn_check = ValidateHardwareConnection();
    if (conn_check.IsError()) {
        return Result<DispensingExecutionReport>::Failure(conn_check.GetError());
    }

    auto coord_result = ConfigureCoordinateSystem(params);
    if (coord_result.IsError()) {
        return Result<DispensingExecutionReport>::Failure(coord_result.GetError());
    }

    CoordinateSystemStopGuard coord_guard(interpolation_port_, kCoordinateSystemMask);
    coord_guard.Arm();

    return ExecutePlanInternal(plan, params, options, stop_flag, pause_flag, pause_applied_flag, observer);
}

Result<DispensingExecutionReport> DispensingProcessService::ExecutePlanInternal(
    const DispensingExecutionPlan& plan,
    const DispensingRuntimeParams& params,
    const DispensingExecutionOptions& options,
    std::atomic<bool>* stop_flag,
    std::atomic<bool>* pause_flag,
    std::atomic<bool>* pause_applied_flag,
    IDispensingExecutionObserver* observer) noexcept {
    if (!interpolation_port_) {
        return Result<DispensingExecutionReport>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "插补端口未初始化", "DispensingProcessService"));
    }

    if (options.dispense_enabled && !valve_port_) {
        return Result<DispensingExecutionReport>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "点胶阀端口未初始化", "DispensingProcessService"));
    }

    DispensingController controller;
    ControllerConfig controller_config;
    controller_config.use_hardware_trigger = options.dispense_enabled;
    controller_config.spatial_interval_mm = params.trigger_spatial_interval_mm;
    controller_config.pulse_per_mm = params.pulse_per_mm;
    controller_config.acceleration_mm_s2 = params.acceleration;
    controller_config.trigger_distances_mm = plan.trigger_distances_mm;

    if (plan.trigger_interval_mm > 0.0f) {
        controller_config.spatial_interval_mm = plan.trigger_interval_mm;
    }

    auto trigger_result = plan.interpolation_points.empty()
                              ? controller.Build(plan.motion_trajectory, controller_config)
                              : controller.Build(plan.interpolation_points, controller_config);
    if (trigger_result.IsError()) {
        return Result<DispensingExecutionReport>::Failure(trigger_result.GetError());
    }

    auto trigger_output = trigger_result.Value();
    if (options.dispense_enabled && trigger_output.trigger_positions.empty()) {
        return Result<DispensingExecutionReport>::Failure(
            Error(ErrorCode::CMP_TRIGGER_SETUP_FAILED,
                  "位置触发不可用，禁止回退为定时触发",
                  "DispensingProcessService"));
    }
    if (plan.interpolation_segments.empty()) {
        return Result<DispensingExecutionReport>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "插补数据为空", "DispensingProcessService"));
    }

    constexpr uint32 kMinBufferSpace = 20;
    constexpr auto kBufferPollInterval = std::chrono::milliseconds(5);
    constexpr auto kBufferStallTimeout = std::chrono::seconds(10);
    constexpr float32 kMinMovingVelocityMmS = 0.1f;

    auto clear_result = interpolation_port_->ClearInterpolationBuffer(kCoordinateSystem);
    if (clear_result.IsError()) {
        return Result<DispensingExecutionReport>::Failure(clear_result.GetError());
    }

    const auto& program = plan.interpolation_segments;
    const auto total_segments = program.size();
    size_t cursor = 0;

    auto query_buffer_space = [&]() -> Result<uint32> {
        auto crd_space_result = interpolation_port_->GetInterpolationBufferSpace(kCoordinateSystem);
        if (crd_space_result.IsError()) {
            return Result<uint32>::Failure(crd_space_result.GetError());
        }
        uint32 space = crd_space_result.Value();

        auto lookahead_space_result = interpolation_port_->GetLookAheadBufferSpace(kCoordinateSystem);
        if (lookahead_space_result.IsSuccess() && lookahead_space_result.Value() > 0) {
            space = std::min(space, lookahead_space_result.Value());
        }

        return Result<uint32>::Success(space);
    };

    auto send_batch = [&](size_t batch_size) -> Result<void> {
        for (size_t i = 0; i < batch_size && cursor < total_segments; ++i, ++cursor) {
            auto add_result = interpolation_port_->AddInterpolationData(kCoordinateSystem, program[cursor]);
            if (add_result.IsError()) {
                return Result<void>::Failure(add_result.GetError());
            }
        }
        return Result<void>::Success();
    };

    auto initial_space_result = query_buffer_space();
    if (initial_space_result.IsError()) {
        return Result<DispensingExecutionReport>::Failure(initial_space_result.GetError());
    }

    const auto initial_space = initial_space_result.Value();
    const auto initial_batch = static_cast<size_t>(
        std::min<uint32>(static_cast<uint32>(total_segments),
                         initial_space > kMinBufferSpace ? (initial_space - kMinBufferSpace) : 0U));

    if (initial_batch == 0 && total_segments > 0) {
        return Result<DispensingExecutionReport>::Failure(
            Error(ErrorCode::HARDWARE_ERROR, "插补缓冲区空间不足，无法发送初始数据", "DispensingProcessService"));
    }

    if (initial_batch > 0) {
        auto send_result = send_batch(initial_batch);
        if (send_result.IsError()) {
            return Result<DispensingExecutionReport>::Failure(send_result.GetError());
        }
    }

    auto initial_flush = interpolation_port_->FlushInterpolationData(kCoordinateSystem);
    if (initial_flush.IsError()) {
        return Result<DispensingExecutionReport>::Failure(initial_flush.GetError());
    }

    SupplyValveGuard supply_guard(valve_port_);
    if (options.dispense_enabled) {
        auto supply_result = supply_guard.Open();
        if (supply_result.IsError()) {
            return Result<DispensingExecutionReport>::Failure(supply_result.GetError());
        }
        auto stabilization_ms = SupplyStabilizationPolicy::Resolve(config_port_);
        if (stabilization_ms.IsError()) {
            return Result<DispensingExecutionReport>::Failure(stabilization_ms.GetError());
        }
        if (stabilization_ms.Value() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(stabilization_ms.Value()));
        }
    }

    DispenserOperationGuard dispenser_guard(valve_port_);
    if (options.dispense_enabled) {
        Ports::PositionTriggeredDispenserParams params_for_trigger;
        params_for_trigger.trigger_positions = trigger_output.trigger_positions;
        params_for_trigger.axis = trigger_output.trigger_axis;
        params_for_trigger.pulse_width_ms = params.dispenser_duration_ms;
        params_for_trigger.start_level = ResolveDispenserStartLevel();
        DispenseCompensationService compensation_service;
        params_for_trigger = compensation_service.ApplyPositionCompensation(params_for_trigger,
                                                                            params.compensation_profile,
                                                                            false);
        auto start_result = dispenser_guard.Start(params_for_trigger);
        if (start_result.IsError()) {
            return Result<DispensingExecutionReport>::Failure(start_result.GetError());
        }
    }

    auto override_result = interpolation_port_->SetCoordinateSystemVelocityOverride(kCoordinateSystem, 100.0f);
    if (override_result.IsError()) {
        SILIGEN_LOG_WARNING("设置坐标系速度倍率失败: " + override_result.GetError().GetMessage());
    }

    auto start_result = interpolation_port_->StartCoordinateSystemMotion(kCoordinateSystemMask);
    if (start_result.IsError()) {
        return Result<DispensingExecutionReport>::Failure(start_result.GetError());
    }

    MotionObserverGuard observer_guard(observer);
    observer_guard.Start();
    PublishProgress(observer, 0U, static_cast<uint32>(total_segments));

    auto last_progress = std::chrono::steady_clock::now();
    while (cursor < total_segments) {
        auto pause_result =
            WaitWhilePaused(stop_flag, pause_flag, pause_applied_flag, options.dispense_enabled, observer);
        if (pause_result.IsError()) {
            return Result<DispensingExecutionReport>::Failure(pause_result.GetError());
        }

        if (IsStopRequested(stop_flag)) {
            SILIGEN_LOG_INFO("DXF执行循环检测到停止请求，中止执行");
            return Result<DispensingExecutionReport>::Failure(
                Error(ErrorCode::INVALID_STATE, "用户请求停止", "DispensingProcessService"));
        }

        auto space_result = query_buffer_space();
        if (space_result.IsError()) {
            return Result<DispensingExecutionReport>::Failure(space_result.GetError());
        }

        auto progress_status_result = interpolation_port_->GetCoordinateSystemStatus(kCoordinateSystem);
        if (progress_status_result.IsSuccess()) {
            const auto& progress_status = progress_status_result.Value();
            const auto remaining_segments = std::min<size_t>(total_segments, progress_status.remaining_segments);
            const auto executed_segments = static_cast<uint32>(total_segments - remaining_segments);
            PublishProgress(observer, executed_segments, static_cast<uint32>(total_segments));
        }

        const auto space = space_result.Value();
        if (space <= kMinBufferSpace) {
            if (IsStopRequested(stop_flag)) {
                SILIGEN_LOG_INFO("缓冲区等待期间检测到停止请求，中止执行");
                return Result<DispensingExecutionReport>::Failure(
                    Error(ErrorCode::INVALID_STATE, "用户请求停止", "DispensingProcessService"));
            }

            auto now = std::chrono::steady_clock::now();
            auto status_result = interpolation_port_->GetCoordinateSystemStatus(kCoordinateSystem);
            if (status_result.IsSuccess()) {
                const auto& status = status_result.Value();
                if (status.is_moving &&
                    (status.current_velocity > kMinMovingVelocityMmS ||
                     status.current_velocity < -kMinMovingVelocityMmS)) {
                    last_progress = now;
                }
            }
            if (now - last_progress > kBufferStallTimeout) {
                uint32 crd_space = space;
                uint32 lookahead_space = 0;
                auto crd_space_result = interpolation_port_->GetInterpolationBufferSpace(kCoordinateSystem);
                if (crd_space_result.IsSuccess()) {
                    crd_space = crd_space_result.Value();
                }
                auto lookahead_space_result = interpolation_port_->GetLookAheadBufferSpace(kCoordinateSystem);
                if (lookahead_space_result.IsSuccess()) {
                    lookahead_space = lookahead_space_result.Value();
                }
                auto stall_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - last_progress).count();
                if (status_result.IsSuccess()) {
                    const auto& status = status_result.Value();
                    SILIGEN_LOG_WARNING_FMT_HELPER(
                        "插补缓冲区无可用空间超时: waited_ms=%lld, space=%u, crd_space=%u, lookahead_space=%u, "
                        "state=%d, moving=%d, remaining=%u, vel=%.3f, raw_status=%d, raw_segment=%d, mc_ret=%d",
                        static_cast<long long>(stall_ms),
                        space,
                        crd_space,
                        lookahead_space,
                        static_cast<int>(status.state),
                        status.is_moving ? 1 : 0,
                        status.remaining_segments,
                        status.current_velocity,
                        status.raw_status_word,
                        status.raw_segment,
                        status.mc_status_ret);
                } else {
                    SILIGEN_LOG_WARNING("插补缓冲区无可用空间超时且获取坐标系状态失败: " +
                                        status_result.GetError().GetMessage());
                }
                return Result<DispensingExecutionReport>::Failure(
                    Error(ErrorCode::MOTION_TIMEOUT, "插补缓冲区长期无可用空间", "DispensingProcessService"));
            }
            std::this_thread::sleep_for(kBufferPollInterval);
            continue;
        }

        const auto batch_size = static_cast<size_t>(
            std::min<uint32>(static_cast<uint32>(total_segments - cursor), space - kMinBufferSpace));
        if (batch_size == 0) {
            std::this_thread::sleep_for(kBufferPollInterval);
            continue;
        }

        auto send_result = send_batch(batch_size);
        if (send_result.IsError()) {
            return Result<DispensingExecutionReport>::Failure(send_result.GetError());
        }

        auto flush_result = interpolation_port_->FlushInterpolationData(kCoordinateSystem);
        if (flush_result.IsError()) {
            return Result<DispensingExecutionReport>::Failure(flush_result.GetError());
        }

        last_progress = std::chrono::steady_clock::now();
    }

    int32 timeout_ms = static_cast<int32>(std::max(0.0f, trigger_output.estimated_motion_time_ms));
    if (timeout_ms == 0) {
        timeout_ms = static_cast<int32>(kMotionCompletionToleranceMs);
    } else {
        timeout_ms += static_cast<int32>(kMotionCompletionToleranceMs);
    }

    auto wait_result = WaitForMotionComplete(
        timeout_ms,
        stop_flag,
        pause_flag,
        pause_applied_flag,
        static_cast<uint32>(total_segments),
        options.dispense_enabled,
        observer);
    if (wait_result.IsError()) {
        return Result<DispensingExecutionReport>::Failure(wait_result.GetError());
    }

    PublishProgress(observer, static_cast<uint32>(total_segments), static_cast<uint32>(total_segments));

    DispensingExecutionReport report;
    report.executed_segments = static_cast<uint32>(total_segments);
    report.total_distance = plan.total_length_mm > 0.0f ? plan.total_length_mm : plan.motion_trajectory.total_length;

    return Result<DispensingExecutionReport>::Success(report);
}

Result<void> DispensingProcessService::WaitForMotionComplete(int32 timeout_ms,
                                                             std::atomic<bool>* stop_flag,
                                                             std::atomic<bool>* pause_flag,
                                                             std::atomic<bool>* pause_applied_flag,
                                                             uint32 total_segments,
                                                             bool dispense_enabled,
                                                             IDispensingExecutionObserver* observer) noexcept {
    if (!interpolation_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "插补端口未初始化", "DispensingProcessService"));
    }

    auto start = std::chrono::steady_clock::now();
    while (true) {
        auto pause_result = WaitWhilePaused(stop_flag, pause_flag, pause_applied_flag, dispense_enabled, observer);
        if (pause_result.IsError()) {
            return pause_result;
        }

        if (IsStopRequested(stop_flag)) {
            StopExecution(stop_flag, pause_flag);
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "执行已被取消", "DispensingProcessService"));
        }

        auto status_result = interpolation_port_->GetCoordinateSystemStatus(kCoordinateSystem);
        if (status_result.IsError()) {
            return Result<void>::Failure(status_result.GetError());
        }

        const auto& status = status_result.Value();
        const auto remaining_segments = std::min<uint32>(total_segments, status.remaining_segments);
        const auto executed_segments = total_segments - remaining_segments;
        PublishProgress(observer, executed_segments, total_segments);
        bool completed = (status.state == Motion::Ports::CoordinateSystemState::IDLE) ||
                         (!status.is_moving && status.remaining_segments == 0);
        if (completed) {
            return Result<void>::Success();
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (timeout_ms > 0 && elapsed > timeout_ms) {
            return Result<void>::Failure(
                Error(ErrorCode::MOTION_TIMEOUT, "等待运动完成超时", "DispensingProcessService"));
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(kStatusPollIntervalMs));
    }
}

Result<void> DispensingProcessService::WaitWhilePaused(std::atomic<bool>* stop_flag,
                                                       std::atomic<bool>* pause_flag,
                                                       std::atomic<bool>* pause_applied_flag,
                                                       bool dispense_enabled,
                                                       IDispensingExecutionObserver* observer) noexcept {
    if (!pause_flag || !pause_flag->load()) {
        return Result<void>::Success();
    }

    if (!interpolation_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "插补端口未初始化", "DispensingProcessService"));
    }

    bool pause_entered = false;
    while (pause_flag->load()) {
        if (!pause_entered) {
            auto override_result = interpolation_port_->SetCoordinateSystemVelocityOverride(kCoordinateSystem, 0.0f);
            if (override_result.IsError()) {
                return Result<void>::Failure(override_result.GetError());
            }

            if (dispense_enabled && valve_port_) {
                auto pause_result = valve_port_->PauseDispenser();
                if (pause_result.IsError()) {
                    SILIGEN_LOG_WARNING("暂停点胶阀失败: " + pause_result.GetError().GetMessage());
                }
            }

            if (pause_applied_flag) {
                pause_applied_flag->store(true);
            }
            PublishPauseState(observer, true);
            pause_entered = true;
        }

        if (IsStopRequested(stop_flag)) {
            StopExecution(stop_flag, pause_flag);
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "执行已被取消", "DispensingProcessService"));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kStatusPollIntervalMs));
    }

    if (pause_entered) {
        if (dispense_enabled && valve_port_) {
            auto resume_result = valve_port_->ResumeDispenser();
            if (resume_result.IsError()) {
                SILIGEN_LOG_WARNING("恢复点胶阀失败: " + resume_result.GetError().GetMessage());
            }
        }

        auto override_result = interpolation_port_->SetCoordinateSystemVelocityOverride(kCoordinateSystem, 100.0f);
        if (override_result.IsError()) {
            return Result<void>::Failure(override_result.GetError());
        }

        if (pause_applied_flag) {
            pause_applied_flag->store(false);
        }
        PublishPauseState(observer, false);
    }

    return Result<void>::Success();
}

void DispensingProcessService::StopExecution(std::atomic<bool>* stop_flag, std::atomic<bool>* pause_flag) noexcept {
    if (stop_flag) {
        stop_flag->store(true);
    }
    if (pause_flag) {
        pause_flag->store(false);
    }
    if (interpolation_port_) {
        auto result = interpolation_port_->StopCoordinateSystemMotion(kCoordinateSystemMask);
        if (result.IsError()) {
            SILIGEN_LOG_WARNING("停止坐标系运动失败: " + result.GetError().GetMessage());
        }
    }
    if (valve_port_) {
        auto result = valve_port_->StopDispenser();
        if (result.IsError()) {
            SILIGEN_LOG_WARNING("停止点胶阀失败: " + result.GetError().GetMessage());
        }
    }
}

short DispensingProcessService::ResolveDispenserStartLevel() const noexcept {
    if (!config_port_) {
        return 0;
    }
    auto config_result = config_port_->GetDispenserValveConfig();
    if (config_result.IsError()) {
        SILIGEN_LOG_WARNING("读取点胶阀配置失败: " + config_result.GetError().GetMessage());
        return 0;
    }
    const auto& dispenser_config = config_result.Value();
    const bool active_high = (dispenser_config.pulse_type == 0);
    return static_cast<short>(active_high ? 0 : 1);
}

bool DispensingProcessService::IsStopRequested(const std::atomic<bool>* stop_flag) const noexcept {
    return stop_flag && stop_flag->load();
}

void DispensingProcessService::PublishProgress(IDispensingExecutionObserver* observer,
                                               uint32 executed_segments,
                                               uint32 total_segments) const noexcept {
    if (!observer) {
        return;
    }
    observer->OnProgress(executed_segments, total_segments);
}

void DispensingProcessService::PublishPauseState(IDispensingExecutionObserver* observer, bool paused) const noexcept {
    if (!observer) {
        return;
    }
    observer->OnPauseStateChanged(paused);
}

}  // namespace Siligen::Domain::Dispensing::DomainServices
