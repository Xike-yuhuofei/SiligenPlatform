// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "InterpolationAdapter"

#include "InterpolationAdapter.h"
#include "siligen/device/adapters/drivers/multicard/error_mapper.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/AxisTypes.h"

#include <chrono>
#include <cmath>
#include <thread>

namespace {
constexpr int kCrdStatusProgRun = 0x00000001;
constexpr int kCrdStatusProgEstop = 0x00000004;
constexpr int kCrdStatusFifoFinish0 = 0x00000010;
constexpr int kCrdStatusAlarm = 0x00000040;
constexpr float kCoordinateVelocityIdleToleranceMmS = 0.001f;
constexpr int kRetriableExecutionFailedErrorCode = 1;
constexpr int kConfigureCrdClearMaxAttempts = 20;
constexpr auto kConfigureCrdClearRetryDelay = std::chrono::milliseconds(10);

struct CrdDiagnosticsSnapshot {
    short status = 0;
    long segment = 0;
    long fifo_space = 0;
    long lookahead_space = 0;
    int status_result = -1;
    int fifo_result = -1;
    int lookahead_result = -1;
};

CrdDiagnosticsSnapshot QueryCrdDiagnostics(
    const std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper>& wrapper,
    int16 coord_sys) {
    CrdDiagnosticsSnapshot snapshot;
    if (!wrapper) {
        return snapshot;
    }

    snapshot.status_result = wrapper->MC_CrdStatus(coord_sys, &snapshot.status, &snapshot.segment, 0);
    snapshot.fifo_result = wrapper->MC_CrdSpace(coord_sys, &snapshot.fifo_space, 0);
    snapshot.lookahead_result = wrapper->MC_GetLookAheadSpace(coord_sys, &snapshot.lookahead_space, 0);
    return snapshot;
}

std::string FormatCrdDiagnostics(int16 coord_sys, const CrdDiagnosticsSnapshot& snapshot) {
    return "coord_sys=" + std::to_string(coord_sys) +
           ", status=" + std::to_string(snapshot.status) +
           ", segment=" + std::to_string(snapshot.segment) +
           ", fifo_space=" + std::to_string(snapshot.fifo_space) +
           ", lookahead_space=" + std::to_string(snapshot.lookahead_space) +
           ", status_ret=" + std::to_string(snapshot.status_result) +
           ", fifo_ret=" + std::to_string(snapshot.fifo_result) +
           ", lookahead_ret=" + std::to_string(snapshot.lookahead_result);
}

void LogCrdDiagnostics(const std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper>& wrapper,
                       int16 coord_sys,
                       const std::string& tag) {
    if (!wrapper) {
        SILIGEN_LOG_WARNING("CrdDiag[" + tag + "]: wrapper为空");
        return;
    }

    const auto snapshot = QueryCrdDiagnostics(wrapper, coord_sys);
    SILIGEN_LOG_DEBUG("CrdDiag[" + tag + "]: " + FormatCrdDiagnostics(coord_sys, snapshot));
}

long BuildCoordinateMask(int16 coord_sys) {
    if (coord_sys <= 0 || coord_sys > 32) {
        return 0;
    }
    return 1L << (coord_sys - 1);
}

int RetryConfigureCrdClear(const std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper>& wrapper,
                           int16 coord_sys,
                           const std::string& phase) {
    if (!wrapper) {
        return -1;
    }

    int error_code = wrapper->MC_CrdClear(coord_sys, 0);
    if (error_code == 0) {
        return 0;
    }
    if (error_code != kRetriableExecutionFailedErrorCode) {
        return error_code;
    }

    const long crd_mask = BuildCoordinateMask(coord_sys);
    for (int attempt = 2; attempt <= kConfigureCrdClearMaxAttempts; ++attempt) {
        SILIGEN_LOG_WARNING("MC_CrdClear returned ExecutionFailed, retrying: phase=" + phase +
                            ", coord_sys=" + std::to_string(coord_sys) +
                            ", attempt=" + std::to_string(attempt));
        LogCrdDiagnostics(wrapper, coord_sys, phase + ":retry:" + std::to_string(attempt));

        if (crd_mask != 0) {
            const int stop_result = wrapper->MC_StopEx(crd_mask, 0);
            if (stop_result != 0) {
                SILIGEN_LOG_WARNING("MC_StopEx during CrdClear retry returned: " + std::to_string(stop_result));
            }
        }

        std::this_thread::sleep_for(kConfigureCrdClearRetryDelay);

        error_code = wrapper->MC_CrdClear(coord_sys, 0);
        if (error_code == 0) {
            SILIGEN_LOG_INFO("MC_CrdClear recovered after retry: phase=" + phase +
                             ", coord_sys=" + std::to_string(coord_sys) +
                             ", attempt=" + std::to_string(attempt));
            return 0;
        }
        if (error_code != kRetriableExecutionFailedErrorCode) {
            return error_code;
        }
    }

    return error_code;
}
}

namespace Siligen::Infrastructure::Adapters {

InterpolationAdapter::InterpolationAdapter(std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> wrapper)
    : wrapper_(wrapper) {}

Result<void> InterpolationAdapter::ConfigureCoordinateSystem(int16 coord_sys,
                                                             const Siligen::Domain::Motion::Ports::CoordinateSystemConfig& config) {
    SILIGEN_LOG_DEBUG(std::string("ConfigureCoordinateSystem ========== 开始 =========="));
    SILIGEN_LOG_INFO("ConfigureCoordinateSystem: coord_sys=" + std::to_string(coord_sys) +
                     ", dimension=" + std::to_string(config.dimension) +
                     ", max_velocity=" + std::to_string(config.max_velocity) +
                     ", max_acceleration=" + std::to_string(config.max_acceleration));
    SILIGEN_LOG_DEBUG("coord_sys = " + std::to_string(coord_sys));
    SILIGEN_LOG_DEBUG("dimension = " + std::to_string(config.dimension));
    SILIGEN_LOG_DEBUG("max_velocity = " + std::to_string(config.max_velocity) + " mm/s");
    SILIGEN_LOG_DEBUG("max_acceleration = " + std::to_string(config.max_acceleration) + " mm/s^2");

    // 1. 设置坐标系参数（轴映射、最大速度、最大加速度）
    // 构建轴映射数组：axis_map[0] = X轴号, axis_map[1] = Y轴号
    short profile[8] = {0};
    for (size_t i = 0; i < config.axis_map.size() && i < 8; ++i) {
        const auto axis_id = config.axis_map[i];
        profile[i] = Siligen::Shared::Types::ToSdkShort(Siligen::Shared::Types::ToSdkAxis(axis_id));
        SILIGEN_LOG_DEBUG("axis_map[" + std::to_string(i) + "] = " + std::to_string(profile[i]));
    }

    // 转换速度和加速度为脉冲单位
    // 重要：MultiCard API 使用毫秒为时间单位！
    // - 速度单位: 脉冲/ms (不是 脉冲/s)
    // - 加速度单位: 脉冲/ms² (不是 脉冲/s²)
    double synVelMax = config.max_velocity * PULSE_PER_MM / 1000.0;        // mm/s → 脉冲/ms
    double synAccMax = config.max_acceleration * PULSE_PER_MM / 1000000.0; // mm/s² → 脉冲/ms²

    SILIGEN_LOG_DEBUG("synVelMax (脉冲/ms) = " + std::to_string(synVelMax));
    SILIGEN_LOG_DEBUG("synAccMax (脉冲/ms^2) = " + std::to_string(synAccMax));

    // 0. 尝试停止并清空历史坐标系（防止上一次执行残留导致SetCrdPrm失败）
    if (coord_sys > 0 && coord_sys <= 32) {
        const long crd_mask = 1L << (coord_sys - 1);
        int stop_result = wrapper_->MC_StopEx(crd_mask, 0);
        if (stop_result != 0) {
            SILIGEN_LOG_WARNING("MC_StopEx 返回: " + std::to_string(stop_result));
        } else {
            SILIGEN_LOG_DEBUG("MC_StopEx 返回: 0 (成功)");
        }
    }

    int pre_clear_result = RetryConfigureCrdClear(wrapper_, coord_sys, "ConfigureCoordinateSystem: pre_clear");
    if (pre_clear_result != 0) {
        SILIGEN_LOG_WARNING("预清空 MC_CrdClear 返回: " + std::to_string(pre_clear_result));
    } else {
        SILIGEN_LOG_DEBUG("预清空 MC_CrdClear 返回: 0 (成功)");
    }

    int error_code = wrapper_->MC_SetCrdPrm(coord_sys, config.dimension, profile, synVelMax, synAccMax);
    if (error_code != 0) {
        SILIGEN_LOG_ERROR("MC_SetCrdPrm 返回: " + std::to_string(error_code));
        return ConvertError(error_code, "ConfigureCoordinateSystem: SetCrdPrm failed");
    }
    SILIGEN_LOG_DEBUG("MC_SetCrdPrm 返回: 0 (成功)");

    // 2. 清空坐标系缓冲区（FIFO 0）
    error_code = RetryConfigureCrdClear(wrapper_, coord_sys, "ConfigureCoordinateSystem: post_set_prm_clear");
    if (error_code != 0) {
        SILIGEN_LOG_ERROR("MC_CrdClear 返回: " + std::to_string(error_code));
        return ConvertError(error_code, "ConfigureCoordinateSystem: CrdClear failed");
    }
    SILIGEN_LOG_DEBUG("MC_CrdClear 返回: 0 (成功)");

    // 3. 初始化前瞻缓冲区（必须在 MC_SetCrdPrm 之后、MC_CrdStart 之前调用）
    // 前瞻段数设为 200，适合点胶路径规划的轨迹段数量
    constexpr int LOOK_AHEAD_NUM = 200;

    // 构建各轴参数数组（最大速度、加速度、启动速度、脉冲当量）
    double speedMax[8] = {0};
    double accMax[8] = {0};
    double maxStepSpeed[8] = {0};
    double scale[8] = {0};

    // 为配置的轴设置参数
    for (size_t i = 0; i < config.axis_map.size() && i < 8; ++i) {
        speedMax[i] = synVelMax;      // 各轴最大速度（脉冲/ms）
        accMax[i] = synAccMax;        // 各轴最大加速度（脉冲/ms^2）
        maxStepSpeed[i] = synVelMax * 0.1;  // 启动速度设为最大速度的 10%
        scale[i] = 1.0;               // 脉冲当量（1:1 映射）
    }

    SILIGEN_LOG_DEBUG("调用 MC_InitLookAhead (前瞻段数=" + std::to_string(LOOK_AHEAD_NUM) + ")");
    error_code = wrapper_->MC_InitLookAhead(coord_sys, 0, LOOK_AHEAD_NUM,
                                             speedMax, accMax, maxStepSpeed, scale);
    if (error_code != 0) {
        SILIGEN_LOG_ERROR("MC_InitLookAhead 返回: " + std::to_string(error_code));
        return ConvertError(error_code, "ConfigureCoordinateSystem: InitLookAhead failed");
    }
    SILIGEN_LOG_DEBUG("MC_InitLookAhead 返回: 0 (成功)");

    SILIGEN_LOG_DEBUG("坐标系配置完成");
    SILIGEN_LOG_INFO("ConfigureCoordinateSystem: 完成 (coord_sys=" + std::to_string(coord_sys) + ")");

    return Result<void>::Success();
}

Result<void> InterpolationAdapter::AddInterpolationData(int16 coord_sys,
                                                        const Siligen::Domain::Motion::Ports::InterpolationData& data) {
    int error_code = 0;

    SILIGEN_LOG_DEBUG(std::string("AddInterpolationData ========== 开始 =========="));
    LogCrdDiagnostics(wrapper_, coord_sys, "Add:before");
    SILIGEN_LOG_DEBUG("coord_sys = " + std::to_string(coord_sys));
    SILIGEN_LOG_DEBUG("type = " + std::to_string(static_cast<int>(data.type)) + " (0=LINEAR, 1=CIRCULAR_CW, 2=CIRCULAR_CCW)");
    SILIGEN_LOG_DEBUG("positions.size() = " + std::to_string(data.positions.size()));

    // 转换物理单位到脉冲
    // 重要：MultiCard API 使用毫秒为时间单位！
    long x_pulse = static_cast<long>(data.positions[0] * PULSE_PER_MM);
    long y_pulse = static_cast<long>(data.positions[1] * PULSE_PER_MM);
    double vel_pulse = data.velocity * PULSE_PER_MM / 1000.0;         // mm/s → 脉冲/ms
    double acc_pulse = data.acceleration * PULSE_PER_MM / 1000000.0;  // mm/s² → 脉冲/ms²
    double end_vel_pulse = data.end_velocity * PULSE_PER_MM / 1000.0; // mm/s → 脉冲/ms

    SILIGEN_LOG_DEBUG("输入参数 (mm): X=" + std::to_string(data.positions[0]) + ", Y=" + std::to_string(data.positions[1]) +
                      ", vel=" + std::to_string(data.velocity) + " mm/s, acc=" + std::to_string(data.acceleration) + " mm/s^2");
    SILIGEN_LOG_DEBUG("转换脉冲: X=" + std::to_string(x_pulse) + ", Y=" + std::to_string(y_pulse) +
                      ", vel=" + std::to_string(vel_pulse) + " 脉冲/ms, acc=" + std::to_string(acc_pulse) + " 脉冲/ms^2");

    switch (data.type) {
        case Siligen::Domain::Motion::Ports::InterpolationType::LINEAR:
            // 直线插补
            SILIGEN_LOG_DEBUG("调用 MC_LnXY(coord=" + std::to_string(coord_sys) + ", X=" + std::to_string(x_pulse) +
                              ", Y=" + std::to_string(y_pulse) + ", vel=" + std::to_string(vel_pulse) +
                              ", acc=" + std::to_string(acc_pulse) + ", endVel=" + std::to_string(end_vel_pulse) + ", fifo=0)");
            error_code = wrapper_->MC_LnXY(coord_sys, x_pulse, y_pulse,
                                           vel_pulse, acc_pulse, end_vel_pulse,
                                           0);  // FIFO 0
            SILIGEN_LOG_DEBUG("MC_LnXY 返回: " + std::to_string(error_code));
            if (error_code != 0) {
                SILIGEN_LOG_ERROR("MC_LnXY 失败!");
                LogCrdDiagnostics(wrapper_, coord_sys, "Add:linear_failed");
                return ConvertError(error_code, "AddInterpolationData: Linear interpolation failed");
            }
            break;

        case Siligen::Domain::Motion::Ports::InterpolationType::CIRCULAR_CW:
        case Siligen::Domain::Motion::Ports::InterpolationType::CIRCULAR_CCW: {
            // 圆弧插补
            double cx_pulse = data.center_x * PULSE_PER_MM;
            double cy_pulse = data.center_y * PULSE_PER_MM;
            short direction = (data.type == Siligen::Domain::Motion::Ports::InterpolationType::CIRCULAR_CW) ? 1 : 2;

            SILIGEN_LOG_DEBUG("调用 MC_ArcXYC(coord=" + std::to_string(coord_sys) + ", X=" + std::to_string(x_pulse) +
                              ", Y=" + std::to_string(y_pulse) + ", CX=" + std::to_string(cx_pulse) +
                              ", CY=" + std::to_string(cy_pulse) + ", dir=" + std::to_string(direction) + ")");
            error_code = wrapper_->MC_ArcXYC(coord_sys, x_pulse, y_pulse,
                                            cx_pulse, cy_pulse, direction,
                                            vel_pulse, acc_pulse, end_vel_pulse,
                                            0);  // FIFO 0
            SILIGEN_LOG_DEBUG("MC_ArcXYC 返回: " + std::to_string(error_code));
            if (error_code != 0) {
                SILIGEN_LOG_ERROR("MC_ArcXYC 失败!");
                LogCrdDiagnostics(wrapper_, coord_sys, "Add:arc_failed");
                return ConvertError(error_code, "AddInterpolationData: Arc interpolation failed");
            }
            break;
        }

        case Siligen::Domain::Motion::Ports::InterpolationType::SPIRAL:
            // MVP不支持螺旋插补
            SILIGEN_LOG_ERROR("不支持螺旋插补");
            return Result<void>::Failure(Error(ErrorCode::NOT_IMPLEMENTED,
                                              "Spiral interpolation not supported in MVP"));

        case Siligen::Domain::Motion::Ports::InterpolationType::BUFFER_STOP:
            // 缓冲区停止（暂不实现）
            SILIGEN_LOG_ERROR("不支持缓冲区停止");
            return Result<void>::Failure(Error(ErrorCode::NOT_IMPLEMENTED,
                                              "Buffer stop not supported in MVP"));

        default:
            SILIGEN_LOG_ERROR("未知插补类型: " + std::to_string(static_cast<int>(data.type)));
            return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER,
                                              "Unknown interpolation type"));
    }

    LogCrdDiagnostics(wrapper_, coord_sys, "Add:after");
    SILIGEN_LOG_INFO("AddInterpolationData: coord_sys=" + std::to_string(coord_sys) +
                     ", type=" + std::to_string(static_cast<int>(data.type)) +
                     ", target=(" + std::to_string(data.positions[0]) +
                     "," + std::to_string(data.positions[1]) + "), vel=" +
                     std::to_string(data.velocity) + ", acc=" + std::to_string(data.acceleration));
    SILIGEN_LOG_DEBUG("插补数据添加成功");
    return Result<void>::Success();
}

Result<void> InterpolationAdapter::ClearInterpolationBuffer(int16 coord_sys) {
    SILIGEN_LOG_DEBUG("ClearInterpolationBuffer called (coord_sys=" + std::to_string(coord_sys) + ")");
    LogCrdDiagnostics(wrapper_, coord_sys, "Clear:before");

    int error_code = wrapper_->MC_CrdClear(coord_sys, 0);
    if (error_code != 0) {
        SILIGEN_LOG_ERROR("MC_CrdClear 返回: " + std::to_string(error_code));
        return ConvertError(error_code, "ClearInterpolationBuffer: CrdClear failed");
    }

    LogCrdDiagnostics(wrapper_, coord_sys, "Clear:after");
    return Result<void>::Success();
}

Result<void> InterpolationAdapter::FlushInterpolationData(int16 coord_sys) {
    SILIGEN_LOG_DEBUG("FlushInterpolationData called (coord_sys=" + std::to_string(coord_sys) + ")");
    LogCrdDiagnostics(wrapper_, coord_sys, "Flush:before");

    int error_code = wrapper_->MC_CrdData(coord_sys, nullptr, 0);
    if (error_code != 0) {
        SILIGEN_LOG_ERROR("MC_CrdData 返回: " + std::to_string(error_code));
        return ConvertError(error_code, "FlushInterpolationData: CrdData failed");
    }

    const auto flush_snapshot = QueryCrdDiagnostics(wrapper_, coord_sys);
    SILIGEN_LOG_INFO("FlushInterpolationData: data_ret=" + std::to_string(error_code) +
                     ", " + FormatCrdDiagnostics(coord_sys, flush_snapshot));
    LogCrdDiagnostics(wrapper_, coord_sys, "Flush:after");
    return Result<void>::Success();
}

Result<void> InterpolationAdapter::StartCoordinateSystemMotion(uint32 coord_sys_mask) {
    // 根据 mask 计算坐标系编号（假设 mask 的每个 bit 代表一个坐标系）
    // MVP 简化：只支持坐标系 1（mask = 0x01）
    // 找到第一个置位的 bit
    int16 coord_sys = 1;
    for (int i = 0; i < 16; ++i) {
        if (coord_sys_mask & (1 << i)) {
            coord_sys = static_cast<int16>(i + 1);
            break;
        }
    }

    // 启动前检查坐标系状态
    const auto before_snapshot = QueryCrdDiagnostics(wrapper_, coord_sys);
    SILIGEN_LOG_DEBUG("Before CrdStart: " + FormatCrdDiagnostics(coord_sys, before_snapshot));
    LogCrdDiagnostics(wrapper_, coord_sys, "Start:before_data");

    // 发送数据流结束标识，确保前瞻缓冲区数据下发到板卡
    int data_result = wrapper_->MC_CrdData(coord_sys, nullptr, 0);
    if (data_result != 0) {
        SILIGEN_LOG_ERROR("MC_CrdData 返回: " + std::to_string(data_result));
        return ConvertError(data_result, "StartCoordinateSystemMotion: CrdData failed");
    }
    SILIGEN_LOG_DEBUG("MC_CrdData 返回: 0 (成功)");

    // 启动坐标系运动（mask 参数：轴掩码，假设为0表示使用默认配置）
    int error_code = wrapper_->MC_CrdStart(coord_sys, 0);

    // 启动后检查坐标系状态
    const auto after_snapshot = QueryCrdDiagnostics(wrapper_, coord_sys);
    SILIGEN_LOG_DEBUG("After CrdStart: error=" + std::to_string(error_code) +
                      ", " + FormatCrdDiagnostics(coord_sys, after_snapshot));
    LogCrdDiagnostics(wrapper_, coord_sys, "Start:after_start");
    SILIGEN_LOG_INFO("CrdStart: coord_sys=" + std::to_string(coord_sys) +
                     ", coord_sys_mask=" + std::to_string(coord_sys_mask) +
                     ", crd_data_ret=" + std::to_string(data_result) +
                     ", crd_start_ret=" + std::to_string(error_code) +
                     ", before={" + FormatCrdDiagnostics(coord_sys, before_snapshot) + "}" +
                     ", after={" + FormatCrdDiagnostics(coord_sys, after_snapshot) + "}");

    if (error_code == 0 &&
        after_snapshot.status == 0 &&
        after_snapshot.segment == 0 &&
        after_snapshot.status_result == 0) {
        SILIGEN_LOG_WARNING(
            "CrdStart returned success but coordinate state still reports idle-empty immediately after start: " +
            FormatCrdDiagnostics(coord_sys, after_snapshot));
    }

    if (error_code != 0) {
        return ConvertError(error_code, "StartCoordinateSystemMotion failed");
    }

    SILIGEN_LOG_DEBUG("Started coordinate system " + std::to_string(coord_sys) + " (mask=0x" +
                      std::to_string(coord_sys_mask) + ")");

    return Result<void>::Success();
}

Result<void> InterpolationAdapter::StopCoordinateSystemMotion(uint32 coord_sys_mask) {
    // MVP 简化实现：停止坐标系运动
    SILIGEN_LOG_DEBUG("StopCoordinateSystemMotion called (mask=0x" + std::to_string(coord_sys_mask) + ")");

    int error_code = wrapper_->MC_StopEx(static_cast<long>(coord_sys_mask), 0);
    if (error_code != 0) {
        SILIGEN_LOG_ERROR("MC_StopEx 返回: " + std::to_string(error_code));
        return ConvertError(error_code, "StopCoordinateSystemMotion: StopEx failed");
    }

    return Result<void>::Success();
}

Result<void> InterpolationAdapter::SetCoordinateSystemVelocityOverride(int16 coord_sys, float32 override_percent) {
    SILIGEN_LOG_DEBUG("SetCoordinateSystemVelocityOverride called (coord_sys=" + std::to_string(coord_sys) +
                      ", override=" + std::to_string(override_percent) + "%)");

    const double ratio = static_cast<double>(override_percent) / 100.0;
    int error_code = wrapper_->MC_SetOverride(coord_sys, ratio);
    if (error_code != 0) {
        SILIGEN_LOG_ERROR("MC_SetOverride 返回: " + std::to_string(error_code));
        return ConvertError(error_code, "SetCoordinateSystemVelocityOverride: SetOverride failed");
    }

    int g00_result = wrapper_->MC_G00SetOverride(coord_sys, ratio);
    if (g00_result != 0) {
        SILIGEN_LOG_WARNING("MC_G00SetOverride 返回: " + std::to_string(g00_result));
    }

    return Result<void>::Success();
}

Result<void> InterpolationAdapter::EnableCoordinateSystemSCurve(int16 coord_sys, float32 jerk) {
    int error_code = wrapper_->MC_CrdSMoveEnable(coord_sys, jerk);
    if (error_code != 0) {
        SILIGEN_LOG_ERROR("MC_CrdSMoveEnable 返回: " + std::to_string(error_code));
        return ConvertError(error_code, "EnableCoordinateSystemSCurve: CrdSMoveEnable failed");
    }

    return Result<void>::Success();
}

Result<void> InterpolationAdapter::DisableCoordinateSystemSCurve(int16 coord_sys) {
    int error_code = wrapper_->MC_CrdSMoveDisable(coord_sys);
    if (error_code != 0) {
        SILIGEN_LOG_ERROR("MC_CrdSMoveDisable 返回: " + std::to_string(error_code));
        return ConvertError(error_code, "DisableCoordinateSystemSCurve: CrdSMoveDisable failed");
    }

    return Result<void>::Success();
}

Result<void> InterpolationAdapter::SetConstLinearVelocityMode(int16 coord_sys, bool enabled, uint32 rotate_axis_mask) {
    short flag = enabled ? 1 : 0;
    int error_code = wrapper_->MC_SetConstLinearVelFlag(coord_sys, 0, flag, static_cast<long>(rotate_axis_mask));
    if (error_code != 0) {
        SILIGEN_LOG_ERROR("MC_SetConstLinearVelFlag 返回: " + std::to_string(error_code));
        return ConvertError(error_code, "SetConstLinearVelocityMode: SetConstLinearVelFlag failed");
    }

    return Result<void>::Success();
}

Result<uint32> InterpolationAdapter::GetInterpolationBufferSpace(int16 coord_sys) const {
    long space = 0;
    int error_code = wrapper_->MC_CrdSpace(coord_sys, &space, 0);
    if (error_code != 0) {
        return Result<uint32>::Failure(
            Error(ErrorCode::HARDWARE_ERROR,
                  "GetInterpolationBufferSpace: MC_CrdSpace failed (error=" + std::to_string(error_code) + ")"));
    }

    return Result<uint32>::Success(static_cast<uint32>(space));
}

Result<uint32> InterpolationAdapter::GetLookAheadBufferSpace(int16 coord_sys) const {
    long space = 0;
    int error_code = wrapper_->MC_GetLookAheadSpace(coord_sys, &space, 0);
    if (error_code != 0) {
        return Result<uint32>::Failure(
            Error(ErrorCode::HARDWARE_ERROR,
                  "GetLookAheadBufferSpace: MC_GetLookAheadSpace failed (error=" + std::to_string(error_code) + ")"));
    }

    return Result<uint32>::Success(static_cast<uint32>(space));
}

Result<Siligen::Domain::Motion::Ports::CoordinateSystemStatus> InterpolationAdapter::GetCoordinateSystemStatus(int16 coord_sys) const {
    Siligen::Domain::Motion::Ports::CoordinateSystemStatus status;

    // 使用 MC_CrdStatus 查询坐标系状态和剩余段数
    short crd_status = 0;
    long segment = 0;
    int error_code = wrapper_->MC_CrdStatus(coord_sys, &crd_status, &segment, 0);

    if (error_code != 0) {
        SILIGEN_LOG_ERROR("MC_CrdStatus failed: " + std::to_string(error_code));
        return Result<Siligen::Domain::Motion::Ports::CoordinateSystemStatus>::Failure(
            Error(ErrorCode::HARDWARE_ERROR,
                  "GetCoordinateSystemStatus: MC_CrdStatus failed (error=" + std::to_string(error_code) + ")"));
    }

    const int status_bits = static_cast<int>(crd_status);
    const bool is_running = (status_bits & kCrdStatusProgRun) != 0;
    const bool is_estop = (status_bits & kCrdStatusProgEstop) != 0;
    const bool is_fifo_finished = (status_bits & kCrdStatusFifoFinish0) != 0;
    const bool is_alarm = (status_bits & kCrdStatusAlarm) != 0;

    status.raw_status_word = static_cast<int32>(crd_status);
    status.raw_segment = static_cast<int32>(segment);
    status.mc_status_ret = error_code;

    bool velocity_valid = false;
    double current_velocity = 0.0;
    int vel_result = wrapper_->MC_GetCrdVel(coord_sys, &current_velocity);
    if (vel_result == 0) {
        // MultiCard 返回单位为 脉冲/ms，这里统一转换为 mm/s
        status.current_velocity = static_cast<float32>(current_velocity * 1000.0 / PULSE_PER_MM);
        velocity_valid = true;
    } else {
        SILIGEN_LOG_WARNING("MC_GetCrdVel failed: " + std::to_string(vel_result));
    }

    const bool coord_velocity_idle =
        !velocity_valid || std::fabs(status.current_velocity) <= kCoordinateVelocityIdleToleranceMmS;
    const bool controller_reported_complete = is_fifo_finished && coord_velocity_idle && segment <= 0;

    status.is_moving = is_running && !controller_reported_complete;
    status.remaining_segments = segment > 0 ? static_cast<uint32>(segment) : 0U;

    if (is_estop || is_alarm) {
        status.state = Siligen::Domain::Motion::Ports::CoordinateSystemState::ERROR_STATE;
    } else if (status.is_moving) {
        status.state = Siligen::Domain::Motion::Ports::CoordinateSystemState::MOVING;
    } else {
        status.state = Siligen::Domain::Motion::Ports::CoordinateSystemState::IDLE;
    }

    SILIGEN_LOG_DEBUG("GetCoordinateSystemStatus: coord_sys=" + std::to_string(coord_sys) +
                      ", raw_status=" + std::to_string(status_bits) +
                      ", is_moving=" + std::to_string(status.is_moving) +
                      ", remaining=" + std::to_string(status.remaining_segments));

    return Result<Siligen::Domain::Motion::Ports::CoordinateSystemStatus>::Success(status);
}

Result<void> InterpolationAdapter::ConvertError(int error_code, const std::string& operation) {
    std::string error_msg =
        Siligen::Hal::Drivers::MultiCard::ErrorMapper::FormatErrorMessage(error_code, operation);
    SILIGEN_LOG_ERROR(error_msg);
    return Result<void>::Failure(Error(ErrorCode::HARDWARE_ERROR, error_msg));
}

}  // namespace Siligen::Infrastructure::Adapters





