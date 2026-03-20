#include "HardwareTestAdapter.h"

#define MODULE_NAME "HardwareTestAdapter"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <thread>

// MultiCard API结构体定义已在MultiCardCPP.h中
#include "siligen/device/adapters/drivers/multicard/MultiCardCPP.h"

namespace Siligen::Infrastructure::Adapters::Hardware {

using namespace Shared::Types;

// ============ 点动测试 ============

Result<void> HardwareTestAdapter::startJog(LogicalAxisId axis_id, JogDirection direction, double speed) {
    const int axis = AxisIndex(axis_id);
    SILIGEN_LOG_DEBUG_FMT_HELPER(
            "HardwareTestAdapter::startJog: axis=%d, direction=%d, speed=%.2f",
            axis,
            static_cast<int>(direction),
            speed);

    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto validateResult = validateAxisNumber(axis);
    if (validateResult.IsError()) {
        SILIGEN_LOG_ERROR("validateAxisNumber failed");
        return validateResult;
    }

    if (!isConnected()) {
        SILIGEN_LOG_ERROR("Hardware not connected");
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Hardware not connected", "HardwareTestAdapter"));
    }

    if (kUseHomeAsHardLimit) {
        auto limit_state_result = readLimitSwitchStateRaw(axis);
        if (limit_state_result.IsError()) {
            return Result<void>::Failure(
                Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                      "无法读取限位开关状态，已阻止点动操作",
                      "HardwareTestAdapter"));
        }

        const auto& limit_state = limit_state_result.Value();
        if (direction == JogDirection::Positive && limit_state.positiveLimitTriggered) {
            return Result<void>::Failure(
                Error(ErrorCode::POSITION_OUT_OF_RANGE,
                      "正向限位已触发，禁止向正向点动",
                      "HardwareTestAdapter"));
        }
        if (direction == JogDirection::Negative && limit_state.negativeLimitTriggered) {
            return Result<void>::Failure(
                Error(ErrorCode::POSITION_OUT_OF_RANGE,
                      "负向限位已触发，禁止向负向点动",
                      "HardwareTestAdapter"));
        }
    }

    // MultiCard API使用1-based轴号（1-2），应用层使用0-based（0-1）
    short axisNum = static_cast<short>(axis + 1);

    // 设置点动模式
    SILIGEN_LOG_DEBUG_FMT_HELPER( "Calling MC_PrfJog(axis=%d, API_axisNum=%d)", axis, axisNum);
    short ret = multicard_->MC_PrfJog(axisNum);
    SILIGEN_LOG_DEBUG_FMT_HELPER( "MC_PrfJog returned: %d", ret);

    if (ret != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER( "MC_PrfJog failed with code %d", ret);
        return Result<void>::Failure(Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                                           "Failed to set jog mode: " + getMultiCardErrorMessage(ret),
                                           "HardwareTestAdapter"));
    }

    // 设置点动参数（加速度、减速度、平滑时间）
    TJogPrm jogPrm;
    jogPrm.dAcc = 0.1;   // 加速度 0.1
    jogPrm.dDec = 0.1;   // 减速度 0.1
    jogPrm.dSmooth = 0;  // 平滑时间 0

    SILIGEN_LOG_DEBUG_FMT_HELPER( "Calling MC_SetJogPrm(axis=%d, acc=%.2f, dec=%.2f)", axisNum, jogPrm.dAcc, jogPrm.dDec);
    ret = multicard_->MC_SetJogPrm(axisNum, &jogPrm);
    SILIGEN_LOG_DEBUG_FMT_HELPER( "MC_SetJogPrm returned: %d", ret);

    if (ret != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER( "MC_SetJogPrm failed with code %d", ret);
        return Result<void>::Failure(Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                                           "Failed to set jog parameters: " + getMultiCardErrorMessage(ret),
                                           "HardwareTestAdapter"));
    }

    // 设置速度(正向或负向) - 单位转换 mm/s → pulse/ms
    // ⚠️ MC_SetVel 期望的单位是 pulse/ms，不是 pulse/s！
    // 参考: docs/02-api/multicard/reference/MC_SetVel.md
    double speed_pulse_s = unit_converter_.VelocityMmSToPS(static_cast<float>(speed));
    double speed_pulse_ms = speed_pulse_s / Units::PULSE_PER_SEC_TO_MS;  // 转换为 pulse/ms
    double vel = (direction == JogDirection::Positive) ? speed_pulse_ms : -speed_pulse_ms;
    SILIGEN_LOG_DEBUG_FMT_HELPER(
            "Speed conversion: %.2f mm/s → %.2f pulse/s → %.4f pulse/ms (pulse_per_mm=%.2f)",
            speed,
            speed_pulse_s,
            speed_pulse_ms,
            unit_converter_.GetPulsePerMm());
    SILIGEN_LOG_DEBUG_FMT_HELPER( "Calling MC_SetVel(axis=%d, vel=%.4f pulse/ms)", axisNum, vel);
    ret = multicard_->MC_SetVel(axisNum, vel);
    SILIGEN_LOG_DEBUG_FMT_HELPER( "MC_SetVel returned: %d", ret);

    if (ret != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER( "MC_SetVel failed with code %d", ret);
        return Result<void>::Failure(Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                                           "Failed to set velocity: " + getMultiCardErrorMessage(ret),
                                           "HardwareTestAdapter"));
    }

    // 更新运动参数
    SILIGEN_LOG_DEBUG_FMT_HELPER( "Calling MC_Update(mask=%d)", 1 << axis);
    ret = multicard_->MC_Update(1 << axis);
    SILIGEN_LOG_DEBUG_FMT_HELPER( "MC_Update returned: %d", ret);

    if (ret != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER( "MC_Update failed with code %d", ret);
        return Result<void>::Failure(Error(ErrorCode::MOTION_START_FAILED,
                                           "Failed to update motion: " + getMultiCardErrorMessage(ret),
                                           "HardwareTestAdapter"));
    }

    SILIGEN_LOG_DEBUG("HardwareTestAdapter::startJog completed successfully");
    return Result<void>::Success();
}

Result<void> HardwareTestAdapter::stopJog(LogicalAxisId axis_id) {
    const int axis = AxisIndex(axis_id);
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto validateResult = validateAxisNumber(axis);
    if (validateResult.IsError()) {
        return validateResult;
    }

    // 停止轴运动
    long mask = 1 << axis;
    short ret = multicard_->MC_Stop(mask, 0);
    if (ret != 0) {
        return Result<void>::Failure(Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                                           "Failed to stop axis: " + getMultiCardErrorMessage(ret),
                                           "HardwareTestAdapter"));
    }

    return Result<void>::Success();
}

Result<void> HardwareTestAdapter::startJogStep(LogicalAxisId axis_id, JogDirection direction, double distance, double speed) {
    const int axis = AxisIndex(axis_id);
    SILIGEN_LOG_DEBUG_FMT_HELPER(
            "HardwareTestAdapter::startJogStep: axis=%d, direction=%d, distance=%.2f, speed=%.2f",
            axis,
            static_cast<int>(direction),
            distance,
            speed);

    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto validateResult = validateAxisNumber(axis);
    if (validateResult.IsError()) {
        SILIGEN_LOG_ERROR("validateAxisNumber failed");
        return validateResult;
    }

    if (!isConnected()) {
        SILIGEN_LOG_ERROR("Hardware not connected");
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Hardware not connected", "HardwareTestAdapter"));
    }

    if (kUseHomeAsHardLimit) {
        auto limit_state_result = readLimitSwitchStateRaw(axis);
        if (limit_state_result.IsError()) {
            return Result<void>::Failure(
                Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                      "无法读取限位开关状态，已阻止步进点动",
                      "HardwareTestAdapter"));
        }

        const auto& limit_state = limit_state_result.Value();
        if (direction == JogDirection::Positive && limit_state.positiveLimitTriggered) {
            return Result<void>::Failure(
                Error(ErrorCode::POSITION_OUT_OF_RANGE,
                      "正向限位已触发，禁止向正向步进",
                      "HardwareTestAdapter"));
        }
        if (direction == JogDirection::Negative && limit_state.negativeLimitTriggered) {
            return Result<void>::Failure(
                Error(ErrorCode::POSITION_OUT_OF_RANGE,
                      "负向限位已触发，禁止向负向步进",
                      "HardwareTestAdapter"));
        }
    }

    // 验证距离参数
    if (distance <= 0) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Distance must be positive", "HardwareTestAdapter"));
    }

    // MultiCard API使用1-based轴号（1-2），应用层使用0-based（0-1）
    short axisNum = static_cast<short>(axis + 1);

    // 1. 设置梯形轮廓模式（点位运动模式）
    SILIGEN_LOG_DEBUG_FMT_HELPER( "Calling MC_PrfTrap(axis=%d, API_axisNum=%d)", axis, axisNum);
    short ret = multicard_->MC_PrfTrap(axisNum);
    SILIGEN_LOG_DEBUG_FMT_HELPER( "MC_PrfTrap returned: %d", ret);

    if (ret != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER( "MC_PrfTrap failed with code %d", ret);
        return Result<void>::Failure(Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                                           "Failed to set trap mode: " + getMultiCardErrorMessage(ret),
                                           "HardwareTestAdapter"));
    }

    // 2. 设置梯形运动参数（加速度、减速度、平滑时间）
    TTrapPrm trapPrm;
    trapPrm.acc = 0.1;       // 加速度 0.1 pulse/ms²
    trapPrm.dec = 0.1;       // 减速度 0.1 pulse/ms²
    trapPrm.smoothTime = 0;  // 平滑时间 0ms
    trapPrm.velStart = 0;    // 起始速度 0

    SILIGEN_LOG_DEBUG_FMT_HELPER( "Calling MC_SetTrapPrm(axis=%d, acc=%.2f, dec=%.2f)", axisNum, trapPrm.acc, trapPrm.dec);
    ret = multicard_->MC_SetTrapPrm(axisNum, &trapPrm);
    SILIGEN_LOG_DEBUG_FMT_HELPER( "MC_SetTrapPrm returned: %d", ret);

    if (ret != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER( "MC_SetTrapPrm failed with code %d", ret);
        return Result<void>::Failure(Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                                           "Failed to set trap parameters: " + getMultiCardErrorMessage(ret),
                                           "HardwareTestAdapter"));
    }

    // 3. 获取当前位置并计算目标位置
    long currentPos = 0;
    ret = multicard_->MC_GetPos(axisNum, &currentPos);
    if (ret != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER( "MC_GetPos failed with code %d", ret);
        return Result<void>::Failure(Error(ErrorCode::POSITION_QUERY_FAILED,
                                           "Failed to get current position: " + getMultiCardErrorMessage(ret),
                                           "HardwareTestAdapter"));
    }

    // 计算目标位置（考虑方向）
    long distance_pulse = static_cast<long>(unit_converter_.MmToPulse(static_cast<float>(distance)));
    long targetPos =
        (direction == JogDirection::Positive) ? (currentPos + distance_pulse) : (currentPos - distance_pulse);

    SILIGEN_LOG_DEBUG_FMT_HELPER(
            "Current position: %ld pulses, Target position: %ld pulses (distance=%.2f mm = %ld pulses)",
            currentPos,
            targetPos,
            distance,
            distance_pulse);

    // 4. 设置目标位置
    ret = multicard_->MC_SetPos(axisNum, targetPos);
    if (ret != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER( "MC_SetPos failed with code %d", ret);
        return Result<void>::Failure(Error(ErrorCode::POSITION_QUERY_FAILED,
                                           "Failed to set target position: " + getMultiCardErrorMessage(ret),
                                           "HardwareTestAdapter"));
    }

    // 5. 设置速度 - 单位转换 mm/s → pulse/ms
    double speed_pulse_s = unit_converter_.VelocityMmSToPS(static_cast<float>(speed));
    double speed_pulse_ms = speed_pulse_s / Units::PULSE_PER_SEC_TO_MS;  // 转换为 pulse/ms
    SILIGEN_LOG_DEBUG_FMT_HELPER(
            "Speed conversion: %.2f mm/s → %.2f pulse/s → %.4f pulse/ms",
            speed,
            speed_pulse_s,
            speed_pulse_ms);
    SILIGEN_LOG_DEBUG_FMT_HELPER( "Calling MC_SetVel(axis=%d, vel=%.4f pulse/ms)", axisNum, speed_pulse_ms);
    ret = multicard_->MC_SetVel(axisNum, speed_pulse_ms);
    SILIGEN_LOG_DEBUG_FMT_HELPER( "MC_SetVel returned: %d", ret);

    if (ret != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER( "MC_SetVel failed with code %d", ret);
        return Result<void>::Failure(Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                                           "Failed to set velocity: " + getMultiCardErrorMessage(ret),
                                           "HardwareTestAdapter"));
    }

    // 6. 更新运动参数，启动运动
    SILIGEN_LOG_DEBUG_FMT_HELPER( "Calling MC_Update(mask=%d)", 1 << axis);
    ret = multicard_->MC_Update(1 << axis);
    SILIGEN_LOG_DEBUG_FMT_HELPER( "MC_Update returned: %d", ret);

    if (ret != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER( "MC_Update failed with code %d", ret);
        return Result<void>::Failure(Error(ErrorCode::MOTION_START_FAILED,
                                           "Failed to update motion: " + getMultiCardErrorMessage(ret),
                                           "HardwareTestAdapter"));
    }

    SILIGEN_LOG_DEBUG("HardwareTestAdapter::startJogStep completed successfully");
    return Result<void>::Success();
}

Result<double> HardwareTestAdapter::getAxisPosition(LogicalAxisId axis_id) const {
    const int axis = AxisIndex(axis_id);
    SILIGEN_LOG_DEBUG_FMT_HELPER( "HardwareTestAdapter::getAxisPosition: axis=%d", axis);

    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto validateResult = validateAxisNumber(axis);
    if (validateResult.IsError()) {
        SILIGEN_LOG_ERROR("validateAxisNumber failed");
        return Result<double>::Failure(validateResult.GetError());
    }

    // 检查轴使能状态
    long status = 0;
    unsigned long clock = 0;
    // MultiCard API使用1-based轴号
    short statusRet = multicard_->MC_GetSts(static_cast<short>(axis + 1), &status, 1, &clock);
    SILIGEN_LOG_DEBUG_FMT_HELPER( "MC_GetSts returned: %d, status=0x%lx", statusRet, status);

    bool isEnabled = (statusRet == 0) && (status & AXIS_STATUS_ENABLE) != 0;
    SILIGEN_LOG_DEBUG_FMT_HELPER( "Axis %d enable status: %s", axis, isEnabled ? "ENABLED" : "DISABLED");

    // 如果轴未使能，自动使能
    if (!isEnabled) {
        SILIGEN_LOG_INFO_FMT_HELPER( "Axis %d not enabled, enabling automatically...", axis);

        // MultiCard API使用1-based轴号
        short enableRet = multicard_->MC_AxisOn(static_cast<short>(axis + 1));
        SILIGEN_LOG_DEBUG_FMT_HELPER( "MC_AxisOn returned: %d", enableRet);

        if (enableRet != 0) {
            SILIGEN_LOG_ERROR_FMT_HELPER( "Failed to enable axis %d, error code: %d", axis, enableRet);
            return Result<double>::Failure(Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                                                 "Failed to enable axis: " + getMultiCardErrorMessage(enableRet),
                                                 "HardwareTestAdapter"));
        }

        // 等待使能生效 (100ms)
        SILIGEN_LOG_DEBUG("Waiting for axis enable to take effect...");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // MC_GetPrfPos 返回脉冲数，需要转换为毫米（接口契约要求返回毫米）
    double position_pulse = 0.0;
    SILIGEN_LOG_DEBUG_FMT_HELPER( "Calling MC_GetPrfPos(axis=%d)", axis);
    // MultiCard API使用1-based轴号
    short ret = multicard_->MC_GetPrfPos(static_cast<short>(axis + 1), &position_pulse);
    SILIGEN_LOG_DEBUG_FMT_HELPER( "MC_GetPrfPos returned: %d, position=%.3f pulses", ret, position_pulse);

    if (ret != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER( "MC_GetPrfPos failed with code %d", ret);
        return Result<double>::Failure(Error(ErrorCode::POSITION_QUERY_FAILED,
                                             "Failed to get axis position: " + getMultiCardErrorMessage(ret),
                                             "HardwareTestAdapter"));
    }

    // 单位转换: 脉冲 → 毫米（适配器层的核心职责是封装硬件差异）
    float32 position_mm = unit_converter_.PulseToMm(static_cast<int32>(position_pulse));

    SILIGEN_LOG_DEBUG_FMT_HELPER(
            "Position conversion: %.0f pulse → %.3f mm (pulse_per_mm=%.2f)",
            position_pulse,
            position_mm,
            unit_converter_.GetPulsePerMm());

    return Result<double>::Success(static_cast<double>(position_mm));
}

std::map<LogicalAxisId, double> HardwareTestAdapter::getAllAxisPositions() const {
    std::map<LogicalAxisId, double> positions;

    for (int axis = 0; axis < axis_count_; ++axis) {
        auto axis_id = FromIndex(static_cast<int16>(axis));
        if (!IsValid(axis_id)) {
            continue;
        }
        auto posResult = getAxisPosition(axis_id);
        if (posResult.IsSuccess()) {
            positions[axis_id] = posResult.Value();
        } else {
            positions[axis_id] = 0.0;
        }
    }

    return positions;
}

}  // namespace Siligen::Infrastructure::Adapters::Hardware



