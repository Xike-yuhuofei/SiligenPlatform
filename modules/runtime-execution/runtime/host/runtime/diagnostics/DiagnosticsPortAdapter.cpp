#include "DiagnosticsPortAdapter.h"

#include "shared/types/AxisTypes.h"
#include "shared/types/Error.h"

#include <algorithm>
#include <unordered_set>

namespace Siligen::Infrastructure::Adapters::Diagnostics {

DiagnosticsPortAdapter::DiagnosticsPortAdapter(std::shared_ptr<MachineHealthPort> machine_health_port)
    : machine_health_port_(std::move(machine_health_port)) {}

Result<HealthReport> DiagnosticsPortAdapter::GetHealthReport() const {
    if (!machine_health_port_) {
        return Result<HealthReport>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED,
                                 "Diagnostics machine health port not initialized",
                                 "DiagnosticsPortAdapter"));
    }

    const auto health_result = machine_health_port_->ReadHealth();
    if (health_result.IsError()) {
        return Result<HealthReport>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::UNKNOWN_ERROR,
                                 health_result.GetError().GetMessage(),
                                 "DiagnosticsPortAdapter"));
    }
    const auto& health = health_result.Value();

    HealthReport report;
    report.active_connections = health.connected ? 1 : 0;
    report.overall_state = EvaluateHealthState(health);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        report.diagnostics = diagnostics_;
    }

    AppendHardwareDiagnostics(health, report.diagnostics);
    return Result<HealthReport>::Success(std::move(report));
}

Result<HealthState> DiagnosticsPortAdapter::GetHealthState() const {
    auto report = GetHealthReport();
    if (report.IsError()) {
        return Result<HealthState>::Failure(report.GetError());
    }
    return Result<HealthState>::Success(report.Value().overall_state);
}

Result<bool> DiagnosticsPortAdapter::IsSystemHealthy() const {
    auto state = GetHealthState();
    if (state.IsError()) {
        return Result<bool>::Failure(state.GetError());
    }
    return Result<bool>::Success(state.Value() == HealthState::HEALTHY);
}

Result<std::vector<DiagnosticInfo>> DiagnosticsPortAdapter::GetDiagnostics(DiagnosticLevel min_level) const {
    std::vector<DiagnosticInfo> result;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::copy_if(diagnostics_.begin(), diagnostics_.end(), std::back_inserter(result), [min_level](const auto& info) {
            return IsAtLeast(info.level, min_level);
        });
    }

    return Result<std::vector<DiagnosticInfo>>::Success(std::move(result));
}

Result<void> DiagnosticsPortAdapter::AddDiagnostic(const DiagnosticInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    diagnostics_.push_back(info);
    return Result<void>::Success();
}

Result<void> DiagnosticsPortAdapter::ClearDiagnostics() {
    std::lock_guard<std::mutex> lock(mutex_);
    diagnostics_.clear();
    return Result<void>::Success();
}

Result<PerformanceMetrics> DiagnosticsPortAdapter::GetPerformanceMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return Result<PerformanceMetrics>::Success(performance_metrics_);
}

Result<void> DiagnosticsPortAdapter::ResetPerformanceMetrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    performance_metrics_ = PerformanceMetrics();
    return Result<void>::Success();
}

Result<bool> DiagnosticsPortAdapter::IsComponentHealthy(const std::string& component) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool has_error = std::any_of(diagnostics_.begin(), diagnostics_.end(), [&component](const auto& info) {
        return info.component == component &&
               (info.level == DiagnosticLevel::ERR || info.level == DiagnosticLevel::CRITICAL);
    });

    return Result<bool>::Success(!has_error);
}

Result<std::vector<std::string>> DiagnosticsPortAdapter::GetUnhealthyComponents() const {
    std::unordered_set<std::string> components;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& info : diagnostics_) {
            if (info.level == DiagnosticLevel::ERR || info.level == DiagnosticLevel::CRITICAL) {
                components.insert(info.component);
            }
        }
    }

    return Result<std::vector<std::string>>::Success(std::vector<std::string>(components.begin(), components.end()));
}

bool DiagnosticsPortAdapter::IsAtLeast(DiagnosticLevel level, DiagnosticLevel min_level) {
    return LevelRank(level) >= LevelRank(min_level);
}

int DiagnosticsPortAdapter::LevelRank(DiagnosticLevel level) {
    switch (level) {
        case DiagnosticLevel::INFO:
            return 0;
        case DiagnosticLevel::WARNING:
            return 1;
        case DiagnosticLevel::ERR:
            return 2;
        case DiagnosticLevel::CRITICAL:
            return 3;
        default:
            return 0;
    }
}

DiagnosticLevel DiagnosticsPortAdapter::ToDiagnosticLevel(DeviceFaultSeverity severity) {
    switch (severity) {
        case DeviceFaultSeverity::kInfo:
            return DiagnosticLevel::INFO;
        case DeviceFaultSeverity::kWarning:
            return DiagnosticLevel::WARNING;
        case DeviceFaultSeverity::kCritical:
            return DiagnosticLevel::CRITICAL;
        case DeviceFaultSeverity::kError:
        default:
            return DiagnosticLevel::ERR;
    }
}

HealthState DiagnosticsPortAdapter::EvaluateHealthState(const MachineHealthSnapshot& health) {
    if (!health.connected) {
        return HealthState::CRITICAL;
    }
    if (health.estop_active) {
        return HealthState::CRITICAL;
    }
    bool has_warning = false;
    for (const auto& fault : health.active_faults) {
        switch (fault.severity) {
            case DeviceFaultSeverity::kCritical:
                return HealthState::CRITICAL;
            case DeviceFaultSeverity::kError:
                return HealthState::UNHEALTHY;
            case DeviceFaultSeverity::kWarning:
                has_warning = true;
                break;
            case DeviceFaultSeverity::kInfo:
            default:
                break;
        }
    }
    if (has_warning) {
        return HealthState::DEGRADED;
    }
    return HealthState::HEALTHY;
}

void DiagnosticsPortAdapter::AppendHardwareDiagnostics(
    const MachineHealthSnapshot& health,
    std::vector<DiagnosticInfo>& output) {
    for (const auto& fault : health.active_faults) {
        DiagnosticInfo info;
        info.level = ToDiagnosticLevel(fault.severity);
        info.component = "hardware";
        info.message = fault.message;
        output.push_back(std::move(info));
    }

    if (health.estop_active) {
        DiagnosticInfo info;
        info.level = DiagnosticLevel::CRITICAL;
        info.component = "safety";
        info.message = "Emergency stop is active";
        info.error_code = static_cast<int32>(Shared::Types::ErrorCode::EMERGENCY_STOP_ACTIVATED);
        output.push_back(std::move(info));
    }
}

}  // namespace Siligen::Infrastructure::Adapters::Diagnostics
