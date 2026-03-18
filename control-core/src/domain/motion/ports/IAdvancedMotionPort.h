#pragma once

#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <string>

// 导入共享类型
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

// 标准类型定义
using int16 = std::int16_t;

namespace Siligen::Domain::Motion::Ports {

/**
 * @brief 运动模式枚举
 */
enum class MotionMode {
    POINT_TO_POINT,  // 点位运动模式 (梯形速度曲线)
    JOG,             // JOG连续运动模式
    INTERPOLATION,   // 插补运动模式
    GEAR,            // 电子齿轮模式
    CAM,             // 凸轮模式
    HANDWHEEL        // 手轮模式
};

/**
 * @brief 手轮配置
 */
struct HandwheelConfig {
    LogicalAxisId selected_axis = LogicalAxisId::X;  // 当前选中的轴
    int16 multiplier = 1;                   // 倍率 (1, 10, 100)
    float32 resolutionMmPerPulse = 0.001f;  // 分辨率
    bool enabled = false;
};

/**
 * @brief 高级运动控制端口接口
 * 定义高级功能: 手轮控制、编码器、串口通信、调试日志、运动模式
 * 符合接口隔离原则(ISP) - 客户端仅依赖高级功能
 */
class IAdvancedMotionPort {
   public:
    virtual ~IAdvancedMotionPort() = default;

    // ========== 手轮控制 ==========

    /**
     * @brief 进入手轮模式
     */
    virtual Result<void> EnterHandwheelMode() = 0;

    /**
     * @brief 退出手轮模式
     */
    virtual Result<void> ExitHandwheelMode() = 0;

    /**
     * @brief 配置手轮
     */
    virtual Result<void> ConfigureHandwheel(const HandwheelConfig& config) = 0;

    /**
     * @brief 选择手轮控制轴
     * @param axis 轴号
     */
    virtual Result<void> SelectHandwheelAxis(LogicalAxisId axis) = 0;

    /**
     * @brief 设置手轮倍率
     * @param multiplier 倍率 (1, 10, 100)
     */
    virtual Result<void> SetHandwheelMultiplier(int16 multiplier) = 0;

    // ========== 编码器控制 ==========

    /**
     * @brief 使能编码器
     * @param axis 轴号
     */
    virtual Result<void> EnableEncoder(LogicalAxisId axis) = 0;

    /**
     * @brief 禁用编码器
     * @param axis 轴号
     */
    virtual Result<void> DisableEncoder(LogicalAxisId axis) = 0;

    /**
     * @brief 读取编码器位置
     * @param axis 轴号
     */
    virtual Result<float32> ReadEncoderPosition(LogicalAxisId axis) = 0;

    // ========== 串口通信 ==========

    /**
     * @brief 配置串口
     * @param uart_id 串口号
     * @param baud_rate 波特率
     * @param data_bits 数据位
     * @param stop_bits 停止位
     * @param parity 校验位
     */
    virtual Result<void> ConfigureUART(
        int16 uart_id, int32 baud_rate, int16 data_bits = 8, int16 stop_bits = 1, char parity = 'N') = 0;

    /**
     * @brief 发送串口数据
     * @param uart_id 串口号
     * @param data 数据
     */
    virtual Result<void> SendUARTData(int16 uart_id, const std::string& data) = 0;

    /**
     * @brief 接收串口数据
     * @param uart_id 串口号
     * @param max_length 最大接收长度
     */
    virtual Result<std::string> ReceiveUARTData(int16 uart_id, int16 max_length = 256) = 0;

    // ========== 调试和日志 ==========

    /**
     * @brief 启动调试日志
     * @param log_file_path 日志文件路径
     */
    virtual Result<void> StartDebugLog(const std::string& log_file_path) = 0;

    /**
     * @brief 停止调试日志
     */
    virtual Result<void> StopDebugLog() = 0;

    /**
     * @brief 获取系统错误信息
     */
    virtual Result<std::string> GetSystemErrorMessage() const = 0;

    // ========== 运动模式切换 ==========

    /**
     * @brief 设置轴运动模式
     * @param axis 轴号
     * @param mode 运动模式
     */
    virtual Result<void> SetMotionMode(LogicalAxisId axis, MotionMode mode) = 0;

    /**
     * @brief 获取轴运动模式
     */
    virtual Result<MotionMode> GetMotionMode(LogicalAxisId axis) const = 0;
};

}  // namespace Siligen::Domain::Motion::Ports

