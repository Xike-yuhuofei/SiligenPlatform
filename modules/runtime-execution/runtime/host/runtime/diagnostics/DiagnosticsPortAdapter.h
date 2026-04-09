#pragma once

#include "trace_diagnostics/contracts/IDiagnosticsPort.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include "shared/types/Result.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Siligen::Infrastructure::Adapters::Diagnostics {

using Siligen::Domain::Diagnostics::Ports::DiagnosticInfo;
using Siligen::Domain::Diagnostics::Ports::DiagnosticLevel;
using Siligen::Domain::Diagnostics::Ports::HealthReport;
using Siligen::Domain::Diagnostics::Ports::HealthState;
using Siligen::Domain::Diagnostics::Ports::IDiagnosticsPort;
using Siligen::Domain::Diagnostics::Ports::PerformanceMetrics;
using Siligen::Device::Contracts::Faults::DeviceFaultSeverity;
using Siligen::Device::Contracts::Ports::MachineHealthPort;
using Siligen::Device::Contracts::State::MachineHealthSnapshot;
using Siligen::Shared::Types::Result;

class DiagnosticsPortAdapter final : public IDiagnosticsPort {
   public:
    explicit DiagnosticsPortAdapter(std::shared_ptr<MachineHealthPort> machine_health_port);

    Result<HealthReport> GetHealthReport() const override;
    Result<HealthState> GetHealthState() const override;
    Result<bool> IsSystemHealthy() const override;

    Result<std::vector<DiagnosticInfo>> GetDiagnostics(
        DiagnosticLevel min_level = DiagnosticLevel::INFO) const override;
    Result<void> AddDiagnostic(const DiagnosticInfo& info) override;
    Result<void> ClearDiagnostics() override;

    Result<PerformanceMetrics> GetPerformanceMetrics() const override;
    Result<void> ResetPerformanceMetrics() override;

    Result<bool> IsComponentHealthy(const std::string& component) const override;
    Result<std::vector<std::string>> GetUnhealthyComponents() const override;

   private:
    std::shared_ptr<MachineHealthPort> machine_health_port_;
    mutable std::mutex mutex_;
    std::vector<DiagnosticInfo> diagnostics_;
    PerformanceMetrics performance_metrics_;

    static bool IsAtLeast(DiagnosticLevel level, DiagnosticLevel min_level);
    static int LevelRank(DiagnosticLevel level);
    static DiagnosticLevel ToDiagnosticLevel(DeviceFaultSeverity severity);
    HealthState EvaluateHealthState(const MachineHealthSnapshot& health) const;
    void AppendHardwareDiagnostics(const MachineHealthSnapshot& health, std::vector<DiagnosticInfo>& output) const;
};

}  // namespace Siligen::Infrastructure::Adapters::Diagnostics
