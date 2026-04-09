#pragma once

#include "shared/types/Result.h"
#include "shared/types/Types.h"

namespace Siligen::Domain::Motion::ValueObjects {
enum class HomingStage;
}

// 导入共享类型
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;
using Siligen::Domain::Motion::ValueObjects::HomingStage;

namespace Siligen::Domain::Motion::Ports {

/**
 * @brief 回零状态
 */
enum class HomingState {
    NOT_HOMED,  // 未回零
    HOMING,     // 回零中
    HOMED,      // 已回零
    FAILED      // 回零失败
};

/**
 * @brief 回零状态结构
 */
struct HomingStatus {
    LogicalAxisId axis = LogicalAxisId::X;
    HomingState state = HomingState::NOT_HOMED;
    HomingStage stage{};
    float32 current_position = 0.0f;
    int32 error_code = 0;
    bool is_searching = false;
};

/**
 * @brief 回零控制端口接口
 * 定义回零操作的核心功能
 */
class IHomingPort {
   public:
    virtual ~IHomingPort() = default;

    // 回零操作
    virtual Result<void> HomeAxis(LogicalAxisId axis) = 0;

    // 回零控制
    virtual Result<void> StopHoming(LogicalAxisId axis) = 0;
    virtual Result<void> ResetHomingState(LogicalAxisId axis) = 0;

    // 状态查询
    virtual Result<HomingStatus> GetHomingStatus(LogicalAxisId axis) const = 0;
    virtual Result<bool> IsAxisHomed(LogicalAxisId axis) const = 0;
    virtual Result<bool> IsHomingInProgress(LogicalAxisId axis) const = 0;

    // 等待回零完成
    virtual Result<void> WaitForHomingComplete(LogicalAxisId axis, int32 timeout_ms = 80000) = 0;
};

}  // namespace Siligen::Domain::Motion::Ports

