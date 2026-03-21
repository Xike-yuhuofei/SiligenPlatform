#define MODULE_NAME "DispensingExecutionUseCase"

#include "DispensingExecutionUseCase.h"
#include "application/services/dxf/DxfPbPreparationService.h"

#include "domain/dispensing/domain-services/DispensingProcessService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/interfaces/ILoggingService.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <thread>
#include <utility>

using namespace Siligen::Shared::Types;

namespace Siligen::Application::UseCases::Dispensing {

namespace {

bool PrepareVelocityTraceFile(const std::string& output_path,
                              std::ofstream& file,
                              std::string& error);

class VelocityTraceObserver : public Domain::Dispensing::Ports::IDispensingExecutionObserver {
   public:
    VelocityTraceObserver(const std::shared_ptr<Domain::Motion::Ports::IMotionStatePort>& motion_state_port,
                          const std::string& output_path,
                          int32 interval_ms,
                          bool dispense_enabled)
        : motion_state_port_(motion_state_port),
          output_path_(output_path),
          interval_ms_(interval_ms),
          dispense_flag_(dispense_enabled ? 1 : 0) {}

    void OnMotionStart() noexcept override {
        if (started_) {
            return;
        }
        started_ = true;
        if (output_path_.empty()) {
            SILIGEN_LOG_WARNING("速度采样启用但输出路径为空，跳过采样");
            return;
        }

        const int trace_interval_ms = std::max(1, interval_ms_);
        const std::string trace_path = output_path_;

        SILIGEN_LOG_INFO_FMT_HELPER(
            "启动速度采样线程: interval_ms=%d, path=%s",
            trace_interval_ms,
            trace_path.c_str());

        stop_flag_.store(false);
        worker_ = std::thread([this, trace_interval_ms, trace_path]() {
            std::ofstream file;
            std::string error;
            if (!PrepareVelocityTraceFile(trace_path, file, error)) {
                SILIGEN_LOG_WARNING("速度采样文件初始化失败: " + error);
                return;
            }

            const auto interval = std::chrono::milliseconds(trace_interval_ms);
            auto start_time = std::chrono::steady_clock::now();
            auto next_tick = start_time;
            size_t count = 0;
            bool warned_position = false;
            bool warned_velocity = false;
            bool warned_motion_port = false;

            while (!stop_flag_.load()) {
                auto now = std::chrono::steady_clock::now();
                if (now < next_tick) {
                    std::this_thread::sleep_for(next_tick - now);
                    continue;
                }

                const double ts = std::chrono::duration<double>(now - start_time).count();
                float32 x = 0.0f;
                float32 y = 0.0f;

                if (motion_state_port_) {
                    auto pos_result = motion_state_port_->GetCurrentPosition();
                    if (pos_result.IsSuccess()) {
                        x = pos_result.Value().x;
                        y = pos_result.Value().y;
                    } else if (!warned_position) {
                        warned_position = true;
                        SILIGEN_LOG_WARNING("速度采样读取位置失败: " +
                                            pos_result.GetError().GetMessage());
                    }
                } else if (!warned_motion_port) {
                    warned_motion_port = true;
                    SILIGEN_LOG_WARNING("速度采样未注入MotionState端口，位置/速度输出为0");
                }

                float32 velocity = 0.0f;

                if (motion_state_port_) {
                    auto vx_result = motion_state_port_->GetAxisVelocity(LogicalAxisId::X);
                    auto vy_result = motion_state_port_->GetAxisVelocity(LogicalAxisId::Y);
                    if (vx_result.IsSuccess() && vy_result.IsSuccess()) {
                        const float32 vx = vx_result.Value();
                        const float32 vy = vy_result.Value();
                        velocity = std::sqrt(vx * vx + vy * vy);
                    } else if (vx_result.IsSuccess() && !vy_result.IsSuccess()) {
                        velocity = std::fabs(vx_result.Value());
                    } else if (vy_result.IsSuccess() && !vx_result.IsSuccess()) {
                        velocity = std::fabs(vy_result.Value());
                    } else if (!warned_velocity) {
                        warned_velocity = true;
                        SILIGEN_LOG_WARNING("速度采样读取轴速度失败，将输出0");
                    }
                }

                file << ts << ',' << x << ',' << y << ',' << velocity << ',' << dispense_flag_ << '\n';
                ++count;
                next_tick += interval;
            }

            file.close();
            SILIGEN_LOG_INFO_FMT_HELPER(
                "速度采样完成: path=%s, points=%zu",
                trace_path.c_str(),
                count);
        });
    }

    void OnMotionStop() noexcept override {
        stop_flag_.store(true);
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    ~VelocityTraceObserver() override {
        OnMotionStop();
    }

   private:
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port_;
    std::string output_path_;
    int32 interval_ms_ = 0;
    int dispense_flag_ = 0;
    std::atomic<bool> stop_flag_{false};
    std::thread worker_;
    bool started_ = false;
};

class TaskTrackingObserver final : public Domain::Dispensing::Ports::IDispensingExecutionObserver {
   public:
    TaskTrackingObserver(
        Domain::Dispensing::Ports::IDispensingExecutionObserver* inner,
        const std::shared_ptr<TaskExecutionContext>& context)
        : inner_(inner), context_(context) {}

    void OnMotionStart() noexcept override {
        if (inner_) {
            inner_->OnMotionStart();
        }
        if (context_) {
            context_->state.store(TaskState::RUNNING);
        }
    }

    void OnMotionStop() noexcept override {
        if (inner_) {
            inner_->OnMotionStop();
        }
    }

    void OnProgress(std::uint32_t executed_segments, std::uint32_t total_segments) noexcept override {
        if (inner_) {
            inner_->OnProgress(executed_segments, total_segments);
        }
        if (context_) {
            context_->executed_segments.store(executed_segments);
            context_->total_segments.store(total_segments);
        }
    }

    void OnPauseStateChanged(bool paused) noexcept override {
        if (inner_) {
            inner_->OnPauseStateChanged(paused);
        }
        if (context_) {
            context_->state.store(paused ? TaskState::PAUSED : TaskState::RUNNING);
        }
    }

   private:
    Domain::Dispensing::Ports::IDispensingExecutionObserver* inner_ = nullptr;
    std::shared_ptr<TaskExecutionContext> context_;
};

std::string ResolveVelocityTracePath(const std::string& dxf_path, const std::string& output_path) {
    std::string resolved = output_path.empty() ? std::string("logs/velocity_trace") : output_path;
    std::filesystem::path path(resolved);
    if (path.extension().empty()) {
        std::filesystem::path dxf(dxf_path);
        std::string stem = dxf.stem().string();
        if (stem.empty()) {
            stem = "dxf_velocity_trace";
        }
        path /= (stem + "_velocity_trace.csv");
    }
    return path.string();
}

bool PrepareVelocityTraceFile(const std::string& output_path,
                              std::ofstream& file,
                              std::string& error) {
    std::filesystem::path path(output_path);
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    file.open(output_path, std::ios::binary);
    if (!file) {
        error = "无法写入速度采样文件: " + output_path;
        return false;
    }

    file.setf(std::ios::fixed);
    file << std::setprecision(6);
    file << "timestamp_s,x_mm,y_mm,velocity_mm_s,dispense_on\n";
    return true;
}

}  // namespace

Result<void> DispensingMVPRequest::Validate() const noexcept {
    if (dxf_filepath.empty()) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "DXF文件路径不能为空"));
    }
    if (dispensing_velocity != 0.0f) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "dispensing_velocity已弃用，请使用dispensing_speed_mm_s"));
    }
    if (dry_run_velocity != 0.0f) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "dry_run_velocity已弃用，请使用dry_run_speed_mm_s"));
    }
    if (max_jerk < 0.0f) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "max_jerk不能为负数"));
    }
    if (arc_tolerance_mm < 0.0f) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "arc_tolerance_mm不能为负数"));
    }
    if (continuity_tolerance_mm < 0.0f) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "continuity_tolerance_mm不能为负数"));
    }
    if (curve_chain_angle_deg < 0.0f || curve_chain_angle_deg > 180.0f) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "curve_chain_angle_deg超出范围(0-180)"));
    }
    if (curve_chain_max_segment_mm < 0.0f) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "curve_chain_max_segment_mm不能为负数"));
    }
    Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides overrides;
    overrides.dry_run = dry_run;
    overrides.dispensing_speed_mm_s = dispensing_speed_mm_s;
    overrides.dry_run_speed_mm_s = dry_run_speed_mm_s;
    overrides.rapid_speed_mm_s = rapid_speed_mm_s;
    overrides.acceleration_mm_s2 = acceleration_mm_s2;
    overrides.velocity_guard_enabled = velocity_guard_enabled;
    overrides.velocity_guard_ratio = velocity_guard_ratio;
    overrides.velocity_guard_abs_mm_s = velocity_guard_abs_mm_s;
    overrides.velocity_guard_min_expected_mm_s = velocity_guard_min_expected_mm_s;
    overrides.velocity_guard_grace_ms = velocity_guard_grace_ms;
    overrides.velocity_guard_interval_ms = velocity_guard_interval_ms;
    overrides.velocity_guard_max_consecutive = velocity_guard_max_consecutive;
    overrides.velocity_guard_stop_on_violation = velocity_guard_stop_on_violation;

    auto override_validation = overrides.Validate();
    if (override_validation.IsError()) {
        return Result<void>::Failure(override_validation.GetError());
    }
    return Result<void>::Success();
}

DispensingExecutionUseCase::DispensingExecutionUseCase(
    std::shared_ptr<DispensingPlanner> planner,
    std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port,
    std::shared_ptr<Domain::Motion::Ports::IInterpolationPort> interpolation_port,
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Domain::Machine::Ports::IHardwareConnectionPort> connection_port,
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
    std::shared_ptr<Domain::System::Ports::IEventPublisherPort> event_port,
    std::shared_ptr<Domain::Dispensing::Ports::ITaskSchedulerPort> task_scheduler_port,
    std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService> pb_preparation_service)
    : valve_port_(std::move(valve_port)),
      interpolation_port_(std::move(interpolation_port)),
      motion_state_port_(std::move(motion_state_port)),
      connection_port_(std::move(connection_port)),
      config_port_(std::move(config_port)),
      event_port_(std::move(event_port)),
      task_scheduler_port_(std::move(task_scheduler_port)),
      pb_preparation_service_(pb_preparation_service
                                  ? std::move(pb_preparation_service)
                                  : std::make_shared<Siligen::Application::Services::DXF::DxfPbPreparationService>(
                                        config_port_)),
      planner_(std::move(planner)),
      process_service_(std::make_shared<::Siligen::Domain::Dispensing::DomainServices::DispensingProcessService>(
          valve_port_,
          interpolation_port_,
          motion_state_port_,
          connection_port_,
          config_port_)) {
    if (!planner_) {
        throw std::invalid_argument("DispensingExecutionUseCase: planner cannot be null");
    }
}

Result<DispensingMVPResult> DispensingExecutionUseCase::Execute(const DispensingMVPRequest& request) {
    return ExecuteInternal(request, nullptr);
}

Result<DispensingMVPResult> DispensingExecutionUseCase::ExecuteInternal(
    const DispensingMVPRequest& request,
    const std::shared_ptr<TaskExecutionContext>& context) {
    SILIGEN_LOG_INFO("开始执行DXF点胶: " + request.dxf_filepath);

    std::lock_guard<std::mutex> lock(execution_mutex_);
    stop_requested_ = false;
    if (context) {
        context->pause_requested.store(false);
        context->pause_applied.store(false);
    }

    auto validation = request.Validate();
    if (!validation.IsSuccess()) {
        SILIGEN_LOG_ERROR("请求参数验证失败: " + validation.GetError().GetMessage());
        return Result<DispensingMVPResult>::Failure(validation.GetError());
    }
    if (!process_service_) {
        return Result<DispensingMVPResult>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "点胶流程服务未初始化", "DispensingExecutionUseCase"));
    }

    auto conn_check = ValidateHardwareConnection();
    if (!conn_check.IsSuccess()) {
        SILIGEN_LOG_ERROR("硬件连接验证失败: " + conn_check.GetError().GetMessage());
        return Result<DispensingMVPResult>::Failure(conn_check.GetError());
    }

    auto runtime_result = RefreshRuntimeParameters(request);
    if (runtime_result.IsError()) {
        return Result<DispensingMVPResult>::Failure(runtime_result.GetError());
    }

    auto pb_result = pb_preparation_service_->EnsurePbReady(request.dxf_filepath);
    if (pb_result.IsError()) {
        return Result<DispensingMVPResult>::Failure(pb_result.GetError());
    }
    const std::string prepared_pb_path = pb_result.Value();

    auto plan_request = BuildPlanRequest(request);
    plan_request.dxf_filepath = prepared_pb_path;
    auto plan_result = planner_->Plan(plan_request);
    if (!plan_result.IsSuccess()) {
        SILIGEN_LOG_ERROR("DXF规划失败: " + plan_result.GetError().GetMessage());
        return Result<DispensingMVPResult>::Failure(plan_result.GetError());
    }

    const auto& plan = plan_result.Value();
    if (plan.motion_trajectory.points.size() < 2) {
        return Result<DispensingMVPResult>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "轨迹点数量不足", "DispensingExecutionUseCase"));
    }

    VelocityTraceSettings trace_settings;
    if (request.velocity_trace_enabled) {
        trace_settings.enabled = true;
        trace_settings.interval_ms = request.velocity_trace_interval_ms;
        trace_settings.output_path = request.velocity_trace_path;

        if ((trace_settings.interval_ms <= 0 || trace_settings.output_path.empty()) && config_port_) {
            auto trace_config_result = config_port_->GetVelocityTraceConfig();
            if (trace_config_result.IsSuccess()) {
                const auto& trace_config = trace_config_result.Value();
                if (trace_settings.interval_ms <= 0) {
                    trace_settings.interval_ms = trace_config.interval_ms;
                }
                if (trace_settings.output_path.empty()) {
                    trace_settings.output_path = trace_config.output_path;
                }
            } else {
                SILIGEN_LOG_WARNING("读取速度采样配置失败: " + trace_config_result.GetError().GetMessage());
            }
        }
        if (trace_settings.interval_ms <= 0) {
            trace_settings.interval_ms = 50;
        }
        if (trace_settings.output_path.empty()) {
            trace_settings.output_path = "logs/velocity_trace";
        }
        trace_settings.output_path = ResolveVelocityTracePath(request.dxf_filepath, trace_settings.output_path);

        SILIGEN_LOG_INFO_FMT_HELPER(
            "速度采样启用: interval_ms=%d, output_path=%s",
            trace_settings.interval_ms,
            trace_settings.output_path.c_str());
    }

    DispensingMVPResult result;
    result.total_segments = static_cast<uint32>(plan.interpolation_segments.size());
    if (result.total_segments == 0) {
        result.total_segments = static_cast<uint32>(plan.process_path.segments.size());
    }
    if (result.total_segments == 0 && !plan.path.segments.empty()) {
        result.total_segments = static_cast<uint32>(plan.path.segments.size());
    }
    if (result.total_segments == 0 && plan.motion_trajectory.points.size() > 1) {
        result.total_segments = static_cast<uint32>(plan.motion_trajectory.points.size() - 1);
    }
    result.total_distance = plan.total_length_mm > 0.0f ? plan.total_length_mm : plan.motion_trajectory.total_length;
    if (context) {
        context->total_segments.store(result.total_segments);
        context->executed_segments.store(0);
        context->state.store(TaskState::RUNNING);
    }

    bool dispense_enabled = !request.dry_run;
    Domain::Dispensing::ValueObjects::DispensingExecutionOptions options;
    options.dispense_enabled = dispense_enabled;
    options.use_hardware_trigger = request.use_hardware_trigger;

    Domain::Dispensing::ValueObjects::DispensingExecutionPlan exec_plan;
    exec_plan.interpolation_segments = plan.interpolation_segments;
    exec_plan.interpolation_points = plan.interpolation_points;
    exec_plan.motion_trajectory = plan.motion_trajectory;
    exec_plan.trigger_distances_mm = plan.trigger_distances_mm;
    exec_plan.trigger_interval_ms = plan.trigger_interval_ms;
    exec_plan.trigger_interval_mm = plan.trigger_interval_mm;
    exec_plan.total_length_mm = plan.total_length_mm;

    std::unique_ptr<VelocityTraceObserver> trace_observer;
    if (trace_settings.enabled) {
        trace_observer = std::make_unique<VelocityTraceObserver>(
            motion_state_port_,
            trace_settings.output_path,
            trace_settings.interval_ms,
            dispense_enabled);
    }
    TaskTrackingObserver task_observer(trace_observer ? trace_observer.get() : nullptr, context);

    auto start_time = std::chrono::steady_clock::now();
    auto exec_result = process_service_->ExecuteProcess(exec_plan,
                                                        runtime_params_,
                                                        options,
                                                        &stop_requested_,
                                                        context ? &context->pause_requested : nullptr,
                                                        context ? &context->pause_applied : nullptr,
                                                        &task_observer);
    auto end_time = std::chrono::steady_clock::now();

    result.execution_time_seconds = std::chrono::duration<float32>(end_time - start_time).count();

    if (!exec_result.IsSuccess()) {
        result.success = false;
        result.message = "点胶执行失败: " + exec_result.GetError().GetMessage();
        result.error_details = exec_result.GetError().GetMessage();
        result.quality_metrics.run_count = 1;
        result.quality_metrics.interruption_count = 1;
        result.quality_metrics_available = true;
        return Result<DispensingMVPResult>::Failure(exec_result.GetError());
    }

    result.executed_segments = exec_result.Value().executed_segments;
    result.total_distance = exec_result.Value().total_distance;

    result.success = true;
    result.executed_segments = result.total_segments;
    result.failed_segments = 0;
    result.message = "点胶执行成功";
    result.quality_metrics.run_count = 1;
    result.quality_metrics.interruption_count = 0;
    result.quality_metrics_available = true;

    SILIGEN_LOG_INFO_FMT_HELPER(
        "点胶执行完成: segments=%u, distance=%.3fmm, time=%.2fs",
        result.executed_segments,
        result.total_distance,
        result.execution_time_seconds);

    return Result<DispensingMVPResult>::Success(result);
}

}  // namespace Siligen::Application::UseCases::Dispensing


