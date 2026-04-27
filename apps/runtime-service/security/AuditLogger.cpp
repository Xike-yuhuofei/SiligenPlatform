#include "AuditLogger.h"

#include "SecurityLogHelper.h"
#include "shared/errors/ErrorHandler.h"

#include <filesystem>
#include <iomanip>
#include <sstream>
#include <windows.h>

#ifdef ERROR
#undef ERROR
#endif

namespace Siligen {

using Shared::Types::LogLevel;

namespace {

LogLevel MapAuditLevelToLogLevel(AuditLevel level) {
    switch (level) {
        case AuditLevel::INFO:
            return LogLevel::INFO;
        case AuditLevel::WARNING:
            return LogLevel::WARNING;
        case AuditLevel::ERROR:
            return LogLevel::ERR;
        case AuditLevel::SECURITY:
            return LogLevel::WARNING;
        default:
            return LogLevel::INFO;
    }
}

}  // namespace

AuditLogger::AuditLogger(const std::string& log_directory) : log_directory_(log_directory) {
    // 创建日志目录
    std::filesystem::create_directories(log_directory);

    // 打开当前日志文件
    current_log_file_ = GenerateLogFileName();
    log_stream_.open(current_log_file_, std::ios::app);

    if (!log_stream_.is_open()) {
        SecurityLogHelper::Log(
            LogLevel::ERR,
            "AuditLogger",
            "无法打开审计日志文件: " + current_log_file_ +
                " (错误码: " + std::to_string(static_cast<int32>(SystemErrorCode::AUDIT_LOG_WRITE_FAILED)) + ")");
    }
}

bool AuditLogger::LogAudit(const AuditEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!log_stream_.is_open()) return false;

    // T060: 检查磁盘空间
    int32 available_mb = GetAvailableDiskSpaceMB();
    if (available_mb < 100) {  // 少于100MB时警告
        SecurityLogHelper::Log(LogLevel::WARNING,
                               "AuditLogger",
                               "磁盘空间不足: 剩余 " + std::to_string(available_mb) + " MB");

        if (available_mb < 50) {  // 少于50MB时尝试清理
            SecurityLogHelper::Log(LogLevel::ERR, "AuditLogger", "磁盘空间严重不足,尝试清理旧日志...");
            CleanOldLogs(90);  // 清理90天前的日志
        }
    }

    std::string json = ToJSON(entry);
    log_stream_ << json << std::endl;

    // 同时输出到系统日志
    SecurityLogHelper::Log(MapAuditLevelToLogLevel(entry.level),
                           "AuditLogger",
                           "[" + CategoryToString(entry.category) + "] " + entry.action);

    return true;
}

bool AuditLogger::LogAuthentication(const std::string& username,
                                    AuditStatus status,
                                    const std::string& details,
                                    const std::string& ip) {
    AuditEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.category = AuditCategory::AUTHENTICATION;
    entry.level = (status == AuditStatus::SUCCESS) ? AuditLevel::INFO : AuditLevel::SECURITY;
    entry.status = status;
    entry.username = username;
    entry.action = (status == AuditStatus::SUCCESS) ? "登录成功" : "登录失败";
    entry.details = details;
    entry.ip_address = ip;

    return LogAudit(entry);
}

bool AuditLogger::LogAuthorization(const std::string& username,
                                   AuditStatus status,
                                   const std::string& action,
                                   const std::string& details) {
    AuditEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.category = AuditCategory::AUTHORIZATION;
    entry.level = (status == AuditStatus::DENIED) ? AuditLevel::SECURITY : AuditLevel::INFO;
    entry.status = status;
    entry.username = username;
    entry.action = action;
    entry.details = details;

    return LogAudit(entry);
}

bool AuditLogger::LogMotionControl(const std::string& username,
                                   AuditStatus status,
                                   const std::string& action,
                                   const std::string& details) {
    AuditEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.category = AuditCategory::MOTION_CONTROL;
    entry.level = (status == AuditStatus::SUCCESS) ? AuditLevel::INFO : AuditLevel::ERROR;
    entry.status = status;
    entry.username = username;
    entry.action = action;
    entry.details = details;

    return LogAudit(entry);
}

bool AuditLogger::LogParameterChange(const std::string& username,
                                     const std::string& parameter,
                                     const std::string& old_value,
                                     const std::string& new_value) {
    AuditEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.category = AuditCategory::PARAMETER_CHANGE;
    entry.level = AuditLevel::INFO;
    entry.status = AuditStatus::SUCCESS;
    entry.username = username;
    entry.action = "参数修改: " + parameter;
    entry.details = "旧值=" + old_value + ", 新值=" + new_value;

    return LogAudit(entry);
}

bool AuditLogger::LogSafetyEvent(const std::string& username, const std::string& event, const std::string& details) {
    AuditEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.category = AuditCategory::SAFETY_EVENT;
    entry.level = AuditLevel::SECURITY;
    entry.status = AuditStatus::DENIED;
    entry.username = username;
    entry.action = event;
    entry.details = details;

    return LogAudit(entry);
}

void AuditLogger::Flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (log_stream_.is_open()) {
        log_stream_.flush();
    }
}

void AuditLogger::RotateIfNeeded(int32 max_size_mb) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (GetCurrentLogSizeMB() >= max_size_mb) {
        log_stream_.close();
        current_log_file_ = GenerateLogFileName();
        log_stream_.open(current_log_file_, std::ios::app);

        SecurityLogHelper::Log(LogLevel::INFO, "AuditLogger", "日志轮转: " + current_log_file_);
    }
}

std::string AuditLogger::ToJSON(const AuditEntry& entry) {
    std::ostringstream oss;
    oss << "{"
        << "\"timestamp\":\"" << TimestampToString(entry.timestamp) << "\","
        << "\"category\":\"" << CategoryToString(entry.category) << "\","
        << "\"level\":\"" << LevelToString(entry.level) << "\","
        << "\"status\":\"" << StatusToString(entry.status) << "\","
        << "\"username\":\"" << entry.username << "\","
        << "\"action\":\"" << entry.action << "\","
        << "\"details\":\"" << entry.details << "\"";

    if (!entry.ip_address.empty()) {
        oss << ",\"ip\":\"" << entry.ip_address << "\"";
    }

    oss << "}";
    return oss.str();
}

std::string AuditLogger::GenerateLogFileName() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << log_directory_ << "/audit_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".jsonl";

    return oss.str();
}

int32 AuditLogger::GetCurrentLogSizeMB() const {
    if (!std::filesystem::exists(current_log_file_)) return 0;

    auto size = std::filesystem::file_size(current_log_file_);
    return static_cast<int32>(size / (1024 * 1024));
}

std::string AuditLogger::CategoryToString(AuditCategory category) {
    switch (category) {
        case AuditCategory::AUTHENTICATION:
            return "认证";
        case AuditCategory::AUTHORIZATION:
            return "权限";
        case AuditCategory::MOTION_CONTROL:
            return "运动控制";
        case AuditCategory::PARAMETER_CHANGE:
            return "参数修改";
        case AuditCategory::SAFETY_EVENT:
            return "安全事件";
        case AuditCategory::NETWORK_ACCESS:
            return "网络访问";
        case AuditCategory::SYSTEM_CONFIG:
            return "系统配置";
        case AuditCategory::USER_MANAGEMENT:
            return "用户管理";
        default:
            return "未知";
    }
}

std::string AuditLogger::LevelToString(AuditLevel level) {
    int32 level_val = static_cast<int32>(level);
    switch (level_val) {
        case 0:
            return "INFO";
        case 1:
            return "WARNING";
        case 2:
            return "ERROR";
        case 3:
            return "SECURITY";
        default:
            return "UNKNOWN";
    }
}

std::string AuditLogger::StatusToString(AuditStatus status) {
    switch (status) {
        case AuditStatus::SUCCESS:
            return "SUCCESS";
        case AuditStatus::FAILURE:
            return "FAILURE";
        case AuditStatus::DENIED:
            return "DENIED";
        default:
            return "UNKNOWN";
    }
}

std::string AuditLogger::TimestampToString(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count() % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3)
        << ms;

    return oss.str();
}

// T056: 查询审计日志
std::vector<AuditEntry> AuditLogger::QueryLogs(const QueryFilter& filter) const {
    std::vector<AuditEntry> results;

    // 遍历日志目录中的所有.jsonl文件
    for (const auto& entry : std::filesystem::directory_iterator(log_directory_)) {
        if (entry.path().extension() != ".jsonl") continue;

        std::ifstream file(entry.path());
        std::string line;

        while (std::getline(file, line)) {
            if (line.empty()) continue;

            // 解析JSON行
            AuditEntry audit_entry;
            if (!ParseJSONLine(line, audit_entry)) continue;

            // 应用时间筛选
            if (audit_entry.timestamp < filter.start_time || audit_entry.timestamp > filter.end_time) {
                continue;
            }

            // 应用用户筛选
            if (!filter.username.empty() && audit_entry.username != filter.username) {
                continue;
            }

            // 应用类别筛选
            if (filter.filter_by_category && audit_entry.category != filter.category) {
                continue;
            }

            // 应用级别筛选
            if (filter.filter_by_level && audit_entry.level != filter.level) {
                continue;
            }

            results.push_back(audit_entry);
        }
    }

    return results;
}

// T057: 导出审计日志到JSON文件
bool AuditLogger::ExportToJSON(const std::string& output_file, const std::vector<AuditEntry>& entries) {
    std::ofstream out(output_file);
    if (!out.is_open()) {
        SecurityLogHelper::Log(LogLevel::ERR, "AuditLogger", "无法创建导出文件: " + output_file);
        return false;
    }

    out << "[" << std::endl;

    for (size_t i = 0; i < entries.size(); ++i) {
        out << "  " << ToJSON(entries[i]);
        if (i < entries.size() - 1) {
            out << ",";
        }
        out << std::endl;
    }

    out << "]" << std::endl;
    out.close();

    SecurityLogHelper::Log(
        LogLevel::INFO, "AuditLogger", "导出了 " + std::to_string(entries.size()) + " 条日志到 " + output_file);

    return true;
}

// T058: 清理旧日志
bool AuditLogger::CleanOldLogs(int32 retention_days) {
    auto now = std::chrono::system_clock::now();
    auto cutoff_time = now - std::chrono::hours(retention_days * 24);

    int32 deleted_count = 0;

    for (const auto& entry : std::filesystem::directory_iterator(log_directory_)) {
        if (entry.path().extension() != ".jsonl") continue;

        // 跳过当前正在写入的日志文件
        if (entry.path() == current_log_file_) continue;

        // 获取文件最后修改时间
        auto file_time = std::filesystem::last_write_time(entry.path());
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            file_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());

        if (sctp < cutoff_time) {
            try {
                std::filesystem::remove(entry.path());
                deleted_count++;
            } catch (const std::exception& e) {
                SecurityLogHelper::Log(LogLevel::ERR,
                                       "AuditLogger",
                                       "删除旧日志文件失败: " + entry.path().string() + " - " + e.what());
            }
        }
    }

    SecurityLogHelper::Log(LogLevel::INFO,
                           "AuditLogger",
                           "清理了 " + std::to_string(deleted_count) + " 个旧日志文件");

    return true;
}

// T060: 获取磁盘剩余空间
int32 AuditLogger::GetAvailableDiskSpaceMB() const {
    try {
        auto space = std::filesystem::space(log_directory_);
        return static_cast<int32>(space.available / (1024 * 1024));
    } catch (const std::exception&) {
        return 0;
    }
}

// 辅助函数：解析JSON行
bool AuditLogger::ParseJSONLine(const std::string& line, AuditEntry& entry) {
    // 简单的JSON解析（针对我们自己生成的格式）
    size_t pos = 0;

    // 解析timestamp
    pos = line.find("\"timestamp\":\"");
    if (pos == std::string::npos) {
        return false;
    }
    {
        const size_t start = pos + 13;
        const size_t end = line.find("\"", start);
        if (end == std::string::npos) {
            return false;
        }
        entry.timestamp = ParseTimestamp(line.substr(start, end - start));
    }

    // 解析category
    pos = line.find("\"category\":\"");
    if (pos == std::string::npos) {
        return false;
    }
    {
        const size_t start = pos + 12;
        const size_t end = line.find("\"", start);
        if (end == std::string::npos) {
            return false;
        }
        entry.category = StringToCategory(line.substr(start, end - start));
    }

    // 解析level
    pos = line.find("\"level\":\"");
    if (pos == std::string::npos) {
        return false;
    }
    {
        const size_t start = pos + 9;
        const size_t end = line.find("\"", start);
        if (end == std::string::npos) {
            return false;
        }
        entry.level = StringToLevel(line.substr(start, end - start));
    }

    // 解析status
    pos = line.find("\"status\":\"");
    if (pos == std::string::npos) {
        return false;
    }
    {
        const size_t start = pos + 10;
        const size_t end = line.find("\"", start);
        if (end == std::string::npos) {
            return false;
        }
        entry.status = StringToStatus(line.substr(start, end - start));
    }

    pos = line.find("\"username\":\"");
    if (pos == std::string::npos) {
        return false;
    }
    {
        const size_t start = pos + 12;
        const size_t end = line.find("\"", start);
        if (end == std::string::npos) {
            return false;
        }
        entry.username = line.substr(start, end - start);
    }

    pos = line.find("\"action\":\"");
    if (pos == std::string::npos) {
        return false;
    }
    {
        const size_t start = pos + 10;
        const size_t end = line.find("\"", start);
        if (end == std::string::npos) {
            return false;
        }
        entry.action = line.substr(start, end - start);
    }

    pos = line.find("\"details\":\"");
    if (pos == std::string::npos) {
        return false;
    }
    {
        const size_t start = pos + 11;
        const size_t end = line.find("\"", start);
        if (end == std::string::npos) {
            return false;
        }
        entry.details = line.substr(start, end - start);
    }

    // 解析ip (可选)
    pos = line.find("\"ip\":\"");
    if (pos != std::string::npos) {
        const size_t start = pos + 6;
        const size_t end = line.find("\"", start);
        if (end == std::string::npos) {
            return false;
        }
        entry.ip_address = line.substr(start, end - start);
    }

    return true;
}

std::chrono::system_clock::time_point AuditLogger::ParseTimestamp(const std::string& ts_str) {
    std::tm tm = {};
    int ms = 0;
    std::istringstream ss(ts_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        return std::chrono::system_clock::now();
    }

    // 解析毫秒
    if (ss.peek() == '.') {
        ss.ignore();
        ss >> ms;
    }

    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    tp += std::chrono::milliseconds(ms);
    return tp;
}

AuditCategory AuditLogger::StringToCategory(const std::string& str) {
    if (str == "认证") return AuditCategory::AUTHENTICATION;
    if (str == "权限") return AuditCategory::AUTHORIZATION;
    if (str == "运动控制") return AuditCategory::MOTION_CONTROL;
    if (str == "参数修改") return AuditCategory::PARAMETER_CHANGE;
    if (str == "安全事件") return AuditCategory::SAFETY_EVENT;
    if (str == "网络访问") return AuditCategory::NETWORK_ACCESS;
    if (str == "系统配置") return AuditCategory::SYSTEM_CONFIG;
    if (str == "用户管理") return AuditCategory::USER_MANAGEMENT;
    return AuditCategory::AUTHENTICATION;
}

AuditLevel AuditLogger::StringToLevel(const std::string& str) {
    if (str == "INFO") return AuditLevel::INFO;
    if (str == "WARNING") return AuditLevel::WARNING;
    if (str == "ERROR") return AuditLevel::ERROR;
    if (str == "SECURITY") return AuditLevel::SECURITY;
    return AuditLevel::INFO;
}

AuditStatus AuditLogger::StringToStatus(const std::string& str) {
    if (str == "SUCCESS") return AuditStatus::SUCCESS;
    if (str == "FAILURE") return AuditStatus::FAILURE;
    if (str == "DENIED") return AuditStatus::DENIED;
    return AuditStatus::SUCCESS;
}

}  // namespace Siligen

