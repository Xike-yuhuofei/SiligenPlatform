#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace Siligen {
namespace Domain {
namespace Diagnostics {
namespace Aggregates {

enum class TestType {
    Jog,
    Homing,
    IO,
    Trigger,
    CMP,
    Interpolation,
    Diagnostic
};

enum class TestStatus {
    Success,
    Failure,
    Error
};

enum class TestState {
    Idle,
    Running,
    Paused,
    Completed,
    Error
};

class TestRecord {
   public:
    std::int64_t id;
    std::int64_t timestamp;
    TestType testType;
    std::string operatorName;
    TestStatus status;
    std::int32_t durationMs;
    std::optional<std::string> errorMessage;
    std::string parametersJson;
    std::string resultJson;

    TestRecord()
        : id(0),
          timestamp(0),
          testType(TestType::Jog),
          operatorName(""),
          status(TestStatus::Success),
          durationMs(0),
          parametersJson("{}"),
          resultJson("{}") {}

    bool isValid() const {
        if (timestamp <= 0) return false;
        if (durationMs < 0) return false;
        if (operatorName.empty()) return false;
        if (status != TestStatus::Success && !errorMessage.has_value()) {
            return false;
        }
        return true;
    }

    static std::string testTypeToString(TestType type) {
        switch (type) {
            case TestType::Jog:
                return "Jog";
            case TestType::Homing:
                return "Homing";
            case TestType::IO:
                return "IO";
            case TestType::Trigger:
                return "Trigger";
            case TestType::CMP:
                return "CMP";
            case TestType::Interpolation:
                return "Interpolation";
            case TestType::Diagnostic:
                return "Diagnostic";
            default:
                return "Unknown";
        }
    }

    static TestType stringToTestType(const std::string& str) {
        if (str == "Jog") return TestType::Jog;
        if (str == "Homing") return TestType::Homing;
        if (str == "IO") return TestType::IO;
        if (str == "Trigger") return TestType::Trigger;
        if (str == "CMP") return TestType::CMP;
        if (str == "Interpolation") return TestType::Interpolation;
        if (str == "Diagnostic") return TestType::Diagnostic;
        return TestType::Jog;
    }
};

}  // namespace Aggregates
}  // namespace Diagnostics
}  // namespace Domain
}  // namespace Siligen
