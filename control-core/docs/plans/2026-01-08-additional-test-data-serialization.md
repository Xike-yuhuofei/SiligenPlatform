# Issue #17 Phase 2: 其他测试数据类型序列化实施计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**目标:** 为 JogTestData、TriggerTestData、CMPTestData 和 DiagnosticResult 实现完整的 JSON 序列化/反序列化功能，扩展 Issue #17 Phase 1 的序列化基础设施。

**架构:** 延续 Phase 1 的三层隔离架构：
- Domain 层定义数据类型（已存在）
- Infrastructure 层实现具体序列化器
- 复用 nlohmann/json v3.11.3 和 Phase 1 的工具函数（JSONUtils, EnumConverters）

**技术栈:**
- nlohmann/json v3.11.3（已集成）
- Result<T> 错误处理模式
- GoogleTest 单元测试
- CMake 静态库

---

## 前置条件检查

### Task 0: 验证 Phase 1 基础设施

**文件:**
- Verify: `src/infrastructure/serialization/EnumConverters.h`
- Verify: `src/infrastructure/serialization/JSONUtils.h`
- Verify: `src/domain/<subdomain>/ports/IDataSerializerPort.h`

**Step 1: 检查现有枚举转换器覆盖范围**

Run: `grep -E "ToString|FromString" src/infrastructure/serialization/EnumConverters.h | grep -E "TriggerAction|IssueSeverity"`

Expected Output:
- `TriggerAction` 和 `IssueSeverity` 枚举转换器**已存在** ✅

**Step 2: 检查数据类型定义**

Run: `grep -E "class (JogTestData|TriggerTestData|CMPTestData|DiagnosticResult)" src/domain/models/TestDataTypes.h`

Expected Output: 四个类定义都**已存在** ✅

**Step 3: 确认嵌套结构类型**

需要为以下嵌套结构添加序列化支持：
- `TriggerPoint` (HardwareTypes.h)
- `TriggerEvent` (HardwareTypes.h)
- `TriggerStatistics` (DiagnosticTypes.h)
- `CMPParameters` (TrajectoryTypes.h)
- `TrajectoryAnalysis` (TrajectoryTypes.h)
- `HardwareCheckResult` (DiagnosticTypes.h)
- `CommunicationCheckResult` (DiagnosticTypes.h)
- `ResponseTimeCheckResult` (DiagnosticTypes.h)
- `AccuracyCheckResult` (DiagnosticTypes.h)
- `DiagnosticIssue` (DiagnosticTypes.h)

---

## Task 1: 扩展枚举转换器

**文件:**
- Modify: `src/infrastructure/serialization/EnumConverters.h:200-250`
- Test: `tests/unit/infrastructure/serialization/test_EnumConverters.cpp`

**Step 1: 添加 TriggerAction 转换器**

在 `EnumConverters.h` 末尾（约 200 行后）添加：

```cpp
// ========================================
// TriggerAction 转换器
// ========================================

/**
 * @brief TriggerAction 转换为字符串
 */
inline std::string ToString(TriggerAction action) {
    switch (action) {
        case TriggerAction::TurnOn:
            return "TurnOn";
        case TriggerAction::TurnOff:
            return "TurnOff";
        default:
            return "Unknown";
    }
}

/**
 * @brief 字符串转换为 TriggerAction
 */
inline Result<TriggerAction> FromString_TriggerAction(const std::string& str) {
    static const std::unordered_map<std::string, TriggerAction> map = {
        {"TurnOn", TriggerAction::TurnOn},
        {"TurnOff", TriggerAction::TurnOff}
    };

    auto it = map.find(str);
    if (it != map.end()) {
        return Result<TriggerAction>::Success(it->second);
    }

    return Result<TriggerAction>::Failure(CreateEnumConversionError("TriggerAction", str));
}
```

**Step 2: 添加 IssueSeverity 转换器**

在 `EnumConverters.h` 末尾添加：

```cpp
// ========================================
// IssueSeverity 转换器
// ========================================

/**
 * @brief IssueSeverity 转换为字符串
 */
inline std::string ToString(IssueSeverity severity) {
    switch (severity) {
        case IssueSeverity::Info:
            return "Info";
        case IssueSeverity::Warning:
            return "Warning";
        case IssueSeverity::Error:
            return "Error";
        case IssueSeverity::Critical:
            return "Critical";
        default:
            return "Unknown";
    }
}

/**
 * @brief 字符串转换为 IssueSeverity
 */
inline Result<IssueSeverity> FromString_IssueSeverity(const std::string& str) {
    static const std::unordered_map<std::string, IssueSeverity> map = {
        {"Info", IssueSeverity::Info},
        {"Warning", IssueSeverity::Warning},
        {"Error", IssueSeverity::Error},
        {"Critical", IssueSeverity::Critical}
    };

    auto it = map.find(str);
    if (it != map.end()) {
        return Result<IssueSeverity>::Success(it->second);
    }

    return Result<IssueSeverity>::Failure(CreateEnumConversionError("IssueSeverity", str));
}
```

**Step 3: 更新 include 指令**

在 `EnumConverters.h` 顶部添加：

```cpp
#include "../../../domain/models/DiagnosticTypes.h"

// 导入领域枚举类型（在现有列表后添加）
using Siligen::IssueSeverity;
```

**Step 4: 编写单元测试**

在 `test_EnumConverters.cpp` 末尾添加：

```cpp
// ========================================
// TriggerAction 转换测试
// ========================================

TEST(EnumConvertersTest, TriggerAction_ToString_TurnOn) {
    EXPECT_EQ(ToString(TriggerAction::TurnOn), "TurnOn");
}

TEST(EnumConvertersTest, TriggerAction_ToString_TurnOff) {
    EXPECT_EQ(ToString(TriggerAction::TurnOff), "TurnOff");
}

TEST(EnumConvertersTest, TriggerAction_FromString_TurnOn) {
    auto result = FromString_TriggerAction("TurnOn");
    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value(), TriggerAction::TurnOn);
}

TEST(EnumConvertersTest, TriggerAction_FromString_TurnOff) {
    auto result = FromString_TriggerAction("TurnOff");
    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value(), TriggerAction::TurnOff);
}

TEST(EnumConvertersTest, TriggerAction_FromString_Invalid) {
    auto result = FromString_TriggerAction("Invalid");
    ASSERT_TRUE(result.IsError());
}

TEST(EnumConvertersTest, TriggerAction_RoundTrip) {
    auto result = FromString_TriggerAction(ToString(TriggerAction::TurnOn));
    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value(), TriggerAction::TurnOn);
}

// ========================================
// IssueSeverity 转换测试
// ========================================

TEST(EnumConvertersTest, IssueSeverity_ToString_Info) {
    EXPECT_EQ(ToString(IssueSeverity::Info), "Info");
}

TEST(EnumConvertersTest, IssueSeverity_ToString_Critical) {
    EXPECT_EQ(ToString(IssueSeverity::Critical), "Critical");
}

TEST(EnumConvertersTest, IssueSeverity_FromString_Error) {
    auto result = FromString_IssueSeverity("Error");
    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value(), IssueSeverity::Error);
}

TEST(EnumConvertersTest, IssueSeverity_FromString_Warning) {
    auto result = FromString_IssueSeverity("Warning");
    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value(), IssueSeverity::Warning);
}

TEST(EnumConvertersTest, IssueSeverity_RoundTrip) {
    auto result = FromString_IssueSeverity(ToString(IssueSeverity::Critical));
    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value(), IssueSeverity::Critical);
}
```

**Step 5: 运行测试验证**

Run:
```bash
cd build
ctest -R test_EnumConverters -V
```

Expected: 所有测试通过 ✅

**Step 6: 提交变更**

```bash
git add src/infrastructure/serialization/EnumConverters.h
git add tests/unit/infrastructure/serialization/test_EnumConverters.cpp
git commit -m "feat(serialization): add TriggerAction and IssueSeverity enum converters"
```

---

## Task 2: 扩展 JSONUtils 支持嵌套结构

**文件:**
- Modify: `src/infrastructure/serialization/JSONUtils.h:250-400`

**Step 1: 添加 TriggerPoint 序列化支持**

在 `JSONUtils.h` 的 `namespace Siligen::Shared::Types` 块末尾添加：

```cpp
/**
 * @brief TriggerPoint 转 JSON
 */
inline void to_json(nlohmann::json& j, const TriggerPoint& p) {
    j = nlohmann::json{
        {"axis", p.axis},
        {"position", p.position},
        {"outputPort", p.outputPort},
        {"action", ToString(p.action)}
    };
}

/**
 * @brief JSON 转 TriggerPoint
 */
inline void from_json(const nlohmann::json& j, TriggerPoint& p) {
    p.axis = j.at("axis").get<int>();
    p.position = j.at("position").get<double>();
    p.outputPort = j.at("outputPort").get<int>();

    auto actionResult = FromString_TriggerAction(j.at("action").get<std::string>());
    if (actionResult.IsSuccess()) {
        p.action = actionResult.Value();
    } else {
        // 设置默认值
        p.action = TriggerAction::TurnOn;
    }
}
```

**Step 2: 添加 TriggerEvent 序列化支持**

```cpp
/**
 * @brief TriggerEvent 转 JSON
 */
inline void to_json(nlohmann::json& j, const TriggerEvent& e) {
    j = nlohmann::json{
        {"timestamp", e.timestamp},
        {"triggerPointId", e.triggerPointId},
        {"actualPosition", e.actualPosition},
        {"positionError", e.positionError},
        {"responseTimeUs", e.responseTimeUs},
        {"outputVerified", e.outputVerified}
    };
}

/**
 * @brief JSON 转 TriggerEvent
 */
inline void from_json(const nlohmann::json& j, TriggerEvent& e) {
    e.timestamp = j.at("timestamp").get<std::int64_t>();
    e.triggerPointId = j.at("triggerPointId").get<int>();
    e.actualPosition = j.at("actualPosition").get<double>();
    e.positionError = j.at("positionError").get<double>();
    e.responseTimeUs = j.at("responseTimeUs").get<std::int32_t>();
    e.outputVerified = j.at("outputVerified").get<bool>();
}
```

**Step 3: 添加 DiagnosticIssue 序列化支持**

```cpp
/**
 * @brief DiagnosticIssue 转 JSON
 */
inline void to_json(nlohmann::json& j, const DiagnosticIssue& i) {
    j = nlohmann::json{
        {"severity", ToString(i.severity)},
        {"description", i.description},
        {"suggestedAction", i.suggestedAction}
    };
}

/**
 * @brief JSON 转 DiagnosticIssue
 */
inline void from_json(const nlohmann::json& j, DiagnosticIssue& i) {
    auto severityResult = FromString_IssueSeverity(j.at("severity").get<std::string>());
    if (severityResult.IsSuccess()) {
        i.severity = severityResult.Value();
    } else {
        i.severity = IssueSeverity::Info;
    }

    i.description = j.at("description").get<std::string>();
    i.suggestedAction = j.at("suggestedAction").get<std::string>();
}
```

**Step 4: 添加必要的 using 声明**

在 `JSONUtils.h` 的 using 声明区域添加：

```cpp
using Siligen::TriggerPoint;
using Siligen::TriggerEvent;
using Siligen::DiagnosticIssue;
using Siligen::Infrastructure::Serialization::ToString;
using Siligen::Infrastructure::Serialization::FromString_TriggerAction;
using Siligen::Infrastructure::Serialization::FromString_IssueSeverity;
```

**Step 5: 编写单元测试**

创建文件 `tests/unit/infrastructure/serialization/test_JSONUtilsNestedTypes.cpp`:

```cpp
// test_JSONUtilsNestedTypes.cpp - 嵌套类型 JSON 序列化测试
#pragma once

#include <gtest/gtest.h>
#include "infrastructure/serialization/JSONUtils.h"
#include "domain/models/HardwareTypes.h"
#include "domain/models/DiagnosticTypes.h"
#include "shared/types/SerializationTypes.h"

using namespace Siligen;
using namespace Siligen::Infrastructure::Serialization;
using namespace Siligen::Shared::Types;

// ========================================
// TriggerPoint 序列化测试
// ========================================

TEST(JSONUtilsNestedTypesTest, TriggerPoint_ToJson) {
    TriggerPoint p(0, 100.5, 3, TriggerAction::TurnOn);
    json j = p;

    EXPECT_EQ(j["axis"], 0);
    EXPECT_DOUBLE_EQ(j["position"], 100.5);
    EXPECT_EQ(j["outputPort"], 3);
    EXPECT_EQ(j["action"], "TurnOn");
}

TEST(JSONUtilsNestedTypesTest, TriggerPoint_FromJson) {
    json j;
    j["axis"] = 1;
    j["position"] = 200.5;
    j["outputPort"] = 4;
    j["action"] = "TurnOff";

    TriggerPoint p = j.get<TriggerPoint>();
    EXPECT_EQ(p.axis, 1);
    EXPECT_DOUBLE_EQ(p.position, 200.5);
    EXPECT_EQ(p.outputPort, 4);
    EXPECT_EQ(p.action, TriggerAction::TurnOff);
}

TEST(JSONUtilsNestedTypesTest, TriggerPoint_RoundTrip) {
    TriggerPoint original(2, 300.75, 5, TriggerAction::TurnOn);

    json j = original;
    TriggerPoint restored = j.get<TriggerPoint>();

    EXPECT_EQ(restored.axis, original.axis);
    EXPECT_DOUBLE_EQ(restored.position, original.position);
    EXPECT_EQ(restored.outputPort, original.outputPort);
    EXPECT_EQ(restored.action, original.action);
}

// ========================================
// TriggerEvent 序列化测试
// ========================================

TEST(JSONUtilsNestedTypesTest, TriggerEvent_ToJson) {
    TriggerEvent e;
    e.timestamp = 1234567890;
    e.triggerPointId = 5;
    e.actualPosition = 100.25;
    e.positionError = 0.05;
    e.responseTimeUs = 500;
    e.outputVerified = true;

    json j = e;
    EXPECT_EQ(j["timestamp"], 1234567890);
    EXPECT_EQ(j["triggerPointId"], 5);
    EXPECT_DOUBLE_EQ(j["actualPosition"], 100.25);
    EXPECT_DOUBLE_EQ(j["positionError"], 0.05);
    EXPECT_EQ(j["responseTimeUs"], 500);
    EXPECT_TRUE(j["outputVerified"]);
}

TEST(JSONUtilsNestedTypesTest, TriggerEvent_RoundTrip) {
    TriggerEvent original;
    original.timestamp = 9876543210;
    original.triggerPointId = 10;
    original.actualPosition = 200.5;
    original.positionError = 0.1;
    original.responseTimeUs = 1000;
    original.outputVerified = false;

    json j = original;
    TriggerEvent restored = j.get<TriggerEvent>();

    EXPECT_EQ(restored.timestamp, original.timestamp);
    EXPECT_EQ(restored.triggerPointId, original.triggerPointId);
    EXPECT_DOUBLE_EQ(restored.actualPosition, original.actualPosition);
    EXPECT_DOUBLE_EQ(restored.positionError, original.positionError);
    EXPECT_EQ(restored.responseTimeUs, original.responseTimeUs);
    EXPECT_EQ(restored.outputVerified, original.outputVerified);
}

// ========================================
// DiagnosticIssue 序列化测试
// ========================================

TEST(JSONUtilsNestedTypesTest, DiagnosticIssue_ToJson) {
    DiagnosticIssue issue(IssueSeverity::Error, "Test error", "Fix it");

    json j = issue;
    EXPECT_EQ(j["severity"], "Error");
    EXPECT_EQ(j["description"], "Test error");
    EXPECT_EQ(j["suggestedAction"], "Fix it");
}

TEST(JSONUtilsNestedTypesTest, DiagnosticIssue_RoundTrip) {
    DiagnosticIssue original(IssueSeverity::Critical, "Critical failure", "Replace part");

    json j = original;
    DiagnosticIssue restored = j.get<DiagnosticIssue>();

    EXPECT_EQ(restored.severity, original.severity);
    EXPECT_EQ(restored.description, original.description);
    EXPECT_EQ(restored.suggestedAction, original.suggestedAction);
}
```

**Step 6: 更新 CMakeLists.txt**

在 `src/infrastructure/CMakeLists.txt` 的测试目标中添加：

```cmake
add_executable(test_JSONUtilsNestedTypes
    tests/unit/infrastructure/serialization/test_JSONUtilsNestedTypes.cpp
)
target_link_libraries(test_JSONUtilsNestedTypes
    PRIVATE siligen_serialization
    gtest_main
)
```

**Step 7: 编译并运行测试**

Run:
```bash
cd build
cmake --build . --target test_JSONUtilsNestedTypes
./tests/unit/infrastructure/serialization/test_JSONUtilsNestedTypes
```

Expected: 所有 10 个测试通过 ✅

**Step 8: 提交变更**

```bash
git add src/infrastructure/serialization/JSONUtils.h
git add tests/unit/infrastructure/serialization/test_JSONUtilsNestedTypes.cpp
git add src/infrastructure/CMakeLists.txt
git commit -m "feat(serialization): add JSON support for nested types (TriggerPoint, TriggerEvent, DiagnosticIssue)"
```

---

## Task 3: 实现 JogTestData 序列化器

**文件:**
- Create: `src/infrastructure/serialization/JogTestDataSerializer.h`
- Create: `src/infrastructure/serialization/JogTestDataSerializer.cpp`
- Test: `tests/unit/infrastructure/serialization/test_JogTestDataSerializer.cpp`

**Step 1: 创建 JogTestDataSerializer.h**

创建文件 `src/infrastructure/serialization/JogTestDataSerializer.h`:

```cpp
// JogTestDataSerializer.h - JogTestData JSON 序列化器
// Task: Issue #17 - Phase 2 其他测试数据类型序列化
#pragma once

#include "../../../domain/<subdomain>/ports/IDataSerializerPort.h"
#include "../../../domain/models/TestDataTypes.h"

namespace Siligen {
namespace Infrastructure {
namespace Serialization {

/**
 * @brief JogTestData 序列化器
 *
 * 实现 IDataSerializerPort 接口，提供 JogTestData 的 JSON 序列化/反序列化功能
 */
class JogTestDataSerializer {
   public:
    JogTestDataSerializer() = default;
    ~JogTestDataSerializer() = default;

    // 禁止拷贝
    JogTestDataSerializer(const JogTestDataSerializer&) = delete;
    JogTestDataSerializer& operator=(const JogTestDataSerializer&) = delete;

    // ========================================
    // 单对象序列化
    // ========================================

    /**
     * @brief 序列化 JogTestData
     * @param data JogTestData 对象
     * @return Result<std::string> JSON 字符串
     */
    Result<std::string> SerializeJogTestData(const JogTestData& data);

    /**
     * @brief 反序列化 JogTestData
     * @param jsonStr JSON 字符串
     * @return Result<JogTestData> JogTestData 对象
     */
    Result<JogTestData> DeserializeJogTestData(const std::string& jsonStr);

    // ========================================
    // 批量序列化
    // ========================================

    /**
     * @brief 批量序列化 JogTestData
     * @param dataList JogTestData 对象列表
     * @return Result<std::string> JSON 数组字符串
     */
    Result<std::string> SerializeJogTestDataBatch(const std::vector<JogTestData>& dataList);

    /**
     * @brief 批量反序列化 JogTestData
     * @param jsonArrStr JSON 数组字符串
     * @return Result<std::vector<JogTestData>> JogTestData 对象列表
     */
    Result<std::vector<JogTestData>> DeserializeJogTestDataBatch(const std::string& jsonArrStr);
};

}  // namespace Serialization
}  // namespace Infrastructure
}  // namespace Siligen
```

**Step 2: 实现 JogTestDataSerializer.cpp**

创建文件 `src/infrastructure/serialization/JogTestDataSerializer.cpp`:

```cpp
// JogTestDataSerializer.cpp - JogTestData 序列化器实现
#pragma once

#include "JogTestDataSerializer.h"
#include "JSONUtils.h"
#include "EnumConverters.h"

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Error;

namespace Siligen {
namespace Infrastructure {
namespace Serialization {

Result<std::string> JogTestDataSerializer::SerializeJogTestData(const JogTestData& data) {
    try {
        json j;

        // 基本信息
        j["recordId"] = data.recordId;

        // 测试参数
        j["axis"] = data.axis;
        j["direction"] = ToString(data.direction);
        j["speed"] = data.speed;
        j["distance"] = data.distance;

        // 测试结果
        j["startPosition"] = data.startPosition;
        j["endPosition"] = data.endPosition;
        j["actualDistance"] = data.actualDistance;
        j["responseTimeMs"] = data.responseTimeMs;

        return Dump(j);

    } catch (const std::exception& e) {
        return Result<std::string>::Failure(
            CreateSerializationError(ErrorCode::SERIALIZATION_FAILED,
                                     std::string("JogTestData 序列化失败: ") + e.what())
        );
    }
}

Result<JogTestData> JogTestDataSerializer::DeserializeJogTestData(const std::string& jsonStr) {
    auto parseResult = Parse(jsonStr);
    if (parseResult.IsError()) {
        return Result<JogTestData>::Failure(parseResult.GetError());
    }
    const json& j = parseResult.Value();

    try {
        JogTestData data;

        // 基本信息
        auto recordIdResult = GetIntField(j, "recordId");
        data.recordId = recordIdResult.IsSuccess() ? recordIdResult.Value() : 0;

        // 测试参数
        auto axisResult = GetIntField(j, "axis");
        if (axisResult.IsError()) {
            return Result<JogTestData>::Failure(axisResult.GetError());
        }
        data.axis = axisResult.Value();

        auto directionResult = GetStringField(j, "direction");
        if (directionResult.IsError()) {
            return Result<JogTestData>::Failure(directionResult.GetError());
        }
        auto directionConvertResult = FromString(directionResult.Value());
        if (directionConvertResult.IsError()) {
            return Result<JogTestData>::Failure(directionConvertResult.GetError());
        }
        data.direction = directionConvertResult.Value();

        auto speedResult = GetDoubleField(j, "speed");
        if (speedResult.IsError()) {
            return Result<JogTestData>::Failure(speedResult.GetError());
        }
        data.speed = speedResult.Value();

        auto distanceResult = GetDoubleField(j, "distance");
        if (distanceResult.IsError()) {
            return Result<JogTestData>::Failure(distanceResult.GetError());
        }
        data.distance = distanceResult.Value();

        // 测试结果
        auto startPosResult = GetDoubleField(j, "startPosition");
        data.startPosition = startPosResult.IsSuccess() ? startPosResult.Value() : 0.0;

        auto endPosResult = GetDoubleField(j, "endPosition");
        data.endPosition = endPosResult.IsSuccess() ? endPosResult.Value() : 0.0;

        auto actualDistResult = GetDoubleField(j, "actualDistance");
        data.actualDistance = actualDistResult.IsSuccess() ? actualDistResult.Value() : 0.0;

        auto responseTimeResult = GetIntField(j, "responseTimeMs");
        data.responseTimeMs = responseTimeResult.IsSuccess() ? responseTimeResult.Value() : 0;

        return Result<JogTestData>::Success(data);

    } catch (const std::exception& e) {
        return Result<JogTestData>::Failure(
            CreateSerializationError(ErrorCode::DESERIALIZATION_FAILED,
                                     std::string("JogTestData 反序列化失败: ") + e.what())
        );
    }
}

Result<std::string> JogTestDataSerializer::SerializeJogTestDataBatch(const std::vector<JogTestData>& dataList) {
    try {
        json j = json::array();

        for (const auto& data : dataList) {
            auto result = SerializeJogTestData(data);
            if (result.IsError()) {
                return Result<std::string>::Failure(result.GetError());
            }

            auto parseResult = Parse(result.Value());
            if (parseResult.IsError()) {
                return Result<std::string>::Failure(parseResult.GetError());
            }
            j.push_back(parseResult.Value());
        }

        return Dump(j);

    } catch (const std::exception& e) {
        return Result<std::string>::Failure(
            CreateSerializationError(ErrorCode::BATCH_SERIALIZATION_FAILED,
                                     std::string("批量序列化失败: ") + e.what())
        );
    }
}

Result<std::vector<JogTestData>> JogTestDataSerializer::DeserializeJogTestDataBatch(const std::string& jsonArrStr) {
    auto parseResult = Parse(jsonArrStr);
    if (parseResult.IsError()) {
        return Result<std::vector<JogTestData>>::Failure(parseResult.GetError());
    }
    const json& j = parseResult.Value();

    if (!j.is_array()) {
        return Result<std::vector<JogTestData>>::Failure(CreateJsonParseError("输入不是 JSON 数组"));
    }

    try {
        std::vector<JogTestData> dataList;
        dataList.reserve(j.size());

        for (const auto& item : j) {
            std::string itemJson = item.dump();
            auto result = DeserializeJogTestData(itemJson);
            if (result.IsError()) {
                return Result<std::vector<JogTestData>>::Failure(result.GetError());
            }
            dataList.push_back(result.Value());
        }

        return Result<std::vector<JogTestData>>::Success(dataList);

    } catch (const std::exception& e) {
        return Result<std::vector<JogTestData>>::Failure(
            CreateSerializationError(ErrorCode::BATCH_DESERIALIZATION_FAILED,
                                     std::string("批量反序列化失败: ") + e.what())
        );
    }
}

}  // namespace Serialization
}  // namespace Infrastructure
}  // namespace Siligen
```

**Step 3: 编写单元测试**

创建文件 `tests/unit/infrastructure/serialization/test_JogTestDataSerializer.cpp`:

```cpp
// test_JogTestDataSerializer.cpp - JogTestData 序列化器单元测试
#pragma once

#include <gtest/gtest.h>
#include "infrastructure/serialization/JogTestDataSerializer.h"
#include "domain/models/TestDataTypes.h"
#include "shared/types/SerializationTypes.h"

using namespace Siligen;
using namespace Siligen::Infrastructure::Serialization;
using namespace Siligen::Shared::Types;

class JogTestDataSerializerTest : public ::testing::Test {
   protected:
    JogTestDataSerializer serializer;

    JogTestData CreateStandardJogTestData() {
        JogTestData data;
        data.recordId = 12345;
        data.axis = 0;
        data.direction = JogDirection::Positive;
        data.speed = 50.0;
        data.distance = 10.0;
        data.startPosition = 0.0;
        data.endPosition = 10.05;
        data.actualDistance = 10.05;
        data.responseTimeMs = 150;
        return data;
    }
};

// ========================================
// 序列化测试
// ========================================

TEST_F(JogTestDataSerializerTest, Serialize_ValidData) {
    JogTestData data = CreateStandardJogTestData();

    auto result = serializer.SerializeJogTestData(data);

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_FALSE(result.Value().empty());

    // 验证 JSON 包含所有字段
    auto parseResult = Parse(result.Value());
    ASSERT_TRUE(parseResult.IsSuccess());
    json j = parseResult.Value();

    EXPECT_EQ(j["recordId"], 12345);
    EXPECT_EQ(j["axis"], 0);
    EXPECT_EQ(j["direction"], "Positive");
    EXPECT_DOUBLE_EQ(j["speed"], 50.0);
    EXPECT_DOUBLE_EQ(j["distance"], 10.0);
}

TEST_F(JogTestDataSerializerTest, Serialize_NegativeDirection) {
    JogTestData data = CreateStandardJogTestData();
    data.direction = JogDirection::Negative;

    auto result = serializer.SerializeJogTestData(data);

    ASSERT_TRUE(result.IsSuccess());
    auto parseResult = Parse(result.Value());
    ASSERT_TRUE(parseResult.IsSuccess());

    EXPECT_EQ(parseResult.Value()["direction"], "Negative");
}

// ========================================
// 反序列化测试
// ========================================

TEST_F(JogTestDataSerializerTest, Deserialize_ValidJson) {
    std::string jsonStr = R"({
        "recordId": 12345,
        "axis": 1,
        "direction": "Positive",
        "speed": 60.0,
        "distance": 15.0,
        "startPosition": 5.0,
        "endPosition": 20.0,
        "actualDistance": 15.0,
        "responseTimeMs": 200
    })";

    auto result = serializer.DeserializeJogTestData(jsonStr);

    ASSERT_TRUE(result.IsSuccess());
    JogTestData data = result.Value();

    EXPECT_EQ(data.recordId, 12345);
    EXPECT_EQ(data.axis, 1);
    EXPECT_EQ(data.direction, JogDirection::Positive);
    EXPECT_DOUBLE_EQ(data.speed, 60.0);
    EXPECT_DOUBLE_EQ(data.distance, 15.0);
    EXPECT_DOUBLE_EQ(data.startPosition, 5.0);
    EXPECT_DOUBLE_EQ(data.endPosition, 20.0);
    EXPECT_DOUBLE_EQ(data.actualDistance, 15.0);
    EXPECT_EQ(data.responseTimeMs, 200);
}

TEST_F(JogTestDataSerializerTest, Deserialize_MissingRequiredField) {
    std::string jsonStr = R"({
        "recordId": 12345,
        "axis": 0
    })";

    auto result = serializer.DeserializeJogTestData(jsonStr);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), static_cast<ErrorCode>(ErrorCode::JSON_MISSING_REQUIRED_FIELD));
}

// ========================================
// 往返测试 (Round-Trip)
// ========================================

TEST_F(JogTestDataSerializerTest, RoundTrip_PreserveAllFields) {
    JogTestData original = CreateStandardJogTestData();

    auto serializeResult = serializer.SerializeJogTestData(original);
    ASSERT_TRUE(serializeResult.IsSuccess());

    auto deserializeResult = serializer.DeserializeJogTestData(serializeResult.Value());
    ASSERT_TRUE(deserializeResult.IsSuccess());

    JogTestData restored = deserializeResult.Value();

    EXPECT_EQ(restored.recordId, original.recordId);
    EXPECT_EQ(restored.axis, original.axis);
    EXPECT_EQ(restored.direction, original.direction);
    EXPECT_DOUBLE_EQ(restored.speed, original.speed);
    EXPECT_DOUBLE_EQ(restored.distance, original.distance);
    EXPECT_DOUBLE_EQ(restored.startPosition, original.startPosition);
    EXPECT_DOUBLE_EQ(restored.endPosition, original.endPosition);
    EXPECT_DOUBLE_EQ(restored.actualDistance, original.actualDistance);
    EXPECT_EQ(restored.responseTimeMs, original.responseTimeMs);
}

// ========================================
// 批量操作测试
// ========================================

TEST_F(JogTestDataSerializerTest, BatchSerialize_EmptyList) {
    std::vector<JogTestData> dataList;

    auto result = serializer.SerializeJogTestDataBatch(dataList);

    ASSERT_TRUE(result.IsSuccess());

    auto parseResult = Parse(result.Value());
    ASSERT_TRUE(parseResult.IsSuccess());
    EXPECT_TRUE(parseResult.Value().is_array());
    EXPECT_EQ(parseResult.Value().size(), 0);
}

TEST_F(JogTestDataSerializerTest, BatchSerialize_MultipleItems) {
    std::vector<JogTestData> dataList;
    for (int i = 0; i < 3; ++i) {
        JogTestData data = CreateStandardJogTestData();
        data.axis = i;
        dataList.push_back(data);
    }

    auto result = serializer.SerializeJogTestDataBatch(dataList);

    ASSERT_TRUE(result.IsSuccess());

    auto parseResult = Parse(result.Value());
    ASSERT_TRUE(parseResult.IsSuccess());
    json j = parseResult.Value();

    EXPECT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 3);
}

TEST_F(JogTestDataSerializerTest, BatchDeserialize_RoundTrip) {
    std::vector<JogTestData> originalList;
    for (int i = 0; i < 5; ++i) {
        JogTestData data = CreateStandardJogTestData();
        data.axis = i;
        data.distance = 10.0 + i;
        originalList.push_back(data);
    }

    auto serializeResult = serializer.SerializeJogTestDataBatch(originalList);
    ASSERT_TRUE(serializeResult.IsSuccess());

    auto deserializeResult = serializer.DeserializeJogTestDataBatch(serializeResult.Value());
    ASSERT_TRUE(deserializeResult.IsSuccess());

    std::vector<JogTestData> restoredList = deserializeResult.Value();

    EXPECT_EQ(restoredList.size(), originalList.size());

    for (size_t i = 0; i < restoredList.size(); ++i) {
        EXPECT_EQ(restoredList[i].axis, originalList[i].axis);
        EXPECT_DOUBLE_EQ(restoredList[i].distance, originalList[i].distance);
    }
}
```

**Step 4: 更新 CMakeLists.txt**

在 `src/infrastructure/CMakeLists.txt` 中添加到 `siligen_serialization` 库：

```cmake
add_library(siligen_serialization STATIC
    serialization/EnumConverters.cpp
    serialization/HomingTestDataSerializer.cpp
    serialization/InterpolationTestDataSerializer.cpp
    serialization/JogTestDataSerializer.cpp  # 新增
    serialization/JSONUtils.cpp
)
```

添加测试目标：

```cmake
add_executable(test_JogTestDataSerializer
    tests/unit/infrastructure/serialization/test_JogTestDataSerializer.cpp
)
target_link_libraries(test_JogTestDataSerializer
    PRIVATE siligen_serialization
    gtest_main
)
```

**Step 5: 编译并运行测试**

Run:
```bash
cd build
cmake --build . --target test_JogTestDataSerializer
./tests/unit/infrastructure/serialization/test_JogTestDataSerializer
```

Expected: 所有 10 个测试通过 ✅

**Step 6: 提交变更**

```bash
git add src/infrastructure/serialization/JogTestDataSerializer.h
git add src/infrastructure/serialization/JogTestDataSerializer.cpp
git add tests/unit/infrastructure/serialization/test_JogTestDataSerializer.cpp
git add src/infrastructure/CMakeLists.txt
git commit -m "feat(serialization): implement JogTestData JSON serializer"
```

---

## Task 4: 实现 TriggerTestData 序列化器

**注意:** 由于嵌套类型已在 Task 2 中实现，本任务可直接使用。

**文件:**
- Create: `src/infrastructure/serialization/TriggerTestDataSerializer.h`
- Create: `src/infrastructure/serialization/TriggerTestDataSerializer.cpp`
- Test: `tests/unit/infrastructure/serialization/test_TriggerTestDataSerializer.cpp`

**Step 1: 创建 TriggerTestDataSerializer.h**

创建文件 `src/infrastructure/serialization/TriggerTestDataSerializer.h`:

```cpp
// TriggerTestDataSerializer.h - TriggerTestData JSON 序列化器
#pragma once

#include "../../../domain/<subdomain>/ports/IDataSerializerPort.h"
#include "../../../domain/models/TestDataTypes.h"

namespace Siligen {
namespace Infrastructure {
namespace Serialization {

class TriggerTestDataSerializer {
   public:
    TriggerTestDataSerializer() = default;
    ~TriggerTestDataSerializer() = default;

    TriggerTestDataSerializer(const TriggerTestDataSerializer&) = delete;
    TriggerTestDataSerializer& operator=(const TriggerTestDataSerializer&) = delete;

    Result<std::string> SerializeTriggerTestData(const TriggerTestData& data);
    Result<TriggerTestData> DeserializeTriggerTestData(const std::string& jsonStr);

    Result<std::string> SerializeTriggerTestDataBatch(const std::vector<TriggerTestData>& dataList);
    Result<std::vector<TriggerTestData>> DeserializeTriggerTestDataBatch(const std::string& jsonArrStr);

   private:
    // 辅助方法：TriggerStatistics 序列化
    json TriggerStatisticsToJson(const TriggerStatistics& stats);
    Result<TriggerStatistics> JsonToTriggerStatistics(const json& j);
};

}  // namespace Serialization
}  // namespace Infrastructure
}  // namespace Siligen
```

**Step 2: 实现 TriggerTestDataSerializer.cpp**

创建文件 `src/infrastructure/serialization/TriggerTestDataSerializer.cpp`:

```cpp
// TriggerTestDataSerializer.cpp - TriggerTestData 序列化器实现
#pragma once

#include "TriggerTestDataSerializer.h"
#include "JSONUtils.h"
#include "EnumConverters.h"

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Error;

namespace Siligen {
namespace Infrastructure {
namespace Serialization {

Result<std::string> TriggerTestDataSerializer::SerializeTriggerTestData(const TriggerTestData& data) {
    try {
        json j;

        j["recordId"] = data.recordId;
        j["triggerType"] = ToString(data.triggerType);
        j["triggerPoints"] = data.triggerPoints;  // TriggerPoint 已支持 JSON
        j["triggerEvents"] = data.triggerEvents;  // TriggerEvent 已支持 JSON
        j["statistics"] = TriggerStatisticsToJson(data.statistics);

        return Dump(j);

    } catch (const std::exception& e) {
        return Result<std::string>::Failure(
            CreateSerializationError(ErrorCode::SERIALIZATION_FAILED,
                                     std::string("TriggerTestData 序列化失败: ") + e.what())
        );
    }
}

Result<TriggerTestData> TriggerTestDataSerializer::DeserializeTriggerTestData(const std::string& jsonStr) {
    auto parseResult = Parse(jsonStr);
    if (parseResult.IsError()) {
        return Result<TriggerTestData>::Failure(parseResult.GetError());
    }
    const json& j = parseResult.Value();

    try {
        TriggerTestData data;

        data.recordId = j.value("recordId", 0);

        auto typeResult = GetStringField(j, "triggerType");
        if (typeResult.IsError()) {
            return Result<TriggerTestData>::Failure(typeResult.GetError());
        }
        auto typeConvertResult = FromString_TriggerType(typeResult.Value());
        if (typeConvertResult.IsError()) {
            return Result<TriggerTestData>::Failure(typeConvertResult.GetError());
        }
        data.triggerType = typeConvertResult.Value();

        auto pointsResult = GetArrayField<TriggerPoint>(j, "triggerPoints");
        if (pointsResult.IsError()) {
            return Result<TriggerTestData>::Failure(pointsResult.GetError());
        }
        data.triggerPoints = pointsResult.Value();

        auto eventsResult = GetArrayField<TriggerEvent>(j, "triggerEvents");
        if (eventsResult.IsError()) {
            return Result<TriggerTestData>::Failure(eventsResult.GetError());
        }
        data.triggerEvents = eventsResult.Value();

        if (!j.contains("statistics")) {
            return Result<TriggerTestData>::Failure(CreateMissingFieldError("statistics"));
        }
        auto statsResult = JsonToTriggerStatistics(j["statistics"]);
        if (statsResult.IsError()) {
            return Result<TriggerTestData>::Failure(statsResult.GetError());
        }
        data.statistics = statsResult.Value();

        return Result<TriggerTestData>::Success(data);

    } catch (const std::exception& e) {
        return Result<TriggerTestData>::Failure(
            CreateSerializationError(ErrorCode::DESERIALIZATION_FAILED,
                                     std::string("TriggerTestData 反序列化失败: ") + e.what())
        );
    }
}

Result<std::string> TriggerTestDataSerializer::SerializeTriggerTestDataBatch(const std::vector<TriggerTestData>& dataList) {
    try {
        json j = json::array();

        for (const auto& data : dataList) {
            auto result = SerializeTriggerTestData(data);
            if (result.IsError()) {
                return Result<std::string>::Failure(result.GetError());
            }

            auto parseResult = Parse(result.Value());
            if (parseResult.IsError()) {
                return Result<std::string>::Failure(parseResult.GetError());
            }
            j.push_back(parseResult.Value());
        }

        return Dump(j);

    } catch (const std::exception& e) {
        return Result<std::string>::Failure(
            CreateSerializationError(ErrorCode::BATCH_SERIALIZATION_FAILED,
                                     std::string("批量序列化失败: ") + e.what())
        );
    }
}

Result<std::vector<TriggerTestData>> TriggerTestDataSerializer::DeserializeTriggerTestDataBatch(const std::string& jsonArrStr) {
    auto parseResult = Parse(jsonArrStr);
    if (parseResult.IsError()) {
        return Result<std::vector<TriggerTestData>>::Failure(parseResult.GetError());
    }
    const json& j = parseResult.Value();

    if (!j.is_array()) {
        return Result<std::vector<TriggerTestData>>::Failure(CreateJsonParseError("输入不是 JSON 数组"));
    }

    try {
        std::vector<TriggerTestData> dataList;
        dataList.reserve(j.size());

        for (const auto& item : j) {
            std::string itemJson = item.dump();
            auto result = DeserializeTriggerTestData(itemJson);
            if (result.IsError()) {
                return Result<std::vector<TriggerTestData>>::Failure(result.GetError());
            }
            dataList.push_back(result.Value());
        }

        return Result<std::vector<TriggerTestData>>::Success(dataList);

    } catch (const std::exception& e) {
        return Result<std::vector<TriggerTestData>>::Failure(
            CreateSerializationError(ErrorCode::BATCH_DESERIALIZATION_FAILED,
                                     std::string("批量反序列化失败: ") + e.what())
        );
    }
}

// ========================================
// 私有辅助方法实现
// ========================================

json TriggerTestDataSerializer::TriggerStatisticsToJson(const TriggerStatistics& stats) {
    json j;
    j["maxPositionError"] = stats.maxPositionError;
    j["avgPositionError"] = stats.avgPositionError;
    j["maxResponseTimeUs"] = stats.maxResponseTimeUs;
    j["avgResponseTimeUs"] = stats.avgResponseTimeUs;
    j["successfulTriggers"] = stats.successfulTriggers;
    j["missedTriggers"] = stats.missedTriggers;
    return j;
}

Result<TriggerStatistics> TriggerTestDataSerializer::JsonToTriggerStatistics(const json& j) {
    try {
        TriggerStatistics stats;

        auto maxPosErrorResult = GetDoubleField(j, "maxPositionError");
        if (maxPosErrorResult.IsError()) {
            return Result<TriggerStatistics>::Failure(maxPosErrorResult.GetError());
        }
        stats.maxPositionError = maxPosErrorResult.Value();

        auto avgPosErrorResult = GetDoubleField(j, "avgPositionError");
        if (avgPosErrorResult.IsError()) {
            return Result<TriggerStatistics>::Failure(avgPosErrorResult.GetError());
        }
        stats.avgPositionError = avgPosErrorResult.Value();

        auto maxRespResult = GetIntField(j, "maxResponseTimeUs");
        if (maxRespResult.IsError()) {
            return Result<TriggerStatistics>::Failure(maxRespResult.GetError());
        }
        stats.maxResponseTimeUs = maxRespResult.Value();

        auto avgRespResult = GetIntField(j, "avgResponseTimeUs");
        if (avgRespResult.IsError()) {
            return Result<TriggerStatistics>::Failure(avgRespResult.GetError());
        }
        stats.avgResponseTimeUs = avgRespResult.Value();

        auto successResult = GetIntField(j, "successfulTriggers");
        if (successResult.IsError()) {
            return Result<TriggerStatistics>::Failure(successResult.GetError());
        }
        stats.successfulTriggers = successResult.Value();

        auto missedResult = GetIntField(j, "missedTriggers");
        if (missedResult.IsError()) {
            return Result<TriggerStatistics>::Failure(missedResult.GetError());
        }
        stats.missedTriggers = missedResult.Value();

        return Result<TriggerStatistics>::Success(stats);

    } catch (const std::exception& e) {
        return Result<TriggerStatistics>::Failure(
            CreateSerializationError(ErrorCode::OBJECT_CONSTRUCTION_FAILED,
                                     std::string("TriggerStatistics 构造失败: ") + e.what())
        );
    }
}

}  // namespace Serialization
}  // namespace Infrastructure
}  // namespace Siligen
```

**Step 3: 编写单元测试**

创建文件 `tests/unit/infrastructure/serialization/test_TriggerTestDataSerializer.cpp`:

```cpp
// test_TriggerTestDataSerializer.cpp - TriggerTestData 序列化器单元测试
#pragma once

#include <gtest/gtest.h>
#include "infrastructure/serialization/TriggerTestDataSerializer.h"
#include "domain/models/TestDataTypes.h"
#include "shared/types/SerializationTypes.h"

using namespace Siligen;
using namespace Siligen::Infrastructure::Serialization;
using namespace Siligen::Shared::Types;

class TriggerTestDataSerializerTest : public ::testing::Test {
   protected:
    TriggerTestDataSerializer serializer;

    TriggerTestData CreateStandardTriggerTestData() {
        TriggerTestData data;
        data.recordId = 12345;
        data.triggerType = TriggerType::SinglePoint;

        // 添加触发点
        data.triggerPoints.push_back(TriggerPoint(0, 100.0, 3, TriggerAction::TurnOn));
        data.triggerPoints.push_back(TriggerPoint(1, 200.0, 4, TriggerAction::TurnOff));

        // 添加触发事件
        TriggerEvent event;
        event.timestamp = 1234567890;
        event.triggerPointId = 0;
        event.actualPosition = 100.05;
        event.positionError = 0.05;
        event.responseTimeUs = 500;
        event.outputVerified = true;
        data.triggerEvents.push_back(event);

        // 统计数据
        data.statistics.maxPositionError = 0.1;
        data.statistics.avgPositionError = 0.05;
        data.statistics.maxResponseTimeUs = 1000;
        data.statistics.avgResponseTimeUs = 600;
        data.statistics.successfulTriggers = 9;
        data.statistics.missedTriggers = 1;

        return data;
    }
};

// ========================================
// 序列化测试
// ========================================

TEST_F(TriggerTestDataSerializerTest, Serialize_ValidData) {
    TriggerTestData data = CreateStandardTriggerTestData();

    auto result = serializer.SerializeTriggerTestData(data);

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_FALSE(result.Value().empty());

    auto parseResult = Parse(result.Value());
    ASSERT_TRUE(parseResult.IsSuccess());
    json j = parseResult.Value();

    EXPECT_EQ(j["recordId"], 12345);
    EXPECT_EQ(j["triggerType"], "SinglePoint");
    EXPECT_EQ(j["triggerPoints"].size(), 2);
    EXPECT_EQ(j["triggerEvents"].size(), 1);
}

TEST_F(TriggerTestDataSerializerTest, Serialize_MultiPointTriggerType) {
    TriggerTestData data = CreateStandardTriggerTestData();
    data.triggerType = TriggerType::MultiPoint;

    auto result = serializer.SerializeTriggerTestData(data);

    ASSERT_TRUE(result.IsSuccess());
    auto parseResult = Parse(result.Value());
    ASSERT_TRUE(parseResult.IsSuccess());

    EXPECT_EQ(parseResult.Value()["triggerType"], "MultiPoint");
}

// ========================================
// 反序列化测试
// ========================================

TEST_F(TriggerTestDataSerializerTest, Deserialize_ValidJson) {
    std::string jsonStr = R"({
        "recordId": 12345,
        "triggerType": "SinglePoint",
        "triggerPoints": [
            {"axis": 0, "position": 100.0, "outputPort": 3, "action": "TurnOn"}
        ],
        "triggerEvents": [
            {
                "timestamp": 1234567890,
                "triggerPointId": 0,
                "actualPosition": 100.05,
                "positionError": 0.05,
                "responseTimeUs": 500,
                "outputVerified": true
            }
        ],
        "statistics": {
            "maxPositionError": 0.1,
            "avgPositionError": 0.05,
            "maxResponseTimeUs": 1000,
            "avgResponseTimeUs": 600,
            "successfulTriggers": 9,
            "missedTriggers": 1
        }
    })";

    auto result = serializer.DeserializeTriggerTestData(jsonStr);

    ASSERT_TRUE(result.IsSuccess());
    TriggerTestData data = result.Value();

    EXPECT_EQ(data.recordId, 12345);
    EXPECT_EQ(data.triggerType, TriggerType::SinglePoint);
    EXPECT_EQ(data.triggerPoints.size(), 1);
    EXPECT_EQ(data.triggerEvents.size(), 1);
    EXPECT_DOUBLE_EQ(data.statistics.maxPositionError, 0.1);
    EXPECT_EQ(data.statistics.successfulTriggers, 9);
}

// ========================================
// 往返测试
// ========================================

TEST_F(TriggerTestDataSerializerTest, RoundTrip_PreserveAllFields) {
    TriggerTestData original = CreateStandardTriggerTestData();

    auto serializeResult = serializer.SerializeTriggerTestData(original);
    ASSERT_TRUE(serializeResult.IsSuccess());

    auto deserializeResult = serializer.DeserializeTriggerTestData(serializeResult.Value());
    ASSERT_TRUE(deserializeResult.IsSuccess());

    TriggerTestData restored = deserializeResult.Value();

    EXPECT_EQ(restored.recordId, original.recordId);
    EXPECT_EQ(restored.triggerType, original.triggerType);
    EXPECT_EQ(restored.triggerPoints.size(), original.triggerPoints.size());
    EXPECT_EQ(restored.triggerEvents.size(), original.triggerEvents.size());
    EXPECT_DOUBLE_EQ(restored.statistics.maxPositionError, original.statistics.maxPositionError);
    EXPECT_EQ(restored.statistics.successfulTriggers, original.statistics.successfulTriggers);
}

// ========================================
// 批量操作测试
// ========================================

TEST_F(TriggerTestDataSerializerTest, BatchSerialize_RoundTrip) {
    std::vector<TriggerTestData> originalList;
    for (int i = 0; i < 3; ++i) {
        TriggerTestData data = CreateStandardTriggerTestData();
        data.recordId = 10000 + i;
        data.triggerType = (i % 2 == 0) ? TriggerType::SinglePoint : TriggerType::MultiPoint;
        originalList.push_back(data);
    }

    auto serializeResult = serializer.SerializeTriggerTestDataBatch(originalList);
    ASSERT_TRUE(serializeResult.IsSuccess());

    auto deserializeResult = serializer.DeserializeTriggerTestDataBatch(serializeResult.Value());
    ASSERT_TRUE(deserializeResult.IsSuccess());

    std::vector<TriggerTestData> restoredList = deserializeResult.Value();

    EXPECT_EQ(restoredList.size(), originalList.size());

    for (size_t i = 0; i < restoredList.size(); ++i) {
        EXPECT_EQ(restoredList[i].recordId, originalList[i].recordId);
        EXPECT_EQ(restoredList[i].triggerType, originalList[i].triggerType);
    }
}
```

**Step 4-6:** 更新 CMakeLists.txt、编译、提交（同 Task 3）

---

## Task 5: 实现 CMPTestData 序列化器

**关键点:** 需要为 CMPParameters 和 TrajectoryAnalysis 添加 JSON 支持。

**文件:**
- Modify: `src/infrastructure/serialization/JSONUtils.h` (添加 CMPParameters 和 TrajectoryAnalysis 序列化)
- Create: `src/infrastructure/serialization/CMPTestDataSerializer.h`
- Create: `src/infrastructure/serialization/CMPTestDataSerializer.cpp`
- Test: `tests/unit/infrastructure/serialization/test_CMPTestDataSerializer.cpp`

**Step 1: 扩展 JSONUtils.h 添加 CMP 相关类型支持**

在 `JSONUtils.h` 的 `namespace Siligen::Shared::Types` 块中添加：

```cpp
// CMP 相关类型
#include "../../../domain/models/TrajectoryTypes.h"

// 在 using 声明区域添加
using Siligen::CMPParameters;
using Siligen::TrajectoryAnalysis;

/**
 * @brief CMPParameters 转 JSON
 */
inline void to_json(nlohmann::json& j, const CMPParameters& p) {
    j = nlohmann::json{
        {"compensationGain", p.compensationGain},
        {"lookaheadPoints", p.lookaheadPoints},
        {"maxCompensation", p.maxCompensation}
    };
}

/**
 * @brief JSON 转 CMPParameters
 */
inline void from_json(const nlohmann::json& j, CMPParameters& p) {
    p.compensationGain = j.at("compensationGain").get<double>();
    p.lookaheadPoints = j.at("lookaheadPoints").get<int>();
    p.maxCompensation = j.at("maxCompensation").get<double>();
}

/**
 * @brief TrajectoryAnalysis 转 JSON
 */
inline void to_json(nlohmann::json& j, const TrajectoryAnalysis& a) {
    j = nlohmann::json{
        {"maxDeviation", a.maxDeviation},
        {"avgDeviation", a.avgDeviation},
        {"rmsDeviation", a.rmsDeviation},
        {"compensationEffectiveness", a.compensationEffectiveness},
        {"totalSamples", a.totalSamples}
    };
}

/**
 * @brief JSON 转 TrajectoryAnalysis
 */
inline void from_json(const nlohmann::json& j, TrajectoryAnalysis& a) {
    a.maxDeviation = j.at("maxDeviation").get<double>();
    a.avgDeviation = j.at("avgDeviation").get<double>();
    a.rmsDeviation = j.at("rmsDeviation").get<double>();
    a.compensationEffectiveness = j.at("compensationEffectiveness").get<double>();
    a.totalSamples = j.at("totalSamples").get<int>();
}
```

**Step 2: 实现 CMPTestDataSerializer**

实现模式同 TriggerTestDataSerializer，关键不同点：
- 使用 `TrajectoryType` 枚举（已有转换器）
- `controlPoints`、`theoreticalPath`、`actualPath`、`compensationData` 都是 `std::vector<Point2D>`（已支持）
- `cmpParams` 是 `CMPParameters`（在 Step 1 添加）
- `analysis` 是 `TrajectoryAnalysis`（在 Step 1 添加）

参考 Task 4 的实现模式，创建以下文件：
- `src/infrastructure/serialization/CMPTestDataSerializer.h`
- `src/infrastructure/serialization/CMPTestDataSerializer.cpp`
- `tests/unit/infrastructure/serialization/test_CMPTestDataSerializer.cpp`

**关键代码片段:**

```cpp
// 序列化
Result<std::string> CMPTestDataSerializer::SerializeCMPTestData(const CMPTestData& data) {
    try {
        json j;
        j["recordId"] = data.recordId;
        j["trajectoryType"] = ToString(data.trajectoryType);
        j["controlPoints"] = data.controlPoints;
        j["cmpParams"] = data.cmpParams;  // CMPParameters 已支持
        j["theoreticalPath"] = data.theoreticalPath;
        j["actualPath"] = data.actualPath;
        j["compensationData"] = data.compensationData;
        j["analysis"] = data.analysis;  // TrajectoryAnalysis 已支持
        return Dump(j);
    } catch (const std::exception& e) {
        return Result<std::string>::Failure(
            CreateSerializationError(ErrorCode::SERIALIZATION_FAILED,
                                     std::string("CMPTestData 序列化失败: ") + e.what())
        );
    }
}
```

**Step 3-6:** 编写测试、更新 CMakeLists.txt、编译、提交（同前）

---

## Task 6: 实现 DiagnosticResult 序列化器

**关键点:** 需要为四个诊断结果结构添加 JSON 支持。

**文件:**
- Modify: `src/infrastructure/serialization/JSONUtils.h` (添加诊断结果类型序列化)
- Create: `src/infrastructure/serialization/DiagnosticResultSerializer.h`
- Create: `src/infrastructure/serialization/DiagnosticResultSerializer.cpp`
- Test: `tests/unit/infrastructure/serialization/test_DiagnosticResultSerializer.cpp`

**Step 1: 扩展 JSONUtils.h 添加诊断结果类型支持**

在 `JSONUtils.h` 中添加：

```cpp
// 诊断相关类型
#include "../../../domain/models/DiagnosticTypes.h"

// 在 using 声明区域添加
using Siligen::HardwareCheckResult;
using Siligen::CommunicationCheckResult;
using Siligen::ResponseTimeCheckResult;
using Siligen::AccuracyCheckResult;

/**
 * @brief HardwareCheckResult 转 JSON
 */
inline void to_json(nlohmann::json& j, const HardwareCheckResult& r) {
    j = nlohmann::json{
        {"controllerConnected", r.controllerConnected},
        {"enabledAxes", r.enabledAxes},
        {"limitSwitchOk", r.limitSwitchOk},
        {"encoderOk", r.encoderOk}
    };
}

/**
 * @brief JSON 转 HardwareCheckResult
 */
inline void from_json(const nlohmann::json& j, HardwareCheckResult& r) {
    r.controllerConnected = j.at("controllerConnected").get<bool>();
    r.enabledAxes = j.at("enabledAxes").get<std::vector<int>>();
    r.limitSwitchOk = j.at("limitSwitchOk").get<std::map<int, bool>>();
    r.encoderOk = j.at("encoderOk").get<std::map<int, bool>>();
}

/**
 * @brief CommunicationCheckResult 转 JSON
 */
inline void to_json(nlohmann::json& j, const CommunicationCheckResult& r) {
    j = nlohmann::json{
        {"avgResponseTimeMs", r.avgResponseTimeMs},
        {"maxResponseTimeMs", r.maxResponseTimeMs},
        {"packetLossRate", r.packetLossRate},
        {"totalCommands", r.totalCommands},
        {"failedCommands", r.failedCommands}
    };
}

/**
 * @brief JSON 转 CommunicationCheckResult
 */
inline void from_json(const nlohmann::json& j, CommunicationCheckResult& r) {
    r.avgResponseTimeMs = j.at("avgResponseTimeMs").get<double>();
    r.maxResponseTimeMs = j.at("maxResponseTimeMs").get<double>();
    r.packetLossRate = j.at("packetLossRate").get<double>();
    r.totalCommands = j.at("totalCommands").get<int>();
    r.failedCommands = j.at("failedCommands").get<int>();
}

/**
 * @brief ResponseTimeCheckResult 转 JSON
 */
inline void to_json(nlohmann::json& j, const ResponseTimeCheckResult& r) {
    j = nlohmann::json{
        {"axisResponseTimeMs", r.axisResponseTimeMs},
        {"allWithinSpec", r.allWithinSpec},
        {"specLimitMs", r.specLimitMs}
    };
}

/**
 * @brief JSON 转 ResponseTimeCheckResult
 */
inline void from_json(const nlohmann::json& j, ResponseTimeCheckResult& r) {
    r.axisResponseTimeMs = j.at("axisResponseTimeMs").get<std::map<int, double>>();
    r.allWithinSpec = j.at("allWithinSpec").get<bool>();
    r.specLimitMs = j.at("specLimitMs").get<double>();
}

/**
 * @brief AccuracyCheckResult 转 JSON
 */
inline void to_json(nlohmann::json& j, const AccuracyCheckResult& r) {
    j = nlohmann::json{
        {"repeatabilityError", r.repeatabilityError},
        {"positioningError", r.positioningError},
        {"meetsAccuracyRequirement", r.meetsAccuracyRequirement},
        {"requiredAccuracy", r.requiredAccuracy}
    };
}

/**
 * @brief JSON 转 AccuracyCheckResult
 */
inline void from_json(const nlohmann::json& j, AccuracyCheckResult& r) {
    r.repeatabilityError = j.at("repeatabilityError").get<std::map<int, double>>();
    r.positioningError = j.at("positioningError").get<std::map<int, double>>();
    r.meetsAccuracyRequirement = j.at("meetsAccuracyRequirement").get<bool>();
    r.requiredAccuracy = j.at("requiredAccuracy").get<double>();
}
```

**Step 2: 实现 DiagnosticResultSerializer**

实现模式同前，关键不同点：
- 四个诊断结果字段（在 Step 1 添加）
- `issues` 是 `std::vector<DiagnosticIssue>`（已在 Task 2 添加支持）
- `healthScore` 是 `int`

**Step 3-6:** 编写测试、更新 CMakeLists.txt、编译、提交（同前）

---

## Task 7: 更新 CMakeLists.txt 整合所有序列化器

**文件:**
- Modify: `src/infrastructure/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: 更新 src/infrastructure/CMakeLists.txt**

```cmake
add_library(siligen_serialization STATIC
    serialization/EnumConverters.cpp
    serialization/HomingTestDataSerializer.cpp
    serialization/InterpolationTestDataSerializer.cpp
    serialization/JogTestDataSerializer.cpp
    serialization/TriggerTestDataSerializer.cpp
    serialization/CMPTestDataSerializer.cpp
    serialization/DiagnosticResultSerializer.cpp
    serialization/JSONUtils.cpp
)
```

**Step 2: 添加所有测试目标**

```cmake
# 枚举转换器测试
add_executable(test_EnumConverters
    tests/unit/infrastructure/serialization/test_EnumConverters.cpp
)
target_link_libraries(test_EnumConverters
    PRIVATE siligen_serialization
    gtest_main
)

# JSON 工具测试
add_executable(test_JSONUtils
    tests/unit/infrastructure/serialization/test_JSONUtils.cpp
)
target_link_libraries(test_JSONUtils
    PRIVATE siligen_serialization
    gtest_main
)

# 嵌套类型 JSON 测试
add_executable(test_JSONUtilsNestedTypes
    tests/unit/infrastructure/serialization/test_JSONUtilsNestedTypes.cpp
)
target_link_libraries(test_JSONUtilsNestedTypes
    PRIVATE siligen_serialization
    gtest_main
)

# 各序列化器测试
add_executable(test_HomingTestDataSerializer
    tests/unit/infrastructure/serialization/test_HomingTestDataSerializer.cpp
)
target_link_libraries(test_HomingTestDataSerializer
    PRIVATE siligen_serialization
    gtest_main
)

add_executable(test_InterpolationTestDataSerializer
    tests/unit/infrastructure/serialization/test_InterpolationTestDataSerializer.cpp
)
target_link_libraries(test_InterpolationTestDataSerializer
    PRIVATE siligen_serialization
    gtest_main
)

add_executable(test_JogTestDataSerializer
    tests/unit/infrastructure/serialization/test_JogTestDataSerializer.cpp
)
target_link_libraries(test_JogTestDataSerializer
    PRIVATE siligen_serialization
    gtest_main
)

add_executable(test_TriggerTestDataSerializer
    tests/unit/infrastructure/serialization/test_TriggerTestDataSerializer.cpp
)
target_link_libraries(test_TriggerTestDataSerializer
    PRIVATE siligen_serialization
    gtest_main
)

add_executable(test_CMPTestDataSerializer
    tests/unit/infrastructure/serialization/test_CMPTestDataSerializer.cpp
)
target_link_libraries(test_CMPTestDataSerializer
    PRIVATE siligen_serialization
    gtest_main
)

add_executable(test_DiagnosticResultSerializer
    tests/unit/infrastructure/serialization/test_DiagnosticResultSerializer.cpp
)
target_link_libraries(test_DiagnosticResultSerializer
    PRIVATE siligen_serialization
    gtest_main
)
```

**Step 3: 编译所有测试**

Run:
```bash
cd build
cmake --build . --target test_EnumConverters
cmake --build . --target test_JSONUtils
cmake --build . --target test_JSONUtilsNestedTypes
cmake --build . --target test_HomingTestDataSerializer
cmake --build . --target test_InterpolationTestDataSerializer
cmake --build . --target test_JogTestDataSerializer
cmake --build . --target test_TriggerTestDataSerializer
cmake --build . --target test_CMPTestDataSerializer
cmake --build . --target test_DiagnosticResultSerializer
```

Expected: 所有目标编译成功 ✅

**Step 4: 运行所有序列化测试**

Run:
```bash
cd build
ctest -R "serialization" -V
```

Expected: 所有测试通过 ✅

**Step 5: 提交所有变更**

```bash
git add .
git commit -m "feat(serialization): complete Phase 2 - add JogTestData, TriggerTestData, CMPTestData, DiagnosticResult serializers"
```

---

## Task 8: 性能测试扩展

**文件:**
- Modify: `tests/unit/infrastructure/serialization/test_SerializationPerformance.cpp`

**Step 1: 添加 JogTestData 性能测试**

在 `test_SerializationPerformance.cpp` 中添加：

```cpp
// JogTestData 性能测试
TEST_F(SerializationPerformanceTest, JogTestData_Serialize_Performance) {
    JogTestData data;
    data.recordId = 12345;
    data.axis = 0;
    data.direction = JogDirection::Positive;
    data.speed = 50.0;
    data.distance = 10.0;

    auto start = std::chrono::steady_clock::now();
    JogTestDataSerializer jogSerializer;
    auto result = jogSerializer.SerializeJogTestData(data);
    auto end = std::chrono::steady_clock::now();

    ASSERT_TRUE(result.IsSuccess());
    double elapsedMs = CalculateElapsedTime(start, end);

    EXPECT_LT(elapsedMs, 1.0) << "JogTestData 序列化耗时: " << elapsedMs << "ms";
}
```

**Step 2: 运行性能测试**

Run:
```bash
cd build
./tests/unit/infrastructure/serialization/test_SerializationPerformance
```

---

## 验收标准

### 功能要求
- [ ] JogTestData 完整序列化/反序列化
- [ ] TriggerTestData 完整序列化/反序列化
- [ ] CMPTestData 完整序列化/反序列化
- [ ] DiagnosticResult 完整序列化/反序列化
- [ ] 支持批量操作
- [ ] 所有嵌套类型正确序列化

### 质量要求
- [ ] 单元测试覆盖率 100%
- [ ] 所有测试通过
- [ ] 无架构违规
- [ ] 符合 C++17 标准
- [ ] 使用 Result<T> 错误处理

### 性能要求
- [ ] JogTestData: < 1ms per operation
- [ ] TriggerTestData: < 1ms per operation
- [ ] CMPTestData: < 5ms per operation
- [ ] DiagnosticResult: < 2ms per operation

---

## 文件清单

### 新增文件 (15 个)

#### 头文件 (4 个)
1. `src/infrastructure/serialization/JogTestDataSerializer.h`
2. `src/infrastructure/serialization/TriggerTestDataSerializer.h`
3. `src/infrastructure/serialization/CMPTestDataSerializer.h`
4. `src/infrastructure/serialization/DiagnosticResultSerializer.h`

#### 实现文件 (4 个)
5. `src/infrastructure/serialization/JogTestDataSerializer.cpp`
6. `src/infrastructure/serialization/TriggerTestDataSerializer.cpp`
7. `src/infrastructure/serialization/CMPTestDataSerializer.cpp`
8. `src/infrastructure/serialization/DiagnosticResultSerializer.cpp`

#### 测试文件 (5 个)
9. `tests/unit/infrastructure/serialization/test_JSONUtilsNestedTypes.cpp`
10. `tests/unit/infrastructure/serialization/test_JogTestDataSerializer.cpp`
11. `tests/unit/infrastructure/serialization/test_TriggerTestDataSerializer.cpp`
12. `tests/unit/infrastructure/serialization/test_CMPTestDataSerializer.cpp`
13. `tests/unit/infrastructure/serialization/test_DiagnosticResultSerializer.cpp`

### 修改文件 (3 个)
14. `src/infrastructure/serialization/EnumConverters.h` (添加 TriggerAction, IssueSeverity)
15. `src/infrastructure/serialization/JSONUtils.h` (添加所有嵌套类型支持)
16. `src/infrastructure/CMakeLists.txt` (添加库和测试目标)

**总计:** 18 个文件，约 3500 行代码（含测试）

---

## 估计时间

- Task 0: 验证 Phase 1 - 15 分钟
- Task 1: 扩展枚举转换器 - 45 分钟
- Task 2: 扩展 JSONUtils - 1.5 小时
- Task 3: JogTestData 序列化器 - 1.5 小时
- Task 4: TriggerTestData 序列化器 - 2 小时
- Task 5: CMPTestData 序列化器 - 2.5 小时
- Task 6: DiagnosticResult 序列化器 - 2 小时
- Task 7: CMakeLists.txt 整合 - 30 分钟
- Task 8: 性能测试扩展 - 30 分钟

**总计:** 约 11 小时

---

## 执行选项

**Plan complete and saved to `docs/plans/2026-01-08-additional-test-data-serialization.md`.**

**Two execution options:**

**1. Subagent-Driven (this session)** - I dispatch fresh subagent per task, review between tasks, fast iteration

**2. Parallel Session (separate)** - Open new session with executing-plans, batch execution with checkpoints

**Which approach?**

