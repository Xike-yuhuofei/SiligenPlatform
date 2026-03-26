#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>

namespace Siligen::Domain::System::Redundancy {

enum class EntityType : std::uint8_t {
    Function,
    Method,
    Class,
    File,
    CMakeTarget
};

enum class SourceLanguage : std::uint8_t {
    Cpp,
    Header,
    CMake
};

enum class StaticSignalType : std::uint8_t {
    Unreferenced,
    Unreachable,
    Duplicate,
    Isolated
};

enum class RuntimeEnvironment : std::uint8_t {
    Test,
    Staging,
    Canary
};

enum class CandidatePriority : std::uint8_t {
    P0,
    P1,
    P2,
    P3
};

enum class CandidateAction : std::uint8_t {
    Observe,
    Deprecate,
    Remove,
    Hold
};

enum class CandidateStatus : std::uint8_t {
    New,
    Reviewed,
    Deprecated,
    Observed,
    Removed,
    RolledBack
};

enum class DecisionAction : std::uint8_t {
    Approve,
    Reject,
    Deprecate,
    Remove,
    Rollback
};

inline std::string ToUpper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

inline std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

inline const char* ToString(EntityType value) {
    switch (value) {
        case EntityType::Function:
            return "function";
        case EntityType::Method:
            return "method";
        case EntityType::Class:
            return "class";
        case EntityType::File:
            return "file";
        case EntityType::CMakeTarget:
            return "cmake_target";
        default:
            return "function";
    }
}

inline std::optional<EntityType> EntityTypeFromString(const std::string& value) {
    const auto lowered = ToLower(value);
    if (lowered == "function") {
        return EntityType::Function;
    }
    if (lowered == "method") {
        return EntityType::Method;
    }
    if (lowered == "class") {
        return EntityType::Class;
    }
    if (lowered == "file") {
        return EntityType::File;
    }
    if (lowered == "cmake_target") {
        return EntityType::CMakeTarget;
    }
    return std::nullopt;
}

inline const char* ToString(SourceLanguage value) {
    switch (value) {
        case SourceLanguage::Cpp:
            return "cpp";
        case SourceLanguage::Header:
            return "header";
        case SourceLanguage::CMake:
            return "cmake";
        default:
            return "cpp";
    }
}

inline std::optional<SourceLanguage> SourceLanguageFromString(const std::string& value) {
    const auto lowered = ToLower(value);
    if (lowered == "cpp") {
        return SourceLanguage::Cpp;
    }
    if (lowered == "header") {
        return SourceLanguage::Header;
    }
    if (lowered == "cmake") {
        return SourceLanguage::CMake;
    }
    return std::nullopt;
}

inline const char* ToString(StaticSignalType value) {
    switch (value) {
        case StaticSignalType::Unreferenced:
            return "unreferenced";
        case StaticSignalType::Unreachable:
            return "unreachable";
        case StaticSignalType::Duplicate:
            return "duplicate";
        case StaticSignalType::Isolated:
            return "isolated";
        default:
            return "unreferenced";
    }
}

inline std::optional<StaticSignalType> StaticSignalTypeFromString(const std::string& value) {
    const auto lowered = ToLower(value);
    if (lowered == "unreferenced") {
        return StaticSignalType::Unreferenced;
    }
    if (lowered == "unreachable") {
        return StaticSignalType::Unreachable;
    }
    if (lowered == "duplicate") {
        return StaticSignalType::Duplicate;
    }
    if (lowered == "isolated") {
        return StaticSignalType::Isolated;
    }
    return std::nullopt;
}

inline const char* ToString(RuntimeEnvironment value) {
    switch (value) {
        case RuntimeEnvironment::Test:
            return "test";
        case RuntimeEnvironment::Staging:
            return "staging";
        case RuntimeEnvironment::Canary:
            return "canary";
        default:
            return "test";
    }
}

inline std::optional<RuntimeEnvironment> RuntimeEnvironmentFromString(const std::string& value) {
    const auto lowered = ToLower(value);
    if (lowered == "test") {
        return RuntimeEnvironment::Test;
    }
    if (lowered == "staging") {
        return RuntimeEnvironment::Staging;
    }
    if (lowered == "canary") {
        return RuntimeEnvironment::Canary;
    }
    return std::nullopt;
}

inline const char* ToString(CandidatePriority value) {
    switch (value) {
        case CandidatePriority::P0:
            return "P0";
        case CandidatePriority::P1:
            return "P1";
        case CandidatePriority::P2:
            return "P2";
        case CandidatePriority::P3:
            return "P3";
        default:
            return "P3";
    }
}

inline std::optional<CandidatePriority> CandidatePriorityFromString(const std::string& value) {
    const auto uppered = ToUpper(value);
    if (uppered == "P0") {
        return CandidatePriority::P0;
    }
    if (uppered == "P1") {
        return CandidatePriority::P1;
    }
    if (uppered == "P2") {
        return CandidatePriority::P2;
    }
    if (uppered == "P3") {
        return CandidatePriority::P3;
    }
    return std::nullopt;
}

inline const char* ToString(CandidateAction value) {
    switch (value) {
        case CandidateAction::Observe:
            return "observe";
        case CandidateAction::Deprecate:
            return "deprecate";
        case CandidateAction::Remove:
            return "remove";
        case CandidateAction::Hold:
            return "hold";
        default:
            return "hold";
    }
}

inline std::optional<CandidateAction> CandidateActionFromString(const std::string& value) {
    const auto lowered = ToLower(value);
    if (lowered == "observe") {
        return CandidateAction::Observe;
    }
    if (lowered == "deprecate") {
        return CandidateAction::Deprecate;
    }
    if (lowered == "remove") {
        return CandidateAction::Remove;
    }
    if (lowered == "hold") {
        return CandidateAction::Hold;
    }
    return std::nullopt;
}

inline const char* ToString(CandidateStatus value) {
    switch (value) {
        case CandidateStatus::New:
            return "NEW";
        case CandidateStatus::Reviewed:
            return "REVIEWED";
        case CandidateStatus::Deprecated:
            return "DEPRECATED";
        case CandidateStatus::Observed:
            return "OBSERVED";
        case CandidateStatus::Removed:
            return "REMOVED";
        case CandidateStatus::RolledBack:
            return "ROLLED_BACK";
        default:
            return "NEW";
    }
}

inline std::optional<CandidateStatus> CandidateStatusFromString(const std::string& value) {
    const auto uppered = ToUpper(value);
    if (uppered == "NEW") {
        return CandidateStatus::New;
    }
    if (uppered == "REVIEWED") {
        return CandidateStatus::Reviewed;
    }
    if (uppered == "DEPRECATED") {
        return CandidateStatus::Deprecated;
    }
    if (uppered == "OBSERVED") {
        return CandidateStatus::Observed;
    }
    if (uppered == "REMOVED") {
        return CandidateStatus::Removed;
    }
    if (uppered == "ROLLED_BACK") {
        return CandidateStatus::RolledBack;
    }
    return std::nullopt;
}

inline const char* ToString(DecisionAction value) {
    switch (value) {
        case DecisionAction::Approve:
            return "approve";
        case DecisionAction::Reject:
            return "reject";
        case DecisionAction::Deprecate:
            return "deprecate";
        case DecisionAction::Remove:
            return "remove";
        case DecisionAction::Rollback:
            return "rollback";
        default:
            return "approve";
    }
}

inline std::optional<DecisionAction> DecisionActionFromString(const std::string& value) {
    const auto lowered = ToLower(value);
    if (lowered == "approve") {
        return DecisionAction::Approve;
    }
    if (lowered == "reject") {
        return DecisionAction::Reject;
    }
    if (lowered == "deprecate") {
        return DecisionAction::Deprecate;
    }
    if (lowered == "remove") {
        return DecisionAction::Remove;
    }
    if (lowered == "rollback") {
        return DecisionAction::Rollback;
    }
    return std::nullopt;
}

struct CodeEntity {
    std::string entity_id;
    EntityType entity_type{EntityType::Function};
    std::string symbol;
    std::string path;
    std::string signature;
    std::string module;
    std::string owner_target;
    SourceLanguage language{SourceLanguage::Cpp};
    std::string last_modified_at;

    bool Validate() const {
        return !entity_id.empty() && !path.empty() && !module.empty();
    }
};

struct StaticEvidenceRecord {
    std::string evidence_id;
    std::string entity_id;
    std::string rule_id;
    std::string rule_version;
    StaticSignalType signal_type{StaticSignalType::Unreferenced};
    double signal_strength{0.0};
    std::string details_json;
    std::string collected_at;

    bool Validate() const {
        return !evidence_id.empty() && !entity_id.empty() && !rule_id.empty() && !rule_version.empty() &&
               signal_strength >= 0.0 && signal_strength <= 1.0;
    }
};

struct RuntimeEvidenceRecord {
    std::string evidence_id;
    std::string entity_id;
    RuntimeEnvironment environment{RuntimeEnvironment::Test};
    std::string window_start;
    std::string window_end;
    std::int64_t hit_count{0};
    std::string last_hit_at;
    std::string source;
    std::string collected_at;

    bool Validate() const {
        return !evidence_id.empty() && !entity_id.empty() && !window_start.empty() && !window_end.empty() && !source.empty() &&
               hit_count >= 0;
    }
};

struct CandidateRecord {
    std::string candidate_id;
    std::string entity_id;
    double redundancy_score{0.0};
    double deletion_risk_score{0.0};
    CandidatePriority priority{CandidatePriority::P3};
    CandidateAction recommended_action{CandidateAction::Hold};
    CandidateStatus status{CandidateStatus::New};
    std::string policy_version;
    std::string snapshot_id;
    std::string computed_at;

    bool Validate() const {
        return !candidate_id.empty() && !entity_id.empty() && redundancy_score >= 0.0 && redundancy_score <= 100.0 &&
               deletion_risk_score >= 0.0 && deletion_risk_score <= 100.0 && !computed_at.empty();
    }
};

struct DecisionLogRecord {
    std::string decision_id;
    std::string candidate_id;
    DecisionAction action{DecisionAction::Approve};
    std::string actor;
    std::string reason;
    std::string evidence_snapshot_json;
    std::string ticket;
    std::string created_at;

    bool Validate() const {
        return !decision_id.empty() && !candidate_id.empty() && !actor.empty() && !reason.empty() && !created_at.empty();
    }
};

}  // namespace Siligen::Domain::System::Redundancy
