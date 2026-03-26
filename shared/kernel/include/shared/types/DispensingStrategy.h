#ifndef SILIGEN_SHARED_TYPES_DISPENSING_STRATEGY_H
#define SILIGEN_SHARED_TYPES_DISPENSING_STRATEGY_H

#include <boost/describe/enum.hpp>

namespace Siligen::Shared::Types {

/**
 * @brief Strategy for optimizing dispensing trigger frequency.
 */
enum class DispensingStrategy {
    /**
     * @brief Legacy mode.
     * Uses a single constant frequency for the entire segment, based on target cruise velocity.
     * Pros: Simplest, stable.
     * Cons: Severe glue piling at start/end due to acceleration ramps.
     */
    BASELINE,

    /**
     * @brief Phase 1 optimization.
     * Splits motion into 3 phases: Accel (Avg V), Cruise (Target V), Decel (Avg V).
     * Generates 3 separate trigger commands.
     * Pros: Quick win, reduces piling.
     * Cons: Still assumes constant speed within the acceleration ramp.
     */
    SEGMENTED,

    /**
     * @brief Phase 1.5 optimization.
     * Splits Accel/Decel ramps into N sub-segments (e.g., 8).
     * Generates 2N + 1 trigger commands.
     * Pros: High precision, hardware-timed stability.
     * Cons: Requires card to support rapid command switching.
     */
    SUBSEGMENTED,

    /**
     * @brief Cruise-only dispensing.
     * Only dispense during constant-velocity segments to avoid ramp piling.
     * Pros: Best spacing stability on time-based triggering.
     * Cons: Requires lead-in/out path or speed reduction on short segments.
     */
    CRUISE_ONLY
};
BOOST_DESCRIBE_ENUM(DispensingStrategy, BASELINE, SEGMENTED, SUBSEGMENTED, CRUISE_ONLY)

} // namespace Siligen::Shared::Types

#endif // SILIGEN_SHARED_TYPES_DISPENSING_STRATEGY_H
