#include "MotionPlanner.h"

namespace {
// Compatibility translation unit. Canonical implementation is in:
// modules/motion-planning/domain/motion/domain-services/MotionPlanner.cpp
[[maybe_unused]] constexpr const char* kMotionPlannerOwnerMigration =
    "M7 owner-closure: remove this shim after all consumers stop referencing legacy path.";
}  // namespace
