// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "MultiCardMotion"

#include "MultiCardMotionAdapter.h"

#include <sstream>
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Error.h"

namespace Siligen::Infrastructure::Adapters {

// 单位转换常量: pulse/s → pulse/ms (MultiCard SDK 要求 pulse/ms 单位)
namespace Units {
constexpr double PULSE_PER_SEC_TO_MS = 1000.0;
}

namespace {
// 当前硬件无硬限位, HOME 仅用于回零
constexpr bool kUseHomeAsHardLimit = false;
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

// === 辅助方法实现 ===

Result<std::array<bool, MultiCardMotionAdapter::kAxisCount>> MultiCardMotionAdapter::ReadHomeLimitState() const {
    long raw = 0;
    short ret = hardware_wrapper_->MC_GetDiRaw(MC_HOME, &raw);
    if (ret != 0) {
        return Result<std::array<bool, kAxisCount>>(Shared::Types::Error(
            Shared::Types::ErrorCode::MOTION_ERROR,
            FormatErrorMessage("MC_GetDiRaw(Home)", 0, ret)));
    }

    // Home输入原始状态读取 (高电平有效, 仅在启用Home作为硬限位时使用)
    constexpr bool kHomeActiveLow = false;
    std::array<bool, kAxisCount> states{};
    for (short axis = 0; axis < kAxisCount; ++axis) {
        bool bit = (raw & (1 << axis)) != 0;
        states[axis] = kHomeActiveLow ? !bit : bit;
    }

    return Result<std::array<bool, kAxisCount>>(states);
}

bool MultiCardMotionAdapter::ValidateAxis(short axis) const {
    return axis >= 0 && axis < kAxisCount;
}

bool MultiCardMotionAdapter::IsEncoderFeedbackEnabled(short axis) const noexcept {
    return hardware_config_.IsEncoderEnabledForAxis(axis);
}

std::string MultiCardMotionAdapter::DescribeSelectedFeedbackSource(short axis) const {
    return IsEncoderFeedbackEnabled(axis) ? "encoder" : "profile";
}

MultiCardMotionAdapter::AxisFeedbackSnapshot MultiCardMotionAdapter::ReadAxisFeedbackSnapshot(short axis,
                                                                                              short sdk_axis) const
    noexcept {
    AxisFeedbackSnapshot snapshot;
    snapshot.encoder_enabled = IsEncoderFeedbackEnabled(axis);

    double prf_pos = 0.0;
    snapshot.profile_position_ret = hardware_wrapper_->MC_GetPrfPos(sdk_axis, &prf_pos);
    if (snapshot.profile_position_ret == 0) {
        snapshot.profile_position_mm = static_cast<float>(unit_converter_.PulsesToPosition(axis, static_cast<long>(prf_pos)));
    }

    double enc_pos = 0.0;
    snapshot.encoder_position_ret = hardware_wrapper_->MC_GetAxisEncPos(sdk_axis, &enc_pos);
    if (snapshot.encoder_position_ret == 0) {
        snapshot.encoder_position_mm = static_cast<float>(unit_converter_.PulsesToPosition(axis, static_cast<long>(enc_pos)));
    }

    double prf_vel = 0.0;
    unsigned long prf_clock = 0;
    snapshot.profile_velocity_ret = hardware_wrapper_->MC_GetAxisPrfVel(sdk_axis, &prf_vel, 1, &prf_clock);
    if (snapshot.profile_velocity_ret == 0) {
        snapshot.profile_velocity_mm_s = static_cast<float>(unit_converter_.PulsePerMsToVelocity(axis, prf_vel));
    }

    double enc_vel = 0.0;
    unsigned long enc_clock = 0;
    snapshot.encoder_velocity_ret = hardware_wrapper_->MC_GetAxisEncVel(sdk_axis, &enc_vel, 1, &enc_clock);
    if (snapshot.encoder_velocity_ret == 0) {
        snapshot.encoder_velocity_mm_s = static_cast<float>(unit_converter_.PulsePerMsToVelocity(axis, enc_vel));
    }

    return snapshot;
}

Result<float32> MultiCardMotionAdapter::ResolveAxisPositionFromSnapshot(short axis,
                                                                        const AxisFeedbackSnapshot& snapshot,
                                                                        const std::string& operation) const {
    if (snapshot.encoder_enabled) {
        if (snapshot.encoder_position_ret != 0) {
            return Result<float32>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                        FormatErrorMessage(operation + "(encoder)", axis,
                                                                           static_cast<short>(snapshot.encoder_position_ret))));
        }
        return Result<float32>(snapshot.encoder_position_mm);
    }

    if (snapshot.profile_position_ret != 0) {
        return Result<float32>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                    FormatErrorMessage(operation + "(profile)", axis,
                                                                       static_cast<short>(snapshot.profile_position_ret))));
    }
    return Result<float32>(snapshot.profile_position_mm);
}

Result<float32> MultiCardMotionAdapter::ResolveAxisVelocityFromSnapshot(short axis,
                                                                        const AxisFeedbackSnapshot& snapshot,
                                                                        const std::string& operation) const {
    if (snapshot.encoder_enabled) {
        if (snapshot.encoder_velocity_ret != 0) {
            return Result<float32>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                        FormatErrorMessage(operation + "(encoder)", axis,
                                                                           static_cast<short>(snapshot.encoder_velocity_ret))));
        }
        return Result<float32>(snapshot.encoder_velocity_mm_s);
    }

    if (snapshot.profile_velocity_ret != 0) {
        return Result<float32>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                    FormatErrorMessage(operation + "(profile)", axis,
                                                                       static_cast<short>(snapshot.profile_velocity_ret))));
    }
    return Result<float32>(snapshot.profile_velocity_mm_s);
}

// 将内部轴号(0-based)转换为SDK轴号(1-based)
// MultiCard SDK 轴号范围是 [1, AXIS_MAX_COUNT]
// 使用类型安全的 AxisTypes.h 转换函数
short MultiCardMotionAdapter::ToSdkAxis(short axis) const {
    return ToSdkShort(Siligen::Shared::Types::ToSdkAxis(FromIndex(axis)));
}


MotionState MultiCardMotionAdapter::MapHardwareState(int32 hardware_state) const {
    // 根据硬件状态码映射到领域状态
    // 这里需要根据实际硬件状态位定义进行映射
    if (hardware_state & AXIS_STATUS_RUNNING) {
        return MotionState::MOVING;
    }
    if (hardware_state & AXIS_STATUS_HOME_SUCESS) {
        return MotionState::HOMED;
    }
    if (hardware_state & AXIS_STATUS_ENABLE) {
        return MotionState::IDLE;
    }
    return MotionState::DISABLED;
}

std::string MultiCardMotionAdapter::FormatErrorMessage(const std::string& operation,
                                                       short axis,
                                                       short error_code) const {
    std::ostringstream oss;
    oss << operation << " failed for axis " << axis;
    if (error_code >= 0) {
        oss << " with error code: " << error_code;
    }
    return oss.str();
}

void MultiCardMotionAdapter::LogAxisSnapshot(const std::string& tag, short axis, short sdk_axis) const {
    if (!hardware_wrapper_) {
        return;
    }

    auto to_hex = [](long value) -> std::string {
        std::ostringstream oss;
        oss << std::hex << value;
        return oss.str();
    };

    long hw_status = 0;
    unsigned long sts_clock = 0;
    short sts_ret = hardware_wrapper_->MC_GetSts(sdk_axis, &hw_status, 1, &sts_clock);

    long cmd_pos = 0;
    short cmd_ret = hardware_wrapper_->MC_GetPos(sdk_axis, &cmd_pos);

    const auto feedback = ReadAxisFeedbackSnapshot(axis, sdk_axis);
    const short prf_pos_ret = static_cast<short>(feedback.profile_position_ret);
    const short enc_pos_ret = static_cast<short>(feedback.encoder_position_ret);
    const short prf_vel_ret = static_cast<short>(feedback.profile_velocity_ret);
    const short enc_vel_ret = static_cast<short>(feedback.encoder_velocity_ret);

    short alarm_status = 0;
    short alarm_ret = hardware_wrapper_->MC_GetAlarmOnOff(sdk_axis, &alarm_status);

    const bool enabled = (hw_status & AXIS_STATUS_ENABLE) != 0;
    const bool running = (hw_status & AXIS_STATUS_RUNNING) != 0;
    const bool arrive = (hw_status & AXIS_STATUS_ARRIVE) != 0;
    const bool estop = (hw_status & AXIS_STATUS_ESTOP) != 0;
    const bool soft_pos = (hw_status & AXIS_STATUS_POS_SOFT_LIMIT) != 0;
    const bool soft_neg = (hw_status & AXIS_STATUS_NEG_SOFT_LIMIT) != 0;
    const bool follow_err = (hw_status & AXIS_STATUS_FOLLOW_ERR) != 0;

    const double pulses_per_mm = unit_converter_.GetPulsesPerMm(axis);
    const double cmd_mm = pulses_per_mm > 0.0 ? static_cast<double>(cmd_pos) / pulses_per_mm : 0.0;
    const double prf_mm = feedback.profile_position_mm;
    const double enc_mm = feedback.encoder_position_mm;
    const double prf_vel_mm_s = feedback.profile_velocity_mm_s;
    const double enc_vel_mm_s = feedback.encoder_velocity_mm_s;

    SILIGEN_LOG_INFO("Diag " + tag +
                     " axis=" + std::to_string(axis) +
                     " sdk=" + std::to_string(sdk_axis) +
                     " sts_ret=" + std::to_string(sts_ret) +
                     " status=0x" + to_hex(hw_status) +
                     " enabled=" + std::to_string(enabled) +
                     " running=" + std::to_string(running) +
                     " arrive=" + std::to_string(arrive) +
                     " estop=" + std::to_string(estop) +
                     " soft+=" + std::to_string(soft_pos) +
                     " soft-=" + std::to_string(soft_neg) +
                     " follow=" + std::to_string(follow_err) +
                     " alarm_ret=" + std::to_string(alarm_ret) +
                     " alarm=" + std::to_string(alarm_status));

    SILIGEN_LOG_INFO("Diag " + tag +
                     " axis=" + std::to_string(axis) +
                     " cmd_pos=" + std::to_string(cmd_pos) +
                     " cmd_mm=" + std::to_string(cmd_mm) +
                     " prf_mm=" + std::to_string(prf_mm) +
                     " enc_mm=" + std::to_string(enc_mm) +
                     " prf_vel_mm_s=" + std::to_string(prf_vel_mm_s) +
                     " enc_vel_mm_s=" + std::to_string(enc_vel_mm_s) +
                     " selected=" + DescribeSelectedFeedbackSource(axis) +
                     " ret(cmd=" + std::to_string(cmd_ret) +
                     " prf_pos=" + std::to_string(prf_pos_ret) +
                     " enc_pos=" + std::to_string(enc_pos_ret) +
                     " prf_vel=" + std::to_string(prf_vel_ret) +
                     " enc_vel=" + std::to_string(enc_vel_ret) +
                     ") clk(sts=" + std::to_string(sts_clock) +
                     " prf=0 enc=0)");

    if (diagnostics_config_.deep_hardware_logging) {
        long di_gpi = 0;
        long di_home = 0;
        short di_gpi_ret = hardware_wrapper_->MC_GetDiRaw(MC_GPI, &di_gpi);
        short di_home_ret = hardware_wrapper_->MC_GetDiRaw(MC_HOME, &di_home);
        SILIGEN_LOG_INFO("Diag " + tag +
                         " io axis=" + std::to_string(axis) +
                         " di_gpi_ret=" + std::to_string(di_gpi_ret) +
                         " di_gpi=0x" + to_hex(di_gpi) +
                         " di_home_ret=" + std::to_string(di_home_ret) +
                         " di_home=0x" + to_hex(di_home));
    }
}


}  // namespace Siligen::Infrastructure::Adapters
