// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "MultiCardMotion"

#include "MultiCardMotionAdapter.h"

#include "shared/interfaces/ILoggingService.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Error.h"

namespace Siligen::Infrastructure::Adapters {

// 单位转换常量: pulse/s → pulse/ms (MultiCard SDK 要求 pulse/ms 单位)
namespace Units {
constexpr double PULSE_PER_SEC_TO_MS = 1000.0;
}

namespace {
// HOME 与硬限位语义分离；未接独立硬限位时，运行态不再把 HOME 伪装为负向硬限位。
constexpr bool kExposeHomeAsHardLimit = false;
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

// === 参数设置实现 ===

Result<void> MultiCardMotionAdapter::SetAxisVelocity(LogicalAxisId axis_id, float32 velocity) {
    auto axis_result = ResolveAxis(axis_id, "SetAxisVelocity");
    if (axis_result.IsError()) {
        return Result<void>(axis_result.GetError());
    }
    const short axis = axis_result.Value();

    auto sdk_axis_result = ResolveSdkAxis(axis_id, "SetAxisVelocity");
    if (sdk_axis_result.IsError()) {
        return Result<void>(sdk_axis_result.GetError());
    }
    short sdk_axis = sdk_axis_result.Value();

    double vel_pulse_ms = unit_converter_.VelocityToPulsePerMs(axis, velocity);
    short ret = hardware_wrapper_->MC_SetVel(sdk_axis, vel_pulse_ms);
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("SetAxisVelocity", axis, ret)));
    }

    return Result<void>();
}

Result<void> MultiCardMotionAdapter::SetAxisAcceleration(LogicalAxisId axis_id, float32 acceleration) {
    auto axis_result = ResolveAxis(axis_id, "SetAxisAcceleration");
    if (axis_result.IsError()) {
        return Result<void>(axis_result.GetError());
    }
    const short axis = axis_result.Value();

    auto sdk_axis_result = ResolveSdkAxis(axis_id, "SetAxisAcceleration");
    if (sdk_axis_result.IsError()) {
        return Result<void>(sdk_axis_result.GetError());
    }
    short sdk_axis = sdk_axis_result.Value();

    double acc_pulse_ms2 = unit_converter_.AccelerationToPulsePerMs2(axis, acceleration);
    // 使用 MC_SetTrapPrm 设置加减速度，起始速度为0，平滑时间为0
    TTrapPrm trap_prm{};
    trap_prm.acc = acc_pulse_ms2;
    trap_prm.dec = acc_pulse_ms2;
    trap_prm.velStart = 0.0;
    trap_prm.smoothTime = 0;
    short ret = hardware_wrapper_->MC_SetTrapPrm(sdk_axis, &trap_prm);

    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("SetAxisAcceleration", axis, ret)));
    }

    return Result<void>();
}

Result<void> MultiCardMotionAdapter::SetSoftLimits(LogicalAxisId axis_id,
                                                   float32 negative_limit,
                                                   float32 positive_limit) {
    auto axis_result = ResolveAxis(axis_id, "SetSoftLimits");
    if (axis_result.IsError()) {
        return Result<void>(axis_result.GetError());
    }
    const short axis = axis_result.Value();

    auto sdk_axis_result = ResolveSdkAxis(axis_id, "SetSoftLimits");
    if (sdk_axis_result.IsError()) {
        return Result<void>(sdk_axis_result.GetError());
    }
    short sdk_axis = sdk_axis_result.Value();

    long neg_pulses = unit_converter_.PositionToPulses(axis, negative_limit);
    long pos_pulses = unit_converter_.PositionToPulses(axis, positive_limit);

    // MC_SetSoftLimit(axis, positive_limit, negative_limit) - 与厂商SDK一致
    short ret = hardware_wrapper_->MC_SetSoftLimit(0, sdk_axis, pos_pulses, neg_pulses);

    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("SetSoftLimits", axis, ret)));
    }

    return Result<void>();
}

// === 硬限位配置实现 ===

Result<void> MultiCardMotionAdapter::SetHardLimits(LogicalAxisId axis_id,
                                                    short positive_io_index,
                                                    short negative_io_index,
                                                    short card_index,
                                                    short signal_type) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                 FormatErrorMessage("SetHardLimits", axis, -1)));
    }

    // 根据厂商文档和示例代码分析：
    // MC_SetHardLimP/N 仅适用于10轴以上控制卡
    // 对于4轴/8轴控制卡，硬件限位信号是固定映射的，不需要手动设置IO映射
    // 官方示例代码 DemoVCDlg.cpp 中只使用 MC_LmtsOn/Off，从不调用 MC_SetHardLimP/N

    short sdk_axis = ToSdkAxis(axis);

    SILIGEN_LOG_DEBUG("SetHardLimits: axis=" + std::to_string(axis) + " -> sdk_axis=" + std::to_string(sdk_axis) +
                      " (4轴控制卡使用固定映射，跳过IO映射设置)");

    // 对于4轴控制卡，硬件限位信号是固定映射的
    // 不需要调用 MC_SetHardLimP/N，直接返回成功
    // 限位功能通过 MC_LmtsOn/Off 控制

    // 存储IO配置供IsHardLimitTriggered使用（保持兼容性）
    limit_configs_[axis] = {positive_io_index, negative_io_index, card_index, true};

    SILIGEN_LOG_DEBUG("SetHardLimits: 配置完成 (使用固定映射)");

    return Result<void>();
}

Result<void> MultiCardMotionAdapter::EnableHardLimits(LogicalAxisId axis_id, bool enable, short limit_type) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                 FormatErrorMessage("EnableHardLimits", axis, -1)));
    }

    // SDK轴号是1-based
    short sdk_axis = ToSdkAxis(axis);

    SILIGEN_LOG_DEBUG("EnableHardLimits: axis=" + std::to_string(axis) + " -> sdk_axis=" + std::to_string(sdk_axis) +
                      ", enable=" + std::string(enable ? "true" : "false") + ", limit_type=" + std::to_string(limit_type));

    short ret;
    if (enable) {
        // 启用限位: MC_LmtsOn(轴号, 限位类型)
        // limit_type: -1=正负向都启用, 0=仅正向, 1=仅负向
        ret = hardware_wrapper_->MC_LmtsOn(sdk_axis, limit_type);
    } else {
        // 禁用限位: MC_LmtsOff(轴号, 限位类型)
        ret = hardware_wrapper_->MC_LmtsOff(sdk_axis, limit_type);
    }

    if (ret != 0) {
        SILIGEN_LOG_ERROR(std::string(enable ? "MC_LmtsOn" : "MC_LmtsOff") + " failed: sdk_axis=" +
                          std::to_string(sdk_axis) + ", ret=" + std::to_string(ret));
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage(enable ? "MC_LmtsOn" : "MC_LmtsOff", axis, ret)));
    }

    SILIGEN_LOG_DEBUG("EnableHardLimits: " + std::string(enable ? "启用" : "禁用") + "成功 (sdk_axis=" + std::to_string(sdk_axis) + ")");

    return Result<void>();
}

Result<void> MultiCardMotionAdapter::SetHardLimitPolarity(LogicalAxisId axis_id,
                                                           short positive_polarity,
                                                           short negative_polarity) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                 FormatErrorMessage("SetHardLimitPolarity", axis, -1)));
    }

    // SDK轴号是1-based (参考厂商示例代码 DemoVCDlg.cpp)
    short sdk_axis = ToSdkAxis(axis);

    SILIGEN_LOG_DEBUG("SetHardLimitPolarity: axis=" + std::to_string(axis) + " -> sdk_axis=" + std::to_string(sdk_axis) +
                      ", pos_polarity=" + std::to_string(positive_polarity) + ", neg_polarity=" + std::to_string(negative_polarity));

    // 设置单轴限位信号极性
    // MC_SetLmtSnsSingle(轴号, 正向极性, 负向极性)
    // 极性: 1=低电平触发(常开), 0=高电平触发(常闭)
    short ret = hardware_wrapper_->MC_SetLmtSnsSingle(sdk_axis, positive_polarity, negative_polarity);
    if (ret != 0) {
        SILIGEN_LOG_ERROR("MC_SetLmtSnsSingle failed: sdk_axis=" + std::to_string(sdk_axis) + ", ret=" + std::to_string(ret));
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("MC_SetLmtSnsSingle", axis, ret)));
    }

    SILIGEN_LOG_DEBUG("SetHardLimitPolarity: 配置成功 (sdk_axis=" + std::to_string(sdk_axis) + ")");

    return Result<void>();
}

Result<bool> MultiCardMotionAdapter::VerifyLimitsEnabled(LogicalAxisId axis_id) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<bool>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                 FormatErrorMessage("VerifyLimitsEnabled", axis, -1)));
    }

    // SDK轴号是1-based (参考厂商示例代码 DemoVCDlg.cpp)
    short sdk_axis = ToSdkAxis(axis);

    // 获取限位启用状态
    short pos_enabled = 0;
    short neg_enabled = 0;
    short ret = hardware_wrapper_->MC_GetLmtsOnOff(sdk_axis, &pos_enabled, &neg_enabled);

    if (ret != 0) {
        SILIGEN_LOG_ERROR("MC_GetLmtsOnOff failed: sdk_axis=" + std::to_string(sdk_axis) + ", ret=" + std::to_string(ret));
        return Result<bool>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("MC_GetLmtsOnOff", axis, ret)));
    }

    SILIGEN_LOG_DEBUG("VerifyLimitsEnabled: axis=" + std::to_string(axis) + " -> sdk_axis=" + std::to_string(sdk_axis) +
                      ", pos_enabled=" + std::to_string(pos_enabled) + ", neg_enabled=" + std::to_string(neg_enabled));

    // 如果正向和负向限位都启用，则返回true
    bool both_enabled = (pos_enabled != 0) && (neg_enabled != 0);
    return Result<bool>(both_enabled);
}

Result<bool> MultiCardMotionAdapter::IsHardLimitTriggered(LogicalAxisId axis_id, bool positive) const {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<bool>(Shared::Types::Error(
            Shared::Types::ErrorCode::INVALID_AXIS,
            FormatErrorMessage("IsHardLimitTriggered", axis, -1)));
    }

    if (!kExposeHomeAsHardLimit) {
        return Result<bool>(false);
    }

    // 兼容旧实现时仅将 HOME 代理为负向限位；默认已禁用该行为。
    if (positive) {
        return Result<bool>(false);
    }

    auto state_result = ReadHomeLimitState();
    if (state_result.IsError()) {
        return Result<bool>(state_result.GetError());
    }

    return Result<bool>(state_result.Value()[axis]);
}

Result<bool> MultiCardMotionAdapter::IsLimitTriggeredRaw(short axis, bool positive) const {
    if (!ValidateAxis(axis)) {
        return Result<bool>(Shared::Types::Error(
            Shared::Types::ErrorCode::INVALID_AXIS,
            FormatErrorMessage("IsLimitTriggeredRaw", axis, -1)));
    }

    if (!kExposeHomeAsHardLimit) {
        return Result<bool>(false);
    }

    // 兼容旧实现时仅将 HOME 代理为负向限位；默认已禁用该行为。
    if (positive) {
        return Result<bool>(false);
    }

    auto state_result = ReadHomeLimitState();
    if (state_result.IsError()) {
        return Result<bool>(state_result.GetError());
    }

    return Result<bool>(state_result.Value()[axis]);
}


}  // namespace Siligen::Infrastructure::Adapters
