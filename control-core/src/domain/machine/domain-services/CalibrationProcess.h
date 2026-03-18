#pragma once

#include "domain/machine/ports/ICalibrationDevicePort.h"
#include "domain/machine/ports/ICalibrationResultPort.h"
#include "domain/machine/value-objects/CalibrationTypes.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <chrono>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace Siligen::Domain::Machine::DomainServices {

using Siligen::Domain::Machine::Ports::ICalibrationDevicePort;
using Siligen::Domain::Machine::Ports::ICalibrationResultPort;
using Siligen::Domain::Machine::ValueObjects::CalibrationRequest;
using Siligen::Domain::Machine::ValueObjects::CalibrationResult;
using Siligen::Domain::Machine::ValueObjects::CalibrationState;
using Siligen::Shared::Types::Result;

/**
 * @brief Calibration workflow and rules.
 *
 * Public API is noexcept and returns Result<T>.
 * No IO/threads/exceptions - uses ports for device and storage.
 */
class CalibrationProcess final {
   public:
    explicit CalibrationProcess(std::shared_ptr<ICalibrationDevicePort> device_port,
                                std::shared_ptr<ICalibrationResultPort> result_port) noexcept;

    CalibrationProcess(const CalibrationProcess&) = delete;
    CalibrationProcess& operator=(const CalibrationProcess&) = delete;
    CalibrationProcess(CalibrationProcess&&) = delete;
    CalibrationProcess& operator=(CalibrationProcess&&) = delete;

    Result<void> Start(const CalibrationRequest& request) noexcept;
    Result<void> ProcessNextStep() noexcept;
    Result<void> Cancel() noexcept;
    void Reset() noexcept;

    CalibrationState GetState() const noexcept {
        return current_state_;
    }

    const CalibrationRequest& GetRequest() const noexcept {
        return context_.request;
    }

    const CalibrationResult& GetResult() const noexcept {
        return context_.result;
    }

    bool IsActive() const noexcept {
        return current_state_ != CalibrationState::Idle && !IsTerminalState(current_state_);
    }

   private:
    struct CalibrationContext {
        CalibrationRequest request;
        CalibrationResult result;
        std::string error_message;
        std::chrono::steady_clock::time_point start_time{};
        std::chrono::steady_clock::time_point end_time{};
    };

    std::shared_ptr<ICalibrationDevicePort> device_port_;
    std::shared_ptr<ICalibrationResultPort> result_port_;
    CalibrationState current_state_{CalibrationState::Idle};
    CalibrationContext context_{};
    std::set<std::pair<CalibrationState, CalibrationState>> valid_transitions_;

    void InitializeValidTransitions() noexcept;
    bool IsValidTransition(CalibrationState from, CalibrationState to) const noexcept;
    Result<void> TransitionTo(CalibrationState new_state) noexcept;
    Result<void> FinalizeResult(CalibrationState terminal_state, const std::string& message) noexcept;
    Result<void> FailWithError(const Siligen::Shared::Types::Error& error) noexcept;

    Result<void> ProcessValidation() noexcept;
    Result<void> ProcessExecution() noexcept;
    Result<void> ProcessVerification() noexcept;

    static bool IsTerminalState(CalibrationState state) noexcept;
    static const char* StateToString(CalibrationState state) noexcept;
};

}  // namespace Siligen::Domain::Machine::DomainServices
