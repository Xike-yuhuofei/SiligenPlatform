#pragma once

#include "domain/configuration/ports/IConfigurationPort.h"
#include "domain/motion/value-objects/HardwareTestTypes.h"
#include "domain/motion/value-objects/MotionTypes.h"
#include "modules/device-hal/src/drivers/multicard/IMultiCardWrapper.h"
#include "modules/device-hal/src/drivers/multicard/MultiCardCPP.h"
#include "shared/types/Error.h"
#include "shared/types/HardwareConfiguration.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace Siligen::Infrastructure::Adapters::Internal::Homing {

using Siligen::Domain::Configuration::Ports::HomingConfig;
using Siligen::Domain::Motion::ValueObjects::HomeInputState;
using Siligen::Domain::Motion::ValueObjects::HomingStage;
using Siligen::Domain::Motion::ValueObjects::HomingTestStatus;
using Siligen::Infrastructure::Hardware::IMultiCardWrapper;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::HardwareConfiguration;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;

constexpr int kDefaultHomeSampleCount = 3;
constexpr int kDefaultHomeSampleIntervalMs = 5;
constexpr double kSearchDistanceMarginMm = 5.0;
constexpr double kAbsoluteMinSearchDistanceMm = 10.0;
constexpr double kSoftLimitMarginMm = 1.0;

struct PreparedHomeParameters {
    TAxisHomePrm home_parameters{};
    double effective_search_distance_mm = 0.0;
};

inline unsigned short ComputeDesiredHomeSense(int axis_count, const std::array<HomingConfig, 4>& homing_configs) {
    unsigned short desired_home_sense = 0;
    for (int axis = 0; axis < axis_count; ++axis) {
        const auto& config = homing_configs[axis];
        if (config.home_input_source == MC_HOME && config.home_active_low) {
            desired_home_sense |= static_cast<unsigned short>(1 << axis);
        }
    }
    return desired_home_sense;
}

class HomeSenseRollbackGuard {
   public:
    HomeSenseRollbackGuard(IMultiCardWrapper* card,
                           bool active,
                           unsigned short previous,
                           const bool* started_any)
        : card_(card), active_(active), previous_(previous), started_any_(started_any) {}

    void Commit() { active_ = false; }

    ~HomeSenseRollbackGuard() {
        if (active_ && card_ && (!started_any_ || !(*started_any_))) {
            card_->MC_HomeSns(previous_);
        }
    }

    HomeSenseRollbackGuard(const HomeSenseRollbackGuard&) = delete;
    HomeSenseRollbackGuard& operator=(const HomeSenseRollbackGuard&) = delete;

   private:
    IMultiCardWrapper* card_ = nullptr;
    bool active_ = false;
    unsigned short previous_ = 0;
    const bool* started_any_ = nullptr;
};

template <typename ConnectedFn>
inline Result<void> ValidateStartHomingRequest(const std::vector<Siligen::Shared::Types::LogicalAxisId>& axes,
                                               ConnectedFn&& is_connected,
                                               const char* owner) {
    if (axes.empty()) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "Axes list is empty", owner));
    }
    if (!is_connected()) {
        return Result<void>::Failure(Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Hardware not connected", owner));
    }
    return Result<void>::Success();
}

inline void ResetHomingState(std::array<bool, 4>& success_latch,
                             std::array<bool, 4>& failed_latch,
                             std::array<bool, 4>& active_latch,
                             std::array<HomingStage, 4>& stage_latch,
                             std::array<bool, 4>& timer_active_latch,
                             int axis) {
    success_latch[axis] = false;
    failed_latch[axis] = false;
    active_latch[axis] = false;
    timer_active_latch[axis] = false;
    stage_latch[axis] = HomingStage::Idle;
}

inline void MarkHomingFailed(std::array<bool, 4>& success_latch,
                             std::array<bool, 4>& failed_latch,
                             std::array<bool, 4>& active_latch,
                             std::array<HomingStage, 4>& stage_latch,
                             std::array<bool, 4>& timer_active_latch,
                             int axis) {
    failed_latch[axis] = true;
    success_latch[axis] = false;
    active_latch[axis] = false;
    timer_active_latch[axis] = false;
    stage_latch[axis] = HomingStage::Failed;
}

inline void MarkHomingSucceeded(std::array<bool, 4>& success_latch,
                                std::array<bool, 4>& failed_latch,
                                std::array<bool, 4>& active_latch,
                                std::array<HomingStage, 4>& stage_latch,
                                std::array<bool, 4>& timer_active_latch,
                                int axis) {
    success_latch[axis] = true;
    failed_latch[axis] = false;
    active_latch[axis] = false;
    timer_active_latch[axis] = false;
    stage_latch[axis] = HomingStage::Completed;
}

template <typename ReadHomeStateFn>
HomingTestStatus FinalizeSuccessfulHoming(int axis,
                                          const HomingConfig& config,
                                          std::array<bool, 4>& success_latch,
                                          std::array<bool, 4>& failed_latch,
                                          std::array<bool, 4>& active_latch,
                                          std::array<HomingStage, 4>& stage_latch,
                                          std::array<bool, 4>& timer_active_latch,
                                          ReadHomeStateFn&& read_home_input_state_raw) {
    if (config.enable_limit_switch && config.enable_escape && config.home_input_source == MC_HOME) {
        auto home_state_result = read_home_input_state_raw(axis);
        if (home_state_result.IsSuccess()) {
            if (home_state_result.Value().triggered) {
                MarkHomingFailed(success_latch, failed_latch, active_latch, stage_latch, timer_active_latch, axis);
                SILIGEN_LOG_WARNING_FMT_HELPER(
                    "Homing completed but home switch still triggered (axis=%d)", axis);
                return HomingTestStatus::Failed;
            }
        } else {
            SILIGEN_LOG_WARNING(
                "Failed to read home input state after homing: " + home_state_result.GetError().GetMessage());
        }
    }

    MarkHomingSucceeded(success_latch, failed_latch, active_latch, stage_latch, timer_active_latch, axis);
    return HomingTestStatus::Completed;
}

template <typename ReadHomeStateFn, typename ErrorTextFn>
HomingTestStatus QueryHomingStatus(int axis,
                                   short axis_num,
                                   IMultiCardWrapper* multicard,
                                   const HomingConfig& config,
                                   const HardwareConfiguration& hardware_config,
                                   std::array<bool, 4>& success_latch,
                                   std::array<bool, 4>& failed_latch,
                                   std::array<bool, 4>& active_latch,
                                   std::array<HomingStage, 4>& stage_latch,
                                   std::array<std::chrono::steady_clock::time_point, 4>& start_time_latch,
                                   std::array<bool, 4>& timer_active_latch,
                                   ReadHomeStateFn&& read_home_input_state_raw,
                                   ErrorTextFn&& get_error_message) {
    if (timer_active_latch[axis]) {
        int32 timeout_ms = config.timeout_ms > 0 ? config.timeout_ms : hardware_config.motion_timeout_ms;
        if (timeout_ms > 0) {
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time_latch[axis]).count();
            if (elapsed_ms >= timeout_ms) {
                MarkHomingFailed(success_latch, failed_latch, active_latch, stage_latch, timer_active_latch, axis);
                SILIGEN_LOG_WARNING_FMT_HELPER("Homing timeout detected (axis=%d, elapsed=%lldms, timeout=%dms)",
                                               axis,
                                               static_cast<long long>(elapsed_ms),
                                               timeout_ms);
                return HomingTestStatus::Failed;
            }
        }
    }

    if (!active_latch[axis]) {
        if (failed_latch[axis]) {
            stage_latch[axis] = HomingStage::Failed;
            return HomingTestStatus::Failed;
        }
        if (success_latch[axis]) {
            stage_latch[axis] = HomingStage::Completed;
            return HomingTestStatus::Completed;
        }
    }

    if (!multicard) {
        stage_latch[axis] = HomingStage::Idle;
        return HomingTestStatus::NotStarted;
    }

    unsigned short home_status = 0;
    short home_ret = multicard->MC_HomeGetSts(axis_num, &home_status);
    if (home_ret == 0) {
        if (home_status == 1) {
            return FinalizeSuccessfulHoming(axis,
                                            config,
                                            success_latch,
                                            failed_latch,
                                            active_latch,
                                            stage_latch,
                                            timer_active_latch,
                                            read_home_input_state_raw);
        }
        if (home_status == 2) {
            MarkHomingFailed(success_latch, failed_latch, active_latch, stage_latch, timer_active_latch, axis);
            return HomingTestStatus::Failed;
        }
    } else {
        SILIGEN_LOG_WARNING("MC_HomeGetSts failed for axis " + std::to_string(axis) + ": " +
                            get_error_message(home_ret));
    }

    long status = 0;
    unsigned long clock = 0;
    short ret = multicard->MC_GetSts(axis_num, &status, 1, &clock);
    if (ret != 0) {
        stage_latch[axis] = HomingStage::Idle;
        return HomingTestStatus::NotStarted;
    }

    const bool homing_in_progress = (status & AXIS_STATUS_HOME_RUNNING) != 0;
    if (homing_in_progress) {
        if (stage_latch[axis] == HomingStage::Idle) {
            stage_latch[axis] = HomingStage::SearchingHome;
        }
        return HomingTestStatus::InProgress;
    }

    const bool homing_succeeded = (status & AXIS_STATUS_HOME_SUCESS) != 0;
    const bool homing_failed = (status & AXIS_STATUS_HOME_FAIL) != 0;
    if (homing_failed) {
        MarkHomingFailed(success_latch, failed_latch, active_latch, stage_latch, timer_active_latch, axis);
        return HomingTestStatus::Failed;
    }
    if (homing_succeeded) {
        return FinalizeSuccessfulHoming(axis,
                                        config,
                                        success_latch,
                                        failed_latch,
                                        active_latch,
                                        stage_latch,
                                        timer_active_latch,
                                        read_home_input_state_raw);
    }

    stage_latch[axis] = HomingStage::Idle;
    return HomingTestStatus::NotStarted;
}

template <typename UnitConverterT>
Result<PreparedHomeParameters> PrepareHomeParameters(int axis,
                                                     const HomingConfig& config,
                                                     const HardwareConfiguration& hardware_config,
                                                     const UnitConverterT& unit_converter,
                                                     const char* owner,
                                                     double pulse_per_sec_to_ms) {
    PreparedHomeParameters prepared;

    prepared.effective_search_distance_mm = config.search_distance;
    const double axis_range_mm =
        hardware_config.soft_limit_positive_mm - hardware_config.soft_limit_negative_mm;
    const bool has_axis_range = axis_range_mm > 0.0;
    const double max_search_distance =
        has_axis_range ? (axis_range_mm + kSearchDistanceMarginMm) : 0.0;

    if (axis_range_mm <= 0.0) {
        SILIGEN_LOG_WARNING_FMT_HELPER(
            "Axis %d soft limit range invalid (pos=%.2f, neg=%.2f), using absolute minimum search distance",
            axis,
            hardware_config.soft_limit_positive_mm,
            hardware_config.soft_limit_negative_mm);
    }

    if (config.enable_limit_switch && axis_range_mm > 0.0) {
        const double min_search_distance = axis_range_mm + kSearchDistanceMarginMm;
        if (prepared.effective_search_distance_mm < min_search_distance) {
            SILIGEN_LOG_WARNING_FMT_HELPER(
                "Axis %d search_distance %.2f mm < axis_range %.2f mm, auto expand to %.2f mm",
                axis,
                prepared.effective_search_distance_mm,
                axis_range_mm,
                min_search_distance);
            prepared.effective_search_distance_mm = min_search_distance;
        }
    } else if (axis_range_mm > 0.0 && prepared.effective_search_distance_mm < axis_range_mm) {
        SILIGEN_LOG_WARNING_FMT_HELPER(
            "Axis %d search_distance %.2f mm < axis_range %.2f mm with limit switch disabled",
            axis,
            prepared.effective_search_distance_mm,
            axis_range_mm);
    }

    if (prepared.effective_search_distance_mm < kAbsoluteMinSearchDistanceMm) {
        SILIGEN_LOG_WARNING_FMT_HELPER(
            "Axis %d search_distance %.2f mm < absolute minimum %.2f mm, auto expand",
            axis,
            prepared.effective_search_distance_mm,
            kAbsoluteMinSearchDistanceMm);
        prepared.effective_search_distance_mm = kAbsoluteMinSearchDistanceMm;
    }

    if (has_axis_range && max_search_distance >= kAbsoluteMinSearchDistanceMm &&
        prepared.effective_search_distance_mm > max_search_distance) {
        SILIGEN_LOG_WARNING_FMT_HELPER(
            "Axis %d search_distance %.2f mm > axis_range_max %.2f mm, auto clamp",
            axis,
            prepared.effective_search_distance_mm,
            max_search_distance);
        prepared.effective_search_distance_mm = max_search_distance;
    }

    if (prepared.effective_search_distance_mm <= 0.0) {
        return Result<PreparedHomeParameters>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "回零搜索距离无效", owner));
    }

    TAxisHomePrm home_parameters = {};
    home_parameters.nHomeMode = static_cast<short>(config.mode);
    home_parameters.nHomeDir = static_cast<short>(config.direction);

    const double pulses_per_mm = static_cast<double>(unit_converter.GetPulsePerMm());
    if (pulses_per_mm <= 0.0 || !std::isfinite(pulses_per_mm)) {
        return Result<PreparedHomeParameters>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "脉冲当量无效", owner));
    }

    auto to_pulse_long = [&](double mm, long& out) -> Result<void> {
        const double pulses = mm * pulses_per_mm;
        if (!std::isfinite(pulses) ||
            pulses < static_cast<double>(std::numeric_limits<long>::min()) ||
            pulses > static_cast<double>(std::numeric_limits<long>::max())) {
            return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "回零距离换算溢出", owner));
        }
        out = static_cast<long>(std::llround(pulses));
        return Result<void>::Success();
    };

    auto to_pulse_ul = [&](double mm, unsigned long& out) -> Result<void> {
        const double pulses = mm * pulses_per_mm;
        if (!std::isfinite(pulses) || pulses <= 0.0 ||
            pulses > static_cast<double>(std::numeric_limits<unsigned long>::max())) {
            return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "回零距离换算无效", owner));
        }
        out = static_cast<unsigned long>(std::llround(pulses));
        return Result<void>::Success();
    };

    auto to_pulse_ul_allow_zero = [&](double mm, unsigned long& out) -> Result<void> {
        const double pulses = mm * pulses_per_mm;
        if (!std::isfinite(pulses) || pulses < 0.0 ||
            pulses > static_cast<double>(std::numeric_limits<unsigned long>::max())) {
            return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "回零距离换算无效", owner));
        }
        out = static_cast<unsigned long>(std::llround(pulses));
        return Result<void>::Success();
    };

    long offset_pulse = 0;
    auto offset_result = to_pulse_long(static_cast<double>(config.offset), offset_pulse);
    if (offset_result.IsError()) {
        return Result<PreparedHomeParameters>::Failure(offset_result.GetError());
    }
    home_parameters.lOffset = offset_pulse;

    if (pulse_per_sec_to_ms <= 0.0) {
        return Result<PreparedHomeParameters>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "回零速度换算除数无效", owner));
    }

    const double rapid_vel_ps = unit_converter.VelocityMmSToPS(config.rapid_velocity);
    const double locate_vel_ps = unit_converter.VelocityMmSToPS(config.locate_velocity);
    const double index_vel_ps = unit_converter.VelocityMmSToPS(config.index_velocity);
    if (!std::isfinite(rapid_vel_ps) || !std::isfinite(locate_vel_ps) || !std::isfinite(index_vel_ps)) {
        return Result<PreparedHomeParameters>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "回零速度换算无效", owner));
    }
    home_parameters.dHomeRapidVel = rapid_vel_ps / pulse_per_sec_to_ms;
    home_parameters.dHomeLocatVel = locate_vel_ps / pulse_per_sec_to_ms;
    home_parameters.dHomeIndexVel = index_vel_ps / pulse_per_sec_to_ms;

    const double acc_ps2 = unit_converter.AccelerationMmS2ToPS2(config.acceleration);
    if (!std::isfinite(acc_ps2)) {
        return Result<PreparedHomeParameters>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "回零加速度换算无效", owner));
    }
    home_parameters.dHomeAcc = acc_ps2 / (pulse_per_sec_to_ms * pulse_per_sec_to_ms);

    unsigned long search_distance_pulse = 0;
    auto search_result =
        to_pulse_ul(static_cast<double>(prepared.effective_search_distance_mm), search_distance_pulse);
    if (search_result.IsError()) {
        return Result<PreparedHomeParameters>::Failure(search_result.GetError());
    }
    home_parameters.ulHomeIndexDis = search_distance_pulse;

    unsigned long escape_distance_pulse = 0;
    auto escape_result =
        to_pulse_ul_allow_zero(static_cast<double>(config.escape_distance), escape_distance_pulse);
    if (escape_result.IsError()) {
        return Result<PreparedHomeParameters>::Failure(escape_result.GetError());
    }
    home_parameters.ulHomeBackDis = escape_distance_pulse;
    home_parameters.nDelayTimeBeforeZero = static_cast<short>(config.settle_time_ms);
    home_parameters.ulHomeMaxDis = search_distance_pulse;

    prepared.home_parameters = home_parameters;
    return Result<PreparedHomeParameters>::Success(prepared);
}

template <typename UnitConverterT, typename TriggerStableFn, typename ReadHomeStateFn, typename ErrorTextFn>
Result<void> EscapeFromHomeLimit(int axis,
                                 short axis_num,
                                 const HomingConfig& config,
                                 IMultiCardWrapper* multicard,
                                 const HardwareConfiguration& hardware_config,
                                 const UnitConverterT& unit_converter,
                                 const char* owner,
                                 double pulse_per_sec_to_ms,
                                 TriggerStableFn&& is_home_input_triggered_stable,
                                 ReadHomeStateFn&& read_home_input_state_raw,
                                 ErrorTextFn&& get_error_message) {
    if (!multicard) {
        return Result<void>::Failure(Error(ErrorCode::HARDWARE_NOT_CONNECTED, "MultiCard未初始化", owner));
    }

    if (config.escape_distance <= 0.0f) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "回零退离距离无效", owner));
    }

    const double configured_escape_velocity =
        (config.escape_velocity > 0.0f) ? static_cast<double>(config.escape_velocity)
                                        : static_cast<double>(config.locate_velocity);
    if (configured_escape_velocity <= 0.0) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "回零退离速度无效", owner));
    }

    if (config.direction != 0 && config.direction != 1) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "回零方向配置无效", owner));
    }

    if (config.escape_max_attempts <= 0) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, "回零退离次数配置无效", owner));
    }

    long status = 0;
    unsigned long clock = 0;
    short status_ret = multicard->MC_GetSts(axis_num, &status, 1, &clock);
    if (status_ret == 0) {
        if (status & AXIS_STATUS_ESTOP) {
            return Result<void>::Failure(
                Error(ErrorCode::EMERGENCY_STOP_ACTIVATED, "轴处于急停状态，禁止退离回零开关", owner));
        }
        if (status & AXIS_STATUS_SV_ALARM) {
            return Result<void>::Failure(
                Error(ErrorCode::HARDWARE_ERROR, "轴伺服报警，禁止退离回零开关", owner));
        }
    }

    const int base_direction = (config.direction == 0) ? 1 : -1;
    const double base_distance = static_cast<double>(config.escape_distance);
    const double base_velocity = configured_escape_velocity;
    const int64 configured_timeout_ms =
        (config.escape_timeout_ms > 0) ? static_cast<int64>(config.escape_timeout_ms)
                                       : static_cast<int64>(config.timeout_ms);
    const int64 max_timeout_ms = (configured_timeout_ms > 0) ? configured_timeout_ms : 10000;
    const int home_sample_count = (config.home_debounce_ms > 0) ? 1 : kDefaultHomeSampleCount;

    const double soft_min_mm = hardware_config.soft_limit_negative_mm - kSoftLimitMarginMm;
    const double soft_max_mm = hardware_config.soft_limit_positive_mm + kSoftLimitMarginMm;
    const bool has_soft_limits = soft_max_mm > soft_min_mm;
    const long soft_min_pulse = static_cast<long>(unit_converter.MmToPulse(static_cast<float32>(soft_min_mm)));
    const long soft_max_pulse = static_cast<long>(unit_converter.MmToPulse(static_cast<float32>(soft_max_mm)));
    const long soft_lower_pulse = std::min(soft_min_pulse, soft_max_pulse);
    const long soft_upper_pulse = std::max(soft_min_pulse, soft_max_pulse);

    struct EscapeAttempt {
        int direction;
        double distance_mm;
        double timeout_factor;
    };

    const std::vector<EscapeAttempt> attempts = {
        {base_direction, base_distance, 1.0},
        {base_direction, base_distance * 2.0, 2.0},
        {-base_direction, base_distance, 2.0},
    };

    auto perform_escape = [&](int direction, double distance_mm, double timeout_factor) -> Result<bool> {
        long current_pos = 0;
        short ret = multicard->MC_GetPos(axis_num, &current_pos);
        if (ret != 0) {
            return Result<bool>::Failure(
                Error(ErrorCode::POSITION_QUERY_FAILED,
                      "读取当前位置失败: " + get_error_message(ret),
                      owner));
        }

        long escape_pulse = static_cast<long>(unit_converter.MmToPulse(distance_mm));
        if (escape_pulse <= 0) {
            return Result<bool>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "回零退离距离换算无效", owner));
        }

        long target_pos = current_pos + direction * escape_pulse;
        if (has_soft_limits) {
            double current_mm = unit_converter.PulseToMm(static_cast<int32>(current_pos));
            if (current_mm >= soft_min_mm && current_mm <= soft_max_mm) {
                long original_target = target_pos;
                target_pos = std::clamp(target_pos, soft_lower_pulse, soft_upper_pulse);
                if (target_pos != original_target) {
                    SILIGEN_LOG_WARNING_FMT_HELPER(
                        "Axis %d escape target clamped by soft limits: %ld -> %ld",
                        axis,
                        original_target,
                        target_pos);
                }
                if (target_pos == current_pos) {
                    return Result<bool>::Failure(
                        Error(ErrorCode::POSITION_OUT_OF_RANGE, "退离目标超出软限位，已取消退离", owner));
                }
            }
        }

        ret = multicard->MC_PrfTrap(axis_num);
        if (ret != 0) {
            return Result<bool>::Failure(
                Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                      "设置退离模式失败: " + get_error_message(ret),
                      owner));
        }

        TTrapPrm trap_prm = {};
        double acc_pulse_ms2 =
            unit_converter.AccelerationMmS2ToPS2(config.acceleration) /
            (pulse_per_sec_to_ms * pulse_per_sec_to_ms);
        if (acc_pulse_ms2 <= 0.0) {
            return Result<bool>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "回零退离加速度无效", owner));
        }
        trap_prm.acc = acc_pulse_ms2;
        trap_prm.dec = acc_pulse_ms2;
        trap_prm.smoothTime = 0;
        trap_prm.velStart = 0;

        ret = multicard->MC_SetTrapPrm(axis_num, &trap_prm);
        if (ret != 0) {
            return Result<bool>::Failure(
                Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                      "设置退离参数失败: " + get_error_message(ret),
                      owner));
        }

        ret = multicard->MC_SetPos(axis_num, target_pos);
        if (ret != 0) {
            return Result<bool>::Failure(
                Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                      "设置退离目标位置失败: " + get_error_message(ret),
                      owner));
        }

        double vel_pulse_ms = unit_converter.VelocityMmSToPS(base_velocity) / pulse_per_sec_to_ms;
        if (vel_pulse_ms <= 0.0) {
            return Result<bool>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "回零退离速度换算无效", owner));
        }

        ret = multicard->MC_SetVel(axis_num, std::abs(vel_pulse_ms));
        if (ret != 0) {
            return Result<bool>::Failure(
                Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                      "设置退离速度失败: " + get_error_message(ret),
                      owner));
        }

        ret = multicard->MC_Update(1 << axis);
        if (ret != 0) {
            return Result<bool>::Failure(
                Error(ErrorCode::MOTION_START_FAILED,
                      "启动退离运动失败: " + get_error_message(ret),
                      owner));
        }

        const double expected_ms = (distance_mm / base_velocity) * 1000.0;
        const int64 timeout_ms = std::clamp(
            static_cast<int64>(expected_ms * timeout_factor) + 500,
            static_cast<int64>(500),
            max_timeout_ms);

        auto start_time = std::chrono::steady_clock::now();
        while (true) {
            long moving_status = 0;
            unsigned long moving_clock = 0;
            short moving_status_ret = multicard->MC_GetSts(axis_num, &moving_status, 1, &moving_clock);
            if (moving_status_ret != 0) {
                return Result<bool>::Failure(
                    Error(ErrorCode::HARDWARE_OPERATION_FAILED,
                          "读取退离状态失败: " + get_error_message(moving_status_ret),
                          owner));
            }

            if (moving_status & AXIS_STATUS_ESTOP) {
                return Result<bool>::Failure(
                    Error(ErrorCode::EMERGENCY_STOP_ACTIVATED, "退离过程中检测到急停", owner));
            }
            if (moving_status & AXIS_STATUS_SV_ALARM) {
                return Result<bool>::Failure(
                    Error(ErrorCode::HARDWARE_ERROR, "退离过程中检测到伺服报警", owner));
            }

            const bool is_running = (moving_status & AXIS_STATUS_RUNNING) != 0;
            const bool is_arrived = (moving_status & AXIS_STATUS_ARRIVE) != 0;
            if (!is_running) {
                static_cast<void>(is_arrived);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                auto triggered_result =
                    is_home_input_triggered_stable(axis, home_sample_count, kDefaultHomeSampleIntervalMs);
                if (triggered_result.IsError()) {
                    return Result<bool>::Failure(triggered_result.GetError());
                }
                return Result<bool>::Success(!triggered_result.Value());
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            if (elapsed >= timeout_ms) {
                return Result<bool>::Failure(Error(ErrorCode::TIMEOUT, "退离回零开关超时", owner));
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    };

    const int max_attempts = std::min(static_cast<int>(attempts.size()), config.escape_max_attempts);
    for (int i = 0; i < max_attempts; ++i) {
        const auto& attempt = attempts[i];
        auto attempt_result = perform_escape(attempt.direction, attempt.distance_mm, attempt.timeout_factor);
        if (attempt_result.IsError()) {
            return Result<void>::Failure(attempt_result.GetError());
        }
        if (attempt_result.Value()) {
            return Result<void>::Success();
        }
    }

    std::string message = "退离后回零开关仍触发，已取消回零";
    auto home_state_result = read_home_input_state_raw(axis);
    if (home_state_result.IsSuccess()) {
        const auto& home_state = home_state_result.Value();
        int input_bit = config.home_input_bit < 0 ? axis : config.home_input_bit;
        message += " [src=" + std::to_string(config.home_input_source) +
                   " bit=" + std::to_string(input_bit) +
                   " active_low=" + std::string(config.home_active_low ? "true" : "false") +
                   " debounce_ms=" + std::to_string(config.home_debounce_ms) +
                   " raw=" + std::string(home_state.raw_level ? "1" : "0") +
                   " triggered=" + std::string(home_state.triggered ? "1" : "0") + "]";
    }

    return Result<void>::Failure(Error(ErrorCode::POSITION_OUT_OF_RANGE, message, owner));
}

}  // namespace Siligen::Infrastructure::Adapters::Internal::Homing
