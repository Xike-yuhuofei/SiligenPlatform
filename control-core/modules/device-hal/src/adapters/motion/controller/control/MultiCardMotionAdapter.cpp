// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "MultiCardMotion"

#include "MultiCardMotionAdapter.h"

#include <chrono>
#include <cstring>
#include <sstream>
#include <thread>
#include <utility>
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

Shared::Types::Result<std::shared_ptr<MultiCardMotionAdapter>> MultiCardMotionAdapter::Create(
    std::shared_ptr<Infrastructure::Hardware::IMultiCardWrapper> hardware_wrapper,
    const Shared::Types::HardwareConfiguration& config,
    const Shared::Types::DiagnosticsConfig& diagnostics) {
    if (!hardware_wrapper) {
        return Shared::Types::Result<std::shared_ptr<MultiCardMotionAdapter>>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER,
                                 "Hardware wrapper cannot be null",
                                 "MultiCardMotionAdapter"));
    }

    return Shared::Types::Result<std::shared_ptr<MultiCardMotionAdapter>>::Success(
        std::shared_ptr<MultiCardMotionAdapter>(
            new MultiCardMotionAdapter(std::move(hardware_wrapper), config, diagnostics)));
}

MultiCardMotionAdapter::MultiCardMotionAdapter(
    std::shared_ptr<Infrastructure::Hardware::IMultiCardWrapper> hardware_wrapper,
    const Shared::Types::HardwareConfiguration& config,
    const Shared::Types::DiagnosticsConfig& diagnostics)
    : hardware_wrapper_(std::move(hardware_wrapper))
    , hardware_config_(config)
    , diagnostics_config_(diagnostics)
    , unit_converter_(config) {
}

namespace {
template <typename T>
Result<T> NotImplementedResult(const std::string& api_name) {
    return Result<T>(Shared::Types::Error(Shared::Types::ErrorCode::NOT_IMPLEMENTED,
                                          api_name + ": 未实现"));
}
}  // namespace

Result<short> MultiCardMotionAdapter::ResolveAxis(LogicalAxisId axis_id, const char* operation) const {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<short>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS, FormatErrorMessage(operation, axis, -1)));
    }
    return Result<short>::Success(axis);
}

Result<short> MultiCardMotionAdapter::ResolveSdkAxis(LogicalAxisId axis_id, const char* operation) const {
    auto axis_result = ResolveAxis(axis_id, operation);
    if (axis_result.IsError()) {
        return Result<short>::Failure(axis_result.GetError());
    }
    return Result<short>::Success(ToSdkAxis(axis_result.Value()));
}

Result<void> MultiCardMotionAdapter::EnsureAxisEnabled(short axis, short sdk_axis, const char* operation) {
    long enable_check_status = 0;
    hardware_wrapper_->MC_GetSts(sdk_axis, &enable_check_status);
    if ((enable_check_status & AXIS_STATUS_ENABLE) != 0) {
        return Result<void>::Success();
    }

    const short ret = hardware_wrapper_->MC_AxisOn(sdk_axis);
    SILIGEN_LOG_DEBUG(std::string("MC_AxisOn(") + std::to_string(sdk_axis) + ") returned " + std::to_string(ret));
    if (diagnostics_config_.deep_hardware_logging) {
        SILIGEN_LOG_INFO(std::string(operation) + " HW: MC_AxisOn axis=" + std::to_string(axis) +
                         ", sdk_axis=" + std::to_string(sdk_axis) +
                         ", ret=" + std::to_string(ret));
    }
    if (ret != 0) {
        return Result<void>::Failure(Shared::Types::Error(
            Shared::Types::ErrorCode::MOTION_ERROR,
            std::string("Axis enable failed: ") + std::to_string(ret)));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return Result<void>::Success();
}

TJogPrm MultiCardMotionAdapter::BuildJogParameters(short axis) const {
    double acc_mm_s2 = static_cast<double>(hardware_config_.max_acceleration_mm_s2);
    double dec_mm_s2 = static_cast<double>(hardware_config_.max_deceleration_mm_s2);
    const double kMaxJogAcc = 500.0;
    if (acc_mm_s2 > kMaxJogAcc) {
        SILIGEN_LOG_DEBUG("BuildJogParameters: Limiting acceleration from " + std::to_string(acc_mm_s2) +
                          " to " + std::to_string(kMaxJogAcc) + " mm/s^2");
        acc_mm_s2 = kMaxJogAcc;
        dec_mm_s2 = kMaxJogAcc;
    }
    if (acc_mm_s2 <= 0.0) {
        acc_mm_s2 = 500.0;
    }
    if (dec_mm_s2 <= 0.0) {
        dec_mm_s2 = acc_mm_s2;
    }

    TJogPrm jog_prm{};
    jog_prm.dAcc = unit_converter_.AccelerationToPulsePerMs2(axis, acc_mm_s2);
    jog_prm.dDec = unit_converter_.AccelerationToPulsePerMs2(axis, dec_mm_s2);
    jog_prm.dSmooth = 0.0;
    return jog_prm;
}

Result<void> MultiCardMotionAdapter::PrepareAxisForJog(short axis,
                                                       short sdk_axis,
                                                       LogicalAxisId axis_id,
                                                       int16 direction,
                                                       const char* operation,
                                                       bool stop_before_clear) {
    if (kUseHomeAsHardLimit) {
        auto limit_state_result = ReadHomeLimitState();
        if (limit_state_result.IsError()) {
            return Result<void>::Failure(limit_state_result.GetError());
        }
        const auto& limit_states = limit_state_result.Value();
        if (limit_states[axis] && direction <= 0) {
            return Result<void>::Failure(Shared::Types::Error(
                Shared::Types::ErrorCode::POSITION_OUT_OF_RANGE,
                std::string("Cannot start ") + operation + " toward limit: limit is already triggered"));
        }
    }

    if (stop_before_clear) {
        (void)StopAxis(axis_id, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    short ret = hardware_wrapper_->MC_ClrSts(sdk_axis);
    SILIGEN_LOG_DEBUG(std::string("MC_ClrSts(") + std::to_string(sdk_axis) + ") returned " + std::to_string(ret));
    if (diagnostics_config_.deep_hardware_logging) {
        SILIGEN_LOG_INFO(std::string(operation) + " HW: MC_ClrSts axis=" + std::to_string(axis) +
                         ", sdk_axis=" + std::to_string(sdk_axis) +
                         ", ret=" + std::to_string(ret));
    }

    auto enable_result = EnsureAxisEnabled(axis, sdk_axis, operation);
    if (enable_result.IsError()) {
        return enable_result;
    }
    return Result<void>::Success();
}

// === IMotionConnectionPort 接口实现 ===

Result<void> MultiCardMotionAdapter::Connect(const std::string& card_ip,
                                             const std::string& pc_ip,
                                             int16 port) {
    if (!hardware_wrapper_) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_STATE,
                                                 "Hardware wrapper未初始化"));
    }

    // 尝试关闭已有连接（忽略失败，保证幂等性）
    short close_ret = hardware_wrapper_->MC_Close();
    if (close_ret != 0) {
        SILIGEN_LOG_WARNING("MC_Close before connect returned " + std::to_string(close_ret));
    }

    char local_ip[32];
    char card_ip_buf[32];
    std::strncpy(local_ip, pc_ip.c_str(), sizeof(local_ip) - 1);
    local_ip[sizeof(local_ip) - 1] = '\0';
    std::strncpy(card_ip_buf, card_ip.c_str(), sizeof(card_ip_buf) - 1);
    card_ip_buf[sizeof(card_ip_buf) - 1] = '\0';

    SILIGEN_LOG_INFO("Connecting MultiCard: pc_ip=" + std::string(local_ip) +
                     ", card_ip=" + std::string(card_ip_buf) +
                     ", port=" + std::to_string(port));

    short ret = hardware_wrapper_->MC_Open(1,
                                           local_ip,
                                           static_cast<unsigned short>(port),
                                           card_ip_buf,
                                           static_cast<unsigned short>(port));
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(
            Shared::Types::ErrorCode::HARDWARE_CONNECTION_FAILED,
            "MC_Open failed with error code: " + std::to_string(ret)));
    }

    // 连接成功后复位控制卡
    ret = hardware_wrapper_->MC_Reset();
    if (ret != 0) {
        hardware_wrapper_->MC_Close();
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 "MC_Reset failed with error code: " + std::to_string(ret)));
    }

    // 追加复位所有模块（部分固件需要）
    int reset_all_ret = hardware_wrapper_->MC_ResetAllM();
    if (reset_all_ret != 0) {
        SILIGEN_LOG_WARNING("MC_ResetAllM failed with error code: " + std::to_string(reset_all_ret));
    }

    // 等待硬件初始化完成
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 连接完成后使能轴，确保状态查询可用
    for (short axis = 0; axis < static_cast<short>(kAxisCount); ++axis) {
        short sdk_axis = ToSdkAxis(axis);
        short axis_ret = hardware_wrapper_->MC_AxisOn(sdk_axis);
        if (axis_ret != 0) {
            hardware_wrapper_->MC_Close();
            return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                     FormatErrorMessage("MC_AxisOn", axis, axis_ret)));
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    return Result<void>();
}

Result<void> MultiCardMotionAdapter::Disconnect() {
    if (!hardware_wrapper_) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_STATE,
                                                 "Hardware wrapper未初始化"));
    }

    short ret = hardware_wrapper_->MC_Close();
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::HARDWARE_COMMAND_FAILED,
                                                 "MC_Close failed with error code: " + std::to_string(ret)));
    }

    return Result<void>();
}

Result<void> MultiCardMotionAdapter::Reset() {
    if (!hardware_wrapper_) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_STATE,
                                                 "Hardware wrapper未初始化"));
    }

    short ret = hardware_wrapper_->MC_Reset();
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 "MC_Reset failed with error code: " + std::to_string(ret)));
    }

    int reset_all_ret = hardware_wrapper_->MC_ResetAllM();
    if (reset_all_ret != 0) {
        SILIGEN_LOG_WARNING("MC_ResetAllM failed with error code: " + std::to_string(reset_all_ret));
    }

    return Result<void>();
}

Result<std::string> MultiCardMotionAdapter::GetCardInfo() const {
    if (!hardware_wrapper_) {
        return Result<std::string>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_STATE,
                                                        "Hardware wrapper未初始化"));
    }

    long status = 0;
    unsigned long clock = 0;
    short ret = hardware_wrapper_->MC_GetSts(1, &status, 1, &clock);
    if (ret != 0) {
        return Result<std::string>(Shared::Types::Error(Shared::Types::ErrorCode::HARDWARE_NOT_RESPONDING,
                                                        "MC_GetSts failed with error code: " + std::to_string(ret)));
    }

    std::ostringstream oss;
    oss << "MultiCard controller (axes=" << kAxisCount
        << ", status=0x" << std::hex << status
        << ", clock=" << std::dec << clock << ")";
    return Result<std::string>(oss.str());
}

// === IAxisControlPort 接口实现 ===

Result<void> MultiCardMotionAdapter::EnableAxis(LogicalAxisId axis_id) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                 FormatErrorMessage("EnableAxis", axis, -1)));
    }

    short sdk_axis = ToSdkAxis(axis);
    short ret = hardware_wrapper_->MC_AxisOn(sdk_axis);
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("MC_AxisOn", axis, ret)));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return Result<void>();
}

Result<void> MultiCardMotionAdapter::DisableAxis(LogicalAxisId axis_id) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                 FormatErrorMessage("DisableAxis", axis, -1)));
    }

    short sdk_axis = ToSdkAxis(axis);
    short ret = hardware_wrapper_->MC_AxisOff(sdk_axis);
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("MC_AxisOff", axis, ret)));
    }

    return Result<void>();
}

Result<bool> MultiCardMotionAdapter::IsAxisEnabled(LogicalAxisId axis_id) const {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<bool>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                 FormatErrorMessage("IsAxisEnabled", axis, -1)));
    }

    short sdk_axis = ToSdkAxis(axis);
    long status = 0;
    short ret = hardware_wrapper_->MC_GetSts(sdk_axis, &status);
    if (ret != 0) {
        return Result<bool>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("MC_GetSts", axis, ret)));
    }

    bool enabled = (status & AXIS_STATUS_ENABLE) != 0;
    return Result<bool>(enabled);
}

Result<void> MultiCardMotionAdapter::ClearAxisStatus(LogicalAxisId axis_id) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                 FormatErrorMessage("ClearAxisStatus", axis, -1)));
    }

    short sdk_axis = ToSdkAxis(axis);
    short ret = hardware_wrapper_->MC_ClrSts(sdk_axis);
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("MC_ClrSts", axis, ret)));
    }

    return Result<void>();
}

Result<void> MultiCardMotionAdapter::ClearPosition(LogicalAxisId axis_id) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                 FormatErrorMessage("ClearPosition", axis, -1)));
    }

    short sdk_axis = ToSdkAxis(axis);
    short ret = hardware_wrapper_->MC_SetPos(sdk_axis, 0);
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("MC_SetPos", axis, ret)));
    }

    return Result<void>();
}

Result<void> MultiCardMotionAdapter::ConfigureAxis(
    LogicalAxisId axis_id,
    const Siligen::Domain::Motion::Ports::AxisConfiguration& config) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                 FormatErrorMessage("ConfigureAxis", axis, -1)));
    }

    if (config.max_velocity > 0.0f) {
        auto vel_result = SetAxisVelocity(axis_id, config.max_velocity);
        if (vel_result.IsError()) {
            return vel_result;
        }
    }

    if (config.max_acceleration > 0.0f) {
        auto acc_result = SetAxisAcceleration(axis_id, config.max_acceleration);
        if (acc_result.IsError()) {
            return acc_result;
        }
    }

    if (config.soft_limits_enabled) {
        if (config.soft_limit_positive > config.soft_limit_negative) {
            auto limit_result = SetSoftLimits(axis_id, config.soft_limit_negative, config.soft_limit_positive);
            if (limit_result.IsError()) {
                return limit_result;
            }
        } else {
            SILIGEN_LOG_WARNING("ConfigureAxis: soft limit range invalid, skip SetSoftLimits");
        }
    }

    auto hard_limit_result = EnableHardLimits(axis_id, config.hard_limits_enabled, -1);
    if (hard_limit_result.IsError()) {
        return hard_limit_result;
    }

    if (config.jerk > 0.0f || config.following_error_limit > 0.0f || config.in_position_tolerance > 0.0f) {
        SILIGEN_LOG_WARNING("ConfigureAxis: jerk/following_error/in_position_tolerance 暂未映射到硬件参数");
    }

    return Result<void>();
}

// === IJogControlPort 接口实现（补充） ===

Result<void> MultiCardMotionAdapter::SetJogParameters(
    LogicalAxisId axis_id,
    const Siligen::Domain::Motion::Ports::JogParameters& params) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                 FormatErrorMessage("SetJogParameters", axis, -1)));
    }

    short sdk_axis = ToSdkAxis(axis);

    // 切换到JOG模式
    short ret = hardware_wrapper_->MC_PrfJog(sdk_axis);
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("MC_PrfJog", axis, ret)));
    }

    // 设置JOG参数
    TJogPrm jog_prm{};
    jog_prm.dAcc = unit_converter_.AccelerationToPulsePerMs2(axis, params.acceleration);
    jog_prm.dDec = unit_converter_.AccelerationToPulsePerMs2(axis, params.deceleration);
    jog_prm.dSmooth = params.smooth_time;
    ret = hardware_wrapper_->MC_SetJogPrm(sdk_axis, &jog_prm);
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("MC_SetJogPrm", axis, ret)));
    }

    if (params.velocity > 0.0f) {
        double vel_pulse_ms = unit_converter_.VelocityToPulsePerMs(axis, params.velocity);
        ret = hardware_wrapper_->MC_SetVel(sdk_axis, vel_pulse_ms);
        if (ret != 0) {
            return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                     FormatErrorMessage("MC_SetVel", axis, ret)));
        }
    }

    return Result<void>();
}

// === IIOControlPort 接口实现（补充） ===

Result<Siligen::Domain::Motion::Ports::IOStatus> MultiCardMotionAdapter::ReadDigitalInput(int16 channel) {
    if (channel < 0 || channel >= 16) {
        return Result<Siligen::Domain::Motion::Ports::IOStatus>(Shared::Types::Error(
            Shared::Types::ErrorCode::INVALID_PARAMETER,
            "ReadDigitalInput: channel out of range"));
    }

    long raw = 0;
    short ret = hardware_wrapper_->MC_GetDiRaw(MC_GPI, &raw);
    if (ret != 0) {
        return Result<Siligen::Domain::Motion::Ports::IOStatus>(Shared::Types::Error(
            Shared::Types::ErrorCode::HARDWARE_OPERATION_FAILED,
            "MC_GetDiRaw failed with error code: " + std::to_string(ret)));
    }

    Siligen::Domain::Motion::Ports::IOStatus status;
    status.channel = channel;
    status.signal_active = (raw & (1L << channel)) != 0;
    status.value = status.signal_active ? 1 : 0;
    status.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return Result<Siligen::Domain::Motion::Ports::IOStatus>(status);
}

Result<Siligen::Domain::Motion::Ports::IOStatus> MultiCardMotionAdapter::ReadDigitalOutput(int16 channel) {
    if (channel < 0 || channel >= 16) {
        return Result<Siligen::Domain::Motion::Ports::IOStatus>(Shared::Types::Error(
            Shared::Types::ErrorCode::INVALID_PARAMETER,
            "ReadDigitalOutput: channel out of range"));
    }

    unsigned long raw = 0;
    short ret = hardware_wrapper_->MC_GetExtDoValue(0, &raw);
    if (ret != 0) {
        return Result<Siligen::Domain::Motion::Ports::IOStatus>(Shared::Types::Error(
            Shared::Types::ErrorCode::HARDWARE_OPERATION_FAILED,
            "MC_GetExtDoValue failed with error code: " + std::to_string(ret)));
    }

    Siligen::Domain::Motion::Ports::IOStatus status;
    status.channel = channel;
    status.signal_active = (raw & (1UL << channel)) != 0;
    status.value = status.signal_active ? 1 : 0;
    status.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return Result<Siligen::Domain::Motion::Ports::IOStatus>(status);
}

Result<void> MultiCardMotionAdapter::WriteDigitalOutput(int16 channel, bool value) {
    if (channel < 0 || channel >= 16) {
        return Result<void>(Shared::Types::Error(
            Shared::Types::ErrorCode::INVALID_PARAMETER,
            "WriteDigitalOutput: channel out of range"));
    }

    short ret = hardware_wrapper_->MC_SetExtDoBit(0, static_cast<short>(channel), value ? 1 : 0);
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::HARDWARE_OPERATION_FAILED,
                                                 "MC_SetExtDoBit failed with error code: " + std::to_string(ret)));
    }

    return Result<void>();
}

Result<bool> MultiCardMotionAdapter::ReadServoAlarm(LogicalAxisId axis_id) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<bool>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                 FormatErrorMessage("ReadServoAlarm", axis, -1)));
    }

    short sdk_axis = ToSdkAxis(axis);
    short alarm = 0;
    short ret = hardware_wrapper_->MC_GetAlarmOnOff(sdk_axis, &alarm);
    if (ret != 0) {
        return Result<bool>(Shared::Types::Error(Shared::Types::ErrorCode::HARDWARE_OPERATION_FAILED,
                                                 FormatErrorMessage("MC_GetAlarmOnOff", axis, ret)));
    }

    return Result<bool>(alarm != 0);
}

// === 位置控制实现 ===

Result<void> MultiCardMotionAdapter::MoveToPosition(const Point2D& position, float32 velocity) {
    // 同步移动X和Y轴到目标位置
    std::vector<MotionCommand> commands;
    commands.push_back({LogicalAxisId::X, position.x, velocity, false});
    commands.push_back({LogicalAxisId::Y, position.y, velocity, false});
    return SynchronizedMove(commands);
}

Result<bool> MultiCardMotionAdapter::IsConnected() const {
    // 通过查询轴状态判断硬件是否响应（与MultiCardAdapter一致，返回false而非错误）
    constexpr int kMaxRetries = 3;
    constexpr int kRetryDelayMs = 50;

    for (int attempt = 1; attempt <= kMaxRetries; ++attempt) {
        long status = 0;
        unsigned long clock = 0;
        short ret = hardware_wrapper_->MC_GetSts(1, &status, 1, &clock);
        if (ret == 0) {
            return Result<bool>::Success(true);
        }
        if (attempt < kMaxRetries) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kRetryDelayMs));
        }
    }

    return Result<bool>::Success(false);
}

Result<void> MultiCardMotionAdapter::MoveAxisToPosition(LogicalAxisId axis_id, float32 position, float32 velocity) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                 FormatErrorMessage("MoveAxisToPosition", axis, -1)));
    }

    auto current_pos_result = GetAxisPosition(axis_id);
    if (current_pos_result.IsError()) {
        return Result<void>(current_pos_result.GetError());
    }

    float32 current_pos = current_pos_result.Value();
    if (kUseHomeAsHardLimit) {
        auto limit_state_result = ReadHomeLimitState();
        if (limit_state_result.IsError()) {
            return Result<void>(limit_state_result.GetError());
        }
        const auto& limit_states = limit_state_result.Value();

        if (position > current_pos) {
            if (limit_states[axis]) {
                SILIGEN_LOG_DEBUG("MoveAxisToPosition: limit triggered, allow escape move in positive direction");
            }
        } else if (position < current_pos) {
            if (limit_states[axis]) {
                return Result<void>(Shared::Types::Error(
                    Shared::Types::ErrorCode::POSITION_OUT_OF_RANGE,
                    "Cannot move in negative direction: limit is already triggered"));
            }
        }
    }

    // 转换为SDK轴号(1-based)
    short sdk_axis = ToSdkAxis(axis);

    // 调用 MC_AxisOn 使能轴
    short ret = hardware_wrapper_->MC_AxisOn(sdk_axis);
    if (ret != 0) {
        SILIGEN_LOG_ERROR("MC_AxisOn(" + std::to_string(sdk_axis) + ") failed with error code " + std::to_string(ret) + ", motion aborted");
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
            std::string("Axis enable failed: ") + std::to_string(ret)));
    }
    // 等待使能稳定
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 1. 切换到点位模式
    ret = hardware_wrapper_->MC_PrfTrap(sdk_axis);
    if (ret != 0) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR, FormatErrorMessage("MC_PrfTrap", axis, ret)));
    }

    // 2. 设置目标位置
    long target_pulses = unit_converter_.PositionToPulses(axis, position);
    ret = hardware_wrapper_->MC_SetPos(sdk_axis, target_pulses);
    if (ret != 0) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR, FormatErrorMessage("MC_SetPos", axis, ret)));
    }

    // 3. 设置速度
    double vel_pulse_ms = unit_converter_.VelocityToPulsePerMs(axis, velocity);
    ret = hardware_wrapper_->MC_SetVel(sdk_axis, vel_pulse_ms);
    if (ret != 0) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR, FormatErrorMessage("MC_SetVel", axis, ret)));
    }

    // 4. 启动运动
    ret = hardware_wrapper_->MC_Update(1 << axis);
    if (ret != 0) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR, FormatErrorMessage("MC_Update", axis, ret)));
    }

    SILIGEN_LOG_DEBUG("MoveAxisToPosition: 运动启动成功");

    return Result<void>();
}

Result<void> MultiCardMotionAdapter::RelativeMove(LogicalAxisId axis_id, float32 distance, float32 velocity) {
    const short axis = static_cast<short>(ToIndex(axis_id));
    if (!ValidateAxis(axis)) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS, FormatErrorMessage("RelativeMove", axis, -1)));
    }

    if (kUseHomeAsHardLimit) {
        auto limit_state_result = ReadHomeLimitState();
        if (limit_state_result.IsError()) {
            return Result<void>(limit_state_result.GetError());
        }
        const auto& limit_states = limit_state_result.Value();

        if (distance > 0.0f) {
            if (limit_states[axis]) {
                SILIGEN_LOG_DEBUG("RelativeMove: limit triggered, allow escape move in positive direction");
            }
        } else if (distance < 0.0f) {
            if (limit_states[axis]) {
                return Result<void>(Shared::Types::Error(
                    Shared::Types::ErrorCode::POSITION_OUT_OF_RANGE,
                    "Cannot move in negative direction: limit is already triggered"));
            }
        }
    }

    // 转换为SDK轴号(1-based)
    short sdk_axis = ToSdkAxis(axis);

    // 获取当前位置
    double current_pos = 0.0;
    short ret = hardware_wrapper_->MC_GetPrfPos(sdk_axis, &current_pos);
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("MC_GetPrfPos", axis, ret)));
    }

    // 计算目标位置
    long distance_pulses = unit_converter_.DistanceToPulses(axis, distance);
    long target_pulses = static_cast<long>(current_pos) + distance_pulses;
    double vel_pulse_ms = unit_converter_.VelocityToPulsePerMs(axis, velocity);

    // 切换到点位模式
    ret = hardware_wrapper_->MC_PrfTrap(sdk_axis);
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("MC_PrfTrap", axis, ret)));
    }

    // 设置目标位置
    ret = hardware_wrapper_->MC_SetPos(sdk_axis, target_pulses);
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("MC_SetPos", axis, ret)));
    }

    // 设置速度
    ret = hardware_wrapper_->MC_SetVel(sdk_axis, vel_pulse_ms);
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("MC_SetVel", axis, ret)));
    }

    // 启动运动
    ret = hardware_wrapper_->MC_Update(1 << axis);
    if (ret != 0) {
        return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                 FormatErrorMessage("MC_Update", axis, ret)));
    }

    return Result<void>();
}

Result<void> MultiCardMotionAdapter::SynchronizedMove(const std::vector<MotionCommand>& commands) {
    if (commands.empty()) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER, "SynchronizedMove: 命令列表为空"));
    }

    for (const auto& cmd : commands) {
        const short axis = static_cast<short>(ToIndex(cmd.axis));
        auto current_pos_result = GetAxisPosition(cmd.axis);
        if (current_pos_result.IsError()) {
            return Result<void>(current_pos_result.GetError());
        }
        float32 current_pos = current_pos_result.Value();
        if (cmd.position > current_pos) {
            auto limit_result = IsLimitTriggeredRaw(axis, true);
            if (limit_result.IsError()) {
                return Result<void>(limit_result.GetError());
            }
            if (limit_result.Value()) {
                return Result<void>(Shared::Types::Error(
                    Shared::Types::ErrorCode::POSITION_OUT_OF_RANGE,
                    "Cannot move in positive direction: Positive limit is triggered"));
            }
        } else if (cmd.position < current_pos) {
            auto limit_result = IsLimitTriggeredRaw(axis, false);
            if (limit_result.IsError()) {
                return Result<void>(limit_result.GetError());
            }
            if (limit_result.Value()) {
                return Result<void>(Shared::Types::Error(
                    Shared::Types::ErrorCode::POSITION_OUT_OF_RANGE,
                    "Cannot move in negative direction: Negative limit is triggered"));
            }
        }
    }

    SILIGEN_LOG_DEBUG("SynchronizedMove: 开始同步运动，命令数=" + std::to_string(commands.size()));

    uint32_t axis_mask = 0;

    // 为每个轴设置运动参数
    for (const auto& cmd : commands) {
        const short axis = static_cast<short>(ToIndex(cmd.axis));
        if (!ValidateAxis(axis)) {
            return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::INVALID_AXIS,
                                                     FormatErrorMessage("SynchronizedMove", axis, -1)));
        }

        long target_pulses = unit_converter_.PositionToPulses(axis, cmd.position);
        double vel_pulse_ms = unit_converter_.VelocityToPulsePerMs(axis, cmd.velocity);
        short sdk_axis = ToSdkAxis(axis);
        SILIGEN_LOG_DEBUG("axis" + std::to_string(axis) + " (SDK=" + std::to_string(sdk_axis) +
                          "): pos=" + std::to_string(cmd.position) + "mm(" + std::to_string(target_pulses) +
                          " pulses), vel=" + std::to_string(cmd.velocity) + "mm/s(" + std::to_string(vel_pulse_ms) + " pulse/ms)");

        // 1. 切换到点位模式
        short ret = hardware_wrapper_->MC_PrfTrap(sdk_axis);
        if (ret != 0) {
            return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                     FormatErrorMessage("MC_PrfTrap", axis, ret)));
        }

        // 2. 设置目标位置
        ret = hardware_wrapper_->MC_SetPos(sdk_axis, target_pulses);
        if (ret != 0) {
            return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                     FormatErrorMessage("MC_SetPos", axis, ret)));
        }

        // 3. 设置速度
        ret = hardware_wrapper_->MC_SetVel(sdk_axis, vel_pulse_ms);
        if (ret != 0) {
            return Result<void>(Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR,
                                                     FormatErrorMessage("MC_SetVel", axis, ret)));
        }

        // 记录轴掩码
        axis_mask |= (1 << axis);
    }

    // 4. 同时启动所有轴的运动
    SILIGEN_LOG_DEBUG("调用MC_Update，轴掩码=0x" + std::to_string(axis_mask));

    short ret = hardware_wrapper_->MC_Update(axis_mask);
    if (ret != 0) {
        return Result<void>(
            Shared::Types::Error(Shared::Types::ErrorCode::MOTION_ERROR, FormatErrorMessage("MC_Update", 0, ret)));
    }

    SILIGEN_LOG_DEBUG("SynchronizedMove: 运动启动成功");

    return Result<void>();
}


}  // namespace Siligen::Infrastructure::Adapters
