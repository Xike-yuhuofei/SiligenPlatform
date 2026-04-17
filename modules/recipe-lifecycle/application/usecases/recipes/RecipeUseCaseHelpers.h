#pragma once

#include "recipe_lifecycle/domain/recipes/value-objects/RecipePlanningPolicyDefaults.h"
#include "recipe_lifecycle/domain/recipes/value-objects/RecipeTypes.h"

#include <chrono>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Siligen::Application::UseCases::Recipes {

inline std::int64_t NowEpochMillis() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

inline std::string GenerateId(const std::string& prefix) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::ostringstream oss;
    oss << prefix << "-";
    for (int i = 0; i < 8; ++i) {
        oss << std::hex << dis(gen);
    }
    oss << "-";
    for (int i = 0; i < 4; ++i) {
        oss << std::hex << dis(gen);
    }
    return oss.str();
}

inline std::string ParameterValueToString(const Siligen::Domain::Recipes::ValueObjects::ParameterValue& value) {
    return std::visit(
        [](auto&& arg) -> std::string {
            std::ostringstream oss;
            oss << std::boolalpha << arg;
            return oss.str();
        },
        value);
}

inline std::vector<Siligen::Domain::Recipes::ValueObjects::FieldChange> DiffParameters(
    const std::vector<Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry>& before,
    const std::vector<Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry>& after) {
    using Siligen::Domain::Recipes::ValueObjects::FieldChange;

    std::unordered_map<std::string, Siligen::Domain::Recipes::ValueObjects::ParameterValue> before_map;
    std::unordered_map<std::string, Siligen::Domain::Recipes::ValueObjects::ParameterValue> after_map;
    for (const auto& entry : before) {
        before_map[entry.key] = entry.value;
    }
    for (const auto& entry : after) {
        after_map[entry.key] = entry.value;
    }

    std::vector<FieldChange> changes;
    for (const auto& pair : before_map) {
        const auto& key = pair.first;
        auto it = after_map.find(key);
        if (it == after_map.end()) {
            FieldChange change;
            change.field = "parameters." + key;
            change.before = ParameterValueToString(pair.second);
            change.after = "<removed>";
            changes.push_back(change);
        } else if (ParameterValueToString(pair.second) != ParameterValueToString(it->second)) {
            FieldChange change;
            change.field = "parameters." + key;
            change.before = ParameterValueToString(pair.second);
            change.after = ParameterValueToString(it->second);
            changes.push_back(change);
        }
    }
    for (const auto& pair : after_map) {
        if (before_map.find(pair.first) == before_map.end()) {
            FieldChange change;
            change.field = "parameters." + pair.first;
            change.before = "<added>";
            change.after = ParameterValueToString(pair.second);
            changes.push_back(change);
        }
    }
    return changes;
}

inline std::vector<Siligen::Domain::Recipes::ValueObjects::FieldChange> DiffStrings(
    const std::string& field,
    const std::string& before,
    const std::string& after) {
    if (before == after) {
        return {};
    }
    Siligen::Domain::Recipes::ValueObjects::FieldChange change;
    change.field = field;
    change.before = before;
    change.after = after;
    return {change};
}

inline std::vector<Siligen::Domain::Recipes::ValueObjects::FieldChange> DiffTags(
    const std::vector<std::string>& before,
    const std::vector<std::string>& after) {
    auto join = [](const std::vector<std::string>& values) {
        std::ostringstream oss;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) {
                oss << ",";
            }
            oss << values[i];
        }
        return oss.str();
    };
    return DiffStrings("tags", join(before), join(after));
}

inline void NormalizeRecipePlanningPolicyParameters(
    std::vector<Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry>& parameters) {
    Siligen::Domain::Recipes::ValueObjects::EnsureRecipePlanningPolicyDefaults(parameters);
}

}  // namespace Siligen::Application::UseCases::Recipes
