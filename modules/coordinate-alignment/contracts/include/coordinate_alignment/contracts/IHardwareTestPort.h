#pragma once

#include "trace_diagnostics/contracts/TraceDiagnosticsContracts.h"
#include "../../../../../motion-planning/domain/motion/value-objects/HardwareTestTypes.h"
#include "../../../../../motion-planning/domain/motion/value-objects/TrajectoryTypes.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <map>
#include <vector>

namespace Siligen::Domain::Machine::Ports {

using Shared::Types::Result;
using Shared::Types::LogicalAxisId;
using Shared::Types::Point2D;
using Siligen::Domain::Diagnostics::ValueObjects::AccuracyCheckResult;
using Siligen::Domain::Diagnostics::ValueObjects::CommunicationCheckResult;
using Siligen::Domain::Diagnostics::ValueObjects::HardwareCheckResult;
using Siligen::Domain::Diagnostics::ValueObjects::ResponseTimeCheckResult;
using Siligen::Domain::Motion::ValueObjects::CMPParameters;
using Siligen::Domain::Motion::ValueObjects::HomingStage;
using Siligen::Domain::Motion::ValueObjects::HomingTestStatus;
using Siligen::Domain::Motion::ValueObjects::InterpolationParameters;
using Siligen::Domain::Motion::ValueObjects::JogDirection;
using Siligen::Domain::Motion::ValueObjects::LimitSwitchState;
using Siligen::Domain::Motion::ValueObjects::TrajectoryDefinition;
using Siligen::Domain::Motion::ValueObjects::TrajectoryInterpolationType;
using Siligen::Domain::Motion::ValueObjects::TriggerAction;
using Siligen::Domain::Motion::ValueObjects::TriggerEvent;

class IHardwareTestPort {
   public:
    virtual ~IHardwareTestPort() = default;

    virtual bool isConnected() const = 0;
    virtual std::map<LogicalAxisId, bool> getAxisEnableStates() const = 0;

    virtual Result<void> startJog(LogicalAxisId axis, JogDirection direction, double speed) = 0;
    virtual Result<void> stopJog(LogicalAxisId axis) = 0;
    virtual Result<void> startJogStep(LogicalAxisId axis, JogDirection direction, double distance, double speed) = 0;
    virtual Result<double> getAxisPosition(LogicalAxisId axis) const = 0;
    virtual std::map<LogicalAxisId, double> getAllAxisPositions() const = 0;

    virtual Result<void> startHoming(const std::vector<LogicalAxisId>& axes) = 0;
    virtual Result<void> stopHoming(const std::vector<LogicalAxisId>& axes) = 0;
    virtual HomingTestStatus getHomingStatus(LogicalAxisId axis) const = 0;
    virtual HomingStage getHomingStage(LogicalAxisId axis) const = 0;
    virtual LimitSwitchState getLimitSwitchState(LogicalAxisId axis) const = 0;

    virtual Result<void> setDigitalOutput(int port, bool value) = 0;
    virtual Result<bool> getDigitalInput(int port) const = 0;
    virtual std::map<int, bool> getAllDigitalInputs() const = 0;
    virtual std::map<int, bool> getAllDigitalOutputs() const = 0;

    virtual Result<int> configureTriggerPoint(LogicalAxisId axis, double position, int outputPort, TriggerAction action) = 0;
    virtual Result<void> enableTrigger(int triggerPointId) = 0;
    virtual Result<void> disableTrigger(int triggerPointId) = 0;
    virtual Result<void> clearAllTriggers() = 0;
    virtual std::vector<TriggerEvent> getTriggerEvents() const = 0;

    virtual Result<void> startCMPTest(const TrajectoryDefinition& trajectory, const CMPParameters& cmpParams) = 0;
    virtual Result<void> stopCMPTest() = 0;
    virtual double getCMPTestProgress() const = 0;
    virtual std::vector<Point2D> getCMPActualPath() const = 0;

    virtual Result<void> executeInterpolationTest(TrajectoryInterpolationType interpolationType,
                                                  const std::vector<Point2D>& controlPoints,
                                                  const InterpolationParameters& interpParams) = 0;
    virtual std::vector<Point2D> getInterpolatedPath() const = 0;

    virtual HardwareCheckResult checkHardwareConnection() = 0;
    virtual CommunicationCheckResult testCommunicationQuality(int testDurationMs) = 0;
    virtual Result<double> measureAxisResponseTime(LogicalAxisId axis) = 0;
    virtual Result<AccuracyCheckResult> testPositioningAccuracy(LogicalAxisId axis, int testCycles) = 0;

    virtual Result<void> emergencyStop() = 0;
    virtual Result<void> resetEmergencyStop() = 0;
    virtual bool isEmergencyStopActive() const = 0;
    virtual Result<void> validateMotionParameters(LogicalAxisId axis, double targetPosition, double speed) const = 0;
};

}  // namespace Siligen::Domain::Machine::Ports
