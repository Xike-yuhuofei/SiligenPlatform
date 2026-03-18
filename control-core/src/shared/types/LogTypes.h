// LogTypes.h - 日志相关类型 (Logging related types)
// Task: T017 - Phase 2 基础设施 - 共享类型系统
#pragma once

#include "Error.h"

#include <chrono>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <thread>

namespace Siligen {
namespace Shared {
namespace Types {

// 日志级别 (Log level)
enum class LogLevel : int32 {
    DEBUG = 0,    // 调试信息 (Debug info)
    INFO = 1,     // 一般信息 (General info)
    WARNING = 2,  // 警告 (Warning)
    ERR = 3,      // 错误 (Error)
    CRITICAL = 4  // 严重错误 (Critical error)
};

// 日志级别转字符串 (Log level to string)
inline const char* LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::ERR:
            return "ERROR";
        case LogLevel::CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}

// 日志级别转标签 (Log level to tag)
inline const char* LogLevelToTag(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:
            return "[DEBUG]";
        case LogLevel::INFO:
            return "[INFO]";
        case LogLevel::WARNING:
            return "[WARNING]";
        case LogLevel::ERR:
            return "[ERROR]";
        case LogLevel::CRITICAL:
            return "[CRITICAL]";
        default:
            return "[UNKNOWN]";
    }
}

// 日志条目 (Log entry)
struct LogEntry {
    LogLevel level;                                   // 日志级别 (Log level)
    std::string message;                              // 日志消息 (Log message)
    std::string category;                             // 日志分类 (Log category/module)
    std::chrono::system_clock::time_point timestamp;  // 时间戳 (Timestamp)
    std::string thread_id;                            // 线程ID (Thread ID)
    std::map<std::string, std::string> metadata;      // 元数据 (Metadata/Structured attributes)

    // 默认构造函数 (Default constructor)
    LogEntry()
        : level(LogLevel::INFO),
          message(""),
          category(""),
          timestamp(std::chrono::system_clock::now()),
          thread_id(GetCurrentThreadId()),
          metadata{} {}

    // 带参数构造函数 (Parameterized constructor)
    LogEntry(LogLevel log_level, const std::string& msg, const std::string& cat = "")
        : level(log_level),
          message(msg),
          category(cat),
          timestamp(std::chrono::system_clock::now()),
          thread_id(GetCurrentThreadId()),
          metadata{} {}

    // 带元数据的构造函数 (Constructor with metadata)
    LogEntry(LogLevel log_level, const std::string& msg, const std::string& cat,
             const std::map<std::string, std::string>& meta)
        : level(log_level),
          message(msg),
          category(cat),
          timestamp(std::chrono::system_clock::now()),
          thread_id(GetCurrentThreadId()),
          metadata(meta) {}

    // 获取当前线程ID字符串 (Get current thread ID as string)
    static std::string GetCurrentThreadId() {
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        return oss.str();
    }

    // 格式化时间戳 (Format timestamp)
    std::string FormatTimestamp() const {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        std::tm tm_buf;

#ifdef _WIN32
        localtime_s(&tm_buf, &time_t);
#else
        localtime_r(&time_t, &tm_buf);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");

        // 添加毫秒
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()) % 1000;
        oss << "." << std::setfill('0') << std::setw(3) << ms.count();

        return oss.str();
    }

    // 转换为字符串 (Convert to string)
    // 格式: [级别] 分类: 消息
    std::string ToString() const {
        std::string result = LogLevelToTag(level);

        if (!category.empty()) {
            result += " " + category + ":";
        }

        result += " " + message;

        return result;
    }

    // 转换为详细字符串 (包含时间戳和线程ID)
    // Convert to detailed string (with timestamp and thread ID)
    std::string ToDetailedString() const {
        std::string result = FormatTimestamp();
        result += " ";
        result += LogLevelToTag(level);
        result += " [线程:";
        result += thread_id;
        result += "]";

        if (!category.empty()) {
            result += " ";
            result += category;
            result += ":";
        }

        result += " ";
        result += message;

        return result;
    }

    // 转换为JSON格式 (Convert to JSON format)
    std::string ToJson() const {
        std::ostringstream oss;
        oss << "{";
        oss << "\"timestamp\":\"" << FormatTimestamp() << "\",";
        oss << "\"level\":\"" << LogLevelToString(level) << "\",";
        oss << "\"category\":\"" << category << "\",";
        oss << "\"message\":\"" << EscapeJsonString(message) << "\",";
        oss << "\"thread_id\":\"" << thread_id << "\"";
        oss << "}";
        return oss.str();
    }

   private:
    // JSON字符串转义 (Escape JSON string)
    static std::string EscapeJsonString(const std::string& str) {
        std::string result;
        result.reserve(str.length());

        for (char c : str) {
            switch (c) {
                case '"':
                    result += "\\\"";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                case '\b':
                    result += "\\b";
                    break;
                case '\f':
                    result += "\\f";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                default:
                    result += c;
                    break;
            }
        }

        return result;
    }
};

// 日志配置 (Log configuration)
struct LogConfig {
    LogLevel min_level;         // 最小日志级别 (Minimum log level)
    bool enable_console;        // 启用控制台输出 (Enable console output)
    bool enable_file;           // 启用文件输出 (Enable file output)
    std::string log_file_path;  // 日志文件路径 (Log file path)
    bool enable_timestamp;      // 启用时间戳 (Enable timestamp)
    bool enable_thread_id;      // 启用线程ID (Enable thread ID)
    size_t max_file_size_mb;    // 最大文件大小 (MB) (Max file size in MB)
    size_t max_backup_files;    // 最大备份文件数 (Max backup files)

    // 默认构造函数 (Default constructor)
    LogConfig()
        : min_level(LogLevel::INFO),
          enable_console(true),
          enable_file(false),
          log_file_path("logs/application.log"),
          enable_timestamp(true),
          enable_thread_id(false),
          max_file_size_mb(10),
          max_backup_files(5) {}

    // 转换为字符串 (Convert to string)
    std::string ToString() const {
        std::string result = "[LogConfig]\n";
        result += "最小日志级别: " + std::string(LogLevelToString(min_level)) + "\n";
        result += "控制台输出: " + std::string(enable_console ? "启用" : "禁用") + "\n";
        result += "文件输出: " + std::string(enable_file ? "启用" : "禁用") + "\n";
        result += "日志文件路径: " + log_file_path + "\n";
        result += "时间戳: " + std::string(enable_timestamp ? "启用" : "禁用") + "\n";
        result += "线程ID: " + std::string(enable_thread_id ? "启用" : "禁用") + "\n";
        result += "最大文件大小: " + std::to_string(max_file_size_mb) + " MB\n";
        result += "最大备份文件数: " + std::to_string(max_backup_files);
        return result;
    }
};


// 类型别名 - 与接口声明保持一致 (Type alias - consistent with interface declaration)
using LogConfiguration = LogConfig;

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen
