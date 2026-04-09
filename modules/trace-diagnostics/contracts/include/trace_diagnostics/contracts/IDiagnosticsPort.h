#pragma once

#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <string>
#include <vector>

namespace Siligen::Domain::Diagnostics::Ports {

using Shared::Types::Result;

enum class DiagnosticLevel {
    INFO,
    WARNING,
    ERR,
    CRITICAL
};

enum class HealthState {
    HEALTHY,
    DEGRADED,
    UNHEALTHY,
    CRITICAL
};

struct DiagnosticInfo {
    DiagnosticLevel level = DiagnosticLevel::INFO;
    std::string component;
    std::string message;
    int32 error_code = 0;
    int64 timestamp = 0;
};

struct HealthReport {
    HealthState overall_state = HealthState::HEALTHY;
    std::vector<DiagnosticInfo> diagnostics;
    float32 cpu_usage = 0.0f;
    float32 memory_usage = 0.0f;
    int32 active_connections = 0;
};

struct PerformanceMetrics {
    float32 average_response_time_ms = 0.0f;
    float32 max_response_time_ms = 0.0f;
    int32 total_operations = 0;
    int32 failed_operations = 0;
    float32 success_rate = 100.0f;
};

class IDiagnosticsPort {
   public:
    virtual ~IDiagnosticsPort() = default;

    virtual Result<HealthReport> GetHealthReport() const = 0;
    virtual Result<HealthState> GetHealthState() const = 0;
    virtual Result<bool> IsSystemHealthy() const = 0;

    virtual Result<std::vector<DiagnosticInfo>> GetDiagnostics(
        DiagnosticLevel min_level = DiagnosticLevel::INFO) const = 0;
    virtual Result<void> AddDiagnostic(const DiagnosticInfo& info) = 0;
    virtual Result<void> ClearDiagnostics() = 0;

    virtual Result<PerformanceMetrics> GetPerformanceMetrics() const = 0;
    virtual Result<void> ResetPerformanceMetrics() = 0;

    virtual Result<bool> IsComponentHealthy(const std::string& component) const = 0;
    virtual Result<std::vector<std::string>> GetUnhealthyComponents() const = 0;
};

}  // namespace Siligen::Domain::Diagnostics::Ports
