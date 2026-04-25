#pragma once

#include "shared/types/Types.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace Siligen::Shared::Types {

enum class PathQualityVerdict {
    Pass,
    PassWithException,
    Fail,
};

inline constexpr char kPathQualityReasonProcessPathFragmentation[] = "process_path_fragmentation";
inline constexpr char kPathQualityReasonPathDiscontinuity[] = "path_discontinuity";
inline constexpr char kPathQualityReasonMicroSegmentBurst[] = "micro_segment_burst";
inline constexpr char kPathQualityReasonAbnormalShortSegment[] = "abnormal_short_segment";
inline constexpr char kPathQualityReasonSmallBacktrack[] = "small_backtrack";
inline constexpr char kPathQualityReasonUnclassifiedPathException[] = "unclassified_path_exception";
inline constexpr char kPathQualityReasonSpacingInvalid[] = "spacing_invalid";
inline constexpr char kPathQualityReasonFormalCompareBlocked[] = "formal_compare_blocked";

struct PathQualityThresholds {
    std::size_t max_micro_segment_count = 3U;
    float32 max_micro_segment_ratio = 0.25f;
    float32 small_backtrack_cosine_threshold = -0.95f;
    float32 small_backtrack_leg_max_mm = 1.0f;
};

inline constexpr PathQualityThresholds kDefaultPathQualityThresholds{};

struct PathQualityAssessment {
    PathQualityVerdict verdict = PathQualityVerdict::Pass;
    bool blocking = false;
    std::vector<std::string> reason_codes;
};

inline constexpr const char* ToString(const PathQualityVerdict verdict) noexcept {
    switch (verdict) {
        case PathQualityVerdict::Pass:
            return "pass";
        case PathQualityVerdict::PassWithException:
            return "pass_with_exception";
        case PathQualityVerdict::Fail:
            return "fail";
        default:
            return "pass";
    }
}

inline bool IsPathQualityReasonCode(const std::string_view reason_code) {
    return reason_code == kPathQualityReasonProcessPathFragmentation ||
           reason_code == kPathQualityReasonPathDiscontinuity ||
           reason_code == kPathQualityReasonMicroSegmentBurst ||
           reason_code == kPathQualityReasonAbnormalShortSegment ||
           reason_code == kPathQualityReasonSmallBacktrack ||
           reason_code == kPathQualityReasonUnclassifiedPathException ||
           reason_code == kPathQualityReasonSpacingInvalid ||
           reason_code == kPathQualityReasonFormalCompareBlocked;
}

inline void AppendPathQualityReasonCode(
    PathQualityAssessment& assessment,
    const std::string_view reason_code) {
    if (!IsPathQualityReasonCode(reason_code)) {
        return;
    }

    const auto reason = std::string(reason_code);
    if (std::find(assessment.reason_codes.begin(), assessment.reason_codes.end(), reason) ==
        assessment.reason_codes.end()) {
        assessment.reason_codes.push_back(reason);
    }
}

}  // namespace Siligen::Shared::Types
