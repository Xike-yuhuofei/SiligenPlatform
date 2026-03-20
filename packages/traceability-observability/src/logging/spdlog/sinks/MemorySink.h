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

template<typename Mutex>
class MemorySink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit MemorySink(size_t max_size = 1000)
        : max_size_(max_size) {
    }

    ~MemorySink() override = default;

    std::vector<Shared::Types::LogEntry> GetRecent(
        int32_t count,
        Shared::Types::LogLevel min_level = static_cast<Shared::Types::LogLevel>(0),
        const std::string& category_filter = "") {
        std::vector<Shared::Types::LogEntry> result;

        for (auto it = entries_.rbegin();
             it != entries_.rend() && result.size() < static_cast<size_t>(count);
             ++it) {
            if (it->level >= min_level) {
                if (category_filter.empty() || it->category == category_filter) {
                    result.push_back(*it);
                }
            }
        }

        return result;
    }

    std::vector<Shared::Types::LogEntry> GetAll() {
        return std::vector<Shared::Types::LogEntry>(entries_.begin(), entries_.end());
    }

    void Clear() {
        entries_.clear();
    }

    size_t GetSize() {
        return entries_.size();
    }

    size_t GetMaxSize() const {
        return max_size_;
    }

    void SetMaxSize(size_t max_size) {
        max_size_ = max_size;

        while (entries_.size() > max_size_) {
            entries_.pop_front();
        }
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        Shared::Types::LogEntry entry;
        entry.level = ConvertLevel(msg.level);

        std::string message = std::string(msg.payload.begin(), msg.payload.end());
        entry.category = "General";

        if (message.size() > 2 && message[0] == '[') {
            const size_t end_bracket = message.find(']');
            if (end_bracket != std::string::npos && end_bracket > 1) {
                const std::string potential_category = message.substr(1, end_bracket - 1);
                if (potential_category.find_first_of(" \t\n\r") == std::string::npos) {
                    entry.category = potential_category;
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
        entries_.push_back(entry);

        if (entries_.size() > max_size_) {
            entries_.pop_front();
        }
    }

    void flush_() override {
    }

private:
    mutable std::deque<Shared::Types::LogEntry> entries_;
    size_t max_size_;

    Shared::Types::LogLevel ConvertLevel(spdlog::level::level_enum level) const {
        switch (level) {
            case spdlog::level::trace:
            case spdlog::level::debug:
                return Shared::Types::LogLevel::DEBUG;
            case spdlog::level::info:
                return Shared::Types::LogLevel::INFO;
            case spdlog::level::warn:
                return Shared::Types::LogLevel::WARNING;
            case spdlog::level::err:
                return Shared::Types::LogLevel::ERR;
            case spdlog::level::critical:
            case spdlog::level::off:
                return Shared::Types::LogLevel::CRITICAL;
            default:
                return Shared::Types::LogLevel::INFO;
        }
    }

    static std::string GetThreadId(size_t thread_id) {
        std::ostringstream oss;
        oss << thread_id;
        return oss.str();
    }
};

using MemorySink_mt = MemorySink<std::mutex>;
using MemorySink_st = MemorySink<spdlog::details::null_mutex>;

}  // namespace Sinks
}  // namespace Logging
}  // namespace Infrastructure
}  // namespace Siligen
