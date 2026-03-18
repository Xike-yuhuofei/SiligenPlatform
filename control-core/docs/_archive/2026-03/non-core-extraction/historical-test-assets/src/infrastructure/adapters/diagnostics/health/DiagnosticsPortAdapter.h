#pragma once

#include "domain/diagnostics/ports/IDiagnosticsPort.h"
#include "domain/machine/ports/IHardwareTestPort.h"
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
using Siligen::Domain::Diagnostics::ValueObjects::HardwareCheckResult;
using Siligen::Domain::Machine::Ports::IHardwareTestPort;
using Siligen::Shared::Types::Result;

/**
 * @brief 诊断端口适配器
 *
 * 基于 IHardwareTestPort 的硬件检查能力，提供基础诊断与健康状态聚合。
 */
class DiagnosticsPortAdapter final : public IDiagnosticsPort {
   public:
    explicit DiagnosticsPortAdapter(std::shared_ptr<IHardwareTestPort> hardware_test_port);

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
    std::shared_ptr<IHardwareTestPort> hardware_test_port_;
    mutable std::mutex mutex_;
    std::vector<DiagnosticInfo> diagnostics_;
    PerformanceMetrics performance_metrics_;

    static bool IsAtLeast(DiagnosticLevel level, DiagnosticLevel min_level);
    static int LevelRank(DiagnosticLevel level);
    HealthState EvaluateHealthState(const HardwareCheckResult& check) const;
    void AppendHardwareDiagnostics(const HardwareCheckResult& check,
                                   std::vector<DiagnosticInfo>& output) const;
};

}  // namespace Siligen::Infrastructure::Adapters::Diagnostics




