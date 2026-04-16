#pragma once

#include "dispense_packaging/contracts/IValvePort.h"
#include "runtime_execution/contracts/motion/IMotionStatePort.h"
#include "runtime_execution/contracts/system/IRuntimeStatusExportPort.h"
#include "runtime_execution/contracts/system/IRuntimeSupervisionPort.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Siligen::Runtime::Service::Status {

struct RuntimeMotionStatusReader {
    std::function<Siligen::Shared::Types::Result<std::vector<Siligen::Domain::Motion::Ports::MotionStatus>>()>
        read_all_axes_motion_status;
    std::function<Siligen::Shared::Types::Result<Siligen::Shared::Types::Point2D>()> read_current_position;
};

struct RuntimeDispenserStatusReader {
    std::function<
        Siligen::Shared::Types::Result<Siligen::Domain::Dispensing::Ports::DispenserValveState>()>
        read_dispenser_status;
    std::function<
        Siligen::Shared::Types::Result<Siligen::Domain::Dispensing::Ports::SupplyValveStatusDetail>()>
        read_supply_status;
};

struct RuntimeJobExecutionStatusReader {
    std::function<
        Siligen::Shared::Types::Result<
            Siligen::RuntimeExecution::Contracts::System::RuntimeJobExecutionExportSnapshot>(
            const std::string&)>
        read_job_status;
};

class RuntimeStatusExportPort final
    : public Siligen::RuntimeExecution::Contracts::System::IRuntimeStatusExportPort {
   public:
    RuntimeStatusExportPort(
        std::shared_ptr<Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort>
            runtime_supervision_port,
        RuntimeMotionStatusReader motion_status_reader = {},
        RuntimeDispenserStatusReader dispenser_status_reader = {},
        RuntimeJobExecutionStatusReader job_execution_status_reader = {});

    Siligen::Shared::Types::Result<Siligen::RuntimeExecution::Contracts::System::RuntimeStatusExportSnapshot>
    ReadSnapshot() const override;

   private:
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort> runtime_supervision_port_;
    RuntimeMotionStatusReader motion_status_reader_;
    RuntimeDispenserStatusReader dispenser_status_reader_;
    RuntimeJobExecutionStatusReader job_execution_status_reader_;
    mutable std::mutex job_execution_mutex_;
    mutable std::string last_observed_job_id_;
    mutable std::optional<
        Siligen::RuntimeExecution::Contracts::System::RuntimeJobExecutionExportSnapshot>
        cached_terminal_job_execution_;
};

}  // namespace Siligen::Runtime::Service::Status
