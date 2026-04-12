#pragma once

#include "trace_diagnostics/contracts/DiagnosticTypes.h"
#include "motion_planning/contracts/HardwareTestTypes.h"
#include "motion_planning/contracts/MotionTypes.h"
#include "motion_planning/contracts/TrajectoryAnalysisTypes.h"
#include "motion_planning/contracts/TrajectoryTypes.h"
#include "shared/types/Point.h"

#include <cmath>
#include <cstdint>
#include <map>
#include <vector>

namespace Siligen {
namespace Domain {
namespace Diagnostics {
namespace ValueObjects {

using Siligen::Shared::Types::Point2D;
using Siligen::Domain::Motion::ValueObjects::CMPParameters;
using Siligen::Domain::Motion::ValueObjects::InterpolationParameters;
using Siligen::Domain::Motion::ValueObjects::JogDirection;
using Siligen::Domain::Motion::ValueObjects::MotionQualityMetrics;
using Siligen::Domain::Motion::ValueObjects::PathQualityMetrics;
using Siligen::Domain::Motion::ValueObjects::TrajectoryAnalysis;
using Siligen::Domain::Motion::ValueObjects::TrajectoryInterpolationType;
using Siligen::Domain::Motion::ValueObjects::TrajectoryType;
using Siligen::Domain::Motion::ValueObjects::HomingTestMode;
using Siligen::Domain::Motion::ValueObjects::HomingTestDirection;
using Siligen::Domain::Motion::ValueObjects::HomingTestStatus;
using Siligen::Domain::Motion::ValueObjects::TriggerType;
using Siligen::Domain::Motion::ValueObjects::TriggerPoint;
using Siligen::Domain::Motion::ValueObjects::TriggerEvent;
using Siligen::Domain::Motion::ValueObjects::LimitSwitchState;
using Siligen::Domain::Motion::ValueObjects::HomingResult;
using Siligen::Shared::Types::LogicalAxisId;

class JogTestData {
   public:
    std::int64_t recordId;
    LogicalAxisId axis;
    JogDirection direction;
    double speed;
    double distance;
    double startPosition;
    double endPosition;
    double actualDistance;
    std::int32_t responseTimeMs;

    JogTestData()
        : recordId(0),
          axis(LogicalAxisId::X),
          direction(JogDirection::Positive),
          speed(0.0),
          distance(0.0),
          startPosition(0.0),
          endPosition(0.0),
          actualDistance(0.0),
          responseTimeMs(0) {}

    double positionError() const {
        return std::abs(actualDistance - distance);
    }

    bool isValid() const {
        return Siligen::Shared::Types::IsValid(axis) && speed > 0 && distance > 0 && responseTimeMs >= 0;
    }
};

class HomingTestData {
   public:
    std::int64_t recordId;
    std::vector<LogicalAxisId> axes;
    HomingTestMode mode;
    double searchSpeed;
    double returnSpeed;
    HomingTestDirection direction;
    std::map<LogicalAxisId, HomingResult> axisResults;
    std::map<LogicalAxisId, LimitSwitchState> limitStates;

    HomingTestData()
        : recordId(0),
          mode(HomingTestMode::SingleAxis),
          searchSpeed(0.0),
          returnSpeed(0.0),
          direction(HomingTestDirection::Negative) {}

    bool allSucceeded() const {
        for (const auto& [axis, result] : axisResults) {
            (void)axis;
            if (!result.success) {
                return false;
            }
        }
        return !axisResults.empty();
    }

    bool isValid() const {
        return !axes.empty() && searchSpeed > 0 && returnSpeed > 0 && returnSpeed < searchSpeed;
    }
};

class TriggerTestData {
   public:
    std::int64_t recordId;
    TriggerType triggerType;
    std::vector<TriggerPoint> triggerPoints;
    std::vector<TriggerEvent> triggerEvents;
    TriggerStatistics statistics;

    TriggerTestData() : recordId(0), triggerType(TriggerType::SinglePoint) {}

    bool isValid() const {
        return !triggerPoints.empty();
    }
};

class CMPTestData {
   public:
    std::int64_t recordId;
    TrajectoryType trajectoryType;
    std::vector<Point2D> controlPoints;
    CMPParameters cmpParams;
    std::vector<Point2D> theoreticalPath;
    std::vector<Point2D> actualPath;
    std::vector<Point2D> compensationData;
    TrajectoryAnalysis analysis;

    CMPTestData() : recordId(0), trajectoryType(TrajectoryType::Linear) {}

    bool isValid() const {
        if (controlPoints.size() < 2) {
            return false;
        }
        if (!theoreticalPath.empty() && !actualPath.empty()) {
            if (theoreticalPath.size() != actualPath.size()) {
                return false;
            }
        }
        return true;
    }
};

class InterpolationTestData {
   public:
    std::int64_t recordId;
    TrajectoryInterpolationType interpolationType;
    std::vector<Point2D> controlPoints;
    InterpolationParameters interpParams;
    std::vector<Point2D> interpolatedPath;
    PathQualityMetrics pathQuality;
    MotionQualityMetrics motionQuality;

    InterpolationTestData() : recordId(0), interpolationType(TrajectoryInterpolationType::Linear) {}

    bool isValid() const {
        int minPoints = 2;
        switch (interpolationType) {
            case TrajectoryInterpolationType::Linear:
                minPoints = 2;
                break;
            case TrajectoryInterpolationType::CircularArc:
                minPoints = 3;
                break;
            case TrajectoryInterpolationType::BSpline:
            case TrajectoryInterpolationType::Bezier:
                minPoints = 4;
                break;
        }
        return controlPoints.size() >= static_cast<size_t>(minPoints) && !interpolatedPath.empty() &&
               interpParams.targetFeedRate > 0;
    }
};

class DiagnosticResult {
   public:
    std::int64_t recordId;
    HardwareCheckResult hardwareCheck;
    CommunicationCheckResult commCheck;
    ResponseTimeCheckResult responseCheck;
    AccuracyCheckResult accuracyCheck;
    int healthScore;
    std::vector<DiagnosticIssue> issues;

    DiagnosticResult() : recordId(0), healthScore(100) {}

    int calculateHealthScore() const {
        int score = 100;
        for (const auto& issue : issues) {
            switch (issue.severity) {
                case IssueSeverity::Critical:
                    score -= 25;
                    break;
                case IssueSeverity::Error:
                    score -= 10;
                    break;
                case IssueSeverity::Warning:
                    score -= 5;
                    break;
                case IssueSeverity::Info:
                    break;
            }
        }
        return (score < 0) ? 0 : score;
    }

    bool isValid() const {
        return healthScore >= 0 && healthScore <= 100;
    }
};

}  // namespace ValueObjects
}  // namespace Diagnostics
}  // namespace Domain
}  // namespace Siligen
