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

int ValveAdapter::CallMC_CmpPluse(int16 channel, uint32 count,
                                   uint32 intervalMs, uint32 durationMs) {
    try {
        // =====================================================
        // 使用 MC_CmpPluse + 软件循环实现点胶阀重复脉冲控制
        // =====================================================
        //
        // 由于 MC_CmpRpt 在当前 MultiCard.lib 中不可用（链接错误），
        // 改用官方示例中验证可行的 MC_CmpPluse 函数。
        //
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
        short nChannelMask = 0;
        short nPluseType1 = 0;
        short nPluseType2 = 0;
        short nTime1 = 0;
        short nTime2 = 0;
        short nTimeFlag1 = 0;
        short nTimeFlag2 = 0;

        if (selected_channel == 1) {
            nChannelMask = 0x01;
            nPluseType1 = 2;  // 通道1脉冲输出
            nTime1 = static_cast<short>(durationMs);
            nTimeFlag1 = 1;   // 毫秒单位
        } else if (selected_channel == 2) {
            nChannelMask = 0x02;
            nPluseType2 = 2;  // 通道2脉冲输出
            nTime2 = static_cast<short>(durationMs);
            nTimeFlag2 = 1;   // 毫秒单位
        } else {
            SILIGEN_LOG_WARNING("CallMC_CmpPluse: 不支持的CMP通道=" + std::to_string(selected_channel) +
                                " (仅支持1/2)");
            return 7;  // 参数错误
        }

        SILIGEN_LOG_INFO(std::string("CallMC_CmpPluse: 调用 MC_CmpPluse + 软件循环: Channel=") +
                         std::to_string(selected_channel) +
                         ", PulseWidth=" + std::to_string(durationMs) + "ms" +
                         ", Interval=" + std::to_string(intervalMs) + "ms" +
                         ", Count=" + std::to_string(count));

        int lastResult = 0;
        for (uint32 i = 0; i < count; i++) {
            // 输出单个脉冲
            int result = hardware_->MC_CmpPluse(nChannelMask, nPluseType1, nPluseType2,
                                                 nTime1, nTime2, nTimeFlag1, nTimeFlag2);
            if (result != 0) {
                SILIGEN_LOG_ERROR("CallMC_CmpPluse: MC_CmpPluse 失败: " + std::to_string(result) +
                                 " (脉冲 " + std::to_string(i + 1) + "/" + std::to_string(count) + ")");
                return result;
            }

            lastResult = result;

            // 等待间隔（最后一个脉冲后不需要等待）
            // MC_CmpPluse 是非阻塞调用，硬件会在 durationMs 内输出脉冲
            // 所以需要等待完整的 intervalMs 来保证脉冲间隔
            if (i < count - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
            }
        }

        SILIGEN_LOG_INFO("CallMC_CmpPluse: MC_CmpPluse 完成: " + std::to_string(count) + " 个脉冲已输出");

        return lastResult;
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

int ValveAdapter::CallMC_CmpBufData(short encoder_axis,
                                     const std::vector<long>& positions,
                                     uint32 pulse_width_ms, short start_level) {
    try {
        // =====================================================
        // 使用 MC_CmpBufData API 加载位置触发缓冲区
        // =====================================================
        //
        // SDK API 原型:
        // int MC_CmpBufData(short nCmpEncodeNum, short nPluseType, short nStartLevel, short nTime,
        //                   long* pBuf1, short nBufLen1, long* pBuf2, short nBufLen2,
        //                   short nAbsPosFlag, short nTimerFlag);
        //
        // 参数说明:
        // - nCmpEncodeNum: 编码器/轴号（1-16），用于位置比较源
        // - nPluseType: 脉冲类型（0=单次，1=持续）
        // - nStartLevel: 起始电平（0=低电平，1=高电平）
        // - nTime: 脉冲宽度（单位由 nTimerFlag 决定）
        // - pBuf1: 位置缓冲区1（轴位置数组）
        // - nBufLen1: 缓冲区1长度
        // - pBuf2: 位置缓冲区2（可选，用于多轴触发）
        // - nBufLen2: 缓冲区2长度
        // - nAbsPosFlag: 位置类型（0=相对，1=绝对）
        // - nTimerFlag: 时间单位标志（0=微秒，1=毫秒）
        //
        // 注意: CMP 输出通道需要通过 MC_CmpBufSetChannel 单独设置！

        // 根据 MultiCard SDK 文档，nPluseType 必须为 2 表示"到比较位置后输出一个脉冲"
        // 配置中的 pulse_type (0=正脉冲,1=负脉冲) 用于其他场景，不适用于 MC_CmpBufData
        short nPluseType = 2;  // 固定为脉冲输出模式

        // 使用传入的脉冲宽度参数，并根据值选择合适的时间单位
        short nTimerFlag;
        short nTime;

        // 检查脉冲宽度是否在 short 范围内（-32768 到 32767）
        if (pulse_width_ms > 32767) {
            // 如果超过 short 范围，使用微秒单位并转换
            nTimerFlag = 0;  // 微秒单位
            nTime = static_cast<short>(pulse_width_ms * 1000);
            // 注意：转换后可能仍然超出范围，需要截断到安全值
            if (nTime < 0) {
                nTime = 32767;  // 最大安全值
            }
        } else if (pulse_width_ms >= 1) {
            // 使用毫秒单位
            nTimerFlag = 1;  // 毫秒单位
            nTime = static_cast<short>(pulse_width_ms);
        } else {
            // 脉冲宽度太小，使用微秒单位
            nTimerFlag = 0;  // 微秒单位
            nTime = static_cast<short>(pulse_width_ms * 1000);
            if (nTime <= 0) {
                nTime = 10;  // 最小 10 微秒
            }
        }

        // 使用配置的绝对位置标志
        short nAbsPosFlag = static_cast<short>(dispenser_config_.abs_position_flag);

        // 创建可修改的位置数组副本（API 需要非 const 指针）
        std::vector<long> positions_copy = positions;
        long* pBuf1 = positions_copy.data();
        short nBufLen1 = static_cast<short>(positions_copy.size());

        // 详细诊断日志
        SILIGEN_LOG_DEBUG("CallMC_CmpBufData: encoder_axis=" + std::to_string(encoder_axis) +
                          " (有效范围: 1-16)");
        SILIGEN_LOG_DEBUG("CallMC_CmpBufData: nPluseType=" + std::to_string(nPluseType) +
                          " (2=脉冲输出, 0/1=电平输出)");
        SILIGEN_LOG_DEBUG("CallMC_CmpBufData: start_level=" + std::to_string(start_level) +
                          " (0=低, 1=高)");
        SILIGEN_LOG_DEBUG("CallMC_CmpBufData: nTime=" + std::to_string(nTime) +
                          (nTimerFlag == 1 ? " ms" : " μs"));
        SILIGEN_LOG_DEBUG("CallMC_CmpBufData: nBufLen1=" + std::to_string(nBufLen1));
        SILIGEN_LOG_DEBUG("CallMC_CmpBufData: nAbsPosFlag=" + std::to_string(nAbsPosFlag) +
                          " (0=相对, 1=绝对)");

        // 打印位置范围和前几个值
        if (!positions_copy.empty()) {
            long minPos = positions_copy[0], maxPos = positions_copy[0];
            for (const auto& p : positions_copy) {
                if (p < minPos) minPos = p;
                if (p > maxPos) maxPos = p;
            }
            SILIGEN_LOG_DEBUG("CallMC_CmpBufData: 位置范围: [" + std::to_string(minPos) +
                              ", " + std::to_string(maxPos) + "] 脉冲");
        }

        SILIGEN_LOG_INFO("CallMC_CmpBufData: 调用 MC_CmpBufData, axis=" + std::to_string(encoder_axis) +
                         ", len=" + std::to_string(nBufLen1) +
                         ", pulse_width_ms=" + std::to_string(pulse_width_ms) +
                         ", start_level=" + std::to_string(start_level) +
                         ", abs_flag=" + std::to_string(nAbsPosFlag) +
                         ", timer_flag=" + std::to_string(nTimerFlag));

        int result = hardware_->MC_CmpBufData(
            encoder_axis,       // 编码器/轴号（位置比较源）
            nPluseType,         // 脉冲类型
            start_level,        // 起始电平
            nTime,              // 脉冲宽度
            pBuf1,              // 位置数组
            nBufLen1,           // 数组长度
            nullptr,            // 无第二个缓冲区
            0,                  // 第二个缓冲区长度为 0
            nAbsPosFlag,        // 绝对位置
            nTimerFlag          // 定时器标志
        );

        SILIGEN_LOG_DEBUG("CallMC_CmpBufData: MC_CmpBufData 返回: " + std::to_string(result));
        return result;
    }
    catch (...) {
        SILIGEN_LOG_ERROR("CallMC_CmpBufData: 异常捕获!");
        return -9999;  // 异常情况返回特殊错误码
    }
}

int ValveAdapter::CallMC_CmpBufDataBuffers(short encoder_axis,
                                           const std::vector<long>* buffer1,
                                           const std::vector<long>* buffer2,
                                           uint32 pulse_width_ms,
                                           short start_level) {
    try {
        short nPluseType = 2;  // 脉冲输出

        short nTimerFlag;
        short nTime;
        if (pulse_width_ms > 32767) {
            nTimerFlag = 0;
            nTime = static_cast<short>(pulse_width_ms * 1000);
            if (nTime < 0) {
                nTime = 32767;
            }
        } else if (pulse_width_ms >= 1) {
            nTimerFlag = 1;
            nTime = static_cast<short>(pulse_width_ms);
        } else {
            nTimerFlag = 0;
            nTime = static_cast<short>(pulse_width_ms * 1000);
            if (nTime <= 0) {
                nTime = 10;
            }
        }

        short nAbsPosFlag = static_cast<short>(dispenser_config_.abs_position_flag);

        std::vector<long> buf1_copy;
        std::vector<long> buf2_copy;
        long* pBuf1 = nullptr;
        long* pBuf2 = nullptr;
        short nBufLen1 = 0;
        short nBufLen2 = 0;

        if (buffer1 && !buffer1->empty()) {
            buf1_copy = *buffer1;
            pBuf1 = buf1_copy.data();
            nBufLen1 = static_cast<short>(buf1_copy.size());
        }
        if (buffer2 && !buffer2->empty()) {
            buf2_copy = *buffer2;
            pBuf2 = buf2_copy.data();
            nBufLen2 = static_cast<short>(buf2_copy.size());
        }

        SILIGEN_LOG_DEBUG("CallMC_CmpBufDataBuffers: axis=" + std::to_string(encoder_axis) +
                          ", buf1_len=" + std::to_string(nBufLen1) +
                          ", buf2_len=" + std::to_string(nBufLen2));

        SILIGEN_LOG_INFO("CallMC_CmpBufDataBuffers: 调用 MC_CmpBufData, axis=" +
                         std::to_string(encoder_axis) +
                         ", buf1_len=" + std::to_string(nBufLen1) +
                         ", buf2_len=" + std::to_string(nBufLen2) +
                         ", pulse_width_ms=" + std::to_string(pulse_width_ms) +
                         ", start_level=" + std::to_string(start_level) +
                         ", abs_flag=" + std::to_string(nAbsPosFlag) +
                         ", timer_flag=" + std::to_string(nTimerFlag));

        int result = hardware_->MC_CmpBufData(
            encoder_axis,
            nPluseType,
            start_level,
            nTime,
            pBuf1,
            nBufLen1,
            pBuf2,
            nBufLen2,
            nAbsPosFlag,
            nTimerFlag);

        SILIGEN_LOG_DEBUG("CallMC_CmpBufDataBuffers: MC_CmpBufData 返回: " + std::to_string(result));
        return result;
    } catch (...) {
        SILIGEN_LOG_ERROR("CallMC_CmpBufDataBuffers: 异常捕获!");
        return -9999;
    }
}

int ValveAdapter::CallMC_EncOff(short axis) {
    try {
        // =====================================================
        // 使用 MC_EncOff API 关闭编码器反馈
        // =====================================================
        //
        // SDK API 原型:
        // int MC_EncOff(short axis);
        //
        // 功能说明:
        // 关闭编码器反馈后，CMP 比较源从编码器实际位置切换为规划位置（Profile Position）。
        // 这样可以保证在非匀速运动（加减速）时触发位置的均匀性。
        //
        // 参数:
        // - axis: 轴号（1-16）

        SILIGEN_LOG_DEBUG("CallMC_EncOff: Axis=" + std::to_string(axis) +
                          " - 关闭编码器，切换到规划位置比较");

        return hardware_->MC_EncOff(axis);
    }
    catch (...) {
        return -9999;  // 异常情况返回特殊错误码
    }
}

void ValveAdapter::RefreshTimedDispenserStateIfCompleted() noexcept {
    if (dispenser_run_mode_ != DispenserRunMode::Timed) {
        return;
    }
    if (dispenser_state_.status != DispenserValveStatus::Running) {
        return;
    }
    UpdateDispenserProgress();
}

void ValveAdapter::UpdateDispenserProgress() {
    // 注意：此方法应在持有 dispenser_mutex_ 锁时调用

    if (dispenser_run_mode_ != DispenserRunMode::Timed) {
        return;
    }

    if (dispenser_state_.totalCount == 0) {
        return;
    }

    // 计算已运行时间（毫秒）
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - dispenser_start_time_).count();

    // 计算已完成次数（基于脉冲持续时间 + 间隔）
    uint32 completedCount = 0;
    if (elapsed >= dispenser_params_.durationMs) {
        completedCount = 1 + static_cast<uint32>(
            (elapsed - dispenser_params_.durationMs) / dispenser_params_.intervalMs);
    }

    // 限制在总次数范围内
    if (completedCount > dispenser_state_.totalCount) {
        completedCount = dispenser_state_.totalCount;
    }

    // 更新状态
    dispenser_state_.completedCount = completedCount;
    dispenser_state_.remainingCount = dispenser_state_.totalCount - completedCount;
    dispenser_state_.progress =
        (static_cast<float>(completedCount) / static_cast<float>(dispenser_state_.totalCount)) * 100.0f;

    // 如果完成，自动切换到空闲状态
    if (completedCount >= dispenser_state_.totalCount) {
        dispenser_state_.status = DispenserValveStatus::Idle;
        dispenser_state_.progress = 100.0f;
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



