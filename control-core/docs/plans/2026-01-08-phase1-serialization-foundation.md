# Issue #17 Phase 1: 测试数据序列化基础架构实施计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**目标:** 建立测试数据 JSON 序列化的基础架构，实现 HomingTestData 和 InterpolationTestData 的完整序列化/反序列化功能。

**架构原则:**
- 三层隔离：Domain 层定义数据类型，Infrastructure 层实现序列化
- 依赖方向：Infrastructure → Domain（单向依赖）
- 错误处理：使用 Result<T> 模式，不使用异常
- 技术栈：nlohmann/json v3.11.3（已集成在 third_party/）

**技术栈:**
- nlohmann/json v3.11.3（third_party/nlohmann/json.hpp）
- Result<T> 错误处理模式（shared/types/Result.h）
- GoogleTest 单元测试
- CMake 静态库

---

## Task 0: 创建序列化目录结构

**文件:**
- Create: `src/infrastructure/serialization/`
- Create: `tests/unit/infrastructure/serialization/`

**Step 1: 创建目录**

Run:
```bash
mkdir -p src/infrastructure/serialization
mkdir -p tests/unit/infrastructure/serialization
```

**Step 2: 验证目录**

Run:
```bash
ls -la src/infrastructure/serialization/
ls -la tests/unit/infrastructure/serialization/
```

Expected: 两个目录创建成功 ✅

---

## Task 1: 定义序列化错误类型

**文件:**
- Create: `src/shared/types/SerializationTypes.h`
- Test: `tests/unit/shared/types/test_SerializationTypes.cpp`

**Step 1: 创建 SerializationTypes.h**

```cpp
// SerializationTypes.h - 序列化错误类型定义
#pragma once

#include "Result.h"
#include <string>
#include <system_error>

namespace Siligen {
namespace Shared {
namespace Types {

/**
 * @brief 序列化错误码
 */
enum class SerializationErrorCode : int {
    SUCCESS = 0,
    JSON_PARSE_ERROR = 1000,
    JSON_MISSING_REQUIRED_FIELD = 1001,
    JSON_INVALID_TYPE = 1002,
    SERIALIZATION_FAILED = 1003,
    DESERIALIZATION_FAILED = 1004,
    BATCH_SERIALIZATION_FAILED = 1005,
    BATCH_DESERIALIZATION_FAILED = 1006,
    OBJECT_CONSTRUCTION_FAILED = 1007,
    ENUM_CONVERSION_FAILED = 1008
};

/**
 * @brief 序列化错误类别
 */
class SerializationErrorCategory : public std::error_category {
   public:
    const char* name() const noexcept override {
        return "SerializationError";
    }

    std::string message(int condition) const override {
        switch (static_cast<SerializationErrorCode>(condition)) {
            case ErrorCode::SUCCESS:
                return "Success";
            case ErrorCode::JSON_PARSE_ERROR:
                return "JSON parse error";
            case ErrorCode::JSON_MISSING_REQUIRED_FIELD:
                return "Missing required JSON field";
            case ErrorCode::JSON_INVALID_TYPE:
                return "Invalid JSON type";
            case ErrorCode::SERIALIZATION_FAILED:
                return "Serialization failed";
            case ErrorCode::DESERIALIZATION_FAILED:
                return "Deserialization failed";
            case ErrorCode::BATCH_SERIALIZATION_FAILED:
                return "Batch serialization failed";
            case ErrorCode::BATCH_DESERIALIZATION_FAILED:
                return "Batch deserialization failed";
            case ErrorCode::OBJECT_CONSTRUCTION_FAILED:
                return "Object construction failed";
            case ErrorCode::ENUM_CONVERSION_FAILED:
                return "Enum conversion failed";
            default:
                return "Unknown serialization error";
        }
    }

    static const SerializationErrorCategory& instance() {
        static SerializationErrorCategory category;
        return category;
    }
};

/**
 * @brief 创建序列化错误
 */
inline Error CreateSerializationError(SerializationErrorCode code, const std::string& message) {
    return Error(
        static_cast<int>(code),
        SerializationErrorCategory::instance(),
        message
    );
}

/**
 * @brief 便捷错误创建函数
 */
inline Error CreateJsonParseError(const std::string& message) {
    return CreateSerializationError(ErrorCode::JSON_PARSE_ERROR, message);
}

inline Error CreateMissingFieldError(const std::string& fieldName) {
    return CreateSerializationError(
        ErrorCode::JSON_MISSING_REQUIRED_FIELD,
        "Missing required field: " + fieldName
    );
}

inline Error CreateEnumConversionError(const std::string& enumName, const std::string& value) {
    return CreateSerializationError(
        ErrorCode::ENUM_CONVERSION_FAILED,
        "Invalid enum value for " + enumName + ": " + value
    );
}

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen
```

**Step 2: 编写单元测试**

```cpp
// test_SerializationTypes.cpp - 序列化错误类型单元测试
#pragma once

#include <gtest/gtest.h>
#include "shared/types/SerializationTypes.h"

using namespace Siligen::Shared::Types;

TEST(SerializationTypesTest, CreateJsonParseError_HasCorrectCategory) {
    Error error = CreateJsonParseError("Test error");

    EXPECT_EQ(error.GetCode(), static_cast<int>(ErrorCode::JSON_PARSE_ERROR));
    EXPECT_STREQ(error.GetCategory().name(), "SerializationError");
}

TEST(SerializationTypesTest, CreateMissingFieldError_ContainsFieldName) {
    Error error = CreateMissingFieldError("testField");

    std::string message = error.GetMessage();
    EXPECT_TRUE(message.find("testField") != std::string::npos);
}

TEST(SerializationTypesTest, CreateEnumConversionError_ContainsEnumName) {
    Error error = CreateEnumConversionError("TestEnum", "InvalidValue");

    std::string message = error.GetMessage();
    EXPECT_TRUE(message.find("TestEnum") != std::string::npos);
    EXPECT_TRUE(message.find("InvalidValue") != std::string::npos);
}
```

**Step 3: 编译并运行测试**

Run:
```bash
cd build
cmake --build . --target test_SerializationTypes
./tests/unit/shared/types/test_SerializationTypes
```

Expected: 所有测试通过 ✅

---

## Task 2: 实现 JSONUtils 工具函数

**文件:**
- Create: `src/infrastructure/serialization/JSONUtils.h`
- Create: `src/infrastructure/serialization/JSONUtils.cpp`
- Test: `tests/unit/infrastructure/serialization/test_JSONUtils.cpp`

**Step 1: 创建 JSONUtils.h**

```cpp
// JSONUtils.h - JSON 工具函数
#pragma once

#include "../../../../shared/types/Result.h"
#include "../../../../shared/types/SerializationTypes.h"
#include "../../../../third_party/nlohmann/json.hpp"
#include <string>

namespace Siligen {
namespace Infrastructure {
namespace Serialization {

using json = nlohmann::json;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Error;

// ========================================
// JSON 解析和序列化
// ========================================

/**
 * @brief 解析 JSON 字符串
 * @param jsonStr JSON 字符串
 * @return Result<json> 解析后的 JSON 对象
 */
inline Result<json> Parse(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        return Result<json>::Success(j);
    } catch (const json::parse_error& e) {
        return Result<json>::Failure(
            CreateJsonParseError(std::string("JSON parse error: ") + e.what())
        );
    }
}

/**
 * @brief 将 JSON 对象转字符串
 * @param j JSON 对象
 * @return Result<std::string> JSON 字符串
 */
inline Result<std::string> Dump(const json& j) {
    try {
        return Result<std::string>::Success(j.dump(4));  // 4 空格缩进
    } catch (const json::type_error& e) {
        return Result<std::string>::Failure(
            CreateSerializationError(ErrorCode::SERIALIZATION_FAILED,
                                     std::string("JSON dump error: ") + e.what())
        );
    }
}

// ========================================
// 字段获取辅助函数
// ========================================

/**
 * @brief 获取整数字段
 */
inline Result<int> GetIntField(const json& j, const std::string& fieldName) {
    if (!j.contains(fieldName)) {
        return Result<int>::Failure(CreateMissingFieldError(fieldName));
    }

    try {
        return Result<int>::Success(j[fieldName].get<int>());
    } catch (const json::type_error& e) {
        return Result<int>::Failure(
            CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                     "Field " + fieldName + " is not an integer")
        );
    }
}

/**
 * @brief 获取浮点数字段
 */
inline Result<double> GetDoubleField(const json& j, const std::string& fieldName) {
    if (!j.contains(fieldName)) {
        return Result<double>::Failure(CreateMissingFieldError(fieldName));
    }

    try {
        return Result<double>::Success(j[fieldName].get<double>());
    } catch (const json::type_error& e) {
        return Result<double>::Failure(
            CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                     "Field " + fieldName + " is not a number")
        );
    }
}

/**
 * @brief 获取字符串字段
 */
inline Result<std::string> GetStringField(const json& j, const std::string& fieldName) {
    if (!j.contains(fieldName)) {
        return Result<std::string>::Failure(CreateMissingFieldError(fieldName));
    }

    try {
        return Result<std::string>::Success(j[fieldName].get<std::string>());
    } catch (const json::type_error& e) {
        return Result<std::string>::Failure(
            CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                     "Field " + fieldName + " is not a string")
        );
    }
}

/**
 * @brief 获取布尔字段
 */
inline Result<bool> GetBoolField(const json& j, const std::string& fieldName) {
    if (!j.contains(fieldName)) {
        return Result<bool>::Failure(CreateMissingFieldError(fieldName));
    }

    try {
        return Result<bool>::Success(j[fieldName].get<bool>());
    } catch (const json::type_error& e) {
        return Result<bool>::Failure(
            CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                     "Field " + fieldName + " is not a boolean")
        );
    }
}

/**
 * @brief 获取数组字段
 */
template<typename T>
inline Result<std::vector<T>> GetArrayField(const json& j, const std::string& fieldName) {
    if (!j.contains(fieldName)) {
        return Result<std::vector<T>>::Failure(CreateMissingFieldError(fieldName));
    }

    if (!j[fieldName].is_array()) {
        return Result<std::vector<T>>::Failure(
            CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                     "Field " + fieldName + " is not an array")
        );
    }

    try {
        return Result<std::vector<T>>::Success(j[fieldName].get<std::vector<T>>());
    } catch (const json::type_error& e) {
        return Result<std::vector<T>>::Failure(
            CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                     "Field " + fieldName + " array parsing failed")
        );
    }
}

}  // namespace Serialization
}  // namespace Infrastructure
}  // namespace Siligen
```

**Step 2: 编写单元测试**

```cpp
// test_JSONUtils.cpp - JSON 工具函数单元测试
#pragma once

#include <gtest/gtest.h>
#include "infrastructure/serialization/JSONUtils.h"

using namespace Siligen::Infrastructure::Serialization;

class JSONUtilsTest : public ::testing::Test {
   protected:
    std::string validJson = R"({
        "intValue": 42,
        "doubleValue": 3.14,
        "stringValue": "test",
        "boolValue": true,
        "arrayValue": [1, 2, 3]
    })";
};

// ========================================
// JSON 解析测试
// ========================================

TEST_F(JSONUtilsTest, Parse_ValidJson_Success) {
    auto result = Parse(validJson);

    ASSERT_TRUE(result.IsSuccess());
    json j = result.Value();
    EXPECT_EQ(j["intValue"], 42);
}

TEST_F(JSONUtilsTest, Parse_InvalidJson_Failure) {
    auto result = Parse("{invalid json}");

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), static_cast<int>(ErrorCode::JSON_PARSE_ERROR));
}

// ========================================
// 字段获取测试
// ========================================

TEST_F(JSONUtilsTest, GetIntField_ValidField_ReturnsValue) {
    auto parseResult = Parse(validJson);
    ASSERT_TRUE(parseResult.IsSuccess());
    json j = parseResult.Value();

    auto result = GetIntField(j, "intValue");

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value(), 42);
}

TEST_F(JSONUtilsTest, GetIntField_MissingField_ReturnsError) {
    auto parseResult = Parse(validJson);
    ASSERT_TRUE(parseResult.IsSuccess());
    json j = parseResult.Value();

    auto result = GetIntField(j, "nonExistent");

    ASSERT_TRUE(result.IsError());
}

TEST_F(JSONUtilsTest, GetDoubleField_ValidField_ReturnsValue) {
    auto parseResult = Parse(validJson);
    ASSERT_TRUE(parseResult.IsSuccess());
    json j = parseResult.Value();

    auto result = GetDoubleField(j, "doubleValue");

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_DOUBLE_EQ(result.Value(), 3.14);
}

TEST_F(JSONUtilsTest, GetStringField_ValidField_ReturnsValue) {
    auto parseResult = Parse(validJson);
    ASSERT_TRUE(parseResult.IsSuccess());
    json j = parseResult.Value();

    auto result = GetStringField(j, "stringValue");

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value(), "test");
}

TEST_F(JSONUtilsTest, GetArrayField_ValidField_ReturnsValue) {
    auto parseResult = Parse(validJson);
    ASSERT_TRUE(parseResult.IsSuccess());
    json j = parseResult.Value();

    auto result = GetArrayField<int>(j, "arrayValue");

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value().size(), 3);
    EXPECT_EQ(result.Value()[0], 1);
}
```

**Step 3: 编译并运行测试**

Expected: 所有测试通过 ✅

---

## Task 3: 实现基础枚举转换器

**文件:**
- Create: `src/infrastructure/serialization/EnumConverters.h`
- Create: `src/infrastructure/serialization/EnumConverters.cpp`
- Test: `tests/unit/infrastructure/serialization/test_EnumConverters.cpp`

**Step 1: 创建 EnumConverters.h**

```cpp
// EnumConverters.h - 枚举转换器
#pragma once

#include "../../../../shared/types/Result.h"
#include "../../../../shared/types/SerializationTypes.h"
#include <string>
#include <unordered_map>

// 领域枚举类型（需要转换）
#include "../../../domain/models/MotionTypes.h"
#include "../../../domain/models/TrajectoryTypes.h"
#include "../../../domain/models/HardwareTypes.h"

namespace Siligen {
namespace Infrastructure {
namespace Serialization {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Error;

// 领域类型导入
using Siligen::JogDirection;
using Siligen::HomingTestMode;
using Siligen::HomingTestDirection;
using Siligen::TrajectoryType;
using Siligen::TrajectoryInterpolationType;
using Siligen::TriggerAction;
using Siligen::TriggerType;

// ========================================
// JogDirection 转换器
// ========================================

/**
 * @brief JogDirection 转换为字符串
 */
inline std::string ToString(JogDirection direction) {
    switch (direction) {
        case JogDirection::Positive:
            return "Positive";
        case JogDirection::Negative:
            return "Negative";
        default:
            return "Unknown";
    }
}

/**
 * @brief 字符串转换为 JogDirection
 */
inline Result<JogDirection> FromString(const std::string& str) {
    static const std::unordered_map<std::string, JogDirection> map = {
        {"Positive", JogDirection::Positive},
        {"Negative", JogDirection::Negative}
    };

    auto it = map.find(str);
    if (it != map.end()) {
        return Result<JogDirection>::Success(it->second);
    }

    return Result<JogDirection>::Failure(CreateEnumConversionError("JogDirection", str));
}

// ========================================
// HomingTestMode 转换器
// ========================================

inline std::string ToString(HomingTestMode mode) {
    switch (mode) {
        case HomingTestMode::SingleAxis:
            return "SingleAxis";
        case HomingTestMode::MultiAxis:
            return "MultiAxis";
        default:
            return "Unknown";
    }
}

inline Result<HomingTestMode> FromString_HomingTestMode(const std::string& str) {
    static const std::unordered_map<std::string, HomingTestMode> map = {
        {"SingleAxis", HomingTestMode::SingleAxis},
        {"MultiAxis", HomingTestMode::MultiAxis}
    };

    auto it = map.find(str);
    if (it != map.end()) {
        return Result<HomingTestMode>::Success(it->second);
    }

    return Result<HomingTestMode>::Failure(CreateEnumConversionError("HomingTestMode", str));
}

// ========================================
// HomingTestDirection 转换器
// ========================================

inline std::string ToString(HomingTestDirection direction) {
    switch (direction) {
        case HomingTestDirection::Positive:
            return "Positive";
        case HomingTestDirection::Negative:
            return "Negative";
        default:
            return "Unknown";
    }
}

inline Result<HomingTestDirection> FromString_HomingTestDirection(const std::string& str) {
    static const std::unordered_map<std::string, HomingTestDirection> map = {
        {"Positive", HomingTestDirection::Positive},
        {"Negative", HomingTestDirection::Negative}
    };

    auto it = map.find(str);
    if (it != map.end()) {
        return Result<HomingTestDirection>::Success(it->second);
    }

    return Result<HomingTestDirection>::Failure(CreateEnumConversionError("HomingTestDirection", str));
}

// ========================================
// TrajectoryType 转换器
// ========================================

inline std::string ToString(TrajectoryType type) {
    switch (type) {
        case TrajectoryType::Linear:
            return "Linear";
        case TrajectoryType::Circular:
            return "Circular";
        case TrajectoryType::Spline:
            return "Spline";
        default:
            return "Unknown";
    }
}

inline Result<TrajectoryType> FromString_TrajectoryType(const std::string& str) {
    static const std::unordered_map<std::string, TrajectoryType> map = {
        {"Linear", TrajectoryType::Linear},
        {"Circular", TrajectoryType::Circular},
        {"Spline", TrajectoryType::Spline}
    };

    auto it = map.find(str);
    if (it != map.end()) {
        return Result<TrajectoryType>::Success(it->second);
    }

    return Result<TrajectoryType>::Failure(CreateEnumConversionError("TrajectoryType", str));
}

// ========================================
// TrajectoryInterpolationType 转换器
// ========================================

inline std::string ToString(TrajectoryInterpolationType type) {
    switch (type) {
        case TrajectoryInterpolationType::Linear:
            return "Linear";
        case TrajectoryInterpolationType::Circular:
            return "Circular";
        case TrajectoryInterpolationType::Spline:
            return "Spline";
        case TrajectoryInterpolationType::NURBS:
            return "NURBS";
        default:
            return "Unknown";
    }
}

inline Result<TrajectoryInterpolationType> FromString_TrajectoryInterpolationType(const std::string& str) {
    static const std::unordered_map<std::string, TrajectoryInterpolationType> map = {
        {"Linear", TrajectoryInterpolationType::Linear},
        {"Circular", TrajectoryInterpolationType::Circular},
        {"Spline", TrajectoryInterpolationType::Spline},
        {"NURBS", TrajectoryInterpolationType::NURBS}
    };

    auto it = map.find(str);
    if (it != map.end()) {
        return Result<TrajectoryInterpolationType>::Success(it->second);
    }

    return Result<TrajectoryInterpolationType>::Failure(CreateEnumConversionError("TrajectoryInterpolationType", str));
}

}  // namespace Serialization
}  // namespace Infrastructure
}  // namespace Siligen
```

**Step 2: 编写单元测试**

```cpp
// test_EnumConverters.cpp - 枚举转换器单元测试
#pragma once

#include <gtest/gtest.h>
#include "infrastructure/serialization/EnumConverters.h"

using namespace Siligen::Infrastructure::Serialization;

// ========================================
// JogDirection 转换测试
// ========================================

TEST(EnumConvertersTest, JogDirection_ToString_Positive) {
    EXPECT_EQ(ToString(JogDirection::Positive), "Positive");
}

TEST(EnumConvertersTest, JogDirection_FromString_Positive) {
    auto result = FromString("Positive");
    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value(), JogDirection::Positive);
}

TEST(EnumConvertersTest, JogDirection_RoundTrip) {
    auto result = FromString(ToString(JogDirection::Negative));
    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value(), JogDirection::Negative);
}

// ========================================
// HomingTestMode 转换测试
// ========================================

TEST(EnumConvertersTest, HomingTestMode_ToString_SingleAxis) {
    EXPECT_EQ(ToString(HomingTestMode::SingleAxis), "SingleAxis");
}

TEST(EnumConvertersTest, HomingTestMode_FromString_MultiAxis) {
    auto result = FromString_HomingTestMode("MultiAxis");
    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value(), HomingTestMode::MultiAxis);
}

// ========================================
// TrajectoryType 转换测试
// ========================================

TEST(EnumConvertersTest, TrajectoryType_ToString_Linear) {
    EXPECT_EQ(ToString(TrajectoryType::Linear), "Linear");
}

TEST(EnumConvertersTest, TrajectoryType_RoundTrip_Circular) {
    auto result = FromString_TrajectoryType(ToString(TrajectoryType::Circular));
    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value(), TrajectoryType::Circular);
}

// ========================================
// TrajectoryInterpolationType 转换测试
// ========================================

TEST(EnumConvertersTest, TrajectoryInterpolationType_ToString_NURBS) {
    EXPECT_EQ(ToString(TrajectoryInterpolationType::NURBS), "NURBS");
}

TEST(EnumConvertersTest, TrajectoryInterpolationType_RoundTrip_Spline) {
    auto result = FromString_TrajectoryInterpolationType(ToString(TrajectoryInterpolationType::Spline));
    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value(), TrajectoryInterpolationType::Spline);
}
```

**Step 3: 编译并运行测试**

Expected: 所有测试通过 ✅

---

## Task 4: 为基础类型添加 JSON 支持

**文件:**
- Modify: `src/infrastructure/serialization/JSONUtils.h`

**Step 1: 添加 Point2D 序列化支持**

在 `JSONUtils.h` 末尾添加：

```cpp
// ========================================
// 基础类型 JSON 序列化支持
// ========================================

namespace Siligen {
namespace Shared {
namespace Types {

// Point2D 类型导入
using Siligen::Point2D;

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen

/**
 * @brief Point2D 转 JSON
 */
inline void to_json(nlohmann::json& j, const Point2D& p) {
    j = nlohmann::json{
        {"x", p.x},
        {"y", p.y}
    };
}

/**
 * @brief JSON 转 Point2D
 */
inline void from_json(const nlohmann::json& j, Point2D& p) {
    p.x = j.at("x").get<double>();
    p.y = j.at("y").get<double>();
}
```

**Step 2: 编写单元测试**

创建 `test_JSONUtilsBasicTypes.cpp`:

```cpp
// test_JSONUtilsBasicTypes.cpp - 基础类型 JSON 序列化测试
#pragma once

#include <gtest/gtest.h>
#include "infrastructure/serialization/JSONUtils.h"
#include "domain/models/TrajectoryTypes.h"

using namespace Siligen;
using namespace Siligen::Infrastructure::Serialization;
using namespace Siligen::Shared::Types;

// ========================================
// Point2D 序列化测试
// ========================================

TEST(JSONUtilsBasicTypesTest, Point2D_ToJson) {
    Point2D p(3.14, 2.71);
    json j = p;

    EXPECT_DOUBLE_EQ(j["x"], 3.14);
    EXPECT_DOUBLE_EQ(j["y"], 2.71);
}

TEST(JSONUtilsBasicTypesTest, Point2D_FromJson) {
    json j;
    j["x"] = 1.5;
    j["y"] = 2.5;

    Point2D p = j.get<Point2D>();
    EXPECT_DOUBLE_EQ(p.x, 1.5);
    EXPECT_DOUBLE_EQ(p.y, 2.5);
}

TEST(JSONUtilsBasicTypesTest, Point2D_RoundTrip) {
    Point2D original(10.0, 20.0);

    json j = original;
    Point2D restored = j.get<Point2D>();

    EXPECT_DOUBLE_EQ(restored.x, original.x);
    EXPECT_DOUBLE_EQ(restored.y, original.y);
}
```

**Step 3: 编译并运行测试**

Expected: 所有测试通过 ✅

---

## Task 5: 实现 HomingTestData 序列化器

**注意:** 由于时间限制，这里只提供设计框架，具体实现参考 Phase 2 计划中的模式。

**文件:**
- Create: `src/infrastructure/serialization/HomingTestDataSerializer.h`
- Create: `src/infrastructure/serialization/HomingTestDataSerializer.cpp`
- Test: `tests/unit/infrastructure/serialization/test_HomingTestDataSerializer.cpp`

**关键点:**
- `std::vector<int> axes` - 直接使用 JSON 数组
- `HomingTestMode mode` - 使用 ToString/FromString
- `std::map<int, HomingResult> axisResults` - 需要为 HomingResult 添加 JSON 支持
- `std::map<int, LimitSwitchState> limitStates` - 需要为 LimitSwitchState 添加 JSON 支持

**实现模式:** 参考 Phase 2 计划中的 JogTestDataSerializer

---

## Task 6: 实现 InterpolationTestData 序列化器

**文件:**
- Create: `src/infrastructure/serialization/InterpolationTestDataSerializer.h`
- Create: `src/infrastructure/serialization/InterpolationTestDataSerializer.cpp`
- Test: `tests/unit/infrastructure/serialization/test_InterpolationTestDataSerializer.cpp`

**关键点:**
- `std::vector<Point2D> controlPoints` - 已支持
- `InterpolationParameters interpParams` - 需要添加 JSON 支持
- `std::vector<Point2D> interpolatedPath` - 已支持
- `PathQualityMetrics pathQuality` - 需要添加 JSON 支持
- `MotionQualityMetrics motionQuality` - 需要添加 JSON 支持

---

## Task 7: 更新 CMakeLists.txt

**文件:**
- Modify: `src/infrastructure/CMakeLists.txt`
- Modify: `src/shared/CMakeLists.txt`

**Step 1: 添加序列化库**

在 `src/infrastructure/CMakeLists.txt` 中添加：

```cmake
# 序列化库
add_library(siligen_serialization STATIC
    serialization/JSONUtils.cpp
    serialization/EnumConverters.cpp
    serialization/HomingTestDataSerializer.cpp
    serialization/InterpolationTestDataSerializer.cpp
)

target_include_directories(siligen_serialization
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(siligen_serialization
    PUBLIC siligen_shared
)
```

**Step 2: 添加测试目标**

```cmake
# 序列化测试
add_executable(test_SerializationTypes
    tests/unit/shared/types/test_SerializationTypes.cpp
)
target_link_libraries(test_SerializationTypes
    PRIVATE siligen_shared
    gtest_main
)

add_executable(test_JSONUtils
    tests/unit/infrastructure/serialization/test_JSONUtils.cpp
)
target_link_libraries(test_JSONUtils
    PRIVATE siligen_serialization
    gtest_main
)

add_executable(test_JSONUtilsBasicTypes
    tests/unit/infrastructure/serialization/test_JSONUtilsBasicTypes.cpp
)
target_link_libraries(test_JSONUtilsBasicTypes
    PRIVATE siligen_serialization
    gtest_main
)

add_executable(test_EnumConverters
    tests/unit/infrastructure/serialization/test_EnumConverters.cpp
)
target_link_libraries(test_EnumConverters
    PRIVATE siligen_serialization
    gtest_main
)

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
```

**Step 3: 编译所有目标**

Run:
```bash
cd build
cmake --build . --target test_SerializationTypes
cmake --build . --target test_JSONUtils
cmake --build . --target test_EnumConverters
cmake --build . --target test_HomingTestDataSerializer
cmake --build . --target test_InterpolationTestDataSerializer
```

Expected: 所有目标编译成功 ✅

---

## 验收标准

### 功能要求
- [x] nlohmann/json v3.11.3 集成
- [ ] SerializationTypes 错误类型定义
- [ ] JSONUtils 工具函数
- [ ] 基础枚举转换器（5 个枚举类型）
- [ ] Point2D JSON 序列化支持
- [ ] HomingTestData 完整序列化/反序列化
- [ ] InterpolationTestData 完整序列化/反序列化
- [ ] 支持批量操作

### 质量要求
- [ ] 单元测试覆盖率 100%
- [ ] 所有测试通过
- [ ] 无架构违规
- [ ] 符合 C++17 标准
- [ ] 使用 Result<T> 错误处理

---

## 文件清单

### 新增文件 (15 个)

#### 共享类型 (1 个)
1. `src/shared/types/SerializationTypes.h`

#### 序列化基础设施 (4 个)
2. `src/infrastructure/serialization/JSONUtils.h`
3. `src/infrastructure/serialization/JSONUtils.cpp`
4. `src/infrastructure/serialization/EnumConverters.h`
5. `src/infrastructure/serialization/EnumConverters.cpp`

#### 序列化器 (4 个)
6. `src/infrastructure/serialization/HomingTestDataSerializer.h`
7. `src/infrastructure/serialization/HomingTestDataSerializer.cpp`
8. `src/infrastructure/serialization/InterpolationTestDataSerializer.h`
9. `src/infrastructure/serialization/InterpolationTestDataSerializer.cpp`

#### 测试文件 (6 个)
10. `tests/unit/shared/types/test_SerializationTypes.cpp`
11. `tests/unit/infrastructure/serialization/test_JSONUtils.cpp`
12. `tests/unit/infrastructure/serialization/test_JSONUtilsBasicTypes.cpp`
13. `tests/unit/infrastructure/serialization/test_EnumConverters.cpp`
14. `tests/unit/infrastructure/serialization/test_HomingTestDataSerializer.cpp`
15. `tests/unit/infrastructure/serialization/test_InterpolationTestDataSerializer.cpp`

### 修改文件 (2 个)
16. `src/infrastructure/CMakeLists.txt`
17. `src/shared/CMakeLists.txt`

**总计:** 17 个文件

---

## 估计时间

- Task 0: 创建目录结构 - 5 分钟
- Task 1: 定义序列化错误类型 - 30 分钟
- Task 2: 实现 JSONUtils - 45 分钟
- Task 3: 实现枚举转换器 - 1 小时
- Task 4: 基础类型 JSON 支持 - 30 分钟
- Task 5: HomingTestData 序列化器 - 2 小时
- Task 6: InterpolationTestData 序列化器 - 2 小时
- Task 7: 更新 CMakeLists.txt - 30 分钟

**总计:** 约 7 小时

---

## 执行选项

**Plan complete and ready for execution.**

**Using executing-plans skill to implement this plan task-by-task.**
