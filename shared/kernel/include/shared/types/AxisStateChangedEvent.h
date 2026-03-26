#pragma once

#include "AxisStatus.h"
#include "AxisTypes.h"
#include "DomainEvent.h"

#include <sstream>

namespace Siligen::Shared::Types {

/**
 * @brief 轴状态变化事件
 *
 * 当轴的状态发生变化时触发的领域事件。
 * 包含轴ID、旧状态和新状态信息,用于追踪轴状态转换。
 */
class AxisStateChangedEvent : public DomainEvent {
   private:
    LogicalAxisId axis_id_;
    AxisState old_state_;
    AxisState new_state_;
    std::string state_change_reason_;  // 状态变化原因

   public:
    /**
     * @brief 构造轴状态变化事件
     *
     * @param axis_id 轴ID
     * @param old_state 旧状态
     * @param new_state 新状态
     * @param reason 状态变化原因 (可选)
     */
    AxisStateChangedEvent(LogicalAxisId axis_id,
                          AxisState old_state,
                          AxisState new_state,
                          const std::string& reason = "")
        : DomainEvent(),
          axis_id_(axis_id),
          old_state_(old_state),
          new_state_(new_state),
          state_change_reason_(reason) {}

    /**
     * @brief 获取轴ID
     *
     * @return 轴ID
     */
    LogicalAxisId GetAxisId() const {
        return axis_id_;
    }

    /**
     * @brief 获取旧状态
     *
     * @return 旧状态枚举值
     */
    AxisState GetOldState() const {
        return old_state_;
    }

    /**
     * @brief 获取新状态
     *
     * @return 新状态枚举值
     */
    AxisState GetNewState() const {
        return new_state_;
    }

    /**
     * @brief 获取状态变化原因
     *
     * @return 状态变化原因字符串
     */
    const std::string& GetReason() const {
        return state_change_reason_;
    }

    /**
     * @brief 检查是否从正常状态变为错误状态
     *
     * @return 是否进入错误状态
     */
    bool IsEnteringErrorState() const {
        return old_state_ != AxisState::FAULT && new_state_ == AxisState::FAULT;
    }

    /**
     * @brief 检查是否从错误状态恢复
     *
     * @return 是否从错误状态恢复
     */
    bool IsRecoveringFromError() const {
        return old_state_ == AxisState::FAULT && new_state_ != AxisState::FAULT;
    }

    /**
     * @brief 检查是否开始运动
     *
     * @return 是否开始运动
     */
    bool IsStartingMotion() const {
        return old_state_ != AxisState::MOVING && new_state_ == AxisState::MOVING;
    }

    /**
     * @brief 检查是否停止运动
     *
     * @return 是否停止运动
     */
    bool IsStoppingMotion() const {
        return old_state_ == AxisState::MOVING && new_state_ != AxisState::MOVING;
    }

    /**
     * @brief 获取状态名称字符串
     *
     * @param state 轴状态枚举值
     * @return 状态名称
     */
    static std::string GetStateName(AxisState state) {
        switch (state) {
            case AxisState::DISABLED:
                return "DISABLED";
            case AxisState::ENABLED:
                return "ENABLED";
            case AxisState::HOMING:
                return "HOMING";
            case AxisState::MOVING:
                return "MOVING";
            case AxisState::FAULT:
                return "ERROR";
            case AxisState::EMERGENCY_STOP:
                return "EMERGENCY_STOP";
            default:
                return "UNKNOWN";
        }
    }

    /**
     * @brief 获取事件类型名称
     *
     * @return 事件类型字符串
     */
    std::string GetEventType() const override {
        return "AxisStateChangedEvent";
    }

    /**
     * @brief 转换为字符串表示
     *
     * @return 事件的详细字符串表示
     */
    std::string ToString() const override {
        std::ostringstream oss;
        oss << CreateBaseEventString();
        oss << ", AxisId: " << ToIndex(axis_id_);
        oss << ", OldState: " << GetStateName(old_state_);
        oss << ", NewState: " << GetStateName(new_state_);

        if (!state_change_reason_.empty()) {
            oss << ", Reason: \"" << state_change_reason_ << "\"";
        }

        oss << " }";
        return oss.str();
    }
};

}  // namespace Siligen::Shared::Types
