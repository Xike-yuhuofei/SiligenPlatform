#pragma once

#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <string>
#include <vector>

namespace Siligen::Domain::Diagnostics::Ports {

using Shared::Types::Result;

/**
 * @brief 诊断级别
 */
enum class DiagnosticLevel {
    INFO,     // 信息
    WARNING,  // 警告
    ERR,      // 错误
    CRITICAL  // 严重错误
};

/**
 * @brief 系统健康状态
 */
enum class HealthState {
    HEALTHY,    // 健康
    DEGRADED,   // 性能下降
    UNHEALTHY,  // 不健康
    CRITICAL    // 严重问题
};

/**
 * @brief 诊断信息
 */
struct DiagnosticInfo {
    DiagnosticLevel level = DiagnosticLevel::INFO;
    std::string component;  // 组件名称
    std::string message;    // 诊断消息
    int32 error_code = 0;   // 错误代码
    int64 timestamp = 0;    // 时间戳
};

/**
 * @brief 系统健康报告
 */
struct HealthReport {
    HealthState overall_state = HealthState::HEALTHY;
    std::vector<DiagnosticInfo> diagnostics;
    float32 cpu_usage = 0.0f;
    float32 memory_usage = 0.0f;
    int32 active_connections = 0;
};

/**
 * @brief 性能指标
 */
struct PerformanceMetrics {
    float32 average_response_time_ms = 0.0f;
    float32 max_response_time_ms = 0.0f;
    int32 total_operations = 0;
    int32 failed_operations = 0;
    float32 success_rate = 100.0f;
};

/**
 * @brief 诊断端口接口
 * 定义系统诊断和监控功能
 */
class IDiagnosticsPort {
   public:
    virtual ~IDiagnosticsPort() = default;

    // 健康检查
    virtual Result<HealthReport> GetHealthReport() const = 0;
    virtual Result<HealthState> GetHealthState() const = 0;
    virtual Result<bool> IsSystemHealthy() const = 0;

    // 诊断信息
    virtual Result<std::vector<DiagnosticInfo>> GetDiagnostics(
        DiagnosticLevel min_level = DiagnosticLevel::INFO) const = 0;
    virtual Result<void> AddDiagnostic(const DiagnosticInfo& info) = 0;
    virtual Result<void> ClearDiagnostics() = 0;

    // 性能监控
    virtual Result<PerformanceMetrics> GetPerformanceMetrics() const = 0;
    virtual Result<void> ResetPerformanceMetrics() = 0;

    // 组件状态
    virtual Result<bool> IsComponentHealthy(const std::string& component) const = 0;
    virtual Result<std::vector<std::string>> GetUnhealthyComponents() const = 0;
};

}  // namespace Siligen::Domain::Diagnostics::Ports

