// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "ValveAdapter"

#include "ValveAdapter.h"
#include "shared/interfaces/ILoggingService.h"
#include <cmath>
#include <chrono>
#include <thread>

namespace Siligen {
namespace Infrastructure {
namespace Adapters {

using namespace Domain::Dispensing::Ports;
using namespace Shared::Types;

// ============================================================
// 私有辅助方法
// ============================================================

int ValveAdapter::CallMC_CmpPulseOnce(int16 channel, uint32 durationMs) {
    const short default_start_level =
        static_cast<short>((dispenser_config_.pulse_type == 0) ? 0 : 1);
    return CallMC_CmpPulseOnce(channel, durationMs, default_start_level);
}

int ValveAdapter::CallMC_CmpPulseOnce(int16 channel, uint32 durationMs, short startLevel) {
    try {
        // SDK API 原型 (MultiCardCPP.h:893):
        // int MC_CmpPluse(short nChannelMask, short nPluseType1, short nPluseType2,
        //                 short nTime1, short nTime2, short nTimeFlag1, short nTimeFlag2);
        //
        // 参数说明:
        // - nChannelMask: 通道掩码（bit0=通道1，bit1=通道2）
        // - nPluseType1/2: 输出类型（0=低电平，1=高电平，2=脉冲）
        // - nTime1/2: 脉冲持续时间（仅脉冲类型时有效）
        // - nTimeFlag1/2: 时间单位（0=微秒，1=毫秒）

        const short selected_channel = (channel > 0)
                                           ? channel
                                           : static_cast<short>(dispenser_config_.cmp_channel);
        const short normalized_start_level =
            (startLevel == 0 || startLevel == 1)
                ? startLevel
                : static_cast<short>((dispenser_config_.pulse_type == 0) ? 0 : 1);
        short nChannelMask = 0;
        short presetType1 = 0;
        short presetType2 = 0;
        short nPluseType1 = 0;
        short nPluseType2 = 0;
        short nTime1 = 0;
        short nTime2 = 0;
        short nTimeFlag1 = 0;
        short nTimeFlag2 = 0;

        if (selected_channel == 1) {
            nChannelMask = 0x01;
            presetType1 = normalized_start_level;
            nPluseType1 = 2;  // 通道1脉冲输出
            nTime1 = static_cast<short>(durationMs);
            nTimeFlag1 = 1;   // 毫秒单位
        } else if (selected_channel == 2) {
            nChannelMask = 0x02;
            presetType2 = normalized_start_level;
            nPluseType2 = 2;  // 通道2脉冲输出
            nTime2 = static_cast<short>(durationMs);
            nTimeFlag2 = 1;   // 毫秒单位
        } else {
            SILIGEN_LOG_WARNING("CallMC_CmpPulseOnce: 不支持的CMP通道=" + std::to_string(selected_channel) +
                                " (仅支持1/2)");
            return 7;  // 参数错误
        }

        // 先显式恢复到 contract 指定的空闲电平，再由硬件输出单次脉冲。
        const int preset_result = hardware_->MC_CmpPluse(
            nChannelMask, presetType1, presetType2, 0, 0, 0, 0);
        if (preset_result != 0) {
            SILIGEN_LOG_ERROR("CallMC_CmpPulseOnce: 预设起始电平失败: " + std::to_string(preset_result));
            return preset_result;
        }

        SILIGEN_LOG_INFO(std::string("CallMC_CmpPulseOnce: 调用 MC_CmpPluse 单次脉冲: Channel=") +
                         std::to_string(selected_channel) +
                         ", PulseWidth=" + std::to_string(durationMs) + "ms" +
                         ", StartLevel=" + std::to_string(normalized_start_level));

        int result = hardware_->MC_CmpPluse(nChannelMask, nPluseType1, nPluseType2,
                                            nTime1, nTime2, nTimeFlag1, nTimeFlag2);
        if (result != 0) {
            SILIGEN_LOG_ERROR("CallMC_CmpPulseOnce: MC_CmpPluse 失败: " + std::to_string(result));
        }
        return result;
    }
    catch (...) {
        return -9999;  // 异常情况返回特殊错误码
    }
}

int ValveAdapter::CallMC_SetCmpLevel(bool high) {
    try {
        bool active_high = (dispenser_config_.pulse_type == 0);
        short level = high ? (active_high ? 1 : 0) : (active_high ? 0 : 1);
        short nChannelMask = 0;
        short nPluseType1 = 0;
        short nPluseType2 = 0;
        short nTime1 = 0;
        short nTime2 = 0;
        short nTimeFlag1 = 0;
        short nTimeFlag2 = 0;

        const short selected_channel = static_cast<short>(dispenser_config_.cmp_channel);
        if (selected_channel == 1) {
            nChannelMask = 0x01;
            nPluseType1 = level;
        } else if (selected_channel == 2) {
            nChannelMask = 0x02;
            nPluseType2 = level;
        } else {
            SILIGEN_LOG_WARNING("CallMC_SetCmpLevel: 不支持的CMP通道=" + std::to_string(selected_channel) +
                                " (仅支持1/2)");
            return 7;  // 参数错误
        }

        SILIGEN_LOG_INFO(std::string("CallMC_SetCmpLevel: channel=") +
                         std::to_string(selected_channel) +
                         ", level=" + (level == 1 ? "HIGH" : "LOW") +
                         ", active_high=" + std::to_string(active_high));

        return hardware_->MC_CmpPluse(nChannelMask, nPluseType1, nPluseType2,
                                      nTime1, nTime2, nTimeFlag1, nTimeFlag2);
    }
    catch (...) {
        return -9999;
    }
}

int ValveAdapter::CallMC_SetExtDoBit(int16 value) {
    try {
        // 使用 MC_SetExtDoBit API
        // MC_SetExtDoBit(short nCardIndex, short nBitIndex, unsigned short nValue)
        //
        // 参数：
        // - nCardIndex: 卡索引（0=主卡，1=扩展卡1，...）
        // - nBitIndex: DO位索引（0-based，从配置获取）
        // - nValue: 输出值（0或1）

        return hardware_->MC_SetExtDoBit(
            static_cast<short>(supply_config_.do_card_index),  // 从配置获取
            static_cast<short>(supply_config_.do_bit_index),   // 从配置获取
            static_cast<unsigned short>(value)
        );
    }
    catch (...) {
        return -9999;
    }
}

unsigned short ValveAdapter::BuildCmpChannelMask() const noexcept {
    unsigned short mask = 0x01;  // 通道1（bit0）
    int32 cmp_channel = dispenser_config_.cmp_channel;
    if (cmp_channel >= 1 && cmp_channel <= 16) {
        mask |= static_cast<unsigned short>(1U << static_cast<unsigned short>(cmp_channel - 1));
    }
    return mask;
}

int ValveAdapter::CallMC_StopCmpOutputs(unsigned short channelMask) {
    try {
        // 使用 MC_CmpBufStop API 停止 CMP 触发
        // MC_CmpBufStop(short nChannelMask)
        return hardware_->MC_CmpBufStop(channelMask);
    }
    catch (...) {
        return -9999;
    }
}

int ValveAdapter::ResetDispenserHardwareState(const char* reason, bool strict) noexcept {
    const unsigned short channel_mask = BuildCmpChannelMask();

    // 1. 停止 CMP 位置比较触发
    int stop_result = CallMC_StopCmpOutputs(channel_mask);
    if (stop_result != 0) {
        SILIGEN_LOG_WARNING(std::string(reason) + ": MC_CmpBufStop 返回 " + std::to_string(stop_result));
        if (strict) {
            return stop_result;
        }
    }

    // 2. 断开 CMP 缓冲区输出通道（解决 MC_CmpBufData 电平残留问题）
    // MC_CmpBufSetChannel(0, 0) 将两个缓冲区的输出通道都设为0（无输出）
    // 这确保了即使 MC_CmpBufData 产生的电平状态残留，也不会输出到物理端口
    int channel_result = hardware_->MC_CmpBufSetChannel(0, 0);
    if (channel_result != 0) {
        SILIGEN_LOG_WARNING(std::string(reason) + ": MC_CmpBufSetChannel(0,0) 返回 " + std::to_string(channel_result));
        // 不严格要求成功，继续执行后续步骤
    }

    // 3. 拉低 CMP 脉冲输出电平（备用保险）
    int level_result = CallMC_SetCmpLevel(false);
    if (level_result != 0) {
        SILIGEN_LOG_WARNING(std::string(reason) + ": MC_CmpPluse(关阀) 返回 " + std::to_string(level_result));
        if (strict) {
            return level_result;
        }
    }

    if (compensation_profile_.retract_enabled && compensation_profile_.close_comp_ms > 0.0f) {
        auto retract_ms = static_cast<int>(std::llround(compensation_profile_.close_comp_ms));
        if (retract_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(retract_ms));
        }
    }

    return 0;
}

bool ValveAdapter::ReadCoordinateSystemPositionPulse(short coordinate_system,
                                                     long& x_position_pulse,
                                                     long& y_position_pulse) noexcept {
    try {
        double positions[8] = {};
        const int result = hardware_->MC_GetCrdPos(coordinate_system, positions);
        if (result != 0) {
            SILIGEN_LOG_WARNING("ReadCoordinateSystemPositionPulse: MC_GetCrdPos 失败, result=" +
                                std::to_string(result));
            return false;
        }

        x_position_pulse = static_cast<long>(std::llround(positions[0]));
        y_position_pulse = static_cast<long>(std::llround(positions[1]));
        return true;
    } catch (...) {
        SILIGEN_LOG_WARNING("ReadCoordinateSystemPositionPulse: 读取坐标系位置时发生异常");
        return false;
    }
}

void ValveAdapter::RefreshTimedDispenserStateIfCompleted() noexcept {
    if (dispenser_run_mode_ != DispenserRunMode::Timed) {
        return;
    }
    if (dispenser_state_.status != DispenserValveStatus::Running &&
        dispenser_state_.status != DispenserValveStatus::Paused) {
        return;
    }
    UpdateDispenserProgress();
}

void ValveAdapter::UpdateDispenserProgress() {
    // 注意：此方法应在持有 dispenser_mutex_ 锁时调用

    if (dispenser_state_.totalCount == 0) {
        return;
    }

    if (dispenser_state_.completedCount > dispenser_state_.totalCount) {
        dispenser_state_.completedCount = dispenser_state_.totalCount;
    }

    dispenser_state_.remainingCount = dispenser_state_.totalCount - dispenser_state_.completedCount;
    dispenser_state_.progress =
        (static_cast<float>(dispenser_state_.completedCount) /
         static_cast<float>(dispenser_state_.totalCount)) * 100.0f;

    // 仅定时模式在计数完成时自动切换为空闲。路径触发模式由 StopDispenser 收口。
    if (dispenser_run_mode_ == DispenserRunMode::Timed &&
        dispenser_state_.completedCount >= dispenser_state_.totalCount) {
        dispenser_state_.status = DispenserValveStatus::Idle;
        dispenser_state_.progress = 100.0f;
        dispenser_state_.remainingCount = 0;
        dispenser_run_mode_ = DispenserRunMode::None;
    }
}

std::string ValveAdapter::CreateErrorMessage(int errorCode, const std::string& operation) const {
    // 根据 MultiCard SDK 的错误码创建详细错误信息
    // 错误码定义来自 MultiCardCPP.h
    std::string msg = "[ValveAdapter] MultiCard API Error in " + operation + ": ";

    switch (errorCode) {
        // 成功（不应该调用此方法，但为完整性添加）
        case 0:   // MC_COM_SUCCESS
            msg += "操作成功（不应有此错误）";
            break;

        // 执行相关错误
        case 1:   // MC_COM_ERR_EXEC_FAIL
            msg += "执行失败（检测命令执行条件是否满足）";
            break;

        case 2:   // MC_COM_ERR_LICENSE_WRONG
            msg += "版本不支持该API（如有需要，联系厂家）";
            break;

        case 7:   // MC_COM_ERR_DATA_WORRY
            msg += "参数错误（检测参数是否合理）";
            break;

        // 通信相关错误
        case -1:  // MC_COM_ERR_SEND
            msg += "通讯失败（检查接线是否牢靠，必要时更换板卡）";
            break;

        case -6:  // MC_COM_ERR_CARD_OPEN_FAIL
            msg += "打开控制器失败（检查串口名/IP与端口配置，避免重复调用 MC_Open）";
            break;

        case -7:  // MC_COM_ERR_TIME_OUT
            msg += "运动控制器无响应（检测控制器是否连接、是否打开，必要时更换板卡）";
            break;

        case -8:  // MC_COM_ERR_COM_OPEN_FAIL
            msg += "串口打开失败（检查串口权限、占用或驱动）";
            break;

        // 特殊错误
        case -9999:
            msg += "调用异常（软件内部异常，请检查日志）";
            break;

        // 未知错误
        default:
            msg += "未知错误 (错误码=" + std::to_string(errorCode) + "，请查阅 SDK 文档)";
            break;
    }

    // 添加错误码以便调试
    msg += " [ErrorCode: " + std::to_string(errorCode) + "]";

    return msg;
}


} // namespace Adapters
} // namespace Infrastructure
} // namespace Siligen



