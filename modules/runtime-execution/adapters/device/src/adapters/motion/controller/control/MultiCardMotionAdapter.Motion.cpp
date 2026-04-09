// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "MultiCardMotion"

#include "MultiCardMotionAdapter.h"

#include <chrono>
#include <cmath>
#include <thread>
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Error.h"

namespace Siligen::Infrastructure::Adapters {

// 单位转换常量: pulse/s → pulse/ms (MultiCard SDK 要求 pulse/ms 单位)
namespace Units {
constexpr double PULSE_PER_SEC_TO_MS = 1000.0;
}

namespace {
}

using Siligen::Domain::Motion::Ports::MotionCommand;
using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::Domain::Motion::Ports::MotionStatus;

// 类型安全轴号转换
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::SdkAxisId;
using Siligen::Shared::Types::FromIndex;
using Siligen::Shared::Types::ToSdkAxis;
using Siligen::Shared::Types::ToSdkShort;

// === 运动控制实现 ===

Result<void> MultiCardMotionAdapter::StopAxis(LogicalAxisId axis_id, bool immediate) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS, FormatErrorMessage("StopAxis", axis, -1)));
    }

    short sdk_axis = ToSdkAxis(axis);
    if (diagnostics_config_.deep_motion_logging) {
        LogAxisSnapshot("StopAxis pre", axis, sdk_axis);
    }
    long pre_status = 0;
    long pre_pos = 0;
    short pre_status_ret = hardware_wrapper_->MC_GetSts(sdk_axis, &pre_status);
    short pre_pos_ret = hardware_wrapper_->MC_GetPos(sdk_axis, &pre_pos);
    SILIGEN_LOG_INFO("StopAxis pre: axis=" + std::to_string(axis) +
                     ", immediate=" + std::to_string(immediate) +
                     ", sts_ret=" + std::to_string(pre_status_ret) +
                     ", status=" + std::to_string(pre_status) +
                     ", pos_ret=" + std::to_string(pre_pos_ret) +
                     ", cmd_pos=" + std::to_string(pre_pos));

    // MC_Stop(轴掩码, 选项): 0=减速停止, 1=立即停止
    long axis_mask = 1L << axis;
    long stop_option = immediate ? 1L : 0L;
    short ret = hardware_wrapper_->MC_Stop(axis_mask, stop_option);
    if (diagnostics_config_.deep_hardware_logging) {
        SILIGEN_LOG_INFO("StopAxis HW: axis=" + std::to_string(axis) +
                         ", sdk_axis=" + std::to_string(sdk_axis) +
                         ", mask=" + std::to_string(axis_mask) +
                         ", option=" + std::to_string(stop_option) +
                         ", ret=" + std::to_string(ret));
    }

    long post_status = 0;
    long post_pos = 0;
    short post_status_ret = hardware_wrapper_->MC_GetSts(sdk_axis, &post_status);
    short post_pos_ret = hardware_wrapper_->MC_GetPos(sdk_axis, &post_pos);
    SILIGEN_LOG_INFO("StopAxis post: axis=" + std::to_string(axis) +
                     ", stop_ret=" + std::to_string(ret) +
                     ", sts_ret=" + std::to_string(post_status_ret) +
                     ", status=" + std::to_string(post_status) +
                     ", pos_ret=" + std::to_string(post_pos_ret) +
                     ", cmd_pos=" + std::to_string(post_pos));

    if (diagnostics_config_.deep_motion_logging) {
        LogAxisSnapshot("StopAxis immediate", axis, sdk_axis);
        if (diagnostics_config_.snapshot_after_stop_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(diagnostics_config_.snapshot_after_stop_ms));
            LogAxisSnapshot("StopAxis delayed", axis, sdk_axis);
        }
    }

    if (ret != 0) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR, FormatErrorMessage("MC_Stop", axis, ret)));
    }

    return Result<void>();
}

Result<void> MultiCardMotionAdapter::StopAllAxes(bool immediate) {
    // 停止所有轴
    for (short axis = 0; axis < kAxisCount; ++axis) {
        auto result = StopAxis(FromIndex(axis), immediate);
        if (result.IsError()) {
            return result;
        }
    }

    return Result<void>();
}

Result<void> MultiCardMotionAdapter::EmergencyStop() {
    // 双重保险：同时尝试停止坐标系运动和轴运动
    
    // 1. 尝试停止所有坐标系 (立即停止模式)
    // Mask 0xFFFF 覆盖所有可能的坐标系或轴(取决于具体板卡实现)
    short stop_crd_ret = hardware_wrapper_->MC_StopEx(0xFFFF, 1);
    if (stop_crd_ret != 0) {
        // 记录警告但不中断流程，确保后续的StopAllAxes被执行
        SILIGEN_LOG_WARNING("EmergencyStop: MC_StopEx failed with " + std::to_string(stop_crd_ret));
    }

    // 2. 停止所有轴 (立即停止模式)
    return StopAllAxes(true);
}

Result<void> MultiCardMotionAdapter::RecoverFromEmergencyStop() {
    auto reset_result = Reset();
    if (reset_result.IsError()) {
        return reset_result;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for (short axis = 0; axis < static_cast<short>(kAxisCount); ++axis) {
        const auto axis_id = FromIndex(axis);

        auto clear_result = ClearAxisStatus(axis_id);
        if (clear_result.IsError()) {
            return clear_result;
        }

        auto enable_result = EnableAxis(axis_id);
        if (enable_result.IsError()) {
            return enable_result;
        }
    }

    return Result<void>::Success();
}

// === JOG连续运动实现 ===

Result<void> MultiCardMotionAdapter::StartJog(LogicalAxisId axis_id, int16 direction, float32 velocity) {
    auto axis_result = ResolveAxis(axis_id, "StartJog");
    if (axis_result.IsError()) {
        return Result<void>(axis_result.GetError());
    }
    const short axis = axis_result.Value();
    // 函数入口标记（立即输出，用于诊断）
    SILIGEN_LOG_DEBUG("StartJog ENTRY: axis=" + std::to_string(axis) + ", direction=" + std::to_string(direction) +
                      ", velocity=" + std::to_string(velocity));

    // 转换为SDK轴号(1-based)
    short sdk_axis = ToSdkAxis(axis);

    if (diagnostics_config_.deep_motion_logging) {
        SILIGEN_LOG_INFO("StartJog params: axis=" + std::to_string(axis) +
                         ", sdk_axis=" + std::to_string(sdk_axis) +
                         ", dir=" + std::to_string(direction) +
                         ", vel_mm_s=" + std::to_string(velocity));
        LogAxisSnapshot("StartJog pre", axis, sdk_axis);
    }

    auto prepare_result = PrepareAxisForJog(axis, sdk_axis, axis_id, direction, "StartJog", true);
    if (prepare_result.IsError()) {
        return prepare_result;
    }

    short ret;

    // 2. 切换到JOG模式
    ret = hardware_wrapper_->MC_PrfJog(sdk_axis);
    SILIGEN_LOG_DEBUG("MC_PrfJog(" + std::to_string(sdk_axis) + ") returned " + std::to_string(ret));
    if (diagnostics_config_.deep_hardware_logging) {
        SILIGEN_LOG_INFO("StartJog HW: MC_PrfJog axis=" + std::to_string(axis) +
                         ", sdk_axis=" + std::to_string(sdk_axis) +
                         ", ret=" + std::to_string(ret));
    }
    if (ret != 0) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR, FormatErrorMessage("MC_PrfJog", axis, ret)));
    }

    // 2. 设置JOG参数（加速度、减速度、平滑时间）
    TJogPrm jog_prm = BuildJogParameters(axis);
    double acc_pulse_ms2 = jog_prm.dAcc;
    double dec_pulse_ms2 = jog_prm.dDec;
    SILIGEN_LOG_DEBUG("StartJog: acc_pulse_ms2=" + std::to_string(acc_pulse_ms2) +
                      ", dec_pulse_ms2=" + std::to_string(dec_pulse_ms2));
    if (diagnostics_config_.deep_motion_logging) {
        SILIGEN_LOG_INFO("StartJog accel: axis=" + std::to_string(axis) +
                         ", acc_pulse_ms2=" + std::to_string(acc_pulse_ms2) +
                         ", dec_pulse_ms2=" + std::to_string(dec_pulse_ms2));
    }
    ret = hardware_wrapper_->MC_SetJogPrm(sdk_axis, &jog_prm);
    SILIGEN_LOG_DEBUG("MC_SetJogPrm(" + std::to_string(sdk_axis) + ", acc=" + std::to_string(acc_pulse_ms2) +
                      ", dec=" + std::to_string(dec_pulse_ms2) + ") returned " + std::to_string(ret));
    if (diagnostics_config_.deep_hardware_logging) {
        SILIGEN_LOG_INFO("StartJog HW: MC_SetJogPrm axis=" + std::to_string(axis) +
                         ", sdk_axis=" + std::to_string(sdk_axis) +
                         ", acc_pulse_ms2=" + std::to_string(acc_pulse_ms2) +
                         ", dec_pulse_ms2=" + std::to_string(dec_pulse_ms2) +
                         ", ret=" + std::to_string(ret));
    }
    if (ret != 0) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR, FormatErrorMessage("MC_SetJogPrm", axis, ret)));
    }

    // 3. 设置速度（方向编码在速度符号中）
    double vel_pulse_ms = unit_converter_.VelocityMmSToPS(velocity) / Units::PULSE_PER_SEC_TO_MS;
    if (direction < 0) {
        vel_pulse_ms = -vel_pulse_ms;
    }

    // JOG连续运动：允许低速点动，仅防止零速度
    // 移除强制提升逻辑，确保连续运动模式正常工作
    const double MIN_VALID_VEL = 0.001;
    if (std::abs(vel_pulse_ms) < MIN_VALID_VEL) {
        vel_pulse_ms = (direction < 0 ? -MIN_VALID_VEL : MIN_VALID_VEL);
    }

    // 诊断输出：显示实际传递给硬件的速度值
    SILIGEN_LOG_DEBUG("StartJog: velocity_input=" + std::to_string(velocity) + " mm/s, vel_pulse_ms=" + std::to_string(vel_pulse_ms) + " pulse/ms");

    ret = hardware_wrapper_->MC_SetVel(sdk_axis, vel_pulse_ms);
    SILIGEN_LOG_DEBUG("MC_SetVel(" + std::to_string(sdk_axis) + ", vel=" + std::to_string(vel_pulse_ms) +
                      ") returned " + std::to_string(ret));
    if (diagnostics_config_.deep_hardware_logging) {
        SILIGEN_LOG_INFO("StartJog HW: MC_SetVel axis=" + std::to_string(axis) +
                         ", sdk_axis=" + std::to_string(sdk_axis) +
                         ", vel_pulse_ms=" + std::to_string(vel_pulse_ms) +
                         ", ret=" + std::to_string(ret));
    }
    if (ret != 0) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR, FormatErrorMessage("MC_SetVel", axis, ret)));
    }

    // 4. JOG模式不需要调用MC_SetPos
    // 根据厂家文档和示例代码，JOG模式只需要：
    // - MC_PrfJog: 设置为JOG模式
    // - MC_SetJogPrm/MC_SetJogPrmSingle: 设置JOG参数
    // - MC_SetVel: 设置速度（方向通过速度符号控制）
    // - MC_Update: 启动运动
    // 注意：MC_SetPos只用于点位模式，不应该在JOG模式中调用

    // 5. 启动运动（JOG连续模式）
    ret = hardware_wrapper_->MC_Update(1 << axis);
    SILIGEN_LOG_DEBUG("MC_Update(mask=" + std::to_string(1 << axis) + ") returned " + std::to_string(ret));
    if (diagnostics_config_.deep_hardware_logging) {
        SILIGEN_LOG_INFO("StartJog HW: MC_Update axis=" + std::to_string(axis) +
                         ", mask=" + std::to_string(1 << axis) +
                         ", ret=" + std::to_string(ret));
    }
    if (ret != 0) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR, FormatErrorMessage("MC_Update", axis, ret)));
    }

    // 记录JOG模式启动成功
    SILIGEN_LOG_INFO("JOG continuous mode started: axis=" + std::to_string(axis) + 
                     ", direction=" + std::to_string(direction) + 
                     ", velocity=" + std::to_string(velocity) + "mm/s");

    SILIGEN_LOG_DEBUG("StartJog SUCCESS - all hardware API calls completed");

    // === 高置信度验证：确认电机是否真正运动 ===
    // 由于无法读取编码器，使用以下三个必要条件的组合判断：

    // 1. 检查状态寄存器（MC_GetSts）
    long hw_status = 0;
    hardware_wrapper_->MC_GetSts(sdk_axis, &hw_status);
    bool enabled = (hw_status & AXIS_STATUS_ENABLE) != 0;
    bool in_position = (hw_status & AXIS_STATUS_ARRIVE) != 0;
    bool running = (hw_status & AXIS_STATUS_RUNNING) != 0;
    bool estop = (hw_status & AXIS_STATUS_ESTOP) != 0;
    bool io_emg_stop = (hw_status & AXIS_STATUS_IO_EMG_STOP) != 0;
    bool io_sms_stop = (hw_status & AXIS_STATUS_IO_SMS_STOP) != 0;
    bool soft_pos_limit = (hw_status & AXIS_STATUS_POS_SOFT_LIMIT) != 0;
    bool soft_neg_limit = (hw_status & AXIS_STATUS_NEG_SOFT_LIMIT) != 0;
    bool follow_err = (hw_status & AXIS_STATUS_FOLLOW_ERR) != 0;

    // 2. 检查驱动器报警状态（MC_GetAlarmOnOff）
    short alarm_status = 0;
    short alarm_ret = hardware_wrapper_->MC_GetAlarmOnOff(sdk_axis, &alarm_status);
    bool no_alarm = (alarm_ret == 0 && alarm_status == 0);

    // 3. 检查指令位置（MC_GetPos - 实际发送的脉冲计数）
    long cmd_pos = 0;
    short pos_ret = hardware_wrapper_->MC_GetPos(sdk_axis, &cmd_pos);

    SILIGEN_LOG_DEBUG("=== Motion Verification ===");
    SILIGEN_LOG_DEBUG("  MC_GetSts: hw_status=0x" + std::to_string(hw_status) + ", enabled=" + std::to_string(enabled) +
                      ", in_position=" + std::to_string(in_position));
    SILIGEN_LOG_DEBUG("  MC_GetAlarmOnOff: ret=" + std::to_string(alarm_ret) + ", alarm_status=" + std::to_string(alarm_status) +
                      ", no_alarm=" + std::to_string(no_alarm));
    SILIGEN_LOG_DEBUG("  MC_GetPos: ret=" + std::to_string(pos_ret) + ", cmd_pos=" + std::to_string(cmd_pos) + " pulses");
    SILIGEN_LOG_DEBUG("  ====================================");

    // 高置信度判断 = 指令输出 + 无报警 + 使能有效
    bool high_confidence_moving = enabled && !in_position && no_alarm;

    // INFO诊断：伺服报警/使能状态
    SILIGEN_LOG_INFO("StartJog ServoStatus: axis=" + std::to_string(axis) +
                     ", enabled=" + std::to_string(enabled) +
                     ", alarm_ret=" + std::to_string(alarm_ret) +
                     ", alarm_status=" + std::to_string(alarm_status) +
                     ", in_position=" + std::to_string(in_position) +
                     ", cmd_pos=" + std::to_string(cmd_pos) +
                     ", running=" + std::to_string(running) +
                     ", estop=" + std::to_string(estop) +
                     ", io_emg_stop=" + std::to_string(io_emg_stop) +
                     ", io_sms_stop=" + std::to_string(io_sms_stop) +
                     ", soft_pos_limit=" + std::to_string(soft_pos_limit) +
                     ", soft_neg_limit=" + std::to_string(soft_neg_limit) +
                     ", follow_err=" + std::to_string(follow_err) +
                     ", raw_status=" + std::to_string(hw_status));

    SILIGEN_LOG_DEBUG("HighConfidenceResult: " + std::string(high_confidence_moving ? "MOVING (高置信度)" : "NOT_MOVING"));

    if (diagnostics_config_.deep_motion_logging) {
        LogAxisSnapshot("StartJog immediate", axis, sdk_axis);
        if (diagnostics_config_.snapshot_delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(diagnostics_config_.snapshot_delay_ms));
            LogAxisSnapshot("StartJog delayed", axis, sdk_axis);
        }
    }

    return Result<void>();
}

Result<void> MultiCardMotionAdapter::StartJogStep(LogicalAxisId axis_id,
                                                  int16 direction,
                                                  float32 distance,
                                                  float32 velocity) {
    auto axis_result = ResolveAxis(axis_id, "StartJogStep");
    if (axis_result.IsError()) {
        return Result<void>(axis_result.GetError());
    }
    const short axis = axis_result.Value();

    SILIGEN_LOG_DEBUG("StartJogStep: 轴" + std::to_string(axis) + ", 方向=" + std::string(direction > 0 ? "正向" : "负向") +
                      ", 速度=" + std::to_string(velocity) + "mm/s, 步长=" + std::to_string(distance) + "mm");

    // 转换为SDK轴号(1-based)
    short sdk_axis = ToSdkAxis(axis);
    short ret = 0;

    auto prepare_result = PrepareAxisForJog(axis, sdk_axis, axis_id, direction, "StartJogStep", false);
    if (prepare_result.IsError()) {
        return prepare_result;
    }

    // 转换步长到脉冲
    long step_pulses = static_cast<long>(unit_converter_.MmToPulse(distance));
    if (direction < 0) {
        step_pulses = -step_pulses;
    }

    // 获取当前位置
    double current_pos = 0;
    ret = hardware_wrapper_->MC_GetPrfPos(sdk_axis, &current_pos);
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("MC_GetPrfPos", axis, ret)));
    }

    // 计算目标位置
    long target_pos = static_cast<long>(current_pos) + step_pulses;

    // 使用点位模式进行步进运动
    ret = hardware_wrapper_->MC_PrfTrap(sdk_axis);
    if (ret != 0) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR, FormatErrorMessage("MC_PrfTrap", axis, ret)));
    }

    // 设置目标位置和速度
    double vel_pulse_ms = unit_converter_.VelocityMmSToPS(velocity) / Units::PULSE_PER_SEC_TO_MS;
    ret = hardware_wrapper_->MC_SetPos(sdk_axis, target_pos);
    if (ret != 0) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR, FormatErrorMessage("MC_SetPos", axis, ret)));
    }

    ret = hardware_wrapper_->MC_SetVel(sdk_axis, vel_pulse_ms);
    if (ret != 0) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR, FormatErrorMessage("MC_SetVel", axis, ret)));
    }

    // 启动运动
    ret = hardware_wrapper_->MC_Update(1 << axis);
    if (ret != 0) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR, FormatErrorMessage("MC_Update", axis, ret)));
    }

    return Result<void>();
}

Result<void> MultiCardMotionAdapter::StopJog(LogicalAxisId axis_id) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                 FormatErrorMessage("StopJog", axis, -1)));
    }

    // 使用减速停止
    return StopAxis(axis_id, false);
}

// === 等待操作 ===

Result<void> MultiCardMotionAdapter::WaitForMotionComplete(LogicalAxisId axis_id, int32 timeout_ms) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                 FormatErrorMessage("WaitForMotionComplete", axis, -1)));
    }

    auto start_time = std::chrono::steady_clock::now();

    while (true) {
        auto moving_result = IsAxisMoving(axis_id);
        if (moving_result.IsError()) {
            return Result<void>(moving_result.GetError());
        }

        if (!moving_result.Value()) {
            return Result<void>();  // 运动完成
        }

        // 检查超时
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();

        if (elapsed_ms >= timeout_ms) {
            return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::TIMEOUT,
                                                     FormatErrorMessage("WaitForMotionComplete: 超时", axis, 0)));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}


}  // namespace Siligen::Infrastructure::Adapters
