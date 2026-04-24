#define MODULE_NAME "ExecutionObserverSupport"

#include "ExecutionObserverSupport.h"

#include "runtime_execution/contracts/motion/IMotionStatePort.h"
#include "runtime_execution/contracts/dispensing/IDispensingProcessPort.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <thread>
#include <utility>

using namespace Siligen::Shared::Types;

namespace Siligen::Application::UseCases::Dispensing {
namespace {

class VelocityTraceObserver final : public Domain::Dispensing::Ports::IDispensingExecutionObserver {
   public:
    VelocityTraceObserver(
        const std::shared_ptr<RuntimeExecution::Contracts::Motion::IMotionStatePort>& motion_state_port,
        std::string output_path,
        int32 interval_ms,
        bool dispense_enabled)
        : motion_state_port_(motion_state_port),
          output_path_(std::move(output_path)),
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
            auto prepare_result = VelocityTracePathPolicy::PrepareOutputFile(trace_path, file);
            if (prepare_result.IsError()) {
                SILIGEN_LOG_WARNING("速度采样文件初始化失败: " + prepare_result.GetError().GetMessage());
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
                        SILIGEN_LOG_WARNING("速度采样读取位置失败: " + pos_result.GetError().GetMessage());
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
            SILIGEN_LOG_INFO_FMT_HELPER("速度采样完成: path=%s, points=%zu", trace_path.c_str(), count);
        });
    }

    void OnMotionStop() noexcept override {
        stop_flag_.store(true);
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    ~VelocityTraceObserver() override { OnMotionStop(); }

   private:
    std::shared_ptr<RuntimeExecution::Contracts::Motion::IMotionStatePort> motion_state_port_;
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
        std::shared_ptr<TaskExecutionContext> context)
        : inner_(inner), context_(std::move(context)) {}

    void OnMotionStart() noexcept override {
        if (inner_) {
            inner_->OnMotionStart();
        }
        if (context_ && !context_->terminal_committed.load()) {
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
        if (context_ && !context_->terminal_committed.load()) {
            context_->executed_segments.store(executed_segments);
            context_->total_segments.store(total_segments);
            context_->reported_executed_segments.store(executed_segments);
            const auto progress_percent =
                total_segments > 0
                    ? static_cast<uint32>((static_cast<std::uint64_t>(executed_segments) * 100ULL) / total_segments)
                    : 0U;
            context_->reported_progress_percent.store(progress_percent);
        }
    }

    void OnPauseStateChanged(bool paused) noexcept override {
        if (inner_) {
            inner_->OnPauseStateChanged(paused);
        }
        if (context_ && !context_->terminal_committed.load()) {
            context_->state.store(paused ? TaskState::PAUSED : TaskState::RUNNING);
        }
    }

   private:
    Domain::Dispensing::Ports::IDispensingExecutionObserver* inner_ = nullptr;
    std::shared_ptr<TaskExecutionContext> context_;
};

class CompositeExecutionObserver final : public Domain::Dispensing::Ports::IDispensingExecutionObserver {
   public:
    CompositeExecutionObserver(
        std::unique_ptr<Domain::Dispensing::Ports::IDispensingExecutionObserver> inner,
        std::shared_ptr<TaskExecutionContext> context)
        : inner_(std::move(inner)), tracker_(inner_.get(), std::move(context)) {}

    void OnMotionStart() noexcept override { tracker_.OnMotionStart(); }
    void OnMotionStop() noexcept override { tracker_.OnMotionStop(); }
    void OnProgress(std::uint32_t executed_segments, std::uint32_t total_segments) noexcept override {
        tracker_.OnProgress(executed_segments, total_segments);
    }
    void OnPauseStateChanged(bool paused) noexcept override { tracker_.OnPauseStateChanged(paused); }

   private:
    std::unique_ptr<Domain::Dispensing::Ports::IDispensingExecutionObserver> inner_;
    TaskTrackingObserver tracker_;
};

}  // namespace

std::unique_ptr<Domain::Dispensing::Ports::IDispensingExecutionObserver> CreateExecutionObserver(
    const std::shared_ptr<RuntimeExecution::Contracts::Motion::IMotionStatePort>& motion_state_port,
    const std::string& trace_output_path,
    int32 trace_interval_ms,
    bool trace_enabled,
    bool dispense_enabled,
    const std::shared_ptr<TaskExecutionContext>& context) {
    std::unique_ptr<Domain::Dispensing::Ports::IDispensingExecutionObserver> inner;
    if (trace_enabled) {
        inner = std::make_unique<VelocityTraceObserver>(
            motion_state_port,
            trace_output_path,
            trace_interval_ms,
            dispense_enabled);
    }
    return std::make_unique<CompositeExecutionObserver>(std::move(inner), context);
}

}  // namespace Siligen::Application::UseCases::Dispensing
