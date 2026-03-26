#pragma once

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

namespace Siligen::Shared::Types {

/**
 * @brief 领域事件基类
 *
 * 所有领域事件的基类,提供事件ID和时间戳等基础功能。
 * 领域事件用于表示系统中发生的重要业务变化。
 */
class DomainEvent {
   private:
    std::string event_id_;
    std::chrono::system_clock::time_point timestamp_;

    /**
     * @brief 生成唯一事件ID
     *
     * @return 唯一的事件ID字符串
     */
    static std::string GenerateEventId() {
        static int counter = 0;
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

        std::ostringstream oss;
        oss << "EVT-" << millis << "-" << (++counter);
        return oss.str();
    }

   public:
    DomainEvent() : event_id_(GenerateEventId()), timestamp_(std::chrono::system_clock::now()) {}

    virtual ~DomainEvent() = default;

    /**
     * @brief 获取事件ID
     *
     * @return 事件ID
     */
    const std::string& GetEventId() const {
        return event_id_;
    }

    /**
     * @brief 获取事件时间戳
     *
     * @return 事件时间戳
     */
    const std::chrono::system_clock::time_point& GetTimestamp() const {
        return timestamp_;
    }

    /**
     * @brief 获取事件类型名称
     *
     * @return 事件类型字符串
     */
    virtual std::string GetEventType() const = 0;

    /**
     * @brief 转换为字符串表示
     *
     * @return 事件的字符串表示
     */
    virtual std::string ToString() const = 0;

    /**
     * @brief 获取格式化的时间戳字符串
     *
     * @return 格式化的时间戳
     */
    std::string GetFormattedTimestamp() const {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp_);
        std::tm tm;
        localtime_s(&tm, &time_t);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

        // 添加毫秒
        auto duration = timestamp_.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;
        oss << "." << std::setfill('0') << std::setw(3) << millis;

        return oss.str();
    }

   protected:
    /**
     * @brief 创建基础事件信息字符串
     *
     * @return 包含事件ID和时间戳的基础信息
     */
    std::string CreateBaseEventString() const {
        std::ostringstream oss;
        oss << "Event { ";
        oss << "ID: " << event_id_ << ", ";
        oss << "Type: " << GetEventType() << ", ";
        oss << "Timestamp: " << GetFormattedTimestamp();
        return oss.str();
    }
};

}  // namespace Siligen::Shared::Types
