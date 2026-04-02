#include "HomingPortAdapter.h"

#define MODULE_NAME "HomingPortAdapter"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "adapters/motion/controller/homing/HomingSupport.h"
#include "domain/configuration/services/ReadyZeroSpeedResolver.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <thread>

#include "siligen/device/adapters/drivers/multicard/MultiCardCPP.h"

namespace Siligen::Infrastructure::Adapters::Motion {

using namespace Shared::Types;
namespace HomingSupport = Siligen::Infrastructure::Adapters::Internal::Homing;
using Siligen::Domain::Motion::ValueObjects::LimitSwitchState;

namespace {
constexpr int32 kPollingIntervalMs = 50;
constexpr double kStopVelocityToleranceMmS = 0.1;
}  // namespace

HomingPortAdapter::HomingPortAdapter(
    std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> multicard,
    const Siligen::Shared::Types::HardwareConfiguration& config,
    const std::array<HomingConfig, 4>& homing_configs)
    : multicard_(std::move(multicard)),
      hardware_config_(config),
      unit_converter_(config),
      homing_configs_(homing_configs),
      axis_count_(config.EffectiveAxisCount()) {}

Result<void> HomingPortAdapter::HomeAxis(LogicalAxisId axis) {
    SetHomingInvalidated(axis, false);
    return startHoming({axis});
}

Result<void> HomingPortAdapter::StopHoming(LogicalAxisId axis) {
    SetHomingInvalidated(axis, true);
    return stopHoming({axis});
}

Result<void> HomingPortAdapter::ResetHomingState(LogicalAxisId axis) {
    SetHomingInvalidated(axis, true);
    if (!multicard_) {
        return Result<void>::Success();
    }
    const int axis_index = AxisIndex(axis);
    (void)multicard_->MC_HomeStop(static_cast<short>(axis_index + 1));
    auto stop_result = StopAxisMotionAndWait(axis_index);
    if (stop_result.IsError()) {
        return stop_result;
    }
    return Result<void>::Success();
}

Result<HomingStatus> HomingPortAdapter::GetHomingStatus(LogicalAxisId axis) const {
    if (!multicard_) {
        return Result<HomingStatus>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED,
                                 "Homing runtime未初始化",
                                 "HomingPortAdapter"));
    }

    HomingStatus status;
    status.axis = axis;
    if (IsHomingInvalidated(axis)) {
        status.state = HomingState::NOT_HOMED;
        status.stage = HomingStage::Idle;
        auto position_result = getAxisPosition(axis);
        if (position_result.IsSuccess()) {
            status.current_position = static_cast<Shared::Types::float32>(position_result.Value());
        }
        return Result<HomingStatus>::Success(status);
    }

    auto homing_status = QueryHomingTestStatus(axis);
    status.state = ConvertStatus(homing_status);
    status.stage = getHomingStage(axis);

    auto position_result = getAxisPosition(axis);
    if (position_result.IsSuccess()) {
        status.current_position = static_cast<Shared::Types::float32>(position_result.Value());
    }

    status.is_searching = (homing_status == HomingTestStatus::InProgress);
    if (homing_status == HomingTestStatus::Failed) {
        status.error_code = static_cast<int32>(Shared::Types::ErrorCode::HARDWARE_ERROR);
    }
    return Result<HomingStatus>::Success(status);
}

Result<bool> HomingPortAdapter::IsAxisHomed(LogicalAxisId axis) const {
    if (IsHomingInvalidated(axis)) {
        return Result<bool>::Success(false);
    }
    auto status = GetHomingStatus(axis);
    if (status.IsError()) {
        return Result<bool>::Failure(status.GetError());
    }
    return Result<bool>::Success(status.Value().state == HomingState::HOMED);
}

Result<bool> HomingPortAdapter::IsHomingInProgress(LogicalAxisId axis) const {
    auto status = GetHomingStatus(axis);
    if (status.IsError()) {
        return Result<bool>::Failure(status.GetError());
    }
    return Result<bool>::Success(status.Value().state == HomingState::HOMING);
}

Result<void> HomingPortAdapter::WaitForHomingComplete(LogicalAxisId axis, int32 timeout_ms) {
    if (timeout_ms <= 0) {
        return Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER,
                                 "Timeout must be positive",
                                 "HomingPortAdapter"));
    }

    auto start = std::chrono::steady_clock::now();
    int32 poll_interval_ms = kPollingIntervalMs;
    constexpr int32 kMaxPollingIntervalMs = 200;
    while (true) {
        auto status_result = GetHomingStatus(axis);
        if (status_result.IsError()) {
            return Result<void>::Failure(status_result.GetError());
        }

        const auto state = status_result.Value().state;
        if (state == HomingState::HOMED) {
            SetHomingInvalidated(axis, false);
            return Result<void>::Success();
        }
        if (state == HomingState::FAILED) {
            return Result<void>::Failure(
                Shared::Types::Error(Shared::Types::ErrorCode::HARDWARE_ERROR,
                                     "Homing failed",
                                     "HomingPortAdapter"));
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeout_ms) {
            return Result<void>::Failure(
                Shared::Types::Error(Shared::Types::ErrorCode::TIMEOUT,
                                     "Homing timeout",
                                     "HomingPortAdapter"));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
        poll_interval_ms = std::min(poll_interval_ms * 2, kMaxPollingIntervalMs);
    }
}

bool HomingPortAdapter::isConnected() const {
    if (!multicard_) {
        return false;
    }
    long status = 0;
    return multicard_->MC_GetSts(1, &status) == 0;
}

Result<void> HomingPortAdapter::startHoming(const std::vector<LogicalAxisId>& axes) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (axes.empty()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Axes list is empty", "HomingPortAdapter"));
    }
    if (!multicard_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Homing runtime未初始化", "HomingPortAdapter"));
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
                Error(ErrorCode::HARDWARE_OPERATION_FAILED, message, "HomingPortAdapter"));
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

        auto effective_config_result = Siligen::Domain::Configuration::Services::ApplyUnifiedReadyZeroSpeed(
            homing_configs_[axis],
            "HomingPortAdapter");
        if (effective_config_result.IsError()) {
            return fail(effective_config_result.GetError());
        }
        const auto& config = effective_config_result.Value();
        auto prepared_result = HomingSupport::PrepareHomeParameters(axis,
                                                                    config,
                                                                    hardware_config_,
                                                                    unit_converter_,
                                                                    "HomingPortAdapter",
                                                                    Units::PULSE_PER_SEC_TO_MS);
        if (prepared_result.IsError()) {
            return fail(prepared_result.GetError());
        }
        TAxisHomePrm homePrm = prepared_result.Value().home_parameters;

        // MultiCard API使用1-based轴号
        short axisNum = static_cast<short>(axis + 1);

        // 检查轴使能状态与安全互锁
        SILIGEN_LOG_DEBUG_FMT_HELPER("HomingPortAdapter::startHoming: checking axis %d enable status", axis);

        long status = 0;
        short statusRet = multicard_->MC_GetSts(axisNum, &status);
        SILIGEN_LOG_DEBUG_FMT_HELPER("MC_GetSts returned: %d, status=0x%lx", statusRet, status);

        if (statusRet == 0) {
            if (status & AXIS_STATUS_ESTOP) {
                return fail(Error(ErrorCode::EMERGENCY_STOP_ACTIVATED,
                                  "Cannot start homing: axis is in emergency stop state",
                                  "HomingPortAdapter"));
            }
            if (status & AXIS_STATUS_SV_ALARM) {
                return fail(Error(ErrorCode::HARDWARE_ERROR,
                                  "Cannot start homing: axis has servo alarm",
                                  "HomingPortAdapter"));
            }
            if (status & AXIS_STATUS_IO_EMG_STOP) {
                return fail(Error(ErrorCode::HARDWARE_ERROR,
                                  "Cannot start homing: IO emergency stop active",
                                  "HomingPortAdapter"));
            }
            if (status & AXIS_STATUS_IO_SMS_STOP) {
                return fail(Error(ErrorCode::HARDWARE_ERROR,
                                  "Cannot start homing: IO stop active",
                                  "HomingPortAdapter"));
            }
            if (status & AXIS_STATUS_FOLLOW_ERR) {
                return fail(Error(ErrorCode::HARDWARE_ERROR,
                                  "Cannot start homing: following error detected",
                                  "HomingPortAdapter"));
            }
            if ((status & AXIS_STATUS_RUNNING) || (status & AXIS_STATUS_HOME_RUNNING)) {
                // 先安全停止现有运动
                long axis_mask = static_cast<long>(1) << axis;
                short stop_ret = multicard_->MC_Stop(axis_mask, 0);
                if (stop_ret != 0) {
                    return fail(Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                                      "Failed to stop axis before homing: " + getMultiCardErrorMessage(stop_ret),
                                      "HomingPortAdapter"));
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
                                          "HomingPortAdapter"));
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
                                          "HomingPortAdapter"));
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
                                  "HomingPortAdapter"));
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
                              "HomingPortAdapter"));
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
                          "HomingPortAdapter"));
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
                    "HomingPortAdapter"));
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
                    "HomingPortAdapter"));
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
                                  "HomingPortAdapter"));
            }
            if (currentStatus & AXIS_STATUS_SV_ALARM) {
                SILIGEN_LOG_ERROR_FMT_HELPER( "Axis %d has servo alarm (bit 1)", axis);
                return fail(Error(ErrorCode::HARDWARE_ERROR,
                                  "Cannot start homing: axis has servo alarm",
                                  "HomingPortAdapter"));
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
                "HomingPortAdapter"));
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



Result<void> HomingPortAdapter::stopHoming(const std::vector<LogicalAxisId>& axes) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (!isConnected()) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Hardware not connected", "HomingPortAdapter"));
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
        homing_timer_active_[axis] = false;
        homing_stage_[axis] = HomingStage::Idle;

        // MultiCard API使用1-based轴号
        short axisNum = static_cast<short>(axis + 1);
        short ret = multicard_->MC_HomeStop(axisNum);

        if (ret != 0) {
            return Result<void>::Failure(
                Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                      "Failed to stop homing for axis " + std::to_string(axis) + ": " + getMultiCardErrorMessage(ret),
                      "HomingPortAdapter"));
        }

        auto stop_result = StopAxisMotionAndWait(axis);
        if (stop_result.IsError()) {
            return stop_result;
        }
    }

    return Result<void>::Success();
}

Result<void> HomingPortAdapter::StopAxisMotionAndWait(int axis) const {
    if (!multicard_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Hardware not connected", "HomingPortAdapter"));
    }

    auto validateResult = validateAxisNumber(axis);
    if (validateResult.IsError()) {
        return validateResult;
    }

    const short axis_num = static_cast<short>(axis + 1);
    const auto read_profile_velocity_mm_s = [&]() -> Result<double> {
        double profile_velocity_pulse_per_ms = 0.0;
        unsigned long clock = 0;
        const short velocity_ret = multicard_->MC_GetAxisPrfVel(axis_num, &profile_velocity_pulse_per_ms, 1, &clock);
        if (velocity_ret != 0) {
            return Result<double>::Failure(
                Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                      "Failed to read axis profile velocity while stopping homing",
                      "HomingPortAdapter"));
        }
        return Result<double>::Success(
            profile_velocity_pulse_per_ms * 1000.0 / unit_converter_.GetPulsePerMm());
    };

    long status = 0;
    short status_ret = multicard_->MC_GetSts(axis_num, &status);
    if (status_ret != 0) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                  "Failed to read axis status while stopping homing",
                  "HomingPortAdapter"));
    }

    auto velocity_result = read_profile_velocity_mm_s();
    const double profile_velocity_mm_s = velocity_result.IsSuccess() ? velocity_result.Value() : 0.0;
    const bool status_reports_running =
        (status & AXIS_STATUS_RUNNING) != 0 || (status & AXIS_STATUS_HOME_RUNNING) != 0;
    const bool velocity_reports_running =
        velocity_result.IsSuccess() && std::fabs(profile_velocity_mm_s) > kStopVelocityToleranceMmS;

    SILIGEN_LOG_INFO_FMT_HELPER(
        "StopAxisMotionAndWait pre-check axis=%d hw_status=0x%lx running_bits=%d prf_vel_mm_s=%.4f vel_valid=%d",
        axis,
        status,
        status_reports_running ? 1 : 0,
        profile_velocity_mm_s,
        velocity_result.IsSuccess() ? 1 : 0);

    if (!status_reports_running && !velocity_reports_running) {
        return Result<void>::Success();
    }

    const long axis_mask = static_cast<long>(1) << axis;
    short stop_ret = multicard_->MC_Stop(axis_mask, 0);
    if (stop_ret != 0) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                  "Failed to stop axis after homing abort: " + getMultiCardErrorMessage(stop_ret),
                  "HomingPortAdapter"));
    }

    const int64 stop_timeout_ms = std::max<int32>(hardware_config_.response_timeout_ms, 1000);
    const auto stop_start = std::chrono::steady_clock::now();
    while (true) {
        long stop_status = 0;
        short stop_status_ret = multicard_->MC_GetSts(axis_num, &stop_status);
        if (stop_status_ret != 0) {
            return Result<void>::Failure(
                Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                      "Failed to read axis status while waiting homing stop",
                      "HomingPortAdapter"));
        }
        auto stop_velocity_result = read_profile_velocity_mm_s();
        const double stop_profile_velocity_mm_s =
            stop_velocity_result.IsSuccess() ? stop_velocity_result.Value() : 0.0;
        const bool stop_status_running =
            (stop_status & AXIS_STATUS_RUNNING) != 0 || (stop_status & AXIS_STATUS_HOME_RUNNING) != 0;
        const bool stop_velocity_running =
            stop_velocity_result.IsSuccess() && std::fabs(stop_profile_velocity_mm_s) > kStopVelocityToleranceMmS;

        if (!stop_status_running && !stop_velocity_running) {
            SILIGEN_LOG_INFO_FMT_HELPER(
                "StopAxisMotionAndWait completed axis=%d hw_status=0x%lx prf_vel_mm_s=%.4f vel_valid=%d",
                axis,
                stop_status,
                stop_profile_velocity_mm_s,
                stop_velocity_result.IsSuccess() ? 1 : 0);
            return Result<void>::Success();
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - stop_start).count();
        if (elapsed >= stop_timeout_ms) {
            return Result<void>::Failure(
                Error(ErrorCode::TIMEOUT,
                      "Timeout waiting for axis to stop after homing abort",
                      "HomingPortAdapter"));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}



HomingTestStatus HomingPortAdapter::QueryHomingTestStatus(LogicalAxisId axis_id) const {
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



HomingStage HomingPortAdapter::getHomingStage(LogicalAxisId axis_id) const {
    const int axis = AxisIndex(axis_id);
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto validateResult = validateAxisNumber(axis);
    if (validateResult.IsError()) {
        return HomingStage::Idle;
    }

    (void)QueryHomingTestStatus(axis_id);
    return homing_stage_[axis];
}



Result<double> HomingPortAdapter::getAxisPosition(LogicalAxisId axis_id) const {
    const int axis = AxisIndex(axis_id);
    SILIGEN_LOG_DEBUG_FMT_HELPER( "HomingPortAdapter::getAxisPosition: axis=%d", axis);

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
                                                 "HomingPortAdapter"));
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
                                             "HomingPortAdapter"));
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



Result<bool> HomingPortAdapter::IsHomeInputTriggeredStable(int axis,
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



Result<void> HomingPortAdapter::EscapeFromHomeLimit(int axis, short axisNum, const HomingConfig& config) const {
    return HomingSupport::EscapeFromHomeLimit(axis,
                                              axisNum,
                                              config,
                                              multicard_.get(),
                                              hardware_config_,
                                              unit_converter_,
                                              "HomingPortAdapter",
                                              Units::PULSE_PER_SEC_TO_MS,
                                              [this](int axis_index, int sample_count, int sample_interval_ms) {
                                                  return IsHomeInputTriggeredStable(
                                                      axis_index, sample_count, sample_interval_ms);
                                              },
                                              [this](int axis_index) { return readHomeInputStateRaw(axis_index); },
                                              [this](short error_code) { return getMultiCardErrorMessage(error_code); });
}



Result<HomeInputState> HomingPortAdapter::readHomeInputStateRaw(int axis) const {
    auto validateResult = validateAxisNumber(axis);
    if (validateResult.IsError()) {
        return Result<HomeInputState>::Failure(validateResult.GetError());
    }

    if (!multicard_) {
        return Result<HomeInputState>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "MultiCard未初始化", "HomingPortAdapter"));
    }

    const auto& config = homing_configs_[axis];
    const int input_source = config.home_input_source;
    if (input_source < 0 || input_source > 7) {
        return Result<HomeInputState>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "HOME输入源配置无效", "HomingPortAdapter"));
    }

    int input_bit = config.home_input_bit;
    if (input_bit < 0) {
        input_bit = axis;
    }
    if (input_bit < 0 || input_bit > 15) {
        return Result<HomeInputState>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "HOME输入位配置无效", "HomingPortAdapter"));
    }

    auto sample_once = [&]() -> Result<HomeInputState> {
        long home_raw = 0;
        short home_ret = multicard_->MC_GetDiRaw(static_cast<short>(input_source), &home_raw);
        if (home_ret != 0) {
            return Result<HomeInputState>::Failure(
                Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                      "读取Home信号失败: " + getMultiCardErrorMessage(home_ret),
                      "HomingPortAdapter"));
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



Result<void> HomingPortAdapter::validateAxisNumber(int axis) const {
    if (axis < 0 || axis >= axis_count_) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid axis number (must be 0-1)", "HomingPortAdapter"));
    }

    return Result<void>::Success();
}



std::string HomingPortAdapter::getMultiCardErrorMessage(short errorCode) const {
    switch (errorCode) {
        case MC_COM_SUCCESS:
            return "成功";
        case MC_COM_ERR_EXEC_FAIL:
            return "执行失败 - 检测命令执行条件是否满足";
        case MC_COM_ERR_LICENSE_WRONG:
            return "版本不支持该API";
        case MC_COM_ERR_DATA_WORRY:
            return "参数错误";
        case MC_COM_ERR_SEND:
            return "通讯失败";
        case MC_COM_ERR_CARD_OPEN_FAIL:
            return "打开控制器失败";
        case MC_COM_ERR_TIME_OUT:
            return "运动控制器无响应";
        case MC_COM_ERR_COM_OPEN_FAIL:
            return "串口打开失败";
        default:
            return "未知错误码: " + std::to_string(errorCode);
    }
}



HomingState HomingPortAdapter::ConvertStatus(HomingTestStatus status) {
    switch (status) {
        case HomingTestStatus::Completed:
            return HomingState::HOMED;
        case HomingTestStatus::InProgress:
            return HomingState::HOMING;
        case HomingTestStatus::Failed:
            return HomingState::FAILED;
        case HomingTestStatus::NotStarted:
        default:
            return HomingState::NOT_HOMED;
    }
}

bool HomingPortAdapter::IsHomingInvalidated(LogicalAxisId axis) const {
    const int axis_index = AxisIndex(axis);
    return axis_index >= 0 && axis_index < static_cast<int>(homing_invalidated_.size())
               ? homing_invalidated_[axis_index]
               : false;
}

void HomingPortAdapter::SetHomingInvalidated(LogicalAxisId axis, bool invalidated) {
    const int axis_index = AxisIndex(axis);
    if (axis_index >= 0 && axis_index < static_cast<int>(homing_invalidated_.size())) {
        homing_invalidated_[axis_index] = invalidated;
    }
}

}  // namespace Siligen::Infrastructure::Adapters::Motion

