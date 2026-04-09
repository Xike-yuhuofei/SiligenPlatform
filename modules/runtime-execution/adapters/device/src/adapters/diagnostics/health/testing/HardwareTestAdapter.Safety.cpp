#include "siligen/device/adapters/hardware/HardwareTestAdapter.h"

#define MODULE_NAME "HardwareTestAdapter"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "adapters/motion/controller/homing/HomingSupport.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <thread>

// MultiCard API结构体定义已在MultiCardCPP.h中
#include "siligen/device/adapters/drivers/multicard/MultiCardCPP.h"

namespace Siligen::Infrastructure::Adapters::Hardware {

using namespace Shared::Types;
namespace HomingSupport = Siligen::Infrastructure::Adapters::Internal::Homing;

// ============ 安全控制 ============

Result<void> HardwareTestAdapter::emergencyStop() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // 停止所有轴
    long mask = (1L << axis_count_) - 1;
    short ret = multicard_->MC_Stop(mask, 0);

    if (ret != 0) {
        return Result<void>::Failure(Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                                           "Failed to emergency stop: " + getMultiCardErrorMessage(ret),
                                           "HardwareTestAdapter"));
    }

    return Result<void>::Success();
}

Result<void> HardwareTestAdapter::resetEmergencyStop() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // 急停复位需要硬件按钮手动复位，软件只能读取状态
    // 检查急停是否已释放
    long diValue = 0;
    short ret = multicard_->MC_GetDiRaw(0, &diValue);
    if (ret == 0) {
        // X1引脚对应DI bit 0，高电平表示急停已释放
        bool emergencyReleased = (diValue & 0x01) != 0;
        if (!emergencyReleased) {
            return Result<void>::Failure(Error(
                ErrorCode::EMERGENCY_STOP_ACTIVATED, "急停按钮未释放，请手动复位急停按钮", "HardwareTestAdapter"));
        }
    }

    return Result<void>::Success();
}

bool HardwareTestAdapter::isEmergencyStopActive() const {
    // 读取X1引脚急停状态 (DI bit 0, 低电平有效)
    long diValue = 0;
    short ret = multicard_->MC_GetDiRaw(0, &diValue);

    if (ret == 0) {
        // X1引脚对应DI bit 0，低电平有效
        return (diValue & 0x01) == 0;
    }

    // 读取失败时保守处理，假设急停已触发
    return true;
}

Result<void> HardwareTestAdapter::validateMotionParameters(LogicalAxisId axis_id,
                                                           double targetPosition,
                                                           double speed) const {
    const int axis = AxisIndex(axis_id);
    auto validateResult = validateAxisNumber(axis);
    if (validateResult.IsError()) {
        return validateResult;
    }

    if (speed <= 0) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Speed must be positive", "HardwareTestAdapter"));
    }

    // TODO: 检查目标位置是否在软限位范围内

    return Result<void>::Success();
}

Result<bool> HardwareTestAdapter::IsHomeInputTriggeredStable(int axis,
                                                             int sample_count,
                                                             int sample_interval_ms,
                                                             bool allow_blocking) const {
    int samples = sample_count > 0 ? sample_count : 1;
    int interval_ms = sample_interval_ms > 0 ? sample_interval_ms : 0;

    for (int i = 0; i < samples; ++i) {
        auto state_result = readHomeInputStateRaw(axis);
        if (state_result.IsError()) {
            return Result<bool>::Failure(state_result.GetError());
        }
        if (!state_result.Value().triggered) {
            return Result<bool>::Success(false);
        }
        if (i + 1 < samples && interval_ms > 0) {
            if (!allow_blocking) {
                SILIGEN_LOG_WARNING("IsHomeInputTriggeredStable called in non-blocking mode, debounce skipped");
                return Result<bool>::Success(true);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }
    }

    return Result<bool>::Success(true);
}

Result<void> HardwareTestAdapter::EscapeFromHomeLimit(int axis, short axisNum, const HomingConfig& config) const {
    return HomingSupport::EscapeFromHomeLimit(axis,
                                              axisNum,
                                              config,
                                              multicard_.get(),
                                              hardware_config_,
                                              unit_converter_,
                                              "HardwareTestAdapter",
                                              Units::PULSE_PER_SEC_TO_MS,
                                              [this](int axis_index, int sample_count, int sample_interval_ms) {
                                                  return IsHomeInputTriggeredStable(
                                                      axis_index, sample_count, sample_interval_ms);
                                              },
                                              [this](int axis_index) { return readHomeInputStateRaw(axis_index); },
                                              [this](short error_code) { return getMultiCardErrorMessage(error_code); });
}

Result<LimitSwitchState> HardwareTestAdapter::readLimitSwitchStateRaw(int axis) const {
    auto home_state_result = readHomeInputStateRaw(axis);
    if (home_state_result.IsError()) {
        return Result<LimitSwitchState>::Failure(home_state_result.GetError());
    }

    const auto& home_state = home_state_result.Value();
    LimitSwitchState state;
    state.positiveLimitTriggered = false;
    state.negativeLimitTriggered = home_state.triggered;
    state.timestamp = home_state.timestamp;

    return Result<LimitSwitchState>::Success(state);
}

Result<HomeInputState> HardwareTestAdapter::readHomeInputStateRaw(int axis) const {
    auto validateResult = validateAxisNumber(axis);
    if (validateResult.IsError()) {
        return Result<HomeInputState>::Failure(validateResult.GetError());
    }

    if (!multicard_) {
        return Result<HomeInputState>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "MultiCard未初始化", "HardwareTestAdapter"));
    }

    const auto& config = homing_configs_[axis];
    const int input_source = config.home_input_source;
    if (input_source < 0 || input_source > 7) {
        return Result<HomeInputState>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "HOME输入源配置无效", "HardwareTestAdapter"));
    }

    int input_bit = config.home_input_bit;
    if (input_bit < 0) {
        input_bit = axis;
    }
    if (input_bit < 0 || input_bit > 15) {
        return Result<HomeInputState>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "HOME输入位配置无效", "HardwareTestAdapter"));
    }

    auto sample_once = [&]() -> Result<HomeInputState> {
        long home_raw = 0;
        short home_ret = multicard_->MC_GetDiRaw(static_cast<short>(input_source), &home_raw);
        if (home_ret != 0) {
            return Result<HomeInputState>::Failure(
                Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                      "读取Home信号失败: " + getMultiCardErrorMessage(home_ret),
                      "HardwareTestAdapter"));
        }

        bool raw_level = (home_raw & (1L << input_bit)) != 0;
        bool triggered = config.home_active_low ? !raw_level : raw_level;

        HomeInputState state;
        state.raw_level = raw_level;
        state.triggered = triggered;
        state.timestamp = static_cast<std::int64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        return Result<HomeInputState>::Success(state);
    };

    auto state_result = sample_once();
    if (state_result.IsError()) {
        return state_result;
    }

    const int32 debounce_ms = config.home_debounce_ms;
    if (debounce_ms <= 0) {
        return state_result;
    }

    auto start_time = std::chrono::steady_clock::now();
    auto stable_since = start_time;
    HomeInputState stable = state_result.Value();
    const int32 max_wait_ms = std::max<int32>(debounce_ms * 3, 30);

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        auto now = std::chrono::steady_clock::now();

        auto current_result = sample_once();
        if (current_result.IsError()) {
            return current_result;
        }
        const auto& current = current_result.Value();

        if (current.raw_level != stable.raw_level) {
            stable = current;
            stable_since = now;
        }

        auto stable_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - stable_since).count();
        if (stable_elapsed >= debounce_ms) {
            break;
        }

        auto total_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        if (total_elapsed >= max_wait_ms) {
            break;
        }
    }

    return Result<HomeInputState>::Success(stable);
}

}  // namespace Siligen::Infrastructure::Adapters::Hardware



