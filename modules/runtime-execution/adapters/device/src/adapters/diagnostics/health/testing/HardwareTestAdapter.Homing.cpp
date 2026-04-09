#include "siligen/device/adapters/hardware/HardwareTestAdapter.h"

#define MODULE_NAME "HardwareTestAdapter"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "adapters/motion/controller/homing/HomingSupport.h"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <thread>

// MultiCard API结构体定义已在MultiCardCPP.h中
#include "siligen/device/adapters/drivers/multicard/MultiCardCPP.h"

namespace Siligen::Infrastructure::Adapters::Hardware {

using namespace Shared::Types;
namespace HomingSupport = Siligen::Infrastructure::Adapters::Internal::Homing;
using Siligen::Domain::Motion::ValueObjects::HomingStage;

namespace {
}  // namespace

// ============ 回零测试 ============

Result<void> HardwareTestAdapter::startHoming(const std::vector<LogicalAxisId>& axes) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto request_check =
        HomingSupport::ValidateStartHomingRequest(axes, [this]() { return isConnected(); }, "HardwareTestAdapter");
    if (request_check.IsError()) {
        return request_check;
    }

    const unsigned short desired_home_sense =
        HomingSupport::ComputeDesiredHomeSense(axis_count_, homing_configs_);

    unsigned short previous_home_sense = 0;
    const bool has_previous_home_sense = (multicard_->MC_GetHomeSns(&previous_home_sense) == 0);

    int home_sense_ret = multicard_->MC_HomeSns(desired_home_sense);
    if (home_sense_ret != 0) {
        std::string message = "设置HOME输入极性失败: " +
                              getMultiCardErrorMessage(static_cast<short>(home_sense_ret));
        if (desired_home_sense != 0) {
            return Result<void>::Failure(
                Error(ErrorCode::HARDWARE_OPERATION_FAILED, message, "HardwareTestAdapter"));
        }
        SILIGEN_LOG_WARNING(message);
    }

    bool any_axis_started = false;
    HomingSupport::HomeSenseRollbackGuard home_sense_guard{
        multicard_.get(),
        has_previous_home_sense && previous_home_sense != desired_home_sense,
        previous_home_sense,
        &any_axis_started
    };

    // 为每个轴配置并启动回零
    for (auto axis_id : axes) {
        const int axis = AxisIndex(axis_id);
        auto validateResult = validateAxisNumber(axis);
        if (validateResult.IsError()) {
            return validateResult;
        }

        homing_stage_[axis] = HomingStage::Idle;
        auto fail = [&](const Error& err) -> Result<void> {
            homing_failed_latch_[axis] = true;
            homing_success_latch_[axis] = false;
            homing_active_latch_[axis] = false;
            homing_timer_active_[axis] = false;
            homing_stage_[axis] = HomingStage::Failed;
            return Result<void>::Failure(err);
        };

        HomingSupport::ResetHomingState(homing_success_latch_,
                                        homing_failed_latch_,
                                        homing_active_latch_,
                                        homing_stage_,
                                        homing_timer_active_,
                                        axis);

        const auto& config = homing_configs_[axis];
        auto prepared_result = HomingSupport::PrepareHomeParameters(axis,
                                                                    config,
                                                                    hardware_config_,
                                                                    unit_converter_,
                                                                    "HardwareTestAdapter",
                                                                    Units::PULSE_PER_SEC_TO_MS);
        if (prepared_result.IsError()) {
            return fail(prepared_result.GetError());
        }
        TAxisHomePrm homePrm = prepared_result.Value().home_parameters;

        // MultiCard API使用1-based轴号
        short axisNum = static_cast<short>(axis + 1);

        // 检查轴使能状态与安全互锁
        SILIGEN_LOG_DEBUG_FMT_HELPER("HardwareTestAdapter::startHoming: checking axis %d enable status", axis);

        long status = 0;
        short statusRet = multicard_->MC_GetSts(axisNum, &status);
        SILIGEN_LOG_DEBUG_FMT_HELPER("MC_GetSts returned: %d, status=0x%lx", statusRet, status);

        if (statusRet == 0) {
            if (status & AXIS_STATUS_ESTOP) {
                return fail(Error(ErrorCode::EMERGENCY_STOP_ACTIVATED,
                                  "Cannot start homing: axis is in emergency stop state",
                                  "HardwareTestAdapter"));
            }
            if (status & AXIS_STATUS_SV_ALARM) {
                return fail(Error(ErrorCode::HARDWARE_ERROR,
                                  "Cannot start homing: axis has servo alarm",
                                  "HardwareTestAdapter"));
            }
            if (status & AXIS_STATUS_IO_EMG_STOP) {
                return fail(Error(ErrorCode::HARDWARE_ERROR,
                                  "Cannot start homing: IO emergency stop active",
                                  "HardwareTestAdapter"));
            }
            if (status & AXIS_STATUS_IO_SMS_STOP) {
                return fail(Error(ErrorCode::HARDWARE_ERROR,
                                  "Cannot start homing: IO stop active",
                                  "HardwareTestAdapter"));
            }
            if (status & AXIS_STATUS_FOLLOW_ERR) {
                return fail(Error(ErrorCode::HARDWARE_ERROR,
                                  "Cannot start homing: following error detected",
                                  "HardwareTestAdapter"));
            }
            if ((status & AXIS_STATUS_RUNNING) || (status & AXIS_STATUS_HOME_RUNNING)) {
                // 先安全停止现有运动
                long axis_mask = static_cast<long>(1) << axis;
                short stop_ret = multicard_->MC_Stop(axis_mask, 0);
                if (stop_ret != 0) {
                    return fail(Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                                      "Failed to stop axis before homing: " + getMultiCardErrorMessage(stop_ret),
                                      "HardwareTestAdapter"));
                }

                const int64 stop_timeout_ms =
                    std::max<int32>(hardware_config_.response_timeout_ms, 1000);
                auto stop_start = std::chrono::steady_clock::now();
                while (true) {
                    long stop_status = 0;
                    short stop_status_ret = multicard_->MC_GetSts(axisNum, &stop_status);
                    if (stop_status_ret != 0) {
                        return fail(Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                                          "Failed to read axis status while stopping for homing",
                                          "HardwareTestAdapter"));
                    }
                    if ((stop_status & AXIS_STATUS_RUNNING) == 0 &&
                        (stop_status & AXIS_STATUS_HOME_RUNNING) == 0) {
                        break;
                    }
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - stop_start).count();
                    if (elapsed >= stop_timeout_ms) {
                        return fail(Error(ErrorCode::TIMEOUT,
                                          "Timeout waiting for axis to stop before homing",
                                          "HardwareTestAdapter"));
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            }
        }

        bool isEnabled = (statusRet == 0) && (status & AXIS_STATUS_ENABLE) != 0;
        SILIGEN_LOG_DEBUG_FMT_HELPER("Axis %d enable status: %s", axis, isEnabled ? "ENABLED" : "DISABLED");

        // 如果轴未使能,自动使能
        if (!isEnabled) {
            SILIGEN_LOG_INFO_FMT_HELPER( "Axis %d not enabled, enabling automatically for homing...", axis);

            short enableRet = multicard_->MC_AxisOn(axisNum);
            SILIGEN_LOG_DEBUG_FMT_HELPER( "MC_AxisOn returned: %d", enableRet);

            if (enableRet != 0) {
                SILIGEN_LOG_ERROR_FMT_HELPER( "Failed to enable axis %d, error code: %d", axis, enableRet);
                return fail(Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                                  "Failed to enable axis for homing: " + getMultiCardErrorMessage(enableRet),
                                  "HardwareTestAdapter"));
            }

            // 等待使能生效 (100ms)
            SILIGEN_LOG_DEBUG("Waiting for axis enable to take effect...");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 设置回零参数
        SILIGEN_LOG_DEBUG_FMT_HELPER(
                "Setting homing parameters for axis %d: mode=%d, dir=%d, searchSpeed=%.2f, returnSpeed=%.2f",
                axis,
                homePrm.nHomeMode,
                homePrm.nHomeDir,
                homePrm.dHomeRapidVel,
                homePrm.dHomeLocatVel);

        short ret = multicard_->MC_HomeSetPrm(axisNum, &homePrm);
        SILIGEN_LOG_DEBUG_FMT_HELPER( "MC_HomeSetPrm returned: %d", ret);

        if (ret != 0) {
            SILIGEN_LOG_ERROR_FMT_HELPER( "MC_HomeSetPrm failed with code %d", ret);
            return fail(Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                              "Failed to set homing parameters for axis " + std::to_string(axis) +
                                  ": Error code " + std::to_string(ret),
                              "HardwareTestAdapter"));
        }

        // 清除轴状态和报警
        SILIGEN_LOG_DEBUG("Calling MC_ClrSts to clear axis status and alarms");
        multicard_->MC_ClrSts(axisNum);

        // 停止可能存在的回零操作
        SILIGEN_LOG_DEBUG("Calling MC_HomeStop to clear any existing homing operation");
        multicard_->MC_HomeStop(axisNum);

        // 如果回零开关已触发，先退离开关，避免继续向回零方向运动
        const int home_sample_count =
            (config.home_debounce_ms > 0) ? 1 : HomingSupport::kDefaultHomeSampleCount;
        auto home_triggered_result =
            IsHomeInputTriggeredStable(axis, home_sample_count, HomingSupport::kDefaultHomeSampleIntervalMs);

        auto read_home_switch_status = [&]() -> Result<bool> {
            long switch_status = 0;
            short switch_ret = multicard_->MC_GetSts(axisNum, &switch_status);
            if (switch_ret != 0) {
                return Result<bool>::Failure(
                    Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                          "读取HOME状态失败: " + getMultiCardErrorMessage(switch_ret),
                          "HardwareTestAdapter"));
            }
            return Result<bool>::Success((switch_status & AXIS_STATUS_HOME_SWITCH) != 0);
        };

        auto home_switch_status_result = read_home_switch_status();
        bool home_switch_active = false;
        if (home_switch_status_result.IsSuccess()) {
            home_switch_active = home_switch_status_result.Value();
        }

        if (home_triggered_result.IsError()) {
            if (!home_switch_status_result.IsSuccess()) {
                return fail(home_triggered_result.GetError());
            }
            SILIGEN_LOG_WARNING_FMT_HELPER(
                "Home input read failed, fallback to status bit (axis=%d, status_home=%d)",
                axis,
                home_switch_active ? 1 : 0);
        } else {
            if (home_switch_status_result.IsSuccess() &&
                home_switch_status_result.Value() != home_triggered_result.Value()) {
                SILIGEN_LOG_WARNING_FMT_HELPER(
                    "Home input mismatch (axis=%d, di=%d, status=%d)",
                    axis,
                    home_triggered_result.Value() ? 1 : 0,
                    home_switch_status_result.Value() ? 1 : 0);
            }
            home_switch_active = home_switch_active || home_triggered_result.Value();
        }

        if (home_switch_active) {
            if (!config.enable_escape) {
                return fail(Error(
                    ErrorCode::POSITION_OUT_OF_RANGE,
                    "回零前检测到回零开关已触发，已禁止回零，请先手动退离开关",
                    "HardwareTestAdapter"));
            }

            homing_stage_[axis] = HomingStage::BackingOff;
            auto escape_result = EscapeFromHomeLimit(axis, axisNum, config);
            if (escape_result.IsError()) {
                return fail(escape_result.GetError());
            }

            auto recheck_result =
                IsHomeInputTriggeredStable(axis,
                                           home_sample_count,
                                           HomingSupport::kDefaultHomeSampleIntervalMs);
            auto recheck_status_result = read_home_switch_status();
            bool recheck_active = false;
            if (recheck_status_result.IsSuccess()) {
                recheck_active = recheck_status_result.Value();
            }
            if (recheck_result.IsError()) {
                if (!recheck_status_result.IsSuccess()) {
                    return fail(recheck_result.GetError());
                }
                SILIGEN_LOG_WARNING_FMT_HELPER(
                    "Home input recheck failed, fallback to status bit (axis=%d, status_home=%d)",
                    axis,
                    recheck_active ? 1 : 0);
            } else {
                if (recheck_status_result.IsSuccess() &&
                    recheck_status_result.Value() != recheck_result.Value()) {
                    SILIGEN_LOG_WARNING_FMT_HELPER(
                        "Home recheck mismatch (axis=%d, di=%d, status=%d)",
                        axis,
                        recheck_result.Value() ? 1 : 0,
                        recheck_status_result.Value() ? 1 : 0);
                    if (!recheck_status_result.Value()) {
                        SILIGEN_LOG_WARNING_FMT_HELPER(
                            "Home recheck accepted status-bit release fallback after escape (axis=%d)",
                            axis);
                        recheck_active = false;
                    } else {
                        recheck_active = true;
                    }
                } else {
                    recheck_active = recheck_active || recheck_result.Value();
                }
            }

            if (recheck_active) {
                return fail(Error(
                    ErrorCode::POSITION_OUT_OF_RANGE,
                    "退离后回零开关仍触发，已取消回零",
                    "HardwareTestAdapter"));
            }
        }

        homing_stage_[axis] = HomingStage::SearchingHome;

        // 再次检查轴状态，确保没有急停或报警
        long currentStatus = 0;
        statusRet = multicard_->MC_GetSts(axisNum, &currentStatus);
        SILIGEN_LOG_DEBUG_FMT_HELPER( "Pre-homing status check: ret=%d, status=0x%lx", statusRet, currentStatus);

        if (statusRet == 0) {
            // 检查关键状态位
            if (currentStatus & AXIS_STATUS_ESTOP) {
                SILIGEN_LOG_ERROR_FMT_HELPER( "Axis %d is in emergency stop state (bit 0)", axis);
                return fail(Error(ErrorCode::EMERGENCY_STOP_ACTIVATED,
                                  "Cannot start homing: axis is in emergency stop state",
                                  "HardwareTestAdapter"));
            }
            if (currentStatus & AXIS_STATUS_SV_ALARM) {
                SILIGEN_LOG_ERROR_FMT_HELPER( "Axis %d has servo alarm (bit 1)", axis);
                return fail(Error(ErrorCode::HARDWARE_ERROR,
                                  "Cannot start homing: axis has servo alarm",
                                  "HardwareTestAdapter"));
            }
        }

        // 启动回零
        SILIGEN_LOG_DEBUG_FMT_HELPER( "Calling MC_HomeStart(axis=%d, API_axisNum=%d)", axis, axisNum);
        ret = multicard_->MC_HomeStart(axisNum);
        SILIGEN_LOG_DEBUG_FMT_HELPER( "MC_HomeStart returned: %d", ret);
        if (ret != 0) {
            SILIGEN_LOG_ERROR_FMT_HELPER( "MC_HomeStart failed with code %d", ret);
            return fail(Error(
                ErrorCode::HARDWARE_OPERATION_FAILED,
                "Failed to start homing for axis " + std::to_string(axis) + ": Error code: " + std::to_string(ret),
                "HardwareTestAdapter"));
        }

        homing_active_latch_[axis] = true;
        homing_start_time_[axis] = std::chrono::steady_clock::now();
        homing_timer_active_[axis] = true;
        any_axis_started = true;
        SILIGEN_LOG_INFO_FMT_HELPER( "Homing started successfully for axis %d", axis);
    }

    home_sense_guard.Commit();
    return Result<void>::Success();
}

Result<void> HardwareTestAdapter::stopHoming(const std::vector<LogicalAxisId>& axes) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (!isConnected()) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Hardware not connected", "HardwareTestAdapter"));
    }

    // 停止每个轴的回零
    for (auto axis_id : axes) {
        const int axis = AxisIndex(axis_id);
        auto validateResult = validateAxisNumber(axis);
        if (validateResult.IsError()) {
            return validateResult;
        }

        homing_success_latch_[axis] = false;
        homing_failed_latch_[axis] = false;
        homing_active_latch_[axis] = false;

        // MultiCard API使用1-based轴号
        short axisNum = static_cast<short>(axis + 1);
        short ret = multicard_->MC_HomeStop(axisNum);

        if (ret != 0) {
            return Result<void>::Failure(
                Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                      "Failed to stop homing for axis " + std::to_string(axis) + ": " + getMultiCardErrorMessage(ret),
                      "HardwareTestAdapter"));
        }
    }

    return Result<void>::Success();
}

HomingTestStatus HardwareTestAdapter::getHomingStatus(LogicalAxisId axis_id) const {
    const int axis = AxisIndex(axis_id);
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto validateResult = validateAxisNumber(axis);
    if (validateResult.IsError()) {
        return HomingTestStatus::NotStarted;
    }

    return HomingSupport::QueryHomingStatus(axis,
                                            static_cast<short>(axis + 1),
                                            multicard_.get(),
                                            homing_configs_[axis],
                                            hardware_config_,
                                            homing_success_latch_,
                                            homing_failed_latch_,
                                            homing_active_latch_,
                                            homing_stage_,
                                            homing_start_time_,
                                            homing_timer_active_,
                                            [this](int axis_index) { return readHomeInputStateRaw(axis_index); },
                                            [this](short error_code) { return getMultiCardErrorMessage(error_code); },
                                            [this, axis_num = static_cast<short>(axis + 1)](int axis_index,
                                                                                            const HomingConfig& config) {
                                                return EscapeFromHomeLimit(axis_index, axis_num, config);
                                            });
}

HomingStage HardwareTestAdapter::getHomingStage(LogicalAxisId axis_id) const {
    const int axis = AxisIndex(axis_id);
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto validateResult = validateAxisNumber(axis);
    if (validateResult.IsError()) {
        return HomingStage::Idle;
    }

    (void)getHomingStatus(axis_id);
    return homing_stage_[axis];
}

LimitSwitchState HardwareTestAdapter::getLimitSwitchState(LogicalAxisId axis_id) const {
    if (!kUseHomeAsHardLimit) {
        return LimitSwitchState();
    }

    const int axis = AxisIndex(axis_id);
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto result = readLimitSwitchStateRaw(axis);
    if (result.IsError()) {
        SILIGEN_LOG_WARNING("Failed to read limit switch state: " + result.GetError().GetMessage());
        return LimitSwitchState();
    }

    return result.Value();
}

}  // namespace Siligen::Infrastructure::Adapters::Hardware



