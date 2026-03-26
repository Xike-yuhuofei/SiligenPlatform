#pragma once

#include "DomainEvent.h"

#include <sstream>
#include <string>

namespace Siligen::Shared::Types {

/**
 * @brief 紧急停止事件
 *
 * 当系统触发紧急停止时发出的领域事件。
 * 包含停止原因,用于记录和分析紧急情况。
 */
class EmergencyStopEvent : public DomainEvent {
   private:
    std::string reason_;
    std::string triggered_by_;  // 触发者 (用户/系统/硬件)

   public:
    /**
     * @brief 构造紧急停止事件
     *
     * @param reason 停止原因
     * @param triggered_by 触发者 (默认为"System")
     */
    explicit EmergencyStopEvent(const std::string& reason = "未知原因", const std::string& triggered_by = "System")
        : DomainEvent(), reason_(reason), triggered_by_(triggered_by) {}

    /**
     * @brief 获取停止原因
     *
     * @return 停止原因字符串
     */
    const std::string& GetReason() const {
        return reason_;
    }

    /**
     * @brief 获取触发者
     *
     * @return 触发者字符串
     */
    const std::string& GetTriggeredBy() const {
        return triggered_by_;
    }

    /**
     * @brief 检查是否由用户触发
     *
     * @return 是否由用户触发
     */
    bool IsUserTriggered() const {
        return triggered_by_ == "User";
    }

    /**
     * @brief 检查是否由系统自动触发
     *
     * @return 是否由系统触发
     */
    bool IsSystemTriggered() const {
        return triggered_by_ == "System";
    }

    /**
     * @brief 检查是否由硬件故障触发
     *
     * @return 是否由硬件触发
     */
    bool IsHardwareTriggered() const {
        return triggered_by_ == "Hardware";
    }

    /**
     * @brief 获取事件类型名称
     *
     * @return 事件类型字符串
     */
    std::string GetEventType() const override {
        return "EmergencyStopEvent";
    }

    /**
     * @brief 转换为字符串表示
     *
     * @return 事件的详细字符串表示
     */
    std::string ToString() const override {
        std::ostringstream oss;
        oss << CreateBaseEventString();
        oss << ", Reason: \"" << reason_ << "\"";
        oss << ", TriggeredBy: " << triggered_by_;
        oss << " }";
        return oss.str();
    }

    /**
     * @brief 创建预定义的紧急停止事件
     */
    static EmergencyStopEvent UserTriggered(const std::string& reason = "用户手动触发紧急停止") {
        return EmergencyStopEvent(reason, "User");
    }

    static EmergencyStopEvent HardwareError(const std::string& reason) {
        return EmergencyStopEvent(reason, "Hardware");
    }

    static EmergencyStopEvent SafetyViolation(const std::string& reason) {
        return EmergencyStopEvent(reason, "System");
    }
};

}  // namespace Siligen::Shared::Types
