#pragma once

#include "domain/motion/domain-services/SevenSegmentSCurveProfile.h"

namespace Siligen::Infrastructure::Adapters::Motion::Velocity {

/**
 * @brief 兼容别名
 *
 * 保留旧 include 路径，实际实现已迁移到 domain-services。
 */
using SevenSegmentSCurveAdapter = Siligen::Domain::Motion::DomainServices::SevenSegmentSCurveProfile;

}  // namespace Siligen::Infrastructure::Adapters::Motion::Velocity
