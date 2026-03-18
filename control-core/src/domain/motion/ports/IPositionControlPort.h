#pragma once

#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <vector>

// 导入共享类型
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;

namespace Siligen::Domain::Motion::Ports {

/**
 * @brief 运动命令结构
 */
struct MotionCommand {
    LogicalAxisId axis;  // 逻辑轴号 (0-based)
    float32 position;  // 目标位置
    float32 velocity;  // 运动速度
    bool relative;     // 是否相对运动
};

/**
 * @brief 位置控制端口接口
 * 定义位置运动控制和运动停止操作
 * 符合接口隔离原则(ISP) - 客户端仅依赖位置控制功能
 */
class IPositionControlPort {
   public:
    virtual ~IPositionControlPort() = default;

    // ========== 位置控制 ==========

    /**
     * @brief 2D点位运动
     * @param position 目标位置
     * @param velocity 运动速度
     */
    virtual Result<void> MoveToPosition(const Point2D& position, float32 velocity) = 0;

    /**
     * @brief 单轴点位运动
     * @param axis 轴号
     * @param position 目标位置
     * @param velocity 运动速度
     */
    virtual Result<void> MoveAxisToPosition(LogicalAxisId axis, float32 position, float32 velocity) = 0;

    /**
     * @brief 相对运动
     * @param axis 轴号
     * @param distance 相对距离
     * @param velocity 运动速度
     */
    virtual Result<void> RelativeMove(LogicalAxisId axis, float32 distance, float32 velocity) = 0;

    /**
     * @brief 多轴同步运动
     * @param commands 运动命令列表
     */
    virtual Result<void> SynchronizedMove(const std::vector<MotionCommand>& commands) = 0;

    // ========== 运动控制 ==========

    /**
     * @brief 停止轴运动
     * @param axis 轴号
     * @param immediate 是否立即停止 (true=急停, false=平滑停止)
     */
    virtual Result<void> StopAxis(LogicalAxisId axis, bool immediate = false) = 0;

    /**
     * @brief 停止所有轴运动
     * @param immediate 是否立即停止
     */
    virtual Result<void> StopAllAxes(bool immediate = false) = 0;

    /**
     * @brief 急停
     */
    virtual Result<void> EmergencyStop() = 0;

    /**
     * @brief 等待运动完成
     * @param axis 轴号
     * @param timeout_ms 超时时间(毫秒)
     */
    virtual Result<void> WaitForMotionComplete(LogicalAxisId axis, int32 timeout_ms = 60000) = 0;
};

}  // namespace Siligen::Domain::Motion::Ports

