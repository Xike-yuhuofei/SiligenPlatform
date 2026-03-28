#include "runtime/diagnostics/DiagnosticsPortAdapter.h"

#include "siligen/device/contracts/faults/device_faults.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include "siligen/device/contracts/state/device_state.h"

#include <gtest/gtest.h>

#include <memory>
#include <utility>

namespace {

using DiagnosticsPortAdapter = Siligen::Infrastructure::Adapters::Diagnostics::DiagnosticsPortAdapter;
using DiagnosticInfo = Siligen::Domain::Diagnostics::Ports::DiagnosticInfo;
using DiagnosticLevel = Siligen::Domain::Diagnostics::Ports::DiagnosticLevel;
using HealthState = Siligen::Domain::Diagnostics::Ports::HealthState;
using DeviceFault = Siligen::Device::Contracts::Faults::DeviceFault;
using DeviceFaultCategory = Siligen::Device::Contracts::Faults::DeviceFaultCategory;
using DeviceFaultSeverity = Siligen::Device::Contracts::Faults::DeviceFaultSeverity;
using MachineHealthPort = Siligen::Device::Contracts::Ports::MachineHealthPort;
using MachineHealthSnapshot = Siligen::Device::Contracts::State::MachineHealthSnapshot;
using SharedHealthResult = Siligen::SharedKernel::Result<MachineHealthSnapshot>;

class FakeMachineHealthPort final : public MachineHealthPort {
   public:
    SharedHealthResult ReadHealth() const override {
        return SharedHealthResult::Success(snapshot);
    }

    MachineHealthSnapshot snapshot;
};

TEST(DiagnosticsPortAdapterTest, ReportsCriticalWhenEmergencyStopIsActive) {
    auto machine_health_port = std::make_shared<FakeMachineHealthPort>();
    machine_health_port->snapshot.connected = true;
    machine_health_port->snapshot.estop_active = true;
    machine_health_port->snapshot.active_faults.push_back(DeviceFault{
        "HW-001",
        DeviceFaultSeverity::kWarning,
        DeviceFaultCategory::kIo,
        "door open",
        "",
        true});

    DiagnosticsPortAdapter adapter(machine_health_port);

    DiagnosticInfo manual_diagnostic;
    manual_diagnostic.level = DiagnosticLevel::ERR;
    manual_diagnostic.component = "planner";
    manual_diagnostic.message = "manual regression finding";
    ASSERT_TRUE(adapter.AddDiagnostic(manual_diagnostic).IsSuccess());

    auto report_result = adapter.GetHealthReport();
    ASSERT_TRUE(report_result.IsSuccess()) << report_result.GetError().GetMessage();

    const auto& report = report_result.Value();
    EXPECT_EQ(report.overall_state, HealthState::CRITICAL);
    EXPECT_EQ(report.active_connections, 1);
    EXPECT_GE(report.diagnostics.size(), 3u);

    auto unhealthy_components = adapter.GetUnhealthyComponents();
    ASSERT_TRUE(unhealthy_components.IsSuccess());
    EXPECT_NE(
        std::find(unhealthy_components.Value().begin(), unhealthy_components.Value().end(), "planner"),
        unhealthy_components.Value().end());
}

TEST(DiagnosticsPortAdapterTest, FiltersDiagnosticsByMinimumLevel) {
    auto machine_health_port = std::make_shared<FakeMachineHealthPort>();
    machine_health_port->snapshot.connected = true;

    DiagnosticsPortAdapter adapter(machine_health_port);

    DiagnosticInfo info_diagnostic;
    info_diagnostic.level = DiagnosticLevel::INFO;
    info_diagnostic.component = "host";
    info_diagnostic.message = "info";
    ASSERT_TRUE(adapter.AddDiagnostic(info_diagnostic).IsSuccess());

    DiagnosticInfo err_diagnostic;
    err_diagnostic.level = DiagnosticLevel::ERR;
    err_diagnostic.component = "motion";
    err_diagnostic.message = "error";
    ASSERT_TRUE(adapter.AddDiagnostic(err_diagnostic).IsSuccess());

    auto filtered = adapter.GetDiagnostics(DiagnosticLevel::WARNING);
    ASSERT_TRUE(filtered.IsSuccess());
    ASSERT_EQ(filtered.Value().size(), 1u);
    EXPECT_EQ(filtered.Value().front().component, "motion");
}

}  // namespace
