#pragma once

#include "domain/motion/value-objects/MotionTypes.h"
#include "shared/types/Types.h"

#include <boost/describe/enum.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace Siligen {
namespace Domain {
namespace Motion {
namespace ValueObjects {

// NOTE: JogDirection is owned by the canonical motion-planning MotionTypes surface.

/**
 * @brief 点动模式枚举
 */
enum class JogMode {
    Continuous,  // 连续点动（手动控制停止）
    Step         // 定长点动（移动固定距离）
};

/**
 * @brief 回零模式枚举
 */
enum class HomingTestMode {
    SingleAxis,     // 单轴独立回零
    MultiAxisSync,  // 多轴同步回零
    Sequential      // 多轴顺序回零
};
BOOST_DESCRIBE_ENUM(HomingTestMode, SingleAxis, MultiAxisSync, Sequential)

/**
 * @brief 回零方向枚举
 */
enum class HomingTestDirection {
    Positive,  // 正向回零
    Negative   // 反向回零
};
BOOST_DESCRIBE_ENUM(HomingTestDirection, Positive, Negative)

/**
 * @brief 回零状态枚举
 */
enum class HomingTestStatus {
    NotStarted,  // 未开始
    InProgress,  // 进行中
    Completed,   // 已完成
    Failed       // 失败
};

/**
 * @brief 触发动作枚举
 */
enum class TriggerAction {
    TurnOn,  // 开启输出
    TurnOff  // 关闭输出
};
BOOST_DESCRIBE_ENUM(TriggerAction, TurnOn, TurnOff)

/**
 * @brief 触发类型枚举
 */
enum class TriggerType {
    SinglePoint,  // 单点触发
    MultiPoint,   // 多点触发
    ZoneTrigger   // 区间触发
};
BOOST_DESCRIBE_ENUM(TriggerType, SinglePoint, MultiPoint, ZoneTrigger)

/**
 * @brief 限位开关状态
 */
struct LimitSwitchState {
    bool positiveLimitTriggered;
    bool negativeLimitTriggered;
    std::int64_t timestamp;  // 状态采样时间戳

    LimitSwitchState() : positiveLimitTriggered(false), negativeLimitTriggered(false), timestamp(0) {}
};

/**
 * @brief HOME输入状态
 */
struct HomeInputState {
    bool raw_level;          // 原始电平(未做极性转换)
    bool triggered;          // 触发状态(已考虑active_low)
    std::int64_t timestamp;  // 状态采样时间戳

    HomeInputState() : raw_level(false), triggered(false), timestamp(0) {}
};

/**
 * @brief 触发点配置
 */
struct TriggerPoint {
    Siligen::Shared::Types::LogicalAxisId axis;  // 触发轴
    double position;       // 触发位置(mm)
    int outputPort;        // 输出端口号
    TriggerAction action;  // 触发动作

    TriggerPoint()
        : axis(Siligen::Shared::Types::LogicalAxisId::X),
          position(0.0),
          outputPort(0),
          action(TriggerAction::TurnOn) {}

    TriggerPoint(Siligen::Shared::Types::LogicalAxisId ax, double pos, int port, TriggerAction act)
        : axis(ax), position(pos), outputPort(port), action(act) {}
};

/**
 * @brief 触发事件记录
 */
struct TriggerEvent {
    std::int64_t timestamp;       // 触发时间戳(微秒)
    int triggerPointId;           // 对应的触发点ID
    double actualPosition;        // 实际触发位置(mm)
    double positionError;         // 位置误差(mm)
    std::int32_t responseTimeUs;  // 响应时间(微秒)
    bool outputVerified;          // 输出状态已验证

    TriggerEvent()
        : timestamp(0),
          triggerPointId(0),
          actualPosition(0.0),
          positionError(0.0),
          responseTimeUs(0),
          outputVerified(false) {}
};

/**
 * @brief 回零结果
 */
struct HomingResult {
    bool success;
    std::int32_t durationMs;
    double finalPosition;                      // 回零完成后的位置
    std::optional<std::string> failureReason;  // 失败原因

    HomingResult() : success(false), durationMs(0), finalPosition(0.0) {}

    HomingResult(bool succ, std::int32_t dur, double pos, std::optional<std::string> reason = std::nullopt)
        : success(succ), durationMs(dur), finalPosition(pos), failureReason(reason) {}
};

}  // namespace ValueObjects
}  // namespace Motion
}  // namespace Domain
}  // namespace Siligen
