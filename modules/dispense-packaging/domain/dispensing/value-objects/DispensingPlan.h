#pragma once

#include "DispenseCompensationProfile.h"
#include "QualityMetrics.h"
#include "TriggerPlan.h"
#include "shared/types/Point.h"
#include "shared/types/Types.h"

#include <boost/describe/enum.hpp>

#include <string>
#include <vector>

namespace Siligen::Domain::Dispensing::ValueObjects {

using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::uint32;
using Siligen::MotionSegment;

enum class PlanStatus {
    Draft,
    Ready,
    Executing,
    Completed,
    Failed,
    Aborted
};
BOOST_DESCRIBE_ENUM(PlanStatus, Draft, Ready, Executing, Completed, Failed, Aborted)

struct DispensingPlan {
    std::string plan_id;
    std::string name;
    std::string path_source;
    std::vector<MotionSegment> segments;
    TriggerPlan trigger_plan;
    DispenseCompensationProfile compensation_profile;
    PlanStatus status = PlanStatus::Draft;
    QualityMetrics metrics;

    float32 EstimatedLength() const {
        float32 total = 0.0f;
        for (const auto& seg : segments) {
            total += seg.length;
        }
        return total;
    }
};

}  // namespace Siligen::Domain::Dispensing::ValueObjects
