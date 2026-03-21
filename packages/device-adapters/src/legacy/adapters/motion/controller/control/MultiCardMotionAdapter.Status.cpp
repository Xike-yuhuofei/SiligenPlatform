// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "MultiCardMotion"

#include "MultiCardMotionAdapter.h"

#include "shared/interfaces/ILoggingService.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Error.h"

namespace Siligen::Infrastructure::Adapters {

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

// === 状态查询实现 ===

Result<Point2D> MultiCardMotionAdapter::GetCurrentPosition() const {
    auto x_result = GetAxisPosition(LogicalAxisId::X);
    if (x_result.IsError()) {
        return Result<Point2D>(x_result.GetError());
    }

    auto y_result = GetAxisPosition(LogicalAxisId::Y);
    if (y_result.IsError()) {
        return Result<Point2D>(y_result.GetError());
    }

    return Result<Point2D>(Point2D(x_result.Value(), y_result.Value()));
}

Result<float32> MultiCardMotionAdapter::GetAxisPosition(LogicalAxisId axis_id) const {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<float32>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                    FormatErrorMessage("GetAxisPosition", axis, -1)));
    }

    // 转换为SDK轴号(1-based)
    short sdk_axis = ToSdkAxis(axis);
    const auto snapshot = ReadAxisFeedbackSnapshot(axis, sdk_axis);
    return ResolveAxisPositionFromSnapshot(axis, snapshot, "GetAxisPosition");
}

Result<float32> MultiCardMotionAdapter::GetAxisVelocity(LogicalAxisId axis_id) const {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<float32>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                    FormatErrorMessage("GetAxisVelocity", axis, -1)));
    }

    // 转换为SDK轴号(1-based)
    short sdk_axis = ToSdkAxis(axis);
    const auto snapshot = ReadAxisFeedbackSnapshot(axis, sdk_axis);
    return ResolveAxisVelocityFromSnapshot(axis, snapshot, "GetAxisVelocity");
}

Result<MotionStatus> MultiCardMotionAdapter::GetAxisStatus(LogicalAxisId axis_id) const {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<MotionStatus>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                         FormatErrorMessage("GetAxisStatus", axis, -1)));
    }

    MotionStatus status;
    status.state = MotionState::IDLE;

    // 获取硬件状态寄存器
    short sdk_axis = ToSdkAxis(axis);
    long hw_status = 0;
    short ret = hardware_wrapper_->MC_GetSts(sdk_axis, &hw_status);

    // 始终输出硬件状态（用于诊断）
    SILIGEN_LOG_DEBUG("GetAxisStatus: axis=" + std::to_string(axis) + " sdk_axis=" + std::to_string(sdk_axis) +
                      " MC_GetSts_ret=" + std::to_string(ret) + " hw_status=0x" + std::to_string(hw_status));

    if (ret == 0) {
        const bool estop = (hw_status & AXIS_STATUS_ESTOP) != 0;
        const bool homing = (hw_status & AXIS_STATUS_HOME_RUNNING) != 0;
        const bool homed = (hw_status & AXIS_STATUS_HOME_SUCESS) != 0;
        const bool home_failed = (hw_status & AXIS_STATUS_HOME_FAIL) != 0;
        const bool running = (hw_status & AXIS_STATUS_RUNNING) != 0;

        status.enabled = (hw_status & AXIS_STATUS_ENABLE) != 0;
        status.in_position = (hw_status & AXIS_STATUS_ARRIVE) != 0;
        status.servo_alarm = (hw_status & AXIS_STATUS_SV_ALARM) != 0;
        status.following_error = (hw_status & AXIS_STATUS_FOLLOW_ERR) != 0;
        status.home_signal = (hw_status & AXIS_STATUS_HOME_SWITCH) != 0;
        status.index_signal = (hw_status & AXIS_STATUS_INDEX) != 0;

        if (estop) {
            status.state = MotionState::ESTOP;
        } else if (home_failed || status.servo_alarm || status.following_error) {
            status.state = MotionState::FAULT;
            status.has_error = true;
        } else if (homing) {
            status.state = MotionState::HOMING;
        } else if (homed) {
            status.state = MotionState::HOMED;
        } else if (running) {
            status.state = MotionState::MOVING;
        } else if (!status.enabled) {
            status.state = MotionState::DISABLED;
        } else {
            status.state = MotionState::IDLE;
        }

        SILIGEN_LOG_DEBUG("GetAxisStatus: axis=" + std::to_string(axis) +
                          " enabled=" + std::to_string(status.enabled) +
                          " in_position=" + std::to_string(status.in_position) +
                          " state=" + std::string((status.state == MotionState::MOVING) ? "MOVING" :
                                                  (status.state == MotionState::HOMED) ? "HOMED" :
                                                  (status.state == MotionState::HOMING) ? "HOMING" :
                                                  (status.state == MotionState::DISABLED) ? "DISABLED" :
                                                  (status.state == MotionState::ESTOP) ? "ESTOP" :
                                                  (status.state == MotionState::FAULT) ? "FAULT" : "IDLE"));
    } else {
        status.has_error = true;
        status.error_code = ret;
    }

    const auto feedback = ReadAxisFeedbackSnapshot(axis, sdk_axis);
    status.selected_feedback_source = DescribeSelectedFeedbackSource(axis);
    status.profile_position_mm = feedback.profile_position_mm;
    status.encoder_position_mm = feedback.encoder_position_mm;
    status.profile_velocity_mm_s = feedback.profile_velocity_mm_s;
    status.encoder_velocity_mm_s = feedback.encoder_velocity_mm_s;
    status.profile_position_ret = feedback.profile_position_ret;
    status.encoder_position_ret = feedback.encoder_position_ret;
    status.profile_velocity_ret = feedback.profile_velocity_ret;
    status.encoder_velocity_ret = feedback.encoder_velocity_ret;

    // 获取位置
    auto pos_result = ResolveAxisPositionFromSnapshot(axis, feedback, "GetAxisStatus.position");
    if (pos_result.IsError()) {
        return Result<MotionStatus>(pos_result.GetError());
    }
    status.axis_position_mm = pos_result.Value();
    status.position = Point2D(pos_result.Value(), 0.0f);

    // 获取速度
    auto vel_result = ResolveAxisVelocityFromSnapshot(axis, feedback, "GetAxisStatus.velocity");
    if (vel_result.IsSuccess()) {
        status.velocity = vel_result.Value();
    }

    // 检查硬件限位状态
    auto pos_limit_result = IsHardLimitTriggered(axis_id, true);
    if (pos_limit_result.IsSuccess()) {
        status.hard_limit_positive = pos_limit_result.Value();
    } else {
        // 如果读取失败，记录错误但不影响其他状态
        SILIGEN_LOG_WARNING("Failed to check positive limit for axis " + std::to_string(axis) +
                            ": " + pos_limit_result.GetError().GetMessage());
    }

    auto neg_limit_result = IsHardLimitTriggered(axis_id, false);
    if (neg_limit_result.IsSuccess()) {
        status.hard_limit_negative = neg_limit_result.Value();
    }

    SILIGEN_LOG_DEBUG("GetAxisStatus EXIT: returning status for axis " + std::to_string(axis));

    return Result<MotionStatus>(status);
}

Result<bool> MultiCardMotionAdapter::IsAxisMoving(LogicalAxisId axis_id) const {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<bool>(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS, FormatErrorMessage("IsAxisMoving", axis, -1)));
    }

    // 转换为SDK轴号(1-based)
    short sdk_axis = ToSdkAxis(axis);

    long status = 0;
    short ret = hardware_wrapper_->MC_GetSts(sdk_axis, &status);

    if (ret != 0) {
        return Result<bool>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("IsAxisMoving", axis, ret)));
    }

    bool is_running = (status & AXIS_STATUS_RUNNING) != 0;
    return Result<bool>(is_running);
}

Result<bool> MultiCardMotionAdapter::IsAxisInPosition(LogicalAxisId axis_id) const {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<bool>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                 FormatErrorMessage("IsAxisInPosition", axis, -1)));
    }

    // 转换为SDK轴号(1-based)
    short sdk_axis = ToSdkAxis(axis);

    long status = 0;
    short ret = hardware_wrapper_->MC_GetSts(sdk_axis, &status);

    if (ret != 0) {
        return Result<bool>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("IsAxisInPosition", axis, ret)));
    }

    // 检查到位状态位
    bool in_position = (status & AXIS_STATUS_ARRIVE) != 0;
    return Result<bool>(in_position);
}

Result<std::vector<Siligen::Domain::Motion::Ports::MotionStatus>> MultiCardMotionAdapter::GetAllAxesStatus() const {
    std::vector<Siligen::Domain::Motion::Ports::MotionStatus> all_status;
    all_status.reserve(kAxisCount);

    // 获取所有轴的状态
    for (short axis = 0; axis < kAxisCount; ++axis) {
        auto result = GetAxisStatus(FromIndex(axis));
        if (result.IsError()) {
            // 如果任何一个轴的状态查询失败，返回错误
            return Result<std::vector<Siligen::Domain::Motion::Ports::MotionStatus>>(
                Shared::Types::Error(
                    Shared::Types::ErrorCode::HARDWARE_ERROR,
                    FormatErrorMessage("GetAllAxesStatus", axis, -1) + ": " + result.GetError().GetMessage()
                )
            );
        }
        all_status.push_back(result.Value());
    }

    return Result<std::vector<Siligen::Domain::Motion::Ports::MotionStatus>>(all_status);
}

Result<bool> MultiCardMotionAdapter::ReadLimitStatus(LogicalAxisId axis_id, bool positive) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    return IsLimitTriggeredRaw(axis, positive);
}


}  // namespace Siligen::Infrastructure::Adapters
