// EnumConverters.h - 枚举转换器
#pragma once

#include "shared/types/Result.h"
#include "shared/types/SerializationTypes.h"

#include <boost/describe/enum_from_string.hpp>
#include <boost/describe/enum_to_string.hpp>

#include <string>

// 领域枚举类型（需要转换）
#include "domain/diagnostics/value-objects/DiagnosticTypes.h"
#include "domain/dispensing/value-objects/DispensingPlan.h"
#include "domain/motion/value-objects/HardwareTestTypes.h"
#include "domain/motion/value-objects/MotionTypes.h"
#include "domain/motion/value-objects/TrajectoryTypes.h"
#include "shared/types/DispensingStrategy.h"

namespace Siligen {
namespace Infrastructure {
namespace Serialization {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::CreateEnumConversionError;

// 领域类型导入
using Siligen::Domain::Diagnostics::ValueObjects::IssueSeverity;
using Siligen::Domain::Dispensing::ValueObjects::PlanStatus;
using Siligen::Domain::Motion::ValueObjects::HomingTestDirection;
using Siligen::Domain::Motion::ValueObjects::HomingTestMode;
using Siligen::Domain::Motion::ValueObjects::JogDirection;
using Siligen::Domain::Motion::ValueObjects::TrajectoryInterpolationType;
using Siligen::Domain::Motion::ValueObjects::TrajectoryType;
using Siligen::Domain::Motion::ValueObjects::TriggerAction;
using Siligen::Domain::Motion::ValueObjects::TriggerType;
using Siligen::Shared::Types::DispensingStrategy;

// ========================================
// 通用枚举转换辅助
// ========================================

template <typename EnumType>
inline std::string EnumToString(EnumType value) {
    const char* name = boost::describe::enum_to_string(value, "Unknown");
    return std::string(name);
}

template <typename EnumType>
inline Result<EnumType> EnumFromString(const std::string& str, const char* enum_name) {
    EnumType value{};
    if (boost::describe::enum_from_string(str, value)) {
        return Result<EnumType>::Success(value);
    }
    return Result<EnumType>::Failure(CreateEnumConversionError(enum_name, str));
}

// ========================================
// JogDirection 转换器
// ========================================

/**
 * @brief JogDirection 转换为字符串
 */
inline std::string ToString(JogDirection direction) {
    return EnumToString(direction);
}

/**
 * @brief 字符串转换为 JogDirection
 */
inline Result<JogDirection> FromString_JogDirection(const std::string& str) {
    return EnumFromString<JogDirection>(str, "JogDirection");
}

// ========================================
// HomingTestMode 转换器
// ========================================

inline std::string ToString(HomingTestMode mode) {
    return EnumToString(mode);
}

inline Result<HomingTestMode> FromString_HomingTestMode(const std::string& str) {
    return EnumFromString<HomingTestMode>(str, "HomingTestMode");
}

// ========================================
// HomingTestDirection 转换器
// ========================================

inline std::string ToString(HomingTestDirection direction) {
    return EnumToString(direction);
}

inline Result<HomingTestDirection> FromString_HomingTestDirection(const std::string& str) {
    return EnumFromString<HomingTestDirection>(str, "HomingTestDirection");
}

// ========================================
// TrajectoryType 转换器
// ========================================

inline std::string ToString(TrajectoryType type) {
    return EnumToString(type);
}

inline Result<TrajectoryType> FromString_TrajectoryType(const std::string& str) {
    return EnumFromString<TrajectoryType>(str, "TrajectoryType");
}

// ========================================
// TrajectoryInterpolationType 转换器
// ========================================

inline std::string ToString(TrajectoryInterpolationType type) {
    return EnumToString(type);
}

inline Result<TrajectoryInterpolationType> FromString_TrajectoryInterpolationType(const std::string& str) {
    return EnumFromString<TrajectoryInterpolationType>(str, "TrajectoryInterpolationType");
}

// ========================================
// TriggerAction 转换器
// ========================================

inline std::string ToString(TriggerAction action) {
    return EnumToString(action);
}

inline Result<TriggerAction> FromString_TriggerAction(const std::string& str) {
    return EnumFromString<TriggerAction>(str, "TriggerAction");
}

// ========================================
// TriggerType 转换器
// ========================================

inline std::string ToString(TriggerType type) {
    return EnumToString(type);
}

inline Result<TriggerType> FromString_TriggerType(const std::string& str) {
    return EnumFromString<TriggerType>(str, "TriggerType");
}

// ========================================
// DispensingStrategy 转换器
// ========================================

inline std::string ToString(DispensingStrategy strategy) {
    switch (strategy) {
        case DispensingStrategy::BASELINE:
            return "Baseline";
        case DispensingStrategy::SEGMENTED:
            return "Segmented";
        case DispensingStrategy::SUBSEGMENTED:
            return "Subsegmented";
        case DispensingStrategy::CRUISE_ONLY:
            return "CruiseOnly";
        default:
            return "Unknown";
    }
}

inline Result<DispensingStrategy> FromString_DispensingStrategy(const std::string& str) {
    if (str == "Baseline") {
        return Result<DispensingStrategy>::Success(DispensingStrategy::BASELINE);
    }
    if (str == "Segmented") {
        return Result<DispensingStrategy>::Success(DispensingStrategy::SEGMENTED);
    }
    if (str == "Subsegmented") {
        return Result<DispensingStrategy>::Success(DispensingStrategy::SUBSEGMENTED);
    }
    if (str == "CruiseOnly") {
        return Result<DispensingStrategy>::Success(DispensingStrategy::CRUISE_ONLY);
    }
    return Result<DispensingStrategy>::Failure(CreateEnumConversionError("DispensingStrategy", str));
}

// ========================================
// PlanStatus 转换器
// ========================================

inline std::string ToString(PlanStatus status) {
    return EnumToString(status);
}

inline Result<PlanStatus> FromString_PlanStatus(const std::string& str) {
    return EnumFromString<PlanStatus>(str, "PlanStatus");
}

// ========================================
// IssueSeverity 转换器
// ========================================

/**
 * @brief IssueSeverity 转换为字符串
 */
inline std::string ToString(IssueSeverity severity) {
    return EnumToString(severity);
}

/**
 * @brief 字符串转换为 IssueSeverity
 */
inline Result<IssueSeverity> FromString_IssueSeverity(const std::string& str) {
    return EnumFromString<IssueSeverity>(str, "IssueSeverity");
}

}  // namespace Serialization
}  // namespace Infrastructure
}  // namespace Siligen
