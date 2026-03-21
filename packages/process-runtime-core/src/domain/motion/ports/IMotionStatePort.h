#pragma once

#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <string>
#include <vector>

// 导入共享类型
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;

namespace Siligen::Domain::Motion::Ports {

/**
 * @brief 运动状态枚举
 */
enum class MotionState {
    IDLE,     // 空闲
    MOVING,   // 运动中
    STOPPED,  // 已停止
    FAULT,    // 错误状态
    HOMING,   // 回零中
    HOMED,    // 已回零
    ESTOP,    // 急停状态
    DISABLED  // 轴未使能
};

/**
 * @brief 运动状态结构
 */
struct MotionStatus {
    MotionState state = MotionState::IDLE;
    Point2D position{0, 0};
    float32 velocity = 0.0f;
    float32 acceleration = 0.0f;
    bool in_position = false;
    bool has_error = false;
    int32 error_code = 0;
    bool enabled = false;
    bool following_error = false;
    bool soft_limit_positive = false;
    bool soft_limit_negative = false;
    bool hard_limit_positive = false;
    bool hard_limit_negative = false;
    bool servo_alarm = false;
    bool home_signal = false;
    bool index_signal = false;
    float32 axis_position_mm = 0.0f;
    std::string selected_feedback_source = "encoder";
    float32 profile_position_mm = 0.0f;
    float32 encoder_position_mm = 0.0f;
    float32 profile_velocity_mm_s = 0.0f;
    float32 encoder_velocity_mm_s = 0.0f;
    int32 profile_position_ret = 0;
    int32 encoder_position_ret = 0;
    int32 profile_velocity_ret = 0;
    int32 encoder_velocity_ret = 0;
};

/**
 * @brief 运动状态查询端口接口
 * 定义运动状态查询操作
 * 符合接口隔离原则(ISP) - 客户端仅依赖状态查询功能
 */
class IMotionStatePort {
   public:
    virtual ~IMotionStatePort() = default;

    /**
     * @brief 获取当前2D位置
     */
    virtual Result<Point2D> GetCurrentPosition() const = 0;

    /**
     * @brief 获取轴位置
     */
    virtual Result<float32> GetAxisPosition(LogicalAxisId axis) const = 0;

    /**
     * @brief 获取轴速度
     */
    virtual Result<float32> GetAxisVelocity(LogicalAxisId axis) const = 0;

    /**
     * @brief 获取轴状态
     */
    virtual Result<MotionStatus> GetAxisStatus(LogicalAxisId axis) const = 0;

    /**
     * @brief 检查轴是否在运动
     */
    virtual Result<bool> IsAxisMoving(LogicalAxisId axis) const = 0;

    /**
     * @brief 检查轴是否到位
     */
    virtual Result<bool> IsAxisInPosition(LogicalAxisId axis) const = 0;

    /**
     * @brief 获取所有轴状态
     */
    virtual Result<std::vector<MotionStatus>> GetAllAxesStatus() const = 0;
};

}  // namespace Siligen::Domain::Motion::Ports

