#pragma once

#include "shared/types/Result.h"
#include "shared/types/Types.h"

// 导入共享类型
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

// 标准类型定义
using int16 = std::int16_t;

namespace Siligen::Domain::Motion::Ports {

/**
 * @brief JOG运动参数
 */
struct JogParameters {
    float32 velocity = 0.0f;
    float32 acceleration = 0.0f;
    float32 deceleration = 0.0f;
    float32 smooth_time = 0.0f;
};

/**
 * @brief JOG控制端口接口
 * 定义JOG连续运动控制操作
 * 符合接口隔离原则(ISP) - 客户端仅依赖JOG控制功能
 */
class IJogControlPort {
   public:
    virtual ~IJogControlPort() = default;

    /**
     * @brief 开始JOG运动（连续模式）
     * @param axis 轴号
     * @param direction 方向 (1=正向, -1=负向)
     * @param velocity 运动速度
     */
    virtual Result<void> StartJog(LogicalAxisId axis, int16 direction, float32 velocity) = 0;

    /**
     * @brief 开始定长JOG运动（固定距离）
     * @param axis 轴号 (0-1)
     * @param direction 方向 (1=正向, -1=负向)
     * @param distance 运动距离 (mm)
     * @param velocity 运动速度 (mm/s)
     * @return Result<void> 成功返回Success，失败返回错误信息
     */
    virtual Result<void> StartJogStep(LogicalAxisId axis, int16 direction, float32 distance, float32 velocity) = 0;

    /**
     * @brief 停止JOG运动
     * @param axis 轴号
     */
    virtual Result<void> StopJog(LogicalAxisId axis) = 0;

    /**
     * @brief 设置JOG参数
     */
    virtual Result<void> SetJogParameters(LogicalAxisId axis, const JogParameters& params) = 0;
};

}  // namespace Siligen::Domain::Motion::Ports

