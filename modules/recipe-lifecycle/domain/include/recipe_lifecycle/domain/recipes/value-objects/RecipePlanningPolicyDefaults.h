#pragma once

#include "recipe_lifecycle/domain/recipes/value-objects/RecipeTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace Siligen::Domain::Recipes::ValueObjects {

inline constexpr char kTriggerSpatialIntervalMmParameterKey[] = "trigger_spatial_interval_mm";
inline constexpr char kPointFlyingDirectionModeParameterKey[] = "point_flying_direction_mode";
inline constexpr double kDefaultTriggerSpatialIntervalMm = 3.0;
inline constexpr char kDefaultPointFlyingDirectionMode[] = "approach_direction";

inline const ParameterValueEntry* FindParameterValueEntry(
    const std::vector<ParameterValueEntry>& parameters,
    std::string_view key) noexcept {
    for (const auto& entry : parameters) {
        if (entry.key == key) {
            return &entry;
        }
    }
    return nullptr;
}

inline ParameterValueEntry* FindParameterValueEntry(
    std::vector<ParameterValueEntry>& parameters,
    std::string_view key) noexcept {
    for (auto& entry : parameters) {
        if (entry.key == key) {
            return &entry;
        }
    }
    return nullptr;
}

inline bool EnsureRecipePlanningPolicyDefaults(std::vector<ParameterValueEntry>& parameters) {
    bool changed = false;

    if (FindParameterValueEntry(parameters, kTriggerSpatialIntervalMmParameterKey) == nullptr) {
        ParameterValueEntry entry;
        entry.key = kTriggerSpatialIntervalMmParameterKey;
        entry.value = kDefaultTriggerSpatialIntervalMm;
        parameters.push_back(std::move(entry));
        changed = true;
    }

    if (FindParameterValueEntry(parameters, kPointFlyingDirectionModeParameterKey) == nullptr) {
        ParameterValueEntry entry;
        entry.key = kPointFlyingDirectionModeParameterKey;
        entry.value = std::string(kDefaultPointFlyingDirectionMode);
        parameters.push_back(std::move(entry));
        changed = true;
    }

    return changed;
}

}  // namespace Siligen::Domain::Recipes::ValueObjects
