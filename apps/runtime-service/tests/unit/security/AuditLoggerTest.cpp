#include "security/AuditLogger.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

class ScopedAuditLogDirectory {
public:
    explicit ScopedAuditLogDirectory(const std::string& suffix) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("siligen_audit_logger_" + suffix + "_" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }

    ~ScopedAuditLogDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

    void WriteLogLine(const std::string& line) const {
        std::ofstream out(path_ / "audit_test.jsonl", std::ios::binary | std::ios::app);
        out << line << '\n';
    }

private:
    std::filesystem::path path_;
};

Siligen::AuditLogger::QueryFilter AllAuditEntriesFilter() {
    Siligen::AuditLogger::QueryFilter filter;
    filter.start_time = std::chrono::system_clock::time_point::min();
    filter.end_time = std::chrono::system_clock::time_point::max();
    return filter;
}

std::string CompleteAuditLine() {
    return R"({"timestamp":"2026-04-27 20:00:00.123","category":"认证","level":"INFO","status":"SUCCESS","username":"operator","action":"登录成功","details":"accepted","ip":"127.0.0.1"})";
}

}  // namespace

TEST(AuditLoggerTest, QueryLogsReadsCompleteAuditLine) {
    ScopedAuditLogDirectory logs("complete");
    logs.WriteLogLine(CompleteAuditLine());

    Siligen::AuditLogger logger(logs.path().string());
    const auto entries = logger.QueryLogs(AllAuditEntriesFilter());

    ASSERT_EQ(entries.size(), 1U);
    EXPECT_EQ(entries.front().username, "operator");
    EXPECT_EQ(entries.front().action, "登录成功");
    EXPECT_EQ(entries.front().details, "accepted");
    EXPECT_EQ(entries.front().ip_address, "127.0.0.1");
}

TEST(AuditLoggerTest, QueryLogsAllowsMissingOptionalIp) {
    ScopedAuditLogDirectory logs("optional_ip");
    logs.WriteLogLine(
        R"({"timestamp":"2026-04-27 20:00:00.123","category":"认证","level":"INFO","status":"SUCCESS","username":"operator","action":"登录成功","details":"accepted"})");

    Siligen::AuditLogger logger(logs.path().string());
    const auto entries = logger.QueryLogs(AllAuditEntriesFilter());

    ASSERT_EQ(entries.size(), 1U);
    EXPECT_EQ(entries.front().username, "operator");
    EXPECT_TRUE(entries.front().ip_address.empty());
}

TEST(AuditLoggerTest, QueryLogsSkipsLinesMissingRequiredFields) {
    ScopedAuditLogDirectory logs("missing_required");
    logs.WriteLogLine(
        R"({"timestamp":"2026-04-27 20:00:00.123","category":"认证","level":"INFO","username":"operator","action":"登录成功","details":"accepted"})");
    logs.WriteLogLine(CompleteAuditLine());

    Siligen::AuditLogger logger(logs.path().string());
    const auto entries = logger.QueryLogs(AllAuditEntriesFilter());

    ASSERT_EQ(entries.size(), 1U);
    EXPECT_EQ(entries.front().username, "operator");
    EXPECT_EQ(entries.front().details, "accepted");
}

TEST(AuditLoggerTest, QueryLogsSkipsLinesWithUnclosedRequiredFields) {
    ScopedAuditLogDirectory logs("unclosed_required");
    logs.WriteLogLine(R"({"timestamp":"2026-04-27 20:00:00.123)");
    logs.WriteLogLine(CompleteAuditLine());

    Siligen::AuditLogger logger(logs.path().string());
    const auto entries = logger.QueryLogs(AllAuditEntriesFilter());

    ASSERT_EQ(entries.size(), 1U);
    EXPECT_EQ(entries.front().username, "operator");
}
