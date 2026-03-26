#pragma once

#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <vector>

// 导入共享类型
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

// 标准类型定义
using int16 = std::int16_t;
using int32 = std::int32_t;
using uint32 = std::uint32_t;

namespace Siligen::Domain::Motion::Ports {

/**
 * @brief 插补类型枚举
 */
enum class InterpolationType {
    LINEAR,        // 直线插补
    CIRCULAR_CW,   // 顺时针圆弧插补
    CIRCULAR_CCW,  // 逆时针圆弧插补
    SPIRAL,        // 螺旋插补
    BUFFER_STOP    // 缓冲区停止
};

/**
 * @brief 坐标系参数
 */
struct CoordinateSystemConfig {
    int16 dimension = 2;          // 坐标系维度 (2-4)
    std::vector<LogicalAxisId> axis_map;  // 轴映射表 (0-based)
    float32 max_velocity = 0.0f;
    float32 max_acceleration = 0.0f;
    int16 look_ahead_segments = 100;  // 前瞻段数
    bool look_ahead_enabled = true;
};

/**
 * @brief 插补运动数据
 */
struct InterpolationData {
    InterpolationType type = InterpolationType::LINEAR;
    std::vector<float32> positions;  // 各轴目标位置
    float32 velocity = 0.0f;
    float32 acceleration = 0.0f;
    float32 end_velocity = 0.0f;  // 结束速度
    union {
        struct {
            float32 center_x, center_y;
        };  // 圆弧中心 (X-Y平面)
        struct {
            float32 radius;
        };  // 圆弧半径
        struct {
            float32 spiral_pitch;
        };  // 螺旋螺距
    };
    int16 circle_plane = 0;  // 圆弧平面: 0=XY, 1=YZ, 2=ZX
};

/**
 * @brief 坐标系运动状态枚举
 */
enum class CoordinateSystemState {
    IDLE,         // 空闲
    MOVING,       // 运动中
    PAUSED,       // 暂停
    ERROR_STATE   // 错误 (renamed to avoid Windows ERROR macro conflict)
};

/**
 * @brief 坐标系状态信息
 */
struct CoordinateSystemStatus {
    CoordinateSystemState state = CoordinateSystemState::IDLE;
    bool is_moving = false;           // 是否运动中
    uint32 remaining_segments = 0;    // 剩余段数
    float32 current_velocity = 0.0f;  // 当前速度
    int32 raw_status_word = 0;        // MC_CrdStatus 原始状态字
    int32 raw_segment = 0;            // MC_CrdStatus 原始剩余段数
    int32 mc_status_ret = 0;          // MC_CrdStatus 返回码
};

/**
 * @brief 插补控制端口接口
 * 定义坐标系插补运动控制操作
 * 符合接口隔离原则(ISP) - 客户端仅依赖插补控制功能
 */
class IInterpolationPort {
   public:
    virtual ~IInterpolationPort() = default;

    /**
     * @brief 配置坐标系
     * @param coord_sys 坐标系号 (1-16)
     * @param config 坐标系参数
     */
    virtual Result<void> ConfigureCoordinateSystem(int16 coord_sys, const CoordinateSystemConfig& config) = 0;

    /**
     * @brief 添加插补数据
     * @param coord_sys 坐标系号
     * @param data 插补数据
     */
    virtual Result<void> AddInterpolationData(int16 coord_sys, const InterpolationData& data) = 0;

    /**
     * @brief 清空坐标系插补缓冲区
     * @param coord_sys 坐标系号
     */
    virtual Result<void> ClearInterpolationBuffer(int16 coord_sys) = 0;

    /**
     * @brief 发送插补数据流结束标识（将前瞻数据压入控制器）
     * @param coord_sys 坐标系号
     */
    virtual Result<void> FlushInterpolationData(int16 coord_sys) = 0;

    /**
     * @brief 启动坐标系运动
     * @param coord_sys_mask 坐标系掩码
     */
    virtual Result<void> StartCoordinateSystemMotion(uint32 coord_sys_mask) = 0;

    /**
     * @brief 停止坐标系运动
     * @param coord_sys_mask 坐标系掩码
     */
    virtual Result<void> StopCoordinateSystemMotion(uint32 coord_sys_mask) = 0;

    /**
     * @brief 设置坐标系速度覆盖
     * @param coord_sys 坐标系号
     * @param override_percent 速度覆盖百分比 (0-200%)
     */
    virtual Result<void> SetCoordinateSystemVelocityOverride(int16 coord_sys, float32 override_percent) = 0;

    /**
     * @brief 启用坐标系 S 曲线（加加速度限制）
     * @param coord_sys 坐标系号
     * @param jerk 加加速度参数
     */
    virtual Result<void> EnableCoordinateSystemSCurve(int16 coord_sys, float32 jerk) = 0;

    /**
     * @brief 关闭坐标系 S 曲线
     * @param coord_sys 坐标系号
     */
    virtual Result<void> DisableCoordinateSystemSCurve(int16 coord_sys) = 0;

    /**
     * @brief 设置坐标系恒定线速度模式（角点不断速）
     * @param coord_sys 坐标系号
     * @param enabled 是否启用
     * @param rotate_axis_mask 旋转轴掩码（不需要可为 0）
     */
    virtual Result<void> SetConstLinearVelocityMode(int16 coord_sys, bool enabled, uint32 rotate_axis_mask) = 0;

    /**
     * @brief 获取坐标系插补缓冲区剩余空间
     * @param coord_sys 坐标系号
     * @return 剩余空间
     */
    virtual Result<uint32> GetInterpolationBufferSpace(int16 coord_sys) const = 0;

    /**
     * @brief 获取坐标系前瞻缓冲区剩余空间
     * @param coord_sys 坐标系号
     * @return 剩余空间
     */
    virtual Result<uint32> GetLookAheadBufferSpace(int16 coord_sys) const = 0;

    /**
     * @brief 获取坐标系状态
     * @param coord_sys 坐标系号
     * @return 坐标系状态信息
     */
    virtual Result<CoordinateSystemStatus> GetCoordinateSystemStatus(int16 coord_sys) const = 0;
};

}  // namespace Siligen::Domain::Motion::Ports

