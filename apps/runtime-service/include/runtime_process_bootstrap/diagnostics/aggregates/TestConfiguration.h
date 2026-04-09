#pragma once

#include "shared/types/AxisTypes.h"

#include <cstdint>
#include <map>

namespace Siligen {
namespace Domain {
namespace Diagnostics {
namespace Aggregates {

struct AxisLimits {
    double minPosition;
    double maxPosition;

    AxisLimits() : minPosition(-200.0), maxPosition(200.0) {}
    AxisLimits(double min, double max) : minPosition(min), maxPosition(max) {}

    bool isValid() const {
        return minPosition < maxPosition;
    }
};

struct SafetyLimits {
    double maxJogSpeed;
    double maxHomingSpeed;
    std::map<Siligen::Shared::Types::LogicalAxisId, AxisLimits> axisLimits;

    SafetyLimits() : maxJogSpeed(100.0), maxHomingSpeed(50.0) {
        axisLimits[Siligen::Shared::Types::LogicalAxisId::X] = AxisLimits(-200.0, 200.0);
        axisLimits[Siligen::Shared::Types::LogicalAxisId::Y] = AxisLimits(-150.0, 150.0);
    }
};

struct TestParameters {
    std::int32_t homingTimeoutMs;
    double triggerToleranceMm;
    int cmpSampleRateHz;
    bool dataRecordEnabled;
    int maxStoredRecords;

    TestParameters()
        : homingTimeoutMs(30000),
          triggerToleranceMm(0.1),
          cmpSampleRateHz(100),
          dataRecordEnabled(true),
          maxStoredRecords(10000) {}
};

struct DisplayParameters {
    std::int32_t positionRefreshIntervalMs;
    int chartMaxPoints;
    bool showDetailedErrors;

    DisplayParameters()
        : positionRefreshIntervalMs(100),
          chartMaxPoints(1000),
          showDetailedErrors(true) {}
};

class TestConfiguration {
   public:
    SafetyLimits safetyLimits;
    TestParameters testParams;
    DisplayParameters displayParams;

    TestConfiguration() = default;

    bool isValid() const {
        if (safetyLimits.maxJogSpeed <= 0 || safetyLimits.maxJogSpeed >= 500.0) {
            return false;
        }
        if (safetyLimits.maxHomingSpeed <= 0 || safetyLimits.maxHomingSpeed >= safetyLimits.maxJogSpeed) {
            return false;
        }

        for (const auto& [axis, limits] : safetyLimits.axisLimits) {
            (void)axis;
            if (!limits.isValid()) {
                return false;
            }
        }

        if (testParams.homingTimeoutMs <= 0) {
            return false;
        }
        if (testParams.cmpSampleRateHz <= 0 || testParams.cmpSampleRateHz > 1000) {
            return false;
        }
        if (displayParams.positionRefreshIntervalMs < 50) {
            return false;
        }
        if (displayParams.chartMaxPoints < 100 || displayParams.chartMaxPoints > 10000) {
            return false;
        }

        return true;
    }

    const AxisLimits* getAxisLimits(Siligen::Shared::Types::LogicalAxisId axis) const {
        auto it = safetyLimits.axisLimits.find(axis);
        if (it != safetyLimits.axisLimits.end()) {
            return &it->second;
        }
        return nullptr;
    }

    bool isPositionInRange(Siligen::Shared::Types::LogicalAxisId axis, double position) const {
        const AxisLimits* limits = getAxisLimits(axis);
        if (limits == nullptr) {
            return false;
        }
        return position >= limits->minPosition && position <= limits->maxPosition;
    }

    bool isJogSpeedSafe(double speed) const {
        return speed > 0 && speed <= safetyLimits.maxJogSpeed;
    }

    bool isHomingSpeedSafe(double speed) const {
        return speed > 0 && speed <= safetyLimits.maxHomingSpeed;
    }
};

}  // namespace Aggregates
}  // namespace Diagnostics
}  // namespace Domain
}  // namespace Siligen
