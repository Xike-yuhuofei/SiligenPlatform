#pragma once

#include "shared/errors/ErrorHandler.h"
#include "shared/types/Types.h"

#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace Siligen {

// 审计日志条目
struct AuditEntry {
    std::chrono::system_clock::time_point timestamp;
    AuditCategory category;
    AuditLevel level;
    AuditStatus status;
    std::string username;
    std::string action;
    std::string details;
    std::string ip_address;
};

// 审计日志记录器 (JSON Lines格式)
class AuditLogger {
   public:
    explicit AuditLogger(const std::string& log_directory = "logs/audit");

    // 记录审计日志
    bool LogAudit(const AuditEntry& entry);

    // 便捷方法
    bool LogAuthentication(const std::string& username,
                           AuditStatus status,
                           const std::string& details,
                           const std::string& ip = "");

    bool LogAuthorization(const std::string& username,
                          AuditStatus status,
                          const std::string& action,
                          const std::string& details);

    bool LogMotionControl(const std::string& username,
                          AuditStatus status,
                          const std::string& action,
                          const std::string& details);

    bool LogParameterChange(const std::string& username,
                            const std::string& parameter,
                            const std::string& old_value,
                            const std::string& new_value);

    bool LogSafetyEvent(const std::string& username, const std::string& event, const std::string& details);

    // 刷新日志到磁盘
    void Flush();

    // 日志轮转
    void RotateIfNeeded(int32 max_size_mb = 100);

    // T056: 查询审计日志
    struct QueryFilter {
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point end_time;
        std::string username;    // 空字符串表示不筛选
        AuditCategory category;  // 使用 AuditCategory::AUTHENTICATION 等
        AuditLevel level;        // 使用 AuditLevel::INFO 等
        bool filter_by_category = false;
        bool filter_by_level = false;
    };

    std::vector<AuditEntry> QueryLogs(const QueryFilter& filter) const;

    // T057: 导出审计日志到JSON文件
    static bool ExportToJSON(const std::string& output_file, const std::vector<AuditEntry>& entries);

    // T058: 清理旧日志(保留最近N天)
    bool CleanOldLogs(int32 retention_days = 90);

    // 获取磁盘剩余空间(MB)
    int32 GetAvailableDiskSpaceMB() const;

   private:
    std::string log_directory_;
    std::string current_log_file_;
    std::ofstream log_stream_;
    std::mutex mutex_;

    // 生成JSON格式日志
    static std::string ToJSON(const AuditEntry& entry);

    // 生成日志文件名
    std::string GenerateLogFileName() const;

    // 获取当前日志文件大小(MB)
    int32 GetCurrentLogSizeMB() const;

    // 辅助函数
    static std::string CategoryToString(AuditCategory category);
    static std::string LevelToString(AuditLevel level);
    static std::string StatusToString(AuditStatus status);
    static std::string TimestampToString(const std::chrono::system_clock::time_point& tp);

    // 解析函数
    static bool ParseJSONLine(const std::string& line, AuditEntry& entry);
    static std::chrono::system_clock::time_point ParseTimestamp(const std::string& ts_str);
    static AuditCategory StringToCategory(const std::string& str);
    static AuditLevel StringToLevel(const std::string& str);
    static AuditStatus StringToStatus(const std::string& str);
};

}  // namespace Siligen

