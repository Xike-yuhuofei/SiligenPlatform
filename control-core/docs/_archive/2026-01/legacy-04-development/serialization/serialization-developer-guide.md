# 序列化扩展开发者指南

本指南面向需要在 Siligen 系统中添加新序列化器的开发者。

---

## 目录

- [架构概述](#架构概述)
- [添加新序列化器的步骤](#添加新序列化器的步骤)
- [JSONUtils 扩展指南](#jsonutils-扩展指南)
- [枚举转换器添加](#枚举转换器添加)
- [测试最佳实践](#测试最佳实践)
- [性能优化建议](#性能优化建议)
- [常见问题](#常见问题)

---

## 架构概述

### 序列化层架构

```
┌─────────────────────────────────────────┐
│         Application Layer               │
│  (使用序列化器进行数据持久化)              │
└─────────────┬───────────────────────────┘
              │ 依赖
┌─────────────▼───────────────────────────┐
│      Infrastructure Layer               │
│  ┌─────────────────────────────────┐   │
│  │  Serialization Subsystem        │   │
│  │                                 │   │
│  │  ┌──────────────┐               │   │
│  │  │ JSONUtils.h  │               │   │
│  │  │ (通用工具)    │               │   │
│  │  └──────┬───────┘               │   │
│  │         │                        │   │
│  │  ┌──────▼───────┐               │   │
│  │  │EnumConverters│               │   │
│  │  │  (枚举转换)   │               │   │
│  │  └──────┬───────┘               │   │
│  │         │                        │   │
│  │  ┌──────▼──────────────────┐    │   │
│  │  │ Specific Serializers     │    │   │
│  │  │  • JogTestDataSerializer │    │   │
│  │  │  • TriggerTest...        │    │   │
│  │  │  • CMPTestDataSerializer │    │   │
│  │  │  • Diagnostic...         │    │   │
│  │  └──────────────────────────┘    │   │
│  └─────────────────────────────────┘   │
└─────────────┬───────────────────────────┘
              │ 依赖
┌─────────────▼───────────────────────────┐
│         Domain Layer                    │
│  (领域模型 - 被序列化的数据)               │
└─────────────────────────────────────────┘
```

### 设计原则

1. **统一 API 设计** - 所有序列化器遵循相同的接口模式
2. **Result<T> 错误处理** - 不抛出异常，使用 Result<T> 返回错误
3. **批量操作支持** - 提供单对象和批量两种 API
4. **类型安全** - 编译时类型检查，避免运行时错误
5. **依赖方向** - Infrastructure → Domain (正确方向)

---

## 添加新序列化器的步骤

### 步骤 1: 分析领域模型

首先分析要序列化的领域模型结构。

**示例：添加 ValveTestDataSerializer**

```cpp
// domain/models/TestDataTypes.h
struct ValveTestData {
    int valve_id;                    // 阀门 ID
    float open_time_ms;              // 开启时间 (毫秒)
    float close_time_ms;             // 关闭时间 (毫秒)
    ValveType valve_type;            // 阀门类型 (枚举)
    Point3D position;                // 阀门位置
    std::vector<TestResult> results; // 测试结果列表
};
```

**需要考虑的问题**：
- 有哪些基础类型字段？
- 有哪些枚举类型字段？
- 有哪些嵌套对象？
- 有哪些数组/容器字段？

### 步骤 2: 检查 JSONUtils.h 支持的类型

查看 `JSONUtils.h` 是否已支持所需类型的序列化。

```cpp
// src/infrastructure/serialization/JSONUtils.h

// 已支持的类型:
// - int, double, bool, std::string
// - Point2D, Point3D
// - std::vector<T> (泛型数组)

// 如果类型不存在，需要添加 to_json/from_json 函数
```

### 步骤 3: 添加枚举转换器（如果需要）

如果领域模型包含枚举类型，在 `EnumConverters.h` 中添加转换函数。

```cpp
// src/infrastructure/serialization/EnumConverters.h

namespace Siligen::Infrastructure::Serialization {

// ValveType 枚举转换器
inline std::string ValveTypeToString(ValveType type) {
    switch (type) {
        case ValveType::DISPENSING: return "dispensing";
        case ValveType::SUPPLY:     return "supply";
        case ValveType::DRAIN:      return "drain";
        default:                    return "unknown";
    }
}

inline Result<ValveType> StringToValveType(const std::string& str) {
    if (str == "dispensing") {
        return Result<ValveType>::Success(ValveType::DISPENSING);
    } else if (str == "supply") {
        return Result<ValveType>::Success(ValveType::SUPPLY);
    } else if (str == "drain") {
        return Result<ValveType>::Success(ValveType::DRAIN);
    } else {
        return Result<ValveType>::Failure(
            CreateSerializationError(
                ErrorCode::INVALID_ENUM_VALUE,
                "Unknown valve type: " + str
            )
        );
    }
}

} // namespace Siligen::Infrastructure::Serialization
```

### 步骤 4: 创建序列化器头文件

```cpp
// src/infrastructure/serialization/ValveTestDataSerializer.h
#pragma once

#include "shared/types/Result.h"
#include "domain/models/TestDataTypes.h"
#include <string>
#include <vector>

namespace Siligen {
namespace Infrastructure {
namespace Serialization {

using Siligen::Shared::Types::Result;

/**
 * @brief ValveTestData 序列化器
 *
 * 提供 ValveTestData 的 JSON 序列化/反序列化功能
 */
class ValveTestDataSerializer {
public:
    ValveTestDataSerializer() = default;
    ~ValveTestDataSerializer() = default;

    // 禁止拷贝
    ValveTestDataSerializer(const ValveTestDataSerializer&) = delete;
    ValveTestDataSerializer& operator=(const ValveTestDataSerializer&) = delete;

    // ========================================
    // 单对象序列化
    // ========================================

    /**
     * @brief 序列化 ValveTestData
     * @param data ValveTestData 对象
     * @return Result<std::string> JSON 字符串
     */
    Result<std::string> SerializeValveTestData(const ValveTestData& data);

    /**
     * @brief 反序列化 ValveTestData
     * @param jsonStr JSON 字符串
     * @return Result<ValveTestData> ValveTestData 对象
     */
    Result<ValveTestData> DeserializeValveTestData(const std::string& jsonStr);

    // ========================================
    // 批量序列化
    // ========================================

    /**
     * @brief 批量序列化 ValveTestData
     * @param dataList ValveTestData 对象列表
     * @return Result<std::string> JSON 数组字符串
     */
    Result<std::string> SerializeValveTestDataBatch(
        const std::vector<ValveTestData>& dataList
    );

    /**
     * @brief 批量反序列化 ValveTestData
     * @param jsonArrStr JSON 数组字符串
     * @return Result<std::vector<ValveTestData>> ValveTestData 对象列表
     */
    Result<std::vector<ValveTestData>> DeserializeValveTestDataBatch(
        const std::string& jsonArrStr
    );
};

} // namespace Serialization
} // namespace Infrastructure
} // namespace Siligen
```

### 步骤 5: 实现序列化器

```cpp
// src/infrastructure/serialization/ValveTestDataSerializer.cpp
#include "infrastructure/serialization/ValveTestDataSerializer.h"
#include "infrastructure/serialization/JSONUtils.h"
#include "infrastructure/serialization/EnumConverters.h"
#include "nlohmann/json.hpp"

namespace Siligen {
namespace Infrastructure {
namespace Serialization {

using json = nlohmann::json;

// ========================================
// 单对象序列化
// ========================================

Result<std::string> ValveTestDataSerializer::SerializeValveTestData(
    const ValveTestData& data
) {
    try {
        json j;

        // 基础类型字段
        j["valve_id"] = data.valve_id;
        j["open_time_ms"] = data.open_time_ms;
        j["close_time_ms"] = data.close_time_ms;

        // 枚举类型字段（使用转换器）
        j["valve_type"] = ValveTypeToString(data.valve_type);

        // 嵌套对象（Point3D 已在 JSONUtils.h 中支持）
        j["position"] = {
            {"x", data.position.x},
            {"y", data.position.y},
            {"z", data.position.z}
        };

        // 数组字段（假设 TestResult 有 to_json 支持）
        j["results"] = data.results;  // 自动调用 std::vector<TestResult> 的 to_json

        // 转换为格式化 JSON 字符串
        return Dump(j);

    } catch (const std::exception& e) {
        return Result<std::string>::Failure(
            CreateSerializationError(
                ErrorCode::SERIALIZATION_FAILED,
                std::string("Serialization failed: ") + e.what()
            )
        );
    }
}

Result<ValveTestData> ValveTestDataSerializer::DeserializeValveTestData(
    const std::string& jsonStr
) {
    // 1. 解析 JSON
    auto parseResult = Parse(jsonStr);
    if (!parseResult.IsSuccess()) {
        return Result<ValveTestData>::Failure(parseResult.Error());
    }
    const json& j = parseResult.Value();

    try {
        ValveTestData data;

        // 2. 基础类型字段（使用辅助函数）
        auto valveIdResult = GetIntField(j, "valve_id");
        if (!valveIdResult.IsSuccess()) {
            return Result<ValveTestData>::Failure(valveIdResult.Error());
        }
        data.valve_id = valveIdResult.Value();

        auto openTimeResult = GetDoubleField(j, "open_time_ms");
        if (!openTimeResult.IsSuccess()) {
            return Result<ValveTestData>::Failure(openTimeResult.Error());
        }
        data.open_time_ms = static_cast<float>(openTimeResult.Value());

        auto closeTimeResult = GetDoubleField(j, "close_time_ms");
        if (!closeTimeResult.IsSuccess()) {
            return Result<ValveTestData>::Failure(closeTimeResult.Error());
        }
        data.close_time_ms = static_cast<float>(closeTimeResult.Value());

        // 3. 枚举类型字段（使用转换器）
        if (j.contains("valve_type") && j["valve_type"].is_string()) {
            auto valveTypeResult = StringToValveType(j["valve_type"].get<std::string>());
            if (!valveTypeResult.IsSuccess()) {
                return Result<ValveTestData>::Failure(valveTypeResult.Error());
            }
            data.valve_type = valveTypeResult.Value();
        } else {
            return Result<ValveTestData>::Failure(
                CreateMissingFieldError("valve_type")
            );
        }

        // 4. 嵌套对象（Point3D）
        if (j.contains("position") && j["position"].is_object()) {
            const json& pos = j["position"];
            data.position.x = pos.value("x", 0.0);
            data.position.y = pos.value("y", 0.0);
            data.position.z = pos.value("z", 0.0);
        } else {
            return Result<ValveTestData>::Failure(
                CreateMissingFieldError("position")
            );
        }

        // 5. 数组字段
        if (j.contains("results") && j["results"].is_array()) {
            data.results = j["results"].get<std::vector<TestResult>>();
        }

        return Result<ValveTestData>::Success(data);

    } catch (const json::exception& e) {
        return Result<ValveTestData>::Failure(
            CreateSerializationError(
                ErrorCode::PARSE_FAILED,
                std::string("JSON parsing failed: ") + e.what()
            )
        );
    }
}

// ========================================
// 批量序列化
// ========================================

Result<std::string> ValveTestDataSerializer::SerializeValveTestDataBatch(
    const std::vector<ValveTestData>& dataList
) {
    try {
        json j = json::array();

        for (const auto& data : dataList) {
            auto result = SerializeValveTestData(data);
            if (!result.IsSuccess()) {
                return result;  // 任何一个失败，立即返回错误
            }

            auto parseResult = Parse(result.Value());
            if (!parseResult.IsSuccess()) {
                return Result<std::string>::Failure(parseResult.Error());
            }
            j.push_back(parseResult.Value());
        }

        return Dump(j);

    } catch (const std::exception& e) {
        return Result<std::string>::Failure(
            CreateSerializationError(
                ErrorCode::SERIALIZATION_FAILED,
                std::string("Batch serialization failed: ") + e.what()
            )
        );
    }
}

Result<std::vector<ValveTestData>> ValveTestDataSerializer::DeserializeValveTestDataBatch(
    const std::string& jsonArrStr
) {
    auto parseResult = Parse(jsonArrStr);
    if (!parseResult.IsSuccess()) {
        return Result<std::vector<ValveTestData>>::Failure(parseResult.Error());
    }
    const json& j = parseResult.Value();

    if (!j.is_array()) {
        return Result<std::vector<ValveTestData>>::Failure(
            CreateSerializationError(
                ErrorCode::INVALID_TYPE,
                "Expected JSON array"
            )
        );
    }

    std::vector<ValveTestData> dataList;
    dataList.reserve(j.size());  // 预分配内存

    for (const auto& item : j) {
        std::string itemStr = item.dump();
        auto result = DeserializeValveTestData(itemStr);
        if (!result.IsSuccess()) {
            return Result<std::vector<ValveTestData>>::Failure(result.Error());
        }
        dataList.push_back(result.Value());
    }

    return Result<std::vector<ValveTestData>>::Success(dataList);
}

} // namespace Serialization
} // namespace Infrastructure
} // namespace Siligen
```

### 步骤 6: 更新 CMakeLists.txt

```cmake
# src/infrastructure/CMakeLists.txt

add_library(siligen_serialization STATIC
    # ... 现有序列化器
    serialization/HomingTestDataSerializer.cpp
    serialization/InterpolationTestDataSerializer.cpp
    serialization/JogTestDataSerializer.cpp
    serialization/TriggerTestDataSerializer.cpp
    serialization/CMPTestDataSerializer.cpp
    serialization/DiagnosticResultSerializer.cpp
    # 新增序列化器
    serialization/ValveTestDataSerializer.cpp  # 添加此行
)
```

### 步骤 7: 编写单元测试

```cpp
// tests/unit/infrastructure/serialization/test_ValveTestDataSerializer.cpp
#include <gtest/gtest.h>
#include "infrastructure/serialization/ValveTestDataSerializer.h"

using namespace Siligen::Infrastructure::Serialization;

TEST(ValveTestDataSerializer, SerializeSuccess) {
    ValveTestDataSerializer serializer;

    ValveTestData data;
    data.valve_id = 1;
    data.open_time_ms = 100.0f;
    data.close_time_ms = 150.0f;
    data.valve_type = ValveType::DISPENSING;
    data.position = {10.0f, 20.0f, 30.0f};

    auto result = serializer.SerializeValveTestData(data);

    ASSERT_TRUE(result.IsSuccess());
    std::string jsonStr = result.Value();

    // 验证 JSON 包含关键字段
    EXPECT_NE(jsonStr.find("\"valve_id\": 1"), std::string::npos);
    EXPECT_NE(jsonStr.find("\"open_time_ms\": 100.0"), std::string::npos);
    EXPECT_NE(jsonStr.find("\"valve_type\": \"dispensing\""), std::string::npos);
}

TEST(ValveTestDataSerializer, DeserializeSuccess) {
    ValveTestDataSerializer serializer;

    std::string jsonStr = R"({
        "valve_id": 1,
        "open_time_ms": 100.0,
        "close_time_ms": 150.0,
        "valve_type": "dispensing",
        "position": {"x": 10.0, "y": 20.0, "z": 30.0},
        "results": []
    })";

    auto result = serializer.DeserializeValveTestData(jsonStr);

    ASSERT_TRUE(result.IsSuccess());
    ValveTestData data = result.Value();

    EXPECT_EQ(data.valve_id, 1);
    EXPECT_FLOAT_EQ(data.open_time_ms, 100.0f);
    EXPECT_FLOAT_EQ(data.close_time_ms, 150.0f);
    EXPECT_EQ(data.valve_type, ValveType::DISPENSING);
    EXPECT_FLOAT_EQ(data.position.x, 10.0f);
    EXPECT_FLOAT_EQ(data.position.y, 20.0f);
    EXPECT_FLOAT_EQ(data.position.z, 30.0f);
}

TEST(ValveTestDataSerializer, DeserializeMissingField) {
    ValveTestDataSerializer serializer;

    std::string jsonStr = R"({
        "valve_id": 1,
        "open_time_ms": 100.0
        // 缺少 close_time_ms
    })";

    auto result = serializer.DeserializeValveTestData(jsonStr);

    ASSERT_FALSE(result.IsSuccess());
    EXPECT_EQ(result.Error().Code(), ErrorCode::MISSING_REQUIRED_FIELD);
}

TEST(ValveTestDataSerializer, BatchSerialize) {
    ValveTestDataSerializer serializer;

    std::vector<ValveTestData> dataList;
    ValveTestData data1, data2;
    data1.valve_id = 1;
    data2.valve_id = 2;
    dataList.push_back(data1);
    dataList.push_back(data2);

    auto result = serializer.SerializeValveTestDataBatch(dataList);

    ASSERT_TRUE(result.IsSuccess());
    std::string jsonStr = result.Value();

    // 验证是 JSON 数组
    EXPECT_EQ(jsonStr[0], '[');
    EXPECT_EQ(jsonStr[jsonStr.length() - 1], ']');
}

TEST(ValveTestDataSerializer, RoundTrip) {
    ValveTestDataSerializer serializer;

    ValveTestData original;
    original.valve_id = 5;
    original.open_time_ms = 200.0f;
    original.close_time_ms = 250.0f;
    original.valve_type = ValveType::SUPPLY;
    original.position = {15.0f, 25.0f, 35.0f};

    // 序列化
    auto serializeResult = serializer.SerializeValveTestData(original);
    ASSERT_TRUE(serializeResult.IsSuccess());

    // 反序列化
    auto deserializeResult = serializer.DeserializeValveTestData(serializeResult.Value());
    ASSERT_TRUE(deserializeResult.IsSuccess());

    ValveTestData restored = deserializeResult.Value();

    // 验证数据一致性
    EXPECT_EQ(restored.valve_id, original.valve_id);
    EXPECT_FLOAT_EQ(restored.open_time_ms, original.open_time_ms);
    EXPECT_FLOAT_EQ(restored.close_time_ms, original.close_time_ms);
    EXPECT_EQ(restored.valve_type, original.valve_type);
    EXPECT_FLOAT_EQ(restored.position.x, original.position.x);
}
```

### 步骤 8: 更新测试 CMakeLists.txt

```cmake
# tests/CMakeLists.txt

# ValveTestData 序列化器测试 (新增)
add_serialization_test(test_ValveTestDataSerializer "unit/infrastructure/serialization/test_ValveTestDataSerializer.cpp")
```

---

## JSONUtils 扩展指南

### 添加新类型的 to_json/from_json 支持

如果要序列化的类型不在 `JSONUtils.h` 的支持列表中，需要添加 `to_json` 和 `from_json` 函数。

**示例：添加 Rect 类型支持**

```cpp
// src/infrastructure/serialization/JSONUtils.h

// 假设 Rect 定义在 domain/models/GeometryTypes.h
// struct Rect { float x, y, width, height; };

namespace Siligen {
namespace Infrastructure {
namespace Serialization {

// 添加 Rect 类型支持
inline void to_json(json& j, const Rect& rect) {
    j = json{
        {"x", rect.x},
        {"y", rect.y},
        {"width", rect.width},
        {"height", rect.height}
    };
}

inline void from_json(const json& j, Rect& rect) {
    // 使用辅助函数进行错误处理
    auto xResult = GetDoubleField(j, "x");
    if (xResult.IsSuccess()) rect.x = static_cast<float>(xResult.Value());

    auto yResult = GetDoubleField(j, "y");
    if (yResult.IsSuccess()) rect.y = static_cast<float>(yResult.Value());

    auto widthResult = GetDoubleField(j, "width");
    if (widthResult.IsSuccess()) rect.width = static_cast<float>(widthResult.Value());

    auto heightResult = GetDoubleField(j, "height");
    if (heightResult.IsSuccess()) rect.height = static_cast<float>(heightResult.Value());
}

} // namespace Serialization
} // namespace Infrastructure
} // namespace Siligen
```

**添加后，Rect 类型可以直接使用**：

```cpp
Rect rect = {10.0f, 20.0f, 100.0f, 50.0f};
json j = rect;  // 自动调用 to_json
std::string jsonStr = j.dump();
```

---

## 测试最佳实践

### 1. 测试覆盖完整性

确保覆盖以下场景：

- ✅ 正常序列化
- ✅ 正常反序列化
- ✅ 缺少必需字段
- ✅ 字段类型错误
- ✅ 无效枚举值
- ✅ 批量操作
- ✅ Round-trip（序列化后再反序列化，验证数据一致性）

### 2. 使用测试夹具

```cpp
class ValveTestDataSerializerTest : public ::testing::Test {
protected:
    ValveTestDataSerializer serializer_;

    // 辅助函数：创建标准测试数据
    ValveTestData CreateStandardData() {
        ValveTestData data;
        data.valve_id = 1;
        data.open_time_ms = 100.0f;
        data.close_time_ms = 150.0f;
        data.valve_type = ValveType::DISPENSING;
        data.position = {10.0f, 20.0f, 30.0f};
        return data;
    }
};

TEST_F(ValveTestDataSerializerTest, SerializeStandardData) {
    auto data = CreateStandardData();
    auto result = serializer_.SerializeValveTestData(data);
    ASSERT_TRUE(result.IsSuccess());
}
```

### 3. 参数化测试

```cpp
// 测试不同的枚举值
class ValveTypeSerializationTest : public ValveTestDataSerializerTest,
                                    public ::testing::WithParamInterface<ValveType> {
};

TEST_P(ValveTypeSerializationTest, SerializeAllValveTypes) {
    auto data = CreateStandardData();
    data.valve_type = GetParam();

    auto result = serializer_.SerializeValveTestData(data);
    ASSERT_TRUE(result.IsSuccess());

    auto restoredResult = serializer_.DeserializeValveTestData(result.Value());
    ASSERT_TRUE(restoredResult.IsSuccess());
    EXPECT_EQ(restoredResult.Value().valve_type, GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    AllValveTypes,
    ValveTypeSerializationTest,
    ::testing::Values(
        ValveType::DISPENSING,
        ValveType::SUPPLY,
        ValveType::DRAIN
    )
);
```

---

## 性能优化建议

### 1. 批量操作使用预分配

```cpp
// ✅ 高效 - 预分配内存
std::vector<ValveTestData> dataList;
dataList.reserve(1000);  // 预分配

// ❌ 低效 - 多次重新分配
std::vector<ValveTestData> dataList;
// 每次push_back可能触发重新分配
```

### 2. 避免不必要的 JSON 解析

```cpp
// ❌ 低效 - 序列化后立即反序列化（双重转换）
auto jsonResult = serializer.SerializeValveTestData(data);
auto j = json::parse(jsonResult.Value());  // 不必要的解析
std::string final = j.dump();

// ✅ 高效 - 直接使用序列化结果
auto jsonResult = serializer.SerializeValveTestData(data);
std::string final = jsonResult.Value();
```

### 3. 使用移动语义

```cpp
// ✅ 高效 - 使用移动语义避免拷贝
Result<std::string> Serialize(const MyData& data) {
    json j;
    j["field"] = data.field;
    return Dump(j);  // 返回 Result<std::string>，内部使用移动
}
```

---

## 常见问题

### Q1: 编译错误 "undefined reference to to_json"

**原因**: `to_json/from_json` 函数未正确定义或链接。

**解决方案**:
1. 确保在 `JSONUtils.h` 中定义了 `to_json/from_json`
2. 确保在命名空间 `Siligen::Infrastructure::Serialization` 中定义
3. 检查 CMakeLists.txt 是否正确链接了序列化库

### Q2: 反序列化时字段类型错误

**原因**: JSON 字段类型与 C++ 类型不匹配。

**解决方案**:
```cpp
// 使用类型转换辅助函数
auto doubleResult = GetDoubleField(j, "some_int_field");
if (doubleResult.IsSuccess()) {
    int intValue = static_cast<int>(doubleResult.Value());
}
```

### Q3: 枚举值字符串大小写敏感

**原因**: 枚举转换器通常对大小写敏感。

**解决方案**:
```cpp
// 在转换器中添加大小写不敏感支持
inline Result<ValveType> StringToValveType(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);

    if (lowerStr == "dispensing") { /* ... */ }
    // ...
}
```

### Q4: 批量操作内存占用过高

**原因**: 一次性加载大量数据到内存。

**解决方案**:
```cpp
// 分批处理
constexpr size_t BATCH_SIZE = 100;

for (size_t i = 0; i < largeDataList.size(); i += BATCH_SIZE) {
    size_t end = std::min(i + BATCH_SIZE, largeDataList.size());
    std::vector<MyData> batch(largeDataList.begin() + i,
                              largeDataList.begin() + end);

    auto result = serializer.SerializeMyDataBatch(batch);
    // 处理结果
}
```

### Q5: JSON 格式化缩进不符合预期

**原因**: `Dump()` 函数默认使用 4 空格缩进。

**解决方案**:
```cpp
// 修改缩进（在 JSONUtils.h 中）
inline Result<std::string> Dump(const json& j, int indent = 4) {
    try {
        return Result<std::string>::Success(j.dump(indent));
    } catch (const json::type_error& e) {
        // ...
    }
}

// 使用时
auto result = Dump(j, 2);  // 使用 2 空格缩进
```

---

## 相关文档

- [序列化器使用指南](./serialization-user-guide.md) - 如何使用序列化器
- [架构文档 - 基础设施层](../../01-architecture/docgen-infrastructure-layer.md) - 序列化层在架构中的位置
- [API 错误报告](../../02-api/API_ERROR_REPORTING.md) - Result<T> 错误处理模式
