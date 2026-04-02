#include "application/services/trace_diagnostics/LoggingServiceFactory.h"

#include <gtest/gtest.h>

#include <optional>

namespace {

using Siligen::Shared::Types::LogConfiguration;
using Siligen::Shared::Types::LogEntry;
using Siligen::Shared::Types::LogLevel;
using Siligen::TraceDiagnostics::Application::Services::CreateLoggingService;

TEST(LoggingServiceObservabilityTest, FactoryBootstrapsServiceAndStoresObservableLogEntry) {
    LogConfiguration config;
    config.enable_console = false;
    config.enable_file = false;
    config.min_level = LogLevel::INFO;

    auto bootstrap = CreateLoggingService(config, "trace-diagnostics-test");

    ASSERT_NE(bootstrap.service, nullptr);
    ASSERT_TRUE(bootstrap.configuration_result.IsSuccess());
    EXPECT_STREQ(bootstrap.service->GetImplementationType(), "SpdlogLoggingAdapter");

    ASSERT_TRUE(bootstrap.service->LogInfo("bootstrap complete", "TraceDiagnostics").IsSuccess());

    const auto recent_logs = bootstrap.service->GetRecentLogs(10, LogLevel::DEBUG, "TraceDiagnostics");
    ASSERT_TRUE(recent_logs.IsSuccess());
    ASSERT_FALSE(recent_logs.Value().empty());
    EXPECT_EQ(recent_logs.Value().front().category, "TraceDiagnostics");
    EXPECT_EQ(recent_logs.Value().front().message, "bootstrap complete");
}

TEST(LoggingServiceObservabilityTest, StructuredAttributesReachTestCallback) {
    LogConfiguration config;
    config.enable_console = false;
    config.enable_file = false;

    auto bootstrap = CreateLoggingService(config, "trace-diagnostics-attributes");

    ASSERT_NE(bootstrap.service, nullptr);
    ASSERT_TRUE(bootstrap.configuration_result.IsSuccess());

    std::optional<LogEntry> captured_entry;
    bootstrap.service->SetLogCallback([&captured_entry](const LogEntry& entry) {
        captured_entry = entry;
    });

    ASSERT_TRUE(bootstrap.service->LogWithAttributes(
        LogLevel::ERR,
        "logger bootstrap failed",
        "TraceDiagnostics",
        {{"failure_category", "missing-config"}, {"primary_cause", "CreateLoggingService"}}).IsSuccess());

    ASSERT_TRUE(captured_entry.has_value());
    EXPECT_EQ(captured_entry->level, LogLevel::ERR);
    EXPECT_EQ(captured_entry->category, "TraceDiagnostics");
    EXPECT_EQ(captured_entry->message, "logger bootstrap failed");
    EXPECT_EQ(captured_entry->metadata.at("failure_category"), "missing-config");
    EXPECT_EQ(captured_entry->metadata.at("primary_cause"), "CreateLoggingService");
}

}  // namespace
