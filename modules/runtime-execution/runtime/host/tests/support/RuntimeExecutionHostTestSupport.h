#pragma once

#include "domain/machine/aggregates/DispenserModel.h"
#include "domain/motion/ports/IHomingPort.h"
#include "runtime/supervision/RuntimeSupervisionPortAdapter.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCard.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCardWrapper.h"
#include "siligen/device/adapters/motion/MotionRuntimeConnectionAdapter.h"
#include "siligen/device/adapters/motion/MotionRuntimeFacade.h"
#include "siligen/device/adapters/motion/MultiCardMotionAdapter.h"
#include "siligen/device/contracts/commands/device_commands.h"
#include "siligen/device/contracts/ports/device_ports.h"

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace Siligen::Runtime::Host::Tests {

using CMPTriggerPoint = Siligen::Shared::Types::CMPTriggerPoint;
using DeviceConnection = Siligen::Device::Contracts::Commands::DeviceConnection;
using DeviceConnectionPort = Siligen::Device::Contracts::Ports::DeviceConnectionPort;
using DispenserModel = Siligen::Domain::Machine::Aggregates::DispenserModel;
using DispensingTask = Siligen::Domain::Machine::Aggregates::DispensingTask;
using HeartbeatSnapshot = Siligen::Device::Contracts::State::HeartbeatSnapshot;
using HomingStatus = Siligen::Domain::Motion::Ports::HomingStatus;
using IRuntimeSupervisionBackend = Siligen::Runtime::Host::Supervision::IRuntimeSupervisionBackend;
using LogicalAxisId = Siligen::Shared::Types::LogicalAxisId;
using MockMultiCard = Siligen::Infrastructure::Hardware::MockMultiCard;
using MockMultiCardWrapper = Siligen::Infrastructure::Hardware::MockMultiCardWrapper;
using MotionRuntimeConnectionAdapter = Siligen::Infrastructure::Adapters::Motion::MotionRuntimeConnectionAdapter;
using MotionRuntimeFacade = Siligen::Infrastructure::Adapters::Motion::MotionRuntimeFacade;
using MultiCardMotionAdapter = Siligen::Infrastructure::Adapters::MultiCardMotionAdapter;
using ResultVoid = Siligen::Shared::Types::Result<void>;
using RuntimeSupervisionInputs = Siligen::Runtime::Host::Supervision::RuntimeSupervisionInputs;
using int32 = Siligen::Shared::Types::int32;

class NoOpHomingPort final : public Siligen::Domain::Motion::Ports::IHomingPort {
   public:
    ResultVoid HomeAxis(LogicalAxisId) override { return ResultVoid::Success(); }
    ResultVoid StopHoming(LogicalAxisId) override { return ResultVoid::Success(); }
    ResultVoid ResetHomingState(LogicalAxisId) override { return ResultVoid::Success(); }

    Siligen::Shared::Types::Result<HomingStatus> GetHomingStatus(LogicalAxisId axis) const override {
        HomingStatus status;
        status.axis = axis;
        return Siligen::Shared::Types::Result<HomingStatus>::Success(status);
    }

    Siligen::Shared::Types::Result<bool> IsAxisHomed(LogicalAxisId) const override {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    Siligen::Shared::Types::Result<bool> IsHomingInProgress(LogicalAxisId) const override {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    ResultVoid WaitForHomingComplete(LogicalAxisId, int32) override { return ResultVoid::Success(); }
};

struct MotionRuntimeTestRig {
    std::shared_ptr<MockMultiCard> mock_card;
    std::shared_ptr<MockMultiCardWrapper> wrapper;
    std::shared_ptr<MultiCardMotionAdapter> motion_adapter;
    std::shared_ptr<MotionRuntimeFacade> runtime;
    std::shared_ptr<MotionRuntimeConnectionAdapter> connection_adapter;
};

inline MotionRuntimeTestRig CreateMotionRuntimeTestRig() {
    MotionRuntimeTestRig rig;
    rig.mock_card = std::make_shared<MockMultiCard>();
    rig.wrapper = std::make_shared<MockMultiCardWrapper>(rig.mock_card);

    auto motion_adapter_result = MultiCardMotionAdapter::Create(rig.wrapper);
    if (motion_adapter_result.IsError()) {
        return rig;
    }

    rig.motion_adapter = motion_adapter_result.Value();
    rig.runtime = std::make_shared<MotionRuntimeFacade>(rig.motion_adapter, std::make_shared<NoOpHomingPort>());
    rig.connection_adapter = std::make_shared<MotionRuntimeConnectionAdapter>(rig.runtime);
    return rig;
}

inline DeviceConnection MakeMockDeviceConnection() {
    DeviceConnection connection;
    connection.local_ip = "192.168.10.10";
    connection.card_ip = "192.168.10.20";
    connection.local_port = 5000;
    connection.card_port = 5000;
    connection.timeout_ms = 3000;
    return connection;
}

inline HeartbeatSnapshot MakeFastHeartbeatConfig() {
    HeartbeatSnapshot snapshot;
    snapshot.interval_ms = 100;
    snapshot.timeout_ms = 100;
    snapshot.failure_threshold = 1;
    return snapshot;
}

inline DispensingTask MakePendingTask(const std::string& task_id = "task-1") {
    DispensingTask task;
    task.task_id = task_id;
    task.path = {
        Siligen::Shared::Types::Point2D(0.0f, 0.0f),
        Siligen::Shared::Types::Point2D(10.0f, 0.0f),
    };
    task.cmp_config.AddTriggerPoint(CMPTriggerPoint{});
    task.movement_speed = 12.0f;
    return task;
}

inline bool WaitUntil(
    const std::function<bool()>& predicate,
    std::chrono::milliseconds timeout,
    std::chrono::milliseconds poll_interval = std::chrono::milliseconds(20)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(poll_interval);
    }
    return predicate();
}

class ConnectionOnlyRuntimeSupervisionBackend final : public IRuntimeSupervisionBackend {
   public:
    explicit ConnectionOnlyRuntimeSupervisionBackend(std::shared_ptr<DeviceConnectionPort> device_connection_port)
        : device_connection_port_(std::move(device_connection_port)) {}

    bool door_known = true;
    bool door_open = false;
    bool interlock_latched = false;
    std::string active_job_id;
    bool active_job_status_available = false;
    std::string active_job_state;

    Siligen::Shared::Types::Result<RuntimeSupervisionInputs> ReadInputs() const override {
        RuntimeSupervisionInputs inputs;
        inputs.has_hardware_connection_port = static_cast<bool>(device_connection_port_);

        if (device_connection_port_) {
            auto connection_result = device_connection_port_->ReadConnection();
            if (connection_result.IsSuccess()) {
                inputs.connection_info = connection_result.Value();
            }
            inputs.heartbeat_status = device_connection_port_->ReadHeartbeat();
        }

        const bool hardware_ready =
            !inputs.has_hardware_connection_port ||
            (inputs.connection_info.IsConnected() && !inputs.heartbeat_status.is_degraded);

        inputs.connected = hardware_ready;
        inputs.io.estop = false;
        inputs.io.estop_known = hardware_ready;
        inputs.io.door = door_open;
        inputs.io.door_known = door_known;
        inputs.interlock_latched = interlock_latched;
        inputs.active_job_id = active_job_id;
        inputs.active_job_status_available = active_job_status_available;
        inputs.active_job_state = active_job_state;

        return Siligen::Shared::Types::Result<RuntimeSupervisionInputs>::Success(std::move(inputs));
    }

   private:
    std::shared_ptr<DeviceConnectionPort> device_connection_port_;
};

}  // namespace Siligen::Runtime::Host::Tests
