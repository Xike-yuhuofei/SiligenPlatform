#pragma once

#include "DomainEvent.h"
#include "Point2D.h"

#include <sstream>

namespace Siligen::Shared::Types {

/**
 * @brief 位置变化事件
 *
 * 当机械臂或轴的位置发生变化时触发的领域事件。
 * 包含旧位置和新位置信息,用于追踪运动轨迹。
 */
class PositionChangedEvent : public DomainEvent {
   private:
    Point2D old_position_;
    Point2D new_position_;

   public:
    /**
     * @brief 构造位置变化事件
     *
     * @param old_pos 旧位置
     * @param new_pos 新位置
     */
    PositionChangedEvent(const Point2D& old_pos, const Point2D& new_pos)
        : DomainEvent(), old_position_(old_pos), new_position_(new_pos) {}

    /**
     * @brief 获取旧位置
     *
     * @return 旧位置的常量引用
     */
    const Point2D& GetOldPosition() const {
        return old_position_;
    }

    /**
     * @brief 获取新位置
     *
     * @return 新位置的常量引用
     */
    const Point2D& GetNewPosition() const {
        return new_position_;
    }

    /**
     * @brief 计算位置变化距离
     *
     * @return 位置变化的欧几里得距离
     */
    float GetDistance() const {
        return old_position_.DistanceTo(new_position_);
    }

    /**
     * @brief 计算X轴变化量
     *
     * @return X轴变化量
     */
    float GetDeltaX() const {
        return new_position_.x - old_position_.x;
    }

    /**
     * @brief 计算Y轴变化量
     *
     * @return Y轴变化量
     */
    float GetDeltaY() const {
        return new_position_.y - old_position_.y;
    }

    /**
     * @brief 获取事件类型名称
     *
     * @return 事件类型字符串
     */
    std::string GetEventType() const override {
        return "PositionChangedEvent";
    }

    /**
     * @brief 转换为字符串表示
     *
     * @return 事件的详细字符串表示
     */
    std::string ToString() const override {
        std::ostringstream oss;
        oss << CreateBaseEventString();
        oss << ", OldPosition: " << old_position_.ToString();
        oss << ", NewPosition: " << new_position_.ToString();
        oss << ", Distance: " << GetDistance() << "mm";
        oss << " }";
        return oss.str();
    }
};

}  // namespace Siligen::Shared::Types
