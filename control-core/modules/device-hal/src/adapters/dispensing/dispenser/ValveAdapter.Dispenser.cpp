// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "ValveAdapter"

#include "ValveAdapter.h"
#include "domain/dispensing/domain-services/DispenseCompensationService.h"
#include "shared/types/AxisTypes.h"
#include "shared/interfaces/ILoggingService.h"
#include <chrono>
#include <thread>

namespace Siligen {
namespace Infrastructure {
namespace Adapters {

using namespace Domain::Dispensing::Ports;
using namespace Shared::Types;

// 类型安全的轴号转换 (从 AxisTypes.h)
using Siligen::Shared::Types::ToSdkAxis;
using Siligen::Shared::Types::ToSdkShort;

namespace {
constexpr size_t kCmpBufChunkSize = 1000;
constexpr int kCmpBufPollIntervalMs = 5;
}  // namespace

// ============================================================
// 点胶阀控制实现
// ============================================================

Result<DispenserValveState> ValveAdapter::StartDispenser(
    const DispenserValveParams& params) noexcept
{
    try {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);

        // 1. 验证参数
        if (!params.IsValid()) {
            std::string error_msg = params.GetValidationError();
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = error_msg;
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_PARAMETER, error_msg));
        }

        // 2. 刷新计时模式状态，避免已完成但仍显示运行中
        RefreshTimedDispenserStateIfCompleted();

        // 2. 检查是否已在运行
        if (dispenser_state_.status == DispenserValveStatus::Running) {
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, "点胶阀已在运行中"));
        }

        // 2.5 预清理CMP状态，避免上一次残留导致二次启动异常
        ResetDispenserHardwareState("StartDispenser", false);

        // 3. 调用 MultiCard API（CMP 触发）
        // 使用配置的 CMP 通道输出脉冲
        SILIGEN_LOG_INFO(std::string("StartDispenser: 开始启动点胶阀"));
        SILIGEN_LOG_INFO("参数: count=" + std::to_string(params.count) +
                         ", intervalMs=" + std::to_string(params.intervalMs) +
                         ", durationMs=" + std::to_string(params.durationMs));

        auto steady_start_time = std::chrono::steady_clock::now();
        auto system_start_time = std::chrono::system_clock::now();
        auto start_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            system_start_time.time_since_epoch()).count();

        Domain::Dispensing::DomainServices::DispenseCompensationService compensation_service;
        auto adjusted_params =
            compensation_service.ApplyTimedCompensation(params, compensation_profile_, false);

        int result = CallMC_CmpPluse(
            static_cast<int16>(dispenser_config_.cmp_channel),
            adjusted_params.count,
            adjusted_params.intervalMs,
            adjusted_params.durationMs
        );

        if (result != 0) {
            std::string error_msg = CreateErrorMessage(result, "StartDispenser");

            // 详细错误日志
            SILIGEN_LOG_ERROR("StartDispenser: 点胶阀启动失败");
            SILIGEN_LOG_ERROR(std::string("错误详情: ") + error_msg);
            SILIGEN_LOG_ERROR("硬件调用: MC_CmpPluse(count=" + std::to_string(params.count) +
                             ", interval=" + std::to_string(params.intervalMs) +
                             "ms, width=" + std::to_string(params.durationMs) + "ms)");
            SILIGEN_LOG_ERROR("返回码: " + std::to_string(result));
            SILIGEN_LOG_ERROR("诊断建议: 检查CMP通道配置、硬件连接、点胶阀电源、CMP输出端子");

            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = error_msg;
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
        }

        SILIGEN_LOG_INFO("StartDispenser: 点胶阀启动成功");

        // 4. 更新状态
        dispenser_state_.status = DispenserValveStatus::Running;
        dispenser_state_.completedCount = 0;
        dispenser_state_.totalCount = adjusted_params.count;
        dispenser_state_.remainingCount = adjusted_params.count;
        dispenser_state_.progress = 0.0f;
        dispenser_state_.errorMessage.clear();
        dispenser_continuous_ = false;
        dispenser_run_mode_ = DispenserRunMode::Timed;

        // 5. 保存参数和启动时间
        dispenser_params_ = adjusted_params;
        dispenser_start_time_ = steady_start_time;
        dispenser_elapsed_before_pause_ = 0;

        // 记录时间戳（毫秒）
        dispenser_state_.startTime = static_cast<uint64>(start_timestamp);

        SILIGEN_LOG_INFO("StartDispenser: count=" + std::to_string(adjusted_params.count) +
                         ", intervalMs=" + std::to_string(adjusted_params.intervalMs) +
                         ", durationMs=" + std::to_string(adjusted_params.durationMs));

        return Result<DispenserValveState>::Success(dispenser_state_);
    }
    catch (const std::exception& e) {
        dispenser_state_.status = DispenserValveStatus::Error;
        dispenser_state_.errorMessage = std::string("Exception: ") + e.what();
        return Result<DispenserValveState>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, dispenser_state_.errorMessage));
    }
}

Result<DispenserValveState> ValveAdapter::OpenDispenser() noexcept {
    try {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);

        RefreshTimedDispenserStateIfCompleted();

        if (dispenser_state_.status == DispenserValveStatus::Running) {
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, "点胶阀已在运行中"));
        }

        // 预清理CMP状态，避免上一次残留导致二次启动异常
        ResetDispenserHardwareState("OpenDispenser", false);

        SILIGEN_LOG_INFO("OpenDispenser: 开始连续打开点胶阀");

        int result = CallMC_SetCmpLevel(true);
        if (result != 0) {
            std::string error_msg = CreateErrorMessage(result, "OpenDispenser");
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = error_msg;
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
        }

        dispenser_state_.status = DispenserValveStatus::Running;
        dispenser_state_.completedCount = 0;
        dispenser_state_.totalCount = 0;
        dispenser_state_.remainingCount = 0;
        dispenser_state_.progress = 0.0f;
        dispenser_state_.errorMessage.clear();
        dispenser_continuous_ = true;
        dispenser_run_mode_ = DispenserRunMode::Continuous;

        dispenser_start_time_ = std::chrono::steady_clock::now();
        dispenser_elapsed_before_pause_ = 0;

        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        dispenser_state_.startTime = static_cast<uint64>(timestamp);

        return Result<DispenserValveState>::Success(dispenser_state_);
    }
    catch (const std::exception& e) {
        dispenser_state_.status = DispenserValveStatus::Error;
        dispenser_state_.errorMessage = std::string("Exception: ") + e.what();
        return Result<DispenserValveState>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, dispenser_state_.errorMessage));
    }
}

Result<void> ValveAdapter::CloseDispenser() noexcept {
    try {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);

        if (dispenser_state_.status == DispenserValveStatus::Idle ||
            dispenser_state_.status == DispenserValveStatus::Stopped) {
            dispenser_continuous_ = false;
            dispenser_run_mode_ = DispenserRunMode::None;
            ResetDispenserHardwareState("CloseDispenser", false);
            return Result<void>::Success();
        }

        SILIGEN_LOG_INFO("CloseDispenser: 关闭点胶阀");

        int result = CallMC_SetCmpLevel(false);
        if (result != 0) {
            std::string error_msg = CreateErrorMessage(result, "CloseDispenser");
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = error_msg;
            return Result<void>::Failure(
                Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
        }

        dispenser_state_.status = DispenserValveStatus::Stopped;
        dispenser_state_.errorMessage.clear();
        dispenser_continuous_ = false;
        dispenser_run_mode_ = DispenserRunMode::None;

        ResetDispenserHardwareState("CloseDispenser", false);

        return Result<void>::Success();
    }
    catch (const std::exception& e) {
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, std::string("Exception: ") + e.what()));
    }
}

Result<DispenserValveState> ValveAdapter::StartPositionTriggeredDispenser(
    const PositionTriggeredDispenserParams& params) noexcept
{
    try {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);

        // 1. 验证参数
        if (!params.IsValid()) {
            std::string error_msg = params.GetValidationError();
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = error_msg;
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_PARAMETER, error_msg));
        }

        RefreshTimedDispenserStateIfCompleted();

        // 2. 检查是否已在运行
        if (dispenser_state_.status == DispenserValveStatus::Running) {
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, "点胶阀已在运行中"));
        }

        // 预清理CMP状态，避免上一次残留导致二次启动异常
        ResetDispenserHardwareState("StartPositionTriggeredDispenser", false);
        StopCmpBufferStreaming();

        // 转换轴号: 0-based (Domain) -> 1-based (SDK)
        short sdk_axis = ToSdkShort(ToSdkAxis(params.axis));
        SILIGEN_LOG_DEBUG("StartPositionTriggeredDispenser: axis=" + std::to_string(ToIndex(params.axis)) +
                          " (0-based) -> sdk_axis=" + std::to_string(sdk_axis) + " (1-based)");

        // 3. 关闭编码器反馈（切换到规划位置比较）
        Domain::Dispensing::DomainServices::DispenseCompensationService compensation_service;
        auto adjusted_params =
            compensation_service.ApplyPositionCompensation(params, compensation_profile_, false);

        int result = CallMC_EncOff(sdk_axis);
        if (result != 0) {
            std::string error_msg = CreateErrorMessage(result, "MC_EncOff");
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = error_msg;
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
        }

        const bool use_streaming = params.trigger_positions.size() > kCmpBufChunkSize;

        // 4. 设置 CMP 缓冲区输出通道（从配置读取）
        // MC_CmpBufSetChannel(nBuf1ChannelNum, nBuf2ChannelNum) - 设置两个缓冲区对应的 CMP 输出通道
        short cmp_output_channel = static_cast<short>(dispenser_config_.cmp_channel);
        short buf2_channel = use_streaming ? cmp_output_channel : 0;
        result = hardware_->MC_CmpBufSetChannel(cmp_output_channel, buf2_channel);
        if (result != 0) {
            std::string error_msg = CreateErrorMessage(result, "MC_CmpBufSetChannel");
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = error_msg;
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
        }

        if (compensation_profile_.retract_enabled) {
            int retract_result = CallMC_SetCmpLevel(false);
            if (retract_result != 0) {
                SILIGEN_LOG_WARNING("StartDispenser: 回吸关阀指令失败, code=" +
                                    std::to_string(retract_result));
            }
        }

        SILIGEN_LOG_INFO("CMP output channel set to: " + std::to_string(cmp_output_channel));

        // 5. 加载位置触发缓冲区
        // MC_CmpBufData 第一个参数 nCmpEncodeNum 是编码器/轴号（1-16），不是输出通道！
        if (!use_streaming) {
            result = CallMC_CmpBufData(
                sdk_axis,  // 编码器/轴号（1-based SDK轴号）
                adjusted_params.trigger_positions,
                adjusted_params.pulse_width_ms,
                adjusted_params.start_level
            );

            if (result != 0) {
                std::string error_msg = CreateErrorMessage(result, "StartPositionTriggeredDispenser");
                dispenser_state_.status = DispenserValveStatus::Error;
                dispenser_state_.errorMessage = error_msg;
                return Result<DispenserValveState>::Failure(
                    Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
            }
        } else {
            {
                std::lock_guard<std::mutex> stream_lock(cmp_stream_mutex_);
                cmp_stream_positions_ = adjusted_params.trigger_positions;
                cmp_stream_total_chunks_ =
                    (cmp_stream_positions_.size() + kCmpBufChunkSize - 1) / kCmpBufChunkSize;
                cmp_stream_next_chunk_ = 0;
                cmp_stream_encoder_axis_ = sdk_axis;
                cmp_stream_pulse_width_ms_ = adjusted_params.pulse_width_ms;
                cmp_stream_start_level_ = adjusted_params.start_level;
            }
            cmp_stream_stop_ = false;
            cmp_streaming_active_ = true;

            if (!LoadCmpBufferChunk(true)) {
                StopCmpBufferStreaming();
                std::string error_msg = "CMP缓冲区加载失败(缓冲区1)";
                dispenser_state_.status = DispenserValveStatus::Error;
                dispenser_state_.errorMessage = error_msg;
                return Result<DispenserValveState>::Failure(
                    Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
            }
            if (!LoadCmpBufferChunk(false)) {
                if (cmp_stream_total_chunks_ > 1) {
                    StopCmpBufferStreaming();
                    std::string error_msg = "CMP缓冲区加载失败(缓冲区2)";
                    dispenser_state_.status = DispenserValveStatus::Error;
                    dispenser_state_.errorMessage = error_msg;
                    return Result<DispenserValveState>::Failure(
                        Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
                }
            }

            unsigned short status = 0;
            unsigned short remain1 = 0;
            unsigned short remain2 = 0;
            unsigned short space1 = 0;
            unsigned short space2 = 0;
            int sts_result = hardware_->MC_CmpBufSts(&status, &remain1, &remain2, &space1, &space2);
            if (sts_result != 0) {
                StopCmpBufferStreaming();
                std::string error_msg = CreateErrorMessage(sts_result, "MC_CmpBufSts");
                dispenser_state_.status = DispenserValveStatus::Error;
                dispenser_state_.errorMessage = error_msg;
                return Result<DispenserValveState>::Failure(
                    Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
            }

            StartCmpBufferStreamingThread();
        }

        // 5. 更新状态
        dispenser_state_.status = DispenserValveStatus::Running;
        dispenser_state_.completedCount = 0;
        dispenser_state_.totalCount = static_cast<uint32>(adjusted_params.trigger_positions.size());
        dispenser_state_.remainingCount = dispenser_state_.totalCount;
        dispenser_state_.progress = 0.0f;
        dispenser_state_.errorMessage.clear();
        dispenser_continuous_ = false;
        dispenser_run_mode_ = DispenserRunMode::PositionTriggered;

        // 记录时间戳（毫秒）
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        dispenser_state_.startTime = static_cast<uint64>(timestamp);

        SILIGEN_LOG_INFO("StartPositionTriggeredDispenser: axis=" + std::to_string(ToIndex(adjusted_params.axis)) +
                         ", positions=" + std::to_string(adjusted_params.trigger_positions.size()) +
                         ", pulse_width=" + std::to_string(adjusted_params.pulse_width_ms) + "ms");

        return Result<DispenserValveState>::Success(dispenser_state_);
    }
    catch (const std::exception& e) {
        dispenser_state_.status = DispenserValveStatus::Error;
        dispenser_state_.errorMessage = std::string("Exception: ") + e.what();
        return Result<DispenserValveState>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, dispenser_state_.errorMessage));
    }
}

Result<void> ValveAdapter::StopDispenser() noexcept {
    try {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);
        StopCmpBufferStreaming();

        // 如果已经停止或空闲，直接返回成功
        if (dispenser_state_.status == DispenserValveStatus::Idle ||
            dispenser_state_.status == DispenserValveStatus::Stopped) {
            dispenser_continuous_ = false;
            dispenser_run_mode_ = DispenserRunMode::None;
            ResetDispenserHardwareState("StopDispenser", false);
            return Result<void>::Success();
        }

        if (dispenser_continuous_) {
            int reset_result = ResetDispenserHardwareState("StopDispenser", true);
            if (reset_result != 0) {
                std::string error_msg = CreateErrorMessage(reset_result, "StopDispenser");
                dispenser_state_.status = DispenserValveStatus::Error;
                dispenser_state_.errorMessage = error_msg;
                return Result<void>::Failure(
                    Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
            }

            dispenser_state_.status = DispenserValveStatus::Stopped;
            dispenser_state_.errorMessage.clear();
            dispenser_continuous_ = false;
            dispenser_run_mode_ = DispenserRunMode::None;
            return Result<void>::Success();
        }

        // 位置触发模式（MC_CmpBufData）的停止路径
        // 使用统一的 ResetDispenserHardwareState 确保完整关闭
        SILIGEN_LOG_DEBUG("StopDispenser: 位置触发模式,调用 ResetDispenserHardwareState");
        int reset_result = ResetDispenserHardwareState("StopDispenser-PositionTrigger", false);
        if (reset_result != 0) {
            SILIGEN_LOG_WARNING("StopDispenser: ResetDispenserHardwareState 返回非零值 " + std::to_string(reset_result) + ",继续执行");
        }

        // 更新状态
        UpdateDispenserProgress();  // 最后更新一次进度
        dispenser_state_.status = DispenserValveStatus::Stopped;
        dispenser_state_.errorMessage.clear();
        dispenser_continuous_ = false;
        dispenser_run_mode_ = DispenserRunMode::None;

        SILIGEN_LOG_INFO("StopDispenser: completedCount=" + std::to_string(dispenser_state_.completedCount) +
                         " [诊断:已调用 ResetDispenserHardwareState 断开CMP输出通道]");

        return Result<void>::Success();
    }
    catch (const std::exception& e) {
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, std::string("Exception: ") + e.what()));
    }
}

Result<void> ValveAdapter::PauseDispenser() noexcept {
    try {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);

        if (dispenser_continuous_) {
            return Result<void>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, "连续开阀模式不支持暂停"));
        }

        // 检查是否正在运行
        if (dispenser_state_.status != DispenserValveStatus::Running) {
            return Result<void>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, "点胶阀未在运行"));
        }

        // 调用 MultiCard API 停止 CMP（暂停 = 停止硬件触发）
        int result = CallMC_StopCmpOutputs(BuildCmpChannelMask());

        if (result != 0) {
            std::string error_msg = CreateErrorMessage(result, "PauseDispenser");
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = error_msg;
            return Result<void>::Failure(
                Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
        }

        // 保险关闭输出电平，避免暂停时电平残留
        result = CallMC_SetCmpLevel(false);
        if (result != 0) {
            std::string error_msg = CreateErrorMessage(result, "PauseDispenser");
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = error_msg;
            return Result<void>::Failure(
                Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
        }

        // 更新状态
        UpdateDispenserProgress();  // 计算暂停时的进度
        dispenser_state_.status = DispenserValveStatus::Paused;

        // 保存暂停时间和已运行时间
        dispenser_pause_time_ = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            dispenser_pause_time_ - dispenser_start_time_).count();
        dispenser_elapsed_before_pause_ = static_cast<uint32>(elapsed);

        SILIGEN_LOG_INFO("PauseDispenser: progress=" + std::to_string(dispenser_state_.progress) + "%");

        return Result<void>::Success();
    }
    catch (const std::exception& e) {
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, std::string("Exception: ") + e.what()));
    }
}

Result<void> ValveAdapter::ResumeDispenser() noexcept {
    try {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);

        if (dispenser_continuous_) {
            return Result<void>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, "连续开阀模式不支持恢复"));
        }

        // 检查是否已暂停
        if (dispenser_state_.status != DispenserValveStatus::Paused) {
            return Result<void>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, "点胶阀未处于暂停状态"));
        }

        // 计算剩余次数
        uint32 remainingCount = dispenser_state_.remainingCount;
        if (remainingCount == 0) {
            dispenser_state_.status = DispenserValveStatus::Idle;
            return Result<void>::Success();
        }

        // 重新启动 CMP，使用剩余次数
        int result = CallMC_CmpPluse(
            static_cast<int16>(dispenser_config_.cmp_channel),
            remainingCount,
            dispenser_params_.intervalMs,
            dispenser_params_.durationMs
        );

        if (result != 0) {
            std::string error_msg = CreateErrorMessage(result, "ResumeDispenser");
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = error_msg;
            return Result<void>::Failure(
                Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
        }

        // 更新状态
        dispenser_state_.status = DispenserValveStatus::Running;
        dispenser_state_.errorMessage.clear();
        dispenser_run_mode_ = DispenserRunMode::Timed;

        // 调整启动时间（减去已经运行的时间）
        dispenser_start_time_ = std::chrono::steady_clock::now() -
            std::chrono::milliseconds(dispenser_elapsed_before_pause_);

        SILIGEN_LOG_INFO("ResumeDispenser: remainingCount=" + std::to_string(remainingCount));

        return Result<void>::Success();
    }
    catch (const std::exception& e) {
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, std::string("Exception: ") + e.what()));
    }
}

void ValveAdapter::StopCmpBufferStreaming() noexcept {
    cmp_stream_stop_ = true;
    if (cmp_stream_thread_.joinable()) {
        cmp_stream_thread_.join();
    }
    cmp_streaming_active_ = false;
    std::lock_guard<std::mutex> lock(cmp_stream_mutex_);
    cmp_stream_positions_.clear();
    cmp_stream_next_chunk_ = 0;
    cmp_stream_total_chunks_ = 0;
}

void ValveAdapter::StartCmpBufferStreamingThread() noexcept {
    if (cmp_stream_thread_.joinable()) {
        cmp_stream_thread_.join();
    }
    cmp_stream_thread_ = std::thread([this]() { CmpBufferStreamingLoop(); });
}

bool ValveAdapter::LoadCmpBufferChunk(bool load_buf1) noexcept {
    std::vector<long> chunk;
    short axis = 0;
    uint32 pulse_width = 0;
    short start_level = 0;

    {
        std::lock_guard<std::mutex> lock(cmp_stream_mutex_);
        if (cmp_stream_next_chunk_ >= cmp_stream_total_chunks_) {
            return false;
        }
        size_t start = cmp_stream_next_chunk_ * kCmpBufChunkSize;
        size_t end = std::min(start + kCmpBufChunkSize, cmp_stream_positions_.size());
        chunk.assign(cmp_stream_positions_.begin() + start, cmp_stream_positions_.begin() + end);
        axis = cmp_stream_encoder_axis_;
        pulse_width = cmp_stream_pulse_width_ms_;
        start_level = cmp_stream_start_level_;
        cmp_stream_next_chunk_++;
    }

    int result = CallMC_CmpBufDataBuffers(axis,
                                          load_buf1 ? &chunk : nullptr,
                                          load_buf1 ? nullptr : &chunk,
                                          pulse_width,
                                          start_level);
    if (result != 0) {
        SILIGEN_LOG_WARNING("LoadCmpBufferChunk: MC_CmpBufData失败, result=" + std::to_string(result));
        return false;
    }
    return true;
}

void ValveAdapter::CmpBufferStreamingLoop() noexcept {
    while (!cmp_stream_stop_) {
        unsigned short status = 0;
        unsigned short remain1 = 0;
        unsigned short remain2 = 0;
        unsigned short space1 = 0;
        unsigned short space2 = 0;
        int sts_result = hardware_->MC_CmpBufSts(&status, &remain1, &remain2, &space1, &space2);
        if (sts_result != 0) {
            SILIGEN_LOG_WARNING("CmpBufferStreamingLoop: MC_CmpBufSts失败, result=" + std::to_string(sts_result));
            break;
        }

        bool loaded = false;
        if (remain1 == 0) {
            loaded = LoadCmpBufferChunk(true) || loaded;
        }
        if (remain2 == 0) {
            loaded = LoadCmpBufferChunk(false) || loaded;
        }

        size_t next_chunk = 0;
        size_t total_chunks = 0;
        {
            std::lock_guard<std::mutex> lock(cmp_stream_mutex_);
            next_chunk = cmp_stream_next_chunk_;
            total_chunks = cmp_stream_total_chunks_;
        }

        if (next_chunk >= total_chunks && remain1 == 0 && remain2 == 0) {
            break;
        }

        if (!loaded) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kCmpBufPollIntervalMs));
        }
    }

    cmp_streaming_active_ = false;
}

Result<DispenserValveState> ValveAdapter::GetDispenserStatus() noexcept {
    try {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);

        // 如果正在运行，更新进度
        if (dispenser_state_.status == DispenserValveStatus::Running) {
            UpdateDispenserProgress();
        }

        return Result<DispenserValveState>::Success(dispenser_state_);
    }
    catch (const std::exception& e) {
        return Result<DispenserValveState>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, std::string("Exception: ") + e.what()));
    }
}


} // namespace Adapters
} // namespace Infrastructure
} // namespace Siligen



