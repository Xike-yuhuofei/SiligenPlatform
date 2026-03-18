#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Siligen::Domain::Recipes::ValueObjects {

enum class ParameterValueType {
    Integer,
    Float,
    Boolean,
    String,
    Enum
};

using ParameterValue = std::variant<int64_t, double, bool, std::string>;

struct ParameterConstraints {
    std::optional<double> min_value;
    std::optional<double> max_value;
    std::vector<std::string> allowed_values;
};

struct ParameterSchemaEntry {
    std::string key;
    std::string display_name;
    ParameterValueType value_type;
    std::string unit;
    ParameterConstraints constraints;
    bool required;

    ParameterSchemaEntry() : value_type(ParameterValueType::String), required(false) {}
};

struct ParameterSchema {
    std::string schema_id;
    std::vector<ParameterSchemaEntry> entries;
};

}  // namespace Siligen::Domain::Recipes::ValueObjects
