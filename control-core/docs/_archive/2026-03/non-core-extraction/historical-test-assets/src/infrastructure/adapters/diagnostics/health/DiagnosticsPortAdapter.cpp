#include "DiagnosticsPortAdapter.h"

#include "shared/types/Error.h"
#include "shared/types/AxisTypes.h"

#include <algorithm>
#include <unordered_set>

namespace Siligen::Infrastructure::Adapters::Diagnostics {

DiagnosticsPortAdapter::DiagnosticsPortAdapter(std::shared_ptr<IHardwareTestPort> hardware_test_port)
    : hardware_test_port_(std::move(hardware_test_port)) {}

Result<HealthReport> DiagnosticsPortAdapter::GetHealthReport() const {
    if (!hardware_test_port_) {
        return Result<HealthReport>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED,
                                 "Diagnostics hardware port not initialized",
                                 "DiagnosticsPortAdapter"));
    }

    HealthReport report;
    report.active_connections = hardware_test_port_->isConnected() ? 1 : 0;

    auto hardware_check = hardware_test_port_->checkHardwareConnection();
    report.overall_state = EvaluateHealthState(hardware_check);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        report.diagnostics = diagnostics_;
    }

    AppendHardwareDiagnostics(hardware_check, report.diagnostics);

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
        for (const auto& info : diagnostics_) {
            if (IsAtLeast(info.level, min_level)) {
                result.push_back(info);
            }
        }
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
    for (const auto& info : diagnostics_) {
        if (info.component == component &&
            (info.level == DiagnosticLevel::ERR || info.level == DiagnosticLevel::CRITICAL)) {
            return Result<bool>::Success(false);
        }
    }
    return Result<bool>::Success(true);
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
    return Result<std::vector<std::string>>::Success(
        std::vector<std::string>(components.begin(), components.end()));
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

HealthState DiagnosticsPortAdapter::EvaluateHealthState(const HardwareCheckResult& check) const {
    if (!check.controllerConnected) {
        return HealthState::CRITICAL;
    }

    bool has_warning = false;
    for (const auto& entry : check.limitSwitchOk) {
        if (!entry.second) {
            has_warning = true;
            break;
        }
    }
    for (const auto& entry : check.encoderOk) {
        if (!entry.second) {
            return HealthState::UNHEALTHY;
        }
    }

    if (has_warning) {
        return HealthState::DEGRADED;
    }

    return HealthState::HEALTHY;
}

void DiagnosticsPortAdapter::AppendHardwareDiagnostics(const HardwareCheckResult& check,
                                                       std::vector<DiagnosticInfo>& output) const {
    if (!check.controllerConnected) {
        DiagnosticInfo info;
        info.level = DiagnosticLevel::CRITICAL;
        info.component = "hardware";
        info.message = "Hardware controller not connected";
        info.error_code = static_cast<int32>(Shared::Types::ErrorCode::HARDWARE_NOT_CONNECTED);
        output.push_back(std::move(info));
        return;
    }

    for (const auto& entry : check.limitSwitchOk) {
        if (!entry.second) {
            DiagnosticInfo info;
            info.level = DiagnosticLevel::WARNING;
            info.component = "limit_switch";
            info.message = "Limit switch abnormal on axis " +
                           std::to_string(Siligen::Shared::Types::ToIndex(entry.first));
            output.push_back(std::move(info));
        }
    }

    for (const auto& entry : check.encoderOk) {
        if (!entry.second) {
            DiagnosticInfo info;
            info.level = DiagnosticLevel::ERR;
            info.component = "encoder";
            info.message = "Encoder abnormal on axis " +
                           std::to_string(Siligen::Shared::Types::ToIndex(entry.first));
            output.push_back(std::move(info));
        }
    }
}

}  // namespace Siligen::Infrastructure::Adapters::Diagnostics

