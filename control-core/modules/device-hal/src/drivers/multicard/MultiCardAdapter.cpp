// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "MultiCardAdapter"

#include "MultiCardAdapter.h"

#include "siligen/hal/drivers/multicard/error_mapper.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/types/AxisTypes.h"

#include <chrono>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <thread>

// 明确取消定义Windows宏以避免污染
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#ifdef ERROR
#undef ERROR
#endif
#ifdef SUCCESS
#undef SUCCESS
#endif

namespace Siligen {
namespace Infrastructure {
namespace Hardware {

using namespace Shared::Types;
using AxisStatusStruct = Siligen::Shared::Types::AxisStatus;

// ============================================================
// 构造函数和析构函数
// ============================================================

MultiCardAdapter::MultiCardAdapter(std::shared_ptr<IMultiCardWrapper> multicard, const HardwareConfiguration& config)
    : multicard_(multicard),
      is_connected_(false),
      default_speed_(config.max_velocity_mm_s),
      max_speed_(config.max_velocity_mm_s),
      max_acceleration_(config.max_acceleration_mm_s2),
      unit_converter_(config),
      axis_count_(config.EffectiveAxisCount()) {}

MultiCardAdapter::~MultiCardAdapter() {
    if (is_connected_) {
        Disconnect();
    }
}

// ============================================================
// 连接管理实现
// ============================================================

Result<void> MultiCardAdapter::Connect(const ConnectionConfig& config) {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    // 如果已经连接，先断开旧连接
    if (is_connected_) {
        SILIGEN_LOG_INFO("Disconnecting existing connection before reconnect...");
        multicard_->MC_Close();
        is_connected_ = false;
    }

    // 创建MultiCard对象（如果尚未注入）
    // 注意：这里不能创建wrapper，因为wrapper需要具体的MultiCard或MockMultiCard实例
    // multicard必须由外部注入
    if (!multicard_) {
        return Result<void>::Failure(Error(ErrorCode::INVALID_STATE,
                                           "MultiCardWrapper not injected. Use CreateMultiCard in factory.",
                                           "MultiCardAdapter"));
    }

    // 调用C++ API连接
    char local_ip[32], card_ip[32];
    std::strncpy(local_ip, config.local_ip.c_str(), sizeof(local_ip) - 1);
    std::strncpy(card_ip, config.card_ip.c_str(), sizeof(card_ip) - 1);
    local_ip[sizeof(local_ip) - 1] = '\0';
    card_ip[sizeof(card_ip) - 1] = '\0';

    // MC_Open参数说明（参考厂商示例）：
    // - 卡号从1开始（不是0）
    // - PC端和控制卡端口必须一致
    // 连接配置验证日志
    SILIGEN_LOG_INFO(std::string("========== 连接开始 =========="));
    SILIGEN_LOG_INFO("连接配置验证:");
    SILIGEN_LOG_INFO("  本地IP: " + std::string(local_ip));
    SILIGEN_LOG_INFO("  本机端口: " + std::to_string(static_cast<int>(config.port)) +
                     (config.port == 0 ? " (系统自动分配)" : " (指定端口)"));
    SILIGEN_LOG_INFO("  控制卡IP: " + std::string(card_ip));
    SILIGEN_LOG_INFO("  控制卡端口: " + std::to_string(static_cast<int>(config.port)));

    // 计时: MC_Open开始
    auto open_start = std::chrono::high_resolution_clock::now();
    SILIGEN_LOG_INFO("开始调用 MC_Open...");

    short result = multicard_->MC_Open(1,  // nCardNum - 卡号，从1开始
                                       local_ip,
                                       static_cast<unsigned short>(config.port),
                                       card_ip,
                                       static_cast<unsigned short>(config.port));

    // 计时: MC_Open完成
    auto open_end = std::chrono::high_resolution_clock::now();
    auto open_duration = std::chrono::duration_cast<std::chrono::milliseconds>(open_end - open_start).count();
    SILIGEN_LOG_INFO("MC_Open 耗时: " + std::to_string(open_duration) + "ms");

    // MC_Open结果处理
    if (result != 0) {
        SILIGEN_LOG_ERROR("MC_Open failed with error code: " + std::to_string(result));
        SILIGEN_LOG_ERROR("请检查:");
        SILIGEN_LOG_ERROR("  1. PC和控制卡IP地址是否在同一网段");
        SILIGEN_LOG_ERROR("  2. 端口号配置（推荐使用0自动分配）");
        SILIGEN_LOG_ERROR("  3. 防火墙是否允许端口通信");
        SILIGEN_LOG_ERROR("  4. 网线是否连接,控制卡是否通电");
        return Result<void>::Failure(MapMultiCardError(result, "MC_Open"));
    }
    SILIGEN_LOG_INFO("MC_Open succeeded (result=" + std::to_string(result) + ")");

    // 连接成功，立即复位控制卡
    SILIGEN_LOG_INFO("MC_Open成功，开始调用 MC_Reset...");

    // 计时: MC_Reset开始
    auto reset_start = std::chrono::high_resolution_clock::now();

    short reset_result = multicard_->MC_Reset();

    // 计时: MC_Reset完成
    auto reset_end = std::chrono::high_resolution_clock::now();
    auto reset_duration = std::chrono::duration_cast<std::chrono::milliseconds>(reset_end - reset_start).count();
    SILIGEN_LOG_INFO("MC_Reset 耗时: " + std::to_string(reset_duration) + "ms, 返回值: " + std::to_string(reset_result));

    if (reset_result != 0) {
        SILIGEN_LOG_ERROR("MC_Reset failed with code " + std::to_string(reset_result));

        // Close the connection on reset failure
        multicard_->MC_Close();

        // Do NOT reset multicard_ - keep it for retry
        return Result<void>::Failure(MapMultiCardError(reset_result, "MC_Reset"));
    }

    // 追加控制卡全模块复位（部分固件需要此步骤才能使用坐标系功能）
    SILIGEN_LOG_INFO("MC_Reset成功，开始调用 MC_ResetAllM...");
    auto reset_all_start = std::chrono::high_resolution_clock::now();
    int reset_all_result = multicard_->MC_ResetAllM();
    auto reset_all_end = std::chrono::high_resolution_clock::now();
    auto reset_all_duration = std::chrono::duration_cast<std::chrono::milliseconds>(reset_all_end - reset_all_start).count();
    SILIGEN_LOG_INFO("MC_ResetAllM 耗时: " + std::to_string(reset_all_duration) +
                     "ms, 返回值: " + std::to_string(reset_all_result));
    if (reset_all_result != 0) {
        SILIGEN_LOG_WARNING("MC_ResetAllM failed: " +
                            Siligen::Hal::Drivers::MultiCard::ErrorMapper::FormatErrorMessage(
                                reset_all_result,
                                "MC_ResetAllM"));
    }

    // 等待硬件初始化完成（500ms延迟 - 确保硬件完全就绪）
    SILIGEN_LOG_INFO("Waiting for hardware initialization...");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 🔧 修复：连接成功后自动使能所有轴（1-2）
    // MC_GetSts需要轴已软件使能才能查询状态，否则返回错误码7（超时）
    SILIGEN_LOG_INFO("Enabling all axes (1-" + std::to_string(axis_count_) + ")...");

    for (short axis = 1; axis <= axis_count_; ++axis) {
        short axis_on_result = multicard_->MC_AxisOn(axis);
        if (axis_on_result != 0) {
            SILIGEN_LOG_ERROR("MC_AxisOn(" + std::to_string(axis) + ") failed with code " + std::to_string(axis_on_result));

            // 关闭连接
            multicard_->MC_Close();

            return Result<void>::Failure(
                MapMultiCardError(axis_on_result, "MC_AxisOn during connection")
            );
        }
        SILIGEN_LOG_INFO("Axis " + std::to_string(axis) + " enabled successfully");
    }

    // 等待轴使能完成并稳定（300ms延迟）
    SILIGEN_LOG_INFO("Waiting for axes to stabilize...");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 初始化默认坐标系（用于插补模块预初始化）
    auto init_coord_result = InitializeDefaultCoordinateSystem();
    if (init_coord_result.IsError()) {
        SILIGEN_LOG_ERROR("默认坐标系初始化失败: " + init_coord_result.GetError().GetMessage());
        multicard_->MC_Close();
        return Result<void>::Failure(init_coord_result.GetError());
    }

    // === 配置硬限位 ===
    // 对于4轴/8轴控制卡，硬件限位信号是固定映射的，需要:
    // 1. MC_LmtSns - 全局设置极性 (必须先调用)
    // 2. MC_LmtsOn - 启用限位
    SILIGEN_LOG_INFO("Configuring hard limits for axes 1-2...");

    // 先使用 MC_LmtSns 全局配置轴1和轴2的限位极性
    // Bit0=轴1正限位, Bit1=轴1负限位, Bit2=轴2正限位, Bit3=轴2负限位
    // 1=低电平触发 (NPN NO型开关)
    unsigned short limit_sense = 0x0F;  // 0b00001111 = 轴1和轴2的正负限位都设为低电平触发
    short sense_result = multicard_->MC_LmtSns(limit_sense);
    if (sense_result != 0) {
        SILIGEN_LOG_WARNING("MC_LmtSns failed: " + std::to_string(sense_result));
    } else {
        SILIGEN_LOG_INFO("MC_LmtSns configured: 0x" + std::to_string(limit_sense));
    }

    for (short axis = 1; axis <= 2; ++axis) {  // X轴(1), Y轴(2)
        // 仅启用负向硬限位（LIM+ 未接，避免悬空误触发）
        short neg_result = multicard_->MC_LmtsOn(axis, MC_LIMIT_NEGATIVE);
        if (neg_result != 0) {
            SILIGEN_LOG_ERROR("MC_LmtsOn(NEG, axis=" + std::to_string(axis) + ") failed: " +
                              std::to_string(neg_result));
        } else {
            SILIGEN_LOG_INFO("Axis " + std::to_string(axis) + " negative hard limit enabled");
        }

        // 明确关闭正向硬限位
        short pos_off_result = multicard_->MC_LmtsOff(axis, MC_LIMIT_POSITIVE);
        if (pos_off_result != 0) {
            SILIGEN_LOG_WARNING("MC_LmtsOff(POS, axis=" + std::to_string(axis) + ") failed: " +
                                std::to_string(pos_off_result));
        } else {
            SILIGEN_LOG_INFO("Axis " + std::to_string(axis) + " positive hard limit disabled");
        }
    }

    is_connected_ = true;
    connection_config_ = config;

    SILIGEN_LOG_INFO("Hardware initialized and ready");

    // 计时: 连接总耗时
    auto total_end = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - open_start).count();
    SILIGEN_LOG_INFO(std::string("========== 连接完成 总耗时: ") + std::to_string(total_duration) + "ms ==========");

    return Result<void>::Success();
}

Result<void> MultiCardAdapter::Disconnect() {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    // Always call MC_Close for idempotency (even if not connected)
    short result = 0;
    if (multicard_) {
        result = multicard_->MC_Close();

        // Do NOT reset multicard_ - keep it for reuse in next Connect()
        // Only reset in destructor
    }

    is_connected_ = false;

    return result != 0 ? Result<void>::Failure(MapMultiCardError(result, "MC_Close")) : Result<void>::Success();
}

Result<bool> MultiCardAdapter::IsConnected() const {
    std::unique_lock<std::mutex> lock(hardware_mutex_);

    SILIGEN_LOG_DEBUG("Checking hardware (is_connected_: " + std::string(is_connected_ ? "true" : "false") + ")");

    // Quick check: if not connected at software level, return false
    if (!is_connected_ || !multicard_) {
        SILIGEN_LOG_DEBUG("Software state: not connected");
        return Result<bool>::Success(false);
    }


    // ✅ 添加重试机制：最多重试3次，每次间隔50ms
    constexpr int MAX_RETRIES = 3;
    constexpr int RETRY_DELAY_MS = 50;

    for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt) {
        long status = 0;
        unsigned long clock = 0;
        // 🔧 修复：使用有效轴号1而不是0
        short result = multicard_->MC_GetSts(1, &status, 1, &clock);

        SILIGEN_LOG_DEBUG("IsConnected: Attempt " + std::to_string(attempt) + "/" + std::to_string(MAX_RETRIES) +
                          ": MC_GetSts returned " + std::to_string(result));

        if (result == 0) {
            // ✅ 成功 - 硬件正常响应
            SILIGEN_LOG_DEBUG("Hardware verified responding");
            return Result<bool>::Success(true);
        }

        // ✅ 如果不是最后一次尝试，等待后重试
        if (attempt < MAX_RETRIES) {
            SILIGEN_LOG_DEBUG("Retrying after " + std::to_string(RETRY_DELAY_MS) + "ms (error code: " + std::to_string(result) + ")");

            // 使用 unique_lock 的 unlock/lock 方法，避免竞态条件
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
            lock.lock();

            // 重新检查状态（防止其他线程修改）
            if (!is_connected_ || !multicard_) {
                SILIGEN_LOG_DEBUG("State changed during retry, aborting");
                return Result<bool>::Success(false);
            }
        }
    }

    // ✅ 所有重试都失败 - 返回false但不修改连接状态
    // 连接状态只能通过Connect/Disconnect方法修改
    SILIGEN_LOG_DEBUG("All retries failed, hardware not responding");
    return Result<bool>::Success(false);
}

// ============================================================
// 轴控制实现
// ============================================================

Result<void> MultiCardAdapter::EnableAxis(LogicalAxisId axis_id) {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    auto validate_result = ValidateAxisId(axis_id);
    if (!validate_result.IsSuccess()) {
        return validate_result;
    }

    if (!is_connected_) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Not connected to hardware", "MultiCardAdapter"));
    }

    // 注意: MultiCard SDK 轴号从1开始, 而我们的接口使用0-1
    // 使用类型安全转换: LogicalAxisId (0-based) -> SdkAxisId (1-based)
    short sdk_axis = ToSdkShort(ToSdkAxis(axis_id));
    short result = multicard_->MC_AxisOn(sdk_axis);

    if (result != 0) {
        return Result<void>::Failure(MapMultiCardError(result, "MC_AxisOn"));
    }

    return Result<void>::Success();
}

Result<void> MultiCardAdapter::DisableAxis(LogicalAxisId axis_id) {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    auto validate_result = ValidateAxisId(axis_id);
    if (!validate_result.IsSuccess()) {
        return validate_result;
    }

    if (!is_connected_) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Not connected to hardware", "MultiCardAdapter"));
    }

    // 注意: MultiCard SDK 轴号从1开始, 而我们的接口使用0-1
    // 使用类型安全转换: LogicalAxisId (0-based) -> SdkAxisId (1-based)
    short sdk_axis = ToSdkShort(ToSdkAxis(axis_id));
    short result = multicard_->MC_AxisOff(sdk_axis);

    if (result != 0) {
        return Result<void>::Failure(MapMultiCardError(result, "MC_AxisOff"));
    }

    return Result<void>::Success();
}

Result<void> MultiCardAdapter::MoveToPosition(const Point2D& position, float speed) {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    if (!is_connected_) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Not connected to hardware", "MultiCardAdapter"));
    }

    float actual_speed = (speed < 0) ? default_speed_ : speed;

    if (actual_speed <= 0.0f) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Move speed must be positive", "MultiCardAdapter"));
    }
    if (max_speed_ > 0.0f && actual_speed > max_speed_) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Move speed exceeds Hardware.max_velocity_mm_s", "MultiCardAdapter"));
    }

    // 转换为脉冲
    int32_t x_pulse = MmToPulse(position.x);
    int32_t y_pulse = MmToPulse(position.y);

    SILIGEN_LOG_INFO_FMT_HELPER("MoveToPosition: X:%.2fmm (pulse:%d), Y:%.2fmm (pulse:%d)",
                                position.x, x_pulse, position.y, y_pulse);

    // X轴移动 (注意: MultiCard SDK 轴号从1开始)
    short x_axis = 1;  // X轴对应SDK轴1
    short result = multicard_->MC_PrfTrap(x_axis);
    if (result != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER("MC_PrfTrap(X) failed with code %d", result);
        return Result<void>::Failure(MapMultiCardError(result, "MC_PrfTrap(X)"));
    }

    result = multicard_->MC_SetPos(x_axis, x_pulse);
    if (result != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER("MC_SetPos(X) failed with code %d", result);
        return Result<void>::Failure(MapMultiCardError(result, "MC_SetPos(X)"));
    }

    // ⚠️ MC_SetVel 期望的单位是 pulse/ms，不是 pulse/s
    double x_vel_pulse_ms = unit_converter_.VelocityMmSToPS(actual_speed) / 1000.0;
    result = multicard_->MC_SetVel(x_axis, x_vel_pulse_ms);
    if (result != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER("MC_SetVel(X) failed with code %d", result);
        return Result<void>::Failure(MapMultiCardError(result, "MC_SetVel(X)"));
    }

    // Y轴移动 (注意: MultiCard SDK 轴号从1开始)
    short y_axis = 2;  // Y轴对应SDK轴2
    result = multicard_->MC_PrfTrap(y_axis);
    if (result != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER("MC_PrfTrap(Y) failed with code %d", result);
        return Result<void>::Failure(MapMultiCardError(result, "MC_PrfTrap(Y)"));
    }

    result = multicard_->MC_SetPos(y_axis, y_pulse);
    if (result != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER("MC_SetPos(Y) failed with code %d", result);
        return Result<void>::Failure(MapMultiCardError(result, "MC_SetPos(Y)"));
    }

    double y_vel_pulse_ms = unit_converter_.VelocityMmSToPS(actual_speed) / 1000.0;
    result = multicard_->MC_SetVel(y_axis, y_vel_pulse_ms);
    if (result != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER("MC_SetVel(Y) failed with code %d", result);
        return Result<void>::Failure(MapMultiCardError(result, "MC_SetVel(Y)"));
    }

    // 启动运动 (mask: bit0=X(轴1), bit1=Y(轴2))
    short motion_mask = (1 << (x_axis - 1)) | (1 << (y_axis - 1));
    SILIGEN_LOG_DEBUG_FMT_HELPER("Starting motion with mask 0x%02X", motion_mask);
    result = multicard_->MC_Update(motion_mask);
    if (result != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER("MC_Update failed with code %d", result);
        return Result<void>::Failure(MapMultiCardError(result, "MC_Update"));
    }

    SILIGEN_LOG_INFO("Motion command sent successfully");
    return Result<void>::Success();
}

Result<void> MultiCardAdapter::MoveAxisToPosition(LogicalAxisId axis_id, float position, float speed) {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    const int32_t axis_index = static_cast<int32_t>(ToIndex(axis_id));
    auto validate_result = ValidateAxisId(axis_id);
    if (!validate_result.IsSuccess()) {
        return validate_result;
    }

    if (!is_connected_) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Not connected to hardware", "MultiCardAdapter"));
    }

    // 检查轴使能状态
    auto axis_status_result = GetMultiCardAxisStatus(axis_id);
    if (!axis_status_result.IsSuccess()) {
        SILIGEN_LOG_ERROR("Failed to get axis status for axis " + std::to_string(axis_index) + ": " +
                          axis_status_result.GetError().GetMessage());
        return Result<void>::Failure(axis_status_result.GetError());
    }

    uint32_t mc_status = axis_status_result.Value();
    bool is_enabled = (mc_status & (1 << 8)) != 0;  // bit8: 伺服使能
    if (!is_enabled) {
        SILIGEN_LOG_ERROR("Axis " + std::to_string(axis_index) + " is not enabled, cannot move");
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE,
                  "Axis not enabled. Call EnableAxis() first.",
                  "MultiCardAdapter"));
    }

    float actual_speed = (speed < 0) ? default_speed_ : speed;
    if (actual_speed <= 0.0f) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Move speed must be positive", "MultiCardAdapter"));
    }
    if (max_speed_ > 0.0f && actual_speed > max_speed_) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Move speed exceeds Hardware.max_velocity_mm_s", "MultiCardAdapter"));
    }

    int32_t pulse = MmToPulse(position);
    // 注意: MultiCard SDK 轴号从1开始, 而我们的接口使用0-1
    // 使用类型安全转换: LogicalAxisId (0-based) -> SdkAxisId (1-based)
    short sdk_axis = ToSdkShort(ToSdkAxis(axis_id));

    SILIGEN_LOG_INFO("Moving axis " + std::to_string(axis_index) + " (SDK: " + std::to_string(sdk_axis) +
                     ") to " + std::to_string(position) + "mm (pulse:" + std::to_string(pulse) + ")");

    short result = multicard_->MC_PrfTrap(sdk_axis);
    if (result != 0) {
        SILIGEN_LOG_ERROR("MC_PrfTrap failed with code " + std::to_string(result));
        return Result<void>::Failure(MapMultiCardError(result, "MC_PrfTrap"));
    }

    result = multicard_->MC_SetPos(sdk_axis, pulse);
    if (result != 0) {
        SILIGEN_LOG_ERROR("MC_SetPos failed with code " + std::to_string(result));
        return Result<void>::Failure(MapMultiCardError(result, "MC_SetPos"));
    }

    // ⚠️ MC_SetVel 期望的单位是 pulse/ms，不是 pulse/s
    double vel_pulse_ms = unit_converter_.VelocityMmSToPS(actual_speed) / 1000.0;
    result = multicard_->MC_SetVel(sdk_axis, vel_pulse_ms);
    if (result != 0) {
        SILIGEN_LOG_ERROR("MC_SetVel failed with code " + std::to_string(result));
        return Result<void>::Failure(MapMultiCardError(result, "MC_SetVel"));
    }

    // 启动运动 - 掩码计算: SDK轴1对应bit0, 轴2对应bit1, 以此类推
    short motion_mask = static_cast<short>(1 << axis_index);
    SILIGEN_LOG_DEBUG("Starting motion with mask 0x" + std::to_string(motion_mask));
    result = multicard_->MC_Update(motion_mask);
    if (result != 0) {
        SILIGEN_LOG_ERROR("MC_Update failed with code " + std::to_string(result));
        return Result<void>::Failure(MapMultiCardError(result, "MC_Update"));
    }

    SILIGEN_LOG_INFO("Motion command sent successfully");
    return Result<void>::Success();
}

Result<void> MultiCardAdapter::StopAxis(LogicalAxisId axis_id) {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    const int32_t axis_index = static_cast<int32_t>(ToIndex(axis_id));
    auto validate_result = ValidateAxisId(axis_id);
    if (!validate_result.IsSuccess()) {
        return validate_result;
    }

    if (!is_connected_) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Not connected to hardware", "MultiCardAdapter"));
    }

    // MC_Stop 使用轴掩码: bit0对应SDK轴1, bit1对应SDK轴2, 以此类推
    // 我们的轴号0对应SDK轴1，所以掩码是 1 << axis_id
    long axis_mask = static_cast<long>(1) << axis_index;
    short result = multicard_->MC_Stop(axis_mask, 1);

    if (result != 0) {
        return Result<void>::Failure(MapMultiCardError(result, "MC_Stop"));
    }

    return Result<void>::Success();
}

Result<void> MultiCardAdapter::EmergencyStop() {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    if (!is_connected_) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Not connected to hardware", "MultiCardAdapter"));
    }

    // 立即停止所有轴 (使用厂商API: MC_Stop)
    // MC_Stop参数: lMask=所有轴, lOption=1表示紧急停止
    short result = multicard_->MC_Stop(0xFFFFFFFF, 1);

    if (result != 0) {
        return Result<void>::Failure(MapMultiCardError(result, "MC_Stop(Emergency)"));
    }

    return Result<void>::Success();
}

// ============================================================
// 轨迹执行实现 (新增 - Phase 3)
// ============================================================

Result<int32_t> MultiCardAdapter::GetCoordinateBufferSpace(int32_t crd_num) const {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    if (!is_connected_) {
        return Result<int32_t>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Not connected to hardware", "MultiCardAdapter"));
    }

    long space = 0;
    short result = multicard_->MC_CrdSpace(static_cast<short>(crd_num), &space, 0);

    if (result != 0) {
        return Result<int32_t>::Failure(MapMultiCardError(result, "MC_CrdSpace"));
    }

    return Result<int32_t>::Success(static_cast<int32_t>(space));
}

Result<void> MultiCardAdapter::ClearCoordinateBuffer(int32_t crd_num) {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    if (!is_connected_) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Not connected to hardware", "MultiCardAdapter"));
    }

    // MC_CrdClear(nCrdNum, FifoIndex) - 厂商API需要2个参数
    short result = multicard_->MC_CrdClear(static_cast<short>(crd_num), 0);

    if (result != 0) {
        return Result<void>::Failure(MapMultiCardError(result, "MC_CrdClear"));
    }

    return Result<void>::Success();
}

Result<void> MultiCardAdapter::ExecuteLinearInterpolation(int32_t crd_num,
                                                          int32_t x_pulse,
                                                          int32_t y_pulse,
                                                          double velocity,
                                                          double acceleration,
                                                          double end_velocity,
                                                          int16_t fifo,
                                                          int32_t segment_id) {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    if (!is_connected_) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Not connected to hardware", "MultiCardAdapter"));
    }

    short result = multicard_->MC_LnXY(static_cast<short>(crd_num),
                                       static_cast<long>(x_pulse),
                                       static_cast<long>(y_pulse),
                                       velocity,
                                       acceleration,
                                       end_velocity,
                                       fifo,
                                       static_cast<long>(segment_id));

    if (result != 0) {
        return Result<void>::Failure(MapMultiCardError(result, "MC_LnXY"));
    }

    return Result<void>::Success();
}

Result<void> MultiCardAdapter::ExecuteArcInterpolation(int32_t crd_num,
                                                       int32_t x_pulse,
                                                       int32_t y_pulse,
                                                       double cx,
                                                       double cy,
                                                       int16_t direction,
                                                       double velocity,
                                                       double acceleration,
                                                       double end_velocity,
                                                       int16_t fifo,
                                                       int32_t segment_id) {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    if (!is_connected_) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Not connected to hardware", "MultiCardAdapter"));
    }

    short result = multicard_->MC_ArcXYC(static_cast<short>(crd_num),
                                         static_cast<long>(x_pulse),
                                         static_cast<long>(y_pulse),
                                         cx,
                                         cy,
                                         direction,
                                         velocity,
                                         acceleration,
                                         end_velocity,
                                         fifo,
                                         static_cast<long>(segment_id));

    if (result != 0) {
        return Result<void>::Failure(MapMultiCardError(result, "MC_ArcXYC"));
    }

    return Result<void>::Success();
}

Result<void> MultiCardAdapter::StartCoordinateMotion(int32_t crd_num) {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    if (!is_connected_) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Not connected to hardware", "MultiCardAdapter"));
    }

    short result = multicard_->MC_CrdStart(static_cast<short>(crd_num), 0);

    if (result != 0) {
        return Result<void>::Failure(MapMultiCardError(result, "MC_CrdStart"));
    }

    return Result<void>::Success();
}

Result<void> MultiCardAdapter::StopMotion(int32_t stop_mode) {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    if (!is_connected_) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Not connected to hardware", "MultiCardAdapter"));
    }

    // stop_mode: -1=正常减速停止, 0=急停
    short smooth_stop = (stop_mode == -1) ? 1 : 0;
    short result = multicard_->MC_Stop(0xFF, smooth_stop);

    if (result != 0) {
        return Result<void>::Failure(MapMultiCardError(result, "MC_Stop"));
    }

    return Result<void>::Success();
}

Result<void> MultiCardAdapter::TriggerCompareOutput(uint16_t channel_mask,
                                                    int32_t output_mode,
                                                    int32_t output_pulse_count,
                                                    int32_t pulse_width_us,
                                                    int32_t level_before,
                                                    int32_t level_during,
                                                    int32_t level_after) {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    if (!is_connected_) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Not connected to hardware", "MultiCardAdapter"));
    }

    short result = multicard_->MC_CmpPluse(static_cast<short>(channel_mask),
                                           static_cast<long>(output_mode),
                                           static_cast<long>(output_pulse_count),
                                           static_cast<long>(pulse_width_us),
                                           static_cast<long>(level_before),
                                           static_cast<long>(level_during),
                                           static_cast<long>(level_after));

    if (result != 0) {
        return Result<void>::Failure(MapMultiCardError(result, "MC_CmpPluse"));
    }

    return Result<void>::Success();
}

// ============================================================
// 状态查询实现
// ============================================================

Result<AxisStatusStruct> MultiCardAdapter::GetAxisStatus(LogicalAxisId axis_id) const {
    const int32_t axis_index = static_cast<int32_t>(ToIndex(axis_id));
    SILIGEN_LOG_DEBUG_FMT_HELPER("GetAxisStatus called for axis %d", axis_index);

    std::lock_guard<std::mutex> lock(hardware_mutex_);

    auto validate_result = ValidateAxisId(axis_id);
    if (!validate_result.IsSuccess()) {
        return Result<AxisStatusStruct>::Failure(validate_result.GetError());
    }

    if (!is_connected_) {
        return Result<AxisStatusStruct>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Not connected to hardware", "MultiCardAdapter"));
    }

    auto status_result = GetMultiCardAxisStatus(axis_id);
    if (!status_result.IsSuccess()) {
        SILIGEN_LOG_ERROR_FMT_HELPER("Failed to get MultiCard status for axis %d", axis_index);
        return Result<AxisStatusStruct>::Failure(status_result.GetError());
    }

    AxisStatusStruct status = ConvertAxisStatus(axis_id, status_result.Value());

    // 获取位置信息
    double pos = 0.0;
    // 注意: MultiCard SDK 轴号从1开始, 而我们的接口使用0-1
    // 使用类型安全转换: LogicalAxisId (0-based) -> SdkAxisId (1-based)
    short sdk_axis = ToSdkShort(ToSdkAxis(axis_id));
    short result = multicard_->MC_GetPrfPos(sdk_axis, &pos);
    if (result == 0) {
        status.current_position = PulseToMm(static_cast<int32_t>(pos));
        SILIGEN_LOG_DEBUG_FMT_HELPER("Axis %d (SDK: %d) position: %.2fmm (pulse: %.0f)",
                                     axis_index, sdk_axis, status.current_position, pos);
    } else {
        SILIGEN_LOG_ERROR_FMT_HELPER("MC_GetPrfPos failed for axis %d (SDK: %d) with code %d",
                                     axis_index, sdk_axis, result);
    }

    return Result<AxisStatusStruct>::Success(status);
}

Result<std::vector<AxisStatusStruct>> MultiCardAdapter::GetAllAxisStatus() const {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    if (!is_connected_) {
        return Result<std::vector<AxisStatusStruct>>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Not connected to hardware", "MultiCardAdapter"));
    }

    std::vector<AxisStatusStruct> all_status;

    for (int32_t axis = MIN_AXIS_ID; axis < axis_count_; ++axis) {
        auto axis_id = FromIndex(static_cast<int16>(axis));
        auto status_result = GetMultiCardAxisStatus(axis_id);
        if (status_result.IsSuccess()) {
            AxisStatusStruct status = ConvertAxisStatus(axis_id, status_result.Value());

            // 获取位置 - 使用类型安全转换
            double pos = 0.0;
            short sdk_axis = ToSdkShort(ToSdkAxis(axis_id));
            if (multicard_->MC_GetPrfPos(sdk_axis, &pos) == 0) {
                status.current_position = PulseToMm(static_cast<int32_t>(pos));
            }

            all_status.push_back(status);
        }
    }

    return Result<std::vector<AxisStatusStruct>>::Success(all_status);
}

Result<Point2D> MultiCardAdapter::GetCurrentPosition() const {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    if (!is_connected_) {
        return Result<Point2D>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "Not connected to hardware", "MultiCardAdapter"));
    }

    double x_pos = 0.0, y_pos = 0.0;

    // 注意: MultiCard SDK 轴号从1开始
    short x_axis = 1;  // X轴对应SDK轴1
    short y_axis = 2;  // Y轴对应SDK轴2

    short result = multicard_->MC_GetPrfPos(x_axis, &x_pos);
    if (result != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER("MC_GetPrfPos(X) failed with code %d", result);
        return Result<Point2D>::Failure(MapMultiCardError(result, "MC_GetPrfPos(X)"));
    }

    result = multicard_->MC_GetPrfPos(y_axis, &y_pos);
    if (result != 0) {
        SILIGEN_LOG_ERROR_FMT_HELPER("MC_GetPrfPos(Y) failed with code %d", result);
        return Result<Point2D>::Failure(MapMultiCardError(result, "MC_GetPrfPos(Y)"));
    }

    Point2D position;
    position.x = PulseToMm(static_cast<int32_t>(x_pos));
    position.y = PulseToMm(static_cast<int32_t>(y_pos));

    SILIGEN_LOG_DEBUG_FMT_HELPER("Current position: X:%.2fmm (pulse:%.0f), Y:%.2fmm (pulse:%.0f)",
                                 position.x, x_pos, position.y, y_pos);

    return Result<Point2D>::Success(position);
}

// ============================================================
// 辅助方法实现
// ============================================================

Result<void> MultiCardAdapter::ValidateAxisId(LogicalAxisId axis_id) const {
    const auto axis_index = static_cast<int32_t>(ToIndex(axis_id));
    if (!IsValid(axis_id) || axis_index < MIN_AXIS_ID || axis_index >= axis_count_) {
        std::ostringstream oss;
        oss << "Invalid axis ID: " << axis_index << " (valid range: " << MIN_AXIS_ID << "-" << (axis_count_ - 1) << ")";
        return Result<void>::Failure(Error(ErrorCode::INVALID_PARAMETER, oss.str(), "MultiCardAdapter"));
    }
    return Result<void>::Success();
}

Error MultiCardAdapter::MapMultiCardError(int32_t error_code, const std::string& operation) const {
    // 使用新模块里的错误映射器统一格式化厂商错误
    std::string formatted_msg = Siligen::Hal::Drivers::MultiCard::ErrorMapper::FormatErrorMessage(
        error_code,
        operation);

    // 根据错误类型映射到统一的 ErrorCode 枚举
    ErrorCode mapped_code;
    if (Siligen::Hal::Drivers::MultiCard::ErrorMapper::IsCommunicationError(error_code)) {
        mapped_code = ErrorCode::CONNECTION_FAILED;
    } else if (Siligen::Hal::Drivers::MultiCard::ErrorMapper::IsParameterError(error_code)) {
        mapped_code = ErrorCode::INVALID_PARAMETER;
    } else {
        mapped_code = ErrorCode::HARDWARE_CONNECTION_FAILED;
    }

    return Error(mapped_code, formatted_msg, "MultiCardAdapter");
}

AxisStatusStruct MultiCardAdapter::ConvertAxisStatus(LogicalAxisId axis_id, uint32_t mc_status) const {
    SILIGEN_LOG_DEBUG_FMT_HELPER("ConvertAxisStatus called for axis %d, mc_status=0x%X",
                                 ToIndex(axis_id),
                                 mc_status);

    AxisStatusStruct status;
    status.current_position = 0.0f;  // 由调用者填充
    status.target_position = 0.0f;
    status.current_velocity = 0.0f;

    // 解析MultiCard状态位
    // bit 3: 正限位
    // bit 4: 负限位
    // bit 8: 伺服使能
    // bit 10: 到位信号

    bool is_enabled = (mc_status & (1 << 8)) != 0;
    bool in_position = (mc_status & (1 << 10)) != 0;
    bool pos_limit = (mc_status & (1 << 3)) != 0;
    bool neg_limit = (mc_status & (1 << 4)) != 0;

    SILIGEN_LOG_DEBUG_FMT_HELPER("pos_limit=%d, neg_limit=%d", pos_limit, neg_limit);

    if (!is_enabled) {
        status.state = AxisState::DISABLED;
    } else if (pos_limit || neg_limit) {
        status.state = AxisState::FAULT;
        status.has_error = true;
    } else if (!in_position) {
        status.state = AxisState::MOVING;
    } else {
        status.state = AxisState::ENABLED;
    }

    status.is_homed = false;  // 需要额外状态位判断
    status.has_error = (status.state == AxisState::FAULT);
    status.last_error_code = status.has_error ? ErrorCode::POSITION_OUT_OF_RANGE : ErrorCode::SUCCESS;

    return status;
}

Result<uint32_t> MultiCardAdapter::GetMultiCardAxisStatus(LogicalAxisId axis_id) const {
    long status = 0;
    unsigned long clock = 0;

    // 注意: MultiCard SDK 轴号从1开始, 而我们的接口使用0-1
    // 使用类型安全转换: LogicalAxisId (0-based) -> SdkAxisId (1-based)
    short sdk_axis = ToSdkShort(ToSdkAxis(axis_id));
    short result = multicard_->MC_GetSts(sdk_axis, &status, 1, &clock);

    if (result != 0) {
        return Result<uint32_t>::Failure(MapMultiCardError(result, "MC_GetSts"));
    }

    return Result<uint32_t>::Success(static_cast<uint32_t>(status));
}

int32_t MultiCardAdapter::MmToPulse(float mm) const {
    return unit_converter_.MmToPulse(mm);
}

float MultiCardAdapter::PulseToMm(int32_t pulse) const {
    return unit_converter_.PulseToMm(pulse);
}

Result<void> MultiCardAdapter::WaitForAxisArrive(LogicalAxisId axis_id, int32_t timeout_ms) const {
    auto start = std::chrono::steady_clock::now();

    while (true) {
        auto status_result = GetMultiCardAxisStatus(axis_id);
        if (!status_result.IsSuccess()) {
            return Result<void>::Failure(status_result.GetError());
        }

        uint32_t status = status_result.Value();
        bool in_position = (status & (1 << 10)) != 0;

        if (in_position) {
            return Result<void>::Success();
        }

        auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

        if (elapsed >= timeout_ms) {
            return Result<void>::Failure(
                Error(ErrorCode::MOTION_TIMEOUT, "Axis did not reach target position in time", "MultiCardAdapter"));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

Result<void> MultiCardAdapter::InitializeDefaultCoordinateSystem() {
    constexpr short kDefaultCoordSys = 1;
    constexpr int kMaxProfileAxes = 8;
    constexpr int kDefaultLookAheadNum = 200;

    short profile[kMaxProfileAxes] = {0};
    const short dimension = static_cast<short>(std::max(1, std::min(axis_count_, kMaxProfileAxes)));

    for (short i = 0; i < dimension; ++i) {
        profile[i] = static_cast<short>(i + 1);
    }

    const double syn_vel_max = unit_converter_.VelocityMmSToPS(max_speed_) / 1000.0;
    const double syn_acc_max = unit_converter_.AccelerationMmS2ToPS2(max_acceleration_) / 1000000.0;

    SILIGEN_LOG_INFO("初始化默认坐标系: crd=1, dimension=" + std::to_string(dimension) +
                     ", synVelMax=" + std::to_string(syn_vel_max) +
                     ", synAccMax=" + std::to_string(syn_acc_max));

    int set_prm_result = multicard_->MC_SetCrdPrm(kDefaultCoordSys, dimension, profile, syn_vel_max, syn_acc_max);
    if (set_prm_result != 0) {
        return Result<void>::Failure(MapMultiCardError(set_prm_result, "MC_SetCrdPrm (default coord init)"));
    }

    double speed_max[kMaxProfileAxes] = {0};
    double acc_max[kMaxProfileAxes] = {0};
    double max_step_speed[kMaxProfileAxes] = {0};
    double scale[kMaxProfileAxes] = {0};

    for (short i = 0; i < dimension; ++i) {
        speed_max[i] = syn_vel_max;
        acc_max[i] = syn_acc_max;
        max_step_speed[i] = syn_vel_max * 0.1;
        scale[i] = 1.0;
    }

    int lookahead_result = multicard_->MC_InitLookAhead(kDefaultCoordSys,
                                                        0,
                                                        kDefaultLookAheadNum,
                                                        speed_max,
                                                        acc_max,
                                                        max_step_speed,
                                                        scale);
    if (lookahead_result != 0) {
        return Result<void>::Failure(MapMultiCardError(lookahead_result, "MC_InitLookAhead (default coord init)"));
    }

    return Result<void>::Success();
}

}  // namespace Hardware
}  // namespace Infrastructure
}  // namespace Siligen


