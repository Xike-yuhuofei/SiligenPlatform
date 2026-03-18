// MemorySink.h - 内存Sink (存储日志到内存供UI查询)
// Phase 1.2: 六边形架构日志系统最佳实践
//
// 设计原则:
// - 继承spdlog::base_sink，实现自定义sink
// - 线程安全的环形缓冲区
// - 支持按级别和分类过滤查询

#pragma once

#include "spdlog/common.h"
#include "spdlog/details/log_msg.h"
#include "spdlog/sinks/base_sink.h"
#include "shared/types/LogTypes.h"
#include <deque>
#include <mutex>
#include <sstream>
#include <string>

namespace Siligen {
namespace Infrastructure {
namespace Logging {
namespace Sinks {

/// @brief 内存Sink - 存储日志到内存供UI查询
/// @details 实现spdlog的sink接口，支持GetRecentLogs查询
///
/// 功能特性:
/// - 环形缓冲区，自动清理旧日志
/// - 线程安全 (使用base_sink的Mutex模板参数)
/// - 支持按级别过滤
/// - 支持按分类(category)过滤
///
/// 使用示例:
/// @code
/// auto memory_sink = std::make_shared<MemorySink_mt>(1000);
/// auto logger = std::make_shared<spdlog::logger>("my_logger", memory_sink);
///
/// // 查询最近100条INFO及以上级别的日志
/// auto logs = memory_sink->GetRecent(100, LogLevel::INFO, "");
/// @endcode
template<typename Mutex>
class MemorySink : public spdlog::sinks::base_sink<Mutex> {
public:
    /// @brief 构造函数
    /// @param max_size 最大缓冲区大小 (条目数量)
    explicit MemorySink(size_t max_size = 1000)
        : max_size_(max_size) {
        // deque 没有 reserve() 方法，不需要预分配
    }

    /// @brief 析构函数
    virtual ~MemorySink() = default;

    /// @brief 获取最近的日志
    /// @param count 返回条目数量
    /// @param min_level 最小日志级别过滤
    /// @param category_filter 分类过滤器 (空字符串=不过滤)
    /// @return 日志条目列表 (按时间倒序)
    std::vector<Shared::Types::LogEntry> GetRecent(
        int32_t count,
        Shared::Types::LogLevel min_level = static_cast<Shared::Types::LogLevel>(0),
        const std::string& category_filter = "") {
        // 注意: spdlog::logger 已经保证了线程安全
        // 这里不需要额外的锁定，直接读取 entries_
        std::vector<Shared::Types::LogEntry> result;

        // 从后往前遍历 (最新的在前)
        for (auto it = entries_.rbegin();
             it != entries_.rend() && result.size() < static_cast<size_t>(count);
             ++it) {

            // 级别过滤
            if (it->level >= min_level) {
                // 分类过滤
                if (category_filter.empty() || it->category == category_filter) {
                    result.push_back(*it);
                }
            }
        }

        return result;
    }

    /// @brief 获取所有日志
    /// @return 所有缓存的日志条目
    std::vector<Shared::Types::LogEntry> GetAll() {
        return std::vector<Shared::Types::LogEntry>(entries_.begin(), entries_.end());
    }

    /// @brief 清空日志缓冲区
    void Clear() {
        entries_.clear();
    }

    /// @brief 获取当前缓冲区大小
    size_t GetSize() {
        return entries_.size();
    }

    /// @brief 获取最大缓冲区大小
    size_t GetMaxSize() const {
        return max_size_;
    }

    /// @brief 设置最大缓冲区大小
    /// @param max_size 新的最大大小
    /// @note 如果当前大小超过新大小，将裁剪到新大小
    void SetMaxSize(size_t max_size) {
        max_size_ = max_size;

        // 裁剪到新大小
        while (entries_.size() > max_size_) {
            entries_.pop_front();
        }
    }

protected:
    /// @brief 处理日志消息 (spdlog接口)
    /// @param msg 日志消息
    void sink_it_(const spdlog::details::log_msg& msg) override {
        Shared::Types::LogEntry entry;
        entry.level = ConvertLevel(msg.level);

        // 从消息中提取category和实际消息内容
        std::string message = std::string(msg.payload.begin(), msg.payload.end());
        entry.category = "General";  // 默认分类

        // 尝试从消息开头解析 [Category] 前缀
        if (message.size() > 2 && message[0] == '[') {
            size_t end_bracket = message.find(']');
            if (end_bracket != std::string::npos && end_bracket > 1) {
                std::string potential_category = message.substr(1, end_bracket - 1);
                // 检查是否为有效的category名称 (不含空格和控制字符)
                if (potential_category.find_first_of(" \t\n\r") == std::string::npos) {
                    entry.category = potential_category;
                    // 移除 [Category] 前缀和后面的空格
                    if (message.size() > end_bracket + 1 && message[end_bracket + 1] == ' ') {
                        entry.message = message.substr(end_bracket + 2);
                    } else {
                        entry.message = message.substr(end_bracket + 1);
                    }
                } else {
                    entry.message = message;
                }
            } else {
                entry.message = message;
            }
        } else {
            entry.message = message;
        }

        entry.timestamp = msg.time;
        entry.thread_id = GetThreadId(msg.thread_id);

        // 注意: spdlog::base_sink 已经在调用 sink_it_ 之前锁定了 mutex
        // 这里直接修改 entries_ 即可，不需要再次锁定
        entries_.push_back(entry);

        // 环形缓冲: 超过最大大小时删除最旧的
        if (entries_.size() > max_size_) {
            entries_.pop_front();
        }
    }

    /// @brief 刷新缓冲区 (内存Sink无需刷新)
    void flush_() override {
        // 内存Sink无需flush
    }

private:
    mutable std::deque<Shared::Types::LogEntry> entries_;
    size_t max_size_;

    /// @brief 转换spdlog级别到LogLevel
    Shared::Types::LogLevel ConvertLevel(spdlog::level::level_enum level) const {
        switch (level) {
            case spdlog::level::trace:   return Shared::Types::LogLevel::DEBUG;
            case spdlog::level::debug:   return Shared::Types::LogLevel::DEBUG;
            case spdlog::level::info:    return Shared::Types::LogLevel::INFO;
            case spdlog::level::warn:    return Shared::Types::LogLevel::WARNING;
            case spdlog::level::err:     return Shared::Types::LogLevel::ERR;
            case spdlog::level::critical:return Shared::Types::LogLevel::CRITICAL;
            case spdlog::level::off:     return Shared::Types::LogLevel::CRITICAL;
            default:                     return Shared::Types::LogLevel::INFO;
        }
    }

    /// @brief 获取线程ID字符串
    static std::string GetThreadId(size_t thread_id) {
        std::ostringstream oss;
        oss << thread_id;
        return oss.str();
    }
};

// ============================================================
// 便捷类型定义
// ============================================================

/// @brief 多线程安全版本 (使用std::mutex)
using MemorySink_mt = MemorySink<std::mutex>;

/// @brief 单线程版本 (使用null_mutex，无锁)
using MemorySink_st = MemorySink<spdlog::details::null_mutex>;

}  // namespace Sinks
}  // namespace Logging
}  // namespace Infrastructure
}  // namespace Siligen
