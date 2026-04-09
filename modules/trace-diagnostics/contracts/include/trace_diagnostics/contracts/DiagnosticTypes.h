#pragma once

#include "shared/types/AxisTypes.h"

#include <boost/describe/enum.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Siligen {
namespace Domain {
namespace Diagnostics {
namespace ValueObjects {

enum class IssueSeverity {
    Info,
    Warning,
    Error,
    Critical
};
BOOST_DESCRIBE_ENUM(IssueSeverity, Info, Warning, Error, Critical)

struct DiagnosticIssue {
    IssueSeverity severity;
    std::string description;
    std::string suggestedAction;

    DiagnosticIssue() : severity(IssueSeverity::Info) {}

    DiagnosticIssue(IssueSeverity sev, const std::string& desc, const std::string& action)
        : severity(sev), description(desc), suggestedAction(action) {}
};

struct HardwareCheckResult {
    bool controllerConnected;
    std::vector<Siligen::Shared::Types::LogicalAxisId> enabledAxes;
    std::map<Siligen::Shared::Types::LogicalAxisId, bool> limitSwitchOk;
    std::map<Siligen::Shared::Types::LogicalAxisId, bool> encoderOk;

    HardwareCheckResult() : controllerConnected(false) {}
};

struct CommunicationCheckResult {
    double avgResponseTimeMs;
    double maxResponseTimeMs;
    double packetLossRate;
    int totalCommands;
    int failedCommands;

    CommunicationCheckResult()
        : avgResponseTimeMs(0.0), maxResponseTimeMs(0.0), packetLossRate(0.0), totalCommands(0), failedCommands(0) {}

    double successRate() const {
        if (totalCommands == 0) return 0.0;
        return 100.0 * (totalCommands - failedCommands) / totalCommands;
    }
};

struct ResponseTimeCheckResult {
    std::map<Siligen::Shared::Types::LogicalAxisId, double> axisResponseTimeMs;
    bool allWithinSpec;
    double specLimitMs;

    ResponseTimeCheckResult() : allWithinSpec(true), specLimitMs(10.0) {}
};

struct AccuracyCheckResult {
    std::map<Siligen::Shared::Types::LogicalAxisId, double> repeatabilityError;
    std::map<Siligen::Shared::Types::LogicalAxisId, double> positioningError;
    bool meetsAccuracyRequirement;
    double requiredAccuracy;

    AccuracyCheckResult() : meetsAccuracyRequirement(true), requiredAccuracy(0.05) {}
};

struct TriggerStatistics {
    double maxPositionError;
    double avgPositionError;
    std::int32_t maxResponseTimeUs;
    std::int32_t avgResponseTimeUs;
    int successfulTriggers;
    int missedTriggers;

    TriggerStatistics()
        : maxPositionError(0.0),
          avgPositionError(0.0),
          maxResponseTimeUs(0),
          avgResponseTimeUs(0),
          successfulTriggers(0),
          missedTriggers(0) {}

    double successRate() const {
        int total = successfulTriggers + missedTriggers;
        if (total == 0) return 0.0;
        return 100.0 * successfulTriggers / total;
    }
};

}  // namespace ValueObjects
}  // namespace Diagnostics
}  // namespace Domain
}  // namespace Siligen
