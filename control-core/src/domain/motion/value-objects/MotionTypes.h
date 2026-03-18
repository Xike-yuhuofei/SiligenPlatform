#pragma once

#include "shared/types/AxisTypes.h"

#include <boost/describe/enum.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Siligen {
namespace Domain {
namespace Motion {
namespace ValueObjects {

/**
 * @brief 点动方向
 */
enum class JogDirection { Positive, Negative };
BOOST_DESCRIBE_ENUM(JogDirection, Positive, Negative)

/**
 * @brief 运动模式
 */
enum class MotionMode { Idle, Jog, Position, Trajectory, Homing, Stopped, Error };

/**
 * @brief 回零阶段
 */
enum class HomingStage { Idle, SearchingHome, BackingOff, FindingIndex, SettingPosition, Completed, Failed };


/**
 * @brief 轴限位状态
 */
struct AxisLimitStatus {
    bool positive;
    bool negative;
    bool home;
    bool softPositive;
    bool softNegative;

    AxisLimitStatus() : positive(false), negative(false), home(false), softPositive(false), softNegative(false) {}
};

/**
 * @brief 位置坐标 (3D)
 */
struct Position3D {
    double x;
    double y;
    double z;

    Position3D() : x(0.0), y(0.0), z(0.0) {}

    Position3D(double xVal, double yVal, double zVal) : x(xVal), y(yVal), z(zVal) {}
};

/**
 * @brief 电机状态
 */
struct MotorStatus {
    std::optional<double> temperature;
    std::optional<double> current;
    std::optional<std::string> faultCode;
    std::string status;  // 'idle' | 'running' | 'fault' | 'disabled'

    MotorStatus() : status("idle") {}
};

/**
 * @brief 轴状态
 */
struct AxisStatus {
    Siligen::Shared::Types::LogicalAxisId axisId;
    bool enabled;
    double position;
    double velocity;
    bool isHomed;
    AxisLimitStatus limitStatus;
    MotorStatus motorStatus;

    AxisStatus()
        : axisId(Siligen::Shared::Types::LogicalAxisId::X),
          enabled(false),
          position(0.0),
          velocity(0.0),
          isHomed(false) {}
};

/**
 * @brief 运动状态
 */
struct MotionStatus {
    bool isMoving;
    Position3D currentPosition;
    Position3D targetPosition;
    double velocity;
    double acceleration;
    MotionMode motionMode;
    double estimatedTimeToTarget;

    MotionStatus()
        : isMoving(false), velocity(0.0), acceleration(0.0), motionMode(MotionMode::Idle), estimatedTimeToTarget(0.0) {}
};

/**
 * @brief 硬件限位状态
 */
struct HardwareLimitStatus {
    AxisLimitStatus x;
    AxisLimitStatus y;
    AxisLimitStatus z;
};

/**
 * @brief 安全状态
 */
struct SafetyStatus {
    bool emergencyStop;
    bool limitsEnabled;
    bool softLimitsActive;
    HardwareLimitStatus hardwareLimits;

    SafetyStatus() : emergencyStop(false), limitsEnabled(true), softLimitsActive(true) {}
};

/**
 * @brief 测试会话状态
 */
struct TestSessionStatus {
    std::optional<std::string> activeSession;
    int sessionCount;
    int completedCount;

    TestSessionStatus() : sessionCount(0), completedCount(0) {}
};

}  // namespace ValueObjects
}  // namespace Motion
}  // namespace Domain
}  // namespace Siligen
