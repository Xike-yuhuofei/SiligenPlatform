// BugReportTypes.h - Bug 记录核心领域模型
// Auto Bug Recorder - Phase 1
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Siligen {
namespace Domain {
namespace Diagnostics {
namespace ValueObjects {

/**
 * @brief Bug 严重程度枚举
 */
enum class BugSeverity {
    Info,     // 信息性
    Warning,  // 警告
    Error,    // 错误
    Fatal     // 致命错误
};

/**
 * @brief Bug 报告核心领域模型
 *
 * 不可变或尽量不可变的数据结构，用于记录异常和故障上下文。
 */
class BugReport {
   public:
    // 核心标识
    std::string id;          // UUID
    std::int64_t timestamp;  // Unix 时间戳(毫秒)

    // Bug 分类
    BugSeverity severity;  // 严重程度
    std::string title;     // 标题(简短描述)
    std::string message;   // 详细消息

    // 聚合与去重
    std::string fingerprint;       // 指纹(用于聚合同类 bug)
    std::int32_t occurrenceCount;  // 出现次数(聚合后)

    // 标签与上下文
    std::map<std::string, std::string> tags;     // 键值对标签
    std::map<std::string, std::string> context;  // 结构化上下文

    // 调用栈
    std::vector<std::string> stacktrace;  // 调用栈帧列表

    /**
     * @brief 默认构造函数
     */
    BugReport()
        : id(""),
          timestamp(0),
          severity(BugSeverity::Info),
          title(""),
          message(""),
          fingerprint(""),
          occurrenceCount(1) {}

    /**
     * @brief 验证报告有效性
     * @return true if valid, false otherwise
     */
    bool isValid() const noexcept {
        if (id.empty()) return false;
        if (timestamp <= 0) return false;
        if (title.empty() && message.empty()) return false;
        return true;
    }

    /**
     * @brief 获取严重程度字符串表示
     */
    static std::string severityToString(BugSeverity severity) noexcept {
        switch (severity) {
            case BugSeverity::Info:
                return "INFO";
            case BugSeverity::Warning:
                return "WARNING";
            case BugSeverity::Error:
                return "ERROR";
            case BugSeverity::Fatal:
                return "FATAL";
            default:
                return "UNKNOWN";
        }
    }

    /**
     * @brief 从字符串解析严重程度
     */
    static BugSeverity stringToSeverity(const std::string& str) noexcept {
        if (str == "INFO") return BugSeverity::Info;
        if (str == "WARNING") return BugSeverity::Warning;
        if (str == "ERROR") return BugSeverity::Error;
        if (str == "FATAL") return BugSeverity::Fatal;
        return BugSeverity::Info;  // 默认
    }
};

/**
 * @brief Bug 报告草稿(用于创建报告)
 *
 * 可变数据结构，用于在创建 BugReport 前收集信息。
 */
class BugReportDraft {
   public:
    // Bug 分类
    BugSeverity severity;
    std::string title;
    std::string message;

    // 标签与上下文
    std::map<std::string, std::string> tags;
    std::map<std::string, std::string> context;

    // 调用栈(可选)
    std::optional<std::vector<std::string>> stacktrace;

    /**
     * @brief 默认构造函数
     */
    BugReportDraft()
        : severity(BugSeverity::Error), title(""), message(""), tags(), context(), stacktrace(std::nullopt) {}

    /**
     * @brief 验证草稿有效性
     */
    bool isValid() const noexcept {
        // 至少需要 title 或 message 之一
        return !title.empty() || !message.empty();
    }
};

}  // namespace ValueObjects
}  // namespace Diagnostics
}  // namespace Domain
}  // namespace Siligen
