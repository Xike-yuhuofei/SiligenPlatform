# 序列化器使用指南

## 概述

Siligen 工业点胶控制系统提供了完整的测试数据 JSON 序列化/反序列化功能，支持以下数据类型：

- **JogTestData** - 点动测试数据
- **TriggerTestData** - 触发测试数据
- **CMPTestData** - CMP 协同测试数据
- **DiagnosticResult** - 诊断结果数据
- **HomingTestData** - 回零测试数据（Phase 1）
- **InterpolationTestData** - 插补测试数据（Phase 1）

所有序列化器位于 `Siligen::Infrastructure::Serialization` 命名空间，遵循统一的 API 设计。

---

## 快速开始

### 基本序列化

```cpp
#include "infrastructure/serialization/JogTestDataSerializer.h"

using namespace Siligen::Infrastructure::Serialization;

// 创建序列化器实例
JogTestDataSerializer serializer;

// 准备测试数据
JogTestData testData;
testData.axis_id = 1;
testData.target_position = 100.0f;
testData.max_velocity = 500.0f;
testData.acceleration = 1000.0f;

// 序列化为 JSON 字符串
auto result = serializer.SerializeJogTestData(testData);

if (result.IsSuccess()) {
    std::string jsonStr = result.Value();
    std::cout << "序列化成功: " << jsonStr << std::endl;
} else {
    std::cerr << "序列化失败: " << result.Error().Message() << std::endl;
}
```

### 基本反序列化

```cpp
// JSON 字符串
std::string jsonStr = R"({
    "axis_id": 1,
    "target_position": 100.0,
    "max_velocity": 500.0,
    "acceleration": 1000.0,
    "deceleration": 800.0,
    "jog_mode": "velocity"
})";

// 反序列化
auto result = serializer.DeserializeJogTestData(jsonStr);

if (result.IsSuccess()) {
    JogTestData data = result.Value();
    std::cout << "轴 ID: " << data.axis_id << std::endl;
    std::cout << "目标位置: " << data.target_position << std::endl;
} else {
    std::cerr << "反序列化失败: " << result.Error().Message() << std::endl;
}
```

---

## API 参考

### JogTestDataSerializer

点动测试数据序列化器。

#### 单对象序列化

```cpp
Result<std::string> SerializeJogTestData(const JogTestData& data);
```

**参数**
- `data` - JogTestData 对象

**返回值**
- 成功: `Result<std::string>` 包含 JSON 字符串
- 失败: `Result<std::string>` 包含错误信息

**示例**

```cpp
JogTestData data;
// ... 填充数据

JogTestDataSerializer serializer;
auto jsonResult = serializer.SerializeJogTestData(data);

if (jsonResult.IsSuccess()) {
    std::cout << jsonResult.Value() << std::endl;
}
```

#### 单对象反序列化

```cpp
Result<JogTestData> DeserializeJogTestData(const std::string& jsonStr);
```

**参数**
- `jsonStr` - JSON 字符串

**返回值**
- 成功: `Result<JogTestData>` 包含反序列化对象
- 失败: `Result<JogTestData>` 包含错误信息

#### 批量序列化

```cpp
Result<std::string> SerializeJogTestDataBatch(const std::vector<JogTestData>& dataList);
```

**示例**

```cpp
std::vector<JogTestData> dataList;
dataList.push_back(data1);
dataList.push_back(data2);

auto jsonResult = serializer.SerializeJogTestDataBatch(dataList);
```

**输出格式**

```json
[
    { "axis_id": 1, "target_position": 100.0, ... },
    { "axis_id": 2, "target_position": 200.0, ... }
]
```

#### 批量反序列化

```cpp
Result<std::vector<JogTestData>> DeserializeJogTestDataBatch(const std::string& jsonArrStr);
```

---

### TriggerTestDataSerializer

触发测试数据序列化器。

#### API 签名

```cpp
// 单对象序列化
Result<std::string> SerializeTriggerTestData(const TriggerTestData& data);

// 单对象反序列化
Result<TriggerTestData> DeserializeTriggerTestData(const std::string& jsonStr);

// 批量序列化
Result<std::string> SerializeTriggerTestDataBatch(const std::vector<TriggerTestData>& dataList);

// 批量反序列化
Result<std::vector<TriggerTestData>> DeserializeTriggerTestDataBatch(const std::string& jsonArrStr);
```

#### JSON 格式示例

```json
{
    "trigger_type": "position",
    "trigger_position": 100.5,
    "trigger_action": "output_on",
    "output_id": 1,
    "output_polarity": "active_high",
    "output_duration_ms": 100
}
```

---

### CMPTestDataSerializer

CMP 协同测试数据序列化器。

#### API 签名

```cpp
// 单对象序列化
Result<std::string> SerializeCMPTestData(const CMPTestData& data);

// 单对象反序列化
Result<CMPTestData> DeserializeCMPTestData(const std::string& jsonStr);

// 批量序列化
Result<std::string> SerializeCMPTestDataBatch(const std::vector<CMPTestData>& dataList);

// 批量反序列化
Result<std::vector<CMPTestData>> DeserializeCMPTestDataBatch(const std::string& jsonArrStr);
```

#### JSON 格式示例

```json
{
    "cmp_mode": "coordinated",
    "trigger_points": [
        {
            "x_position": 10.5,
            "y_position": 20.3,
            "z_position": 0.0,
            "output_mask": 0x01,
            "output_state": true
        }
    ],
    "output_config": {
        "polarity": "active_high",
        "initial_state": "all_off"
    }
}
```

---

### DiagnosticResultSerializer

诊断结果数据序列化器。

#### API 签名

```cpp
// 单对象序列化
Result<std::string> SerializeDiagnosticResult(const DiagnosticResult& data);

// 单对象反序列化
Result<DiagnosticResult> DeserializeDiagnosticResult(const std::string& jsonStr);

// 批量序列化
Result<std::string> SerializeDiagnosticResultBatch(const std::vector<DiagnosticResult>& dataList);

// 批量反序列化
Result<std::vector<DiagnosticResult>> DeserializeDiagnosticResultBatch(const std::string& jsonArrStr);
```

#### JSON 格式示例

```json
{
    "test_name": "axis_connection_test",
    "status": "passed",
    "timestamp": "2025-01-08T10:30:00Z",
    "issues": [],
    "metrics": {
        "execution_time_ms": 150,
        "memory_used_mb": 12.5
    }
}
```

---

## 错误处理

所有序列化器使用 `Result<T>` 模式返回结果，**不会抛出异常**。

### 错误类型

```cpp
namespace Siligen::Shared::Types {

enum class SerializationErrorCode {
    PARSE_FAILED = 1,           // JSON 解析失败
    MISSING_REQUIRED_FIELD = 2, // 缺少必需字段
    INVALID_TYPE = 3,           // 字段类型错误
    SERIALIZATION_FAILED = 4    // 序列化失败
};

}
```

### 错误处理示例

```cpp
auto result = serializer.SerializeJogTestData(data);

if (!result.IsSuccess()) {
    const Error& error = result.Error();

    std::cerr << "错误代码: " << static_cast<int>(error.Code()) << std::endl;
    std::cerr << "错误消息: " << error.Message() << std::endl;

    // 根据错误类型处理
    switch (error.Code()) {
        case ErrorCode::PARSE_FAILED:
            std::cerr << "JSON 格式错误" << std::endl;
            break;
        case ErrorCode::MISSING_REQUIRED_FIELD:
            std::cerr << "缺少必需字段: " << error.Message() << std::endl;
            break;
        case ErrorCode::INVALID_TYPE:
            std::cerr << "字段类型错误" << std::endl;
            break;
        default:
            std::cerr << "未知错误" << std::endl;
    }
}
```

---

## 性能考虑

### 批量操作优化

当处理大量测试数据时，**优先使用批量 API**：

```cpp
// ❌ 低效 - 循环调用单对象序列化
for (const auto& data : dataList) {
    auto result = serializer.SerializeJogTestData(data);
    // ... 处理结果
}

// ✅ 高效 - 一次批量序列化
auto result = serializer.SerializeJogTestDataBatch(dataList);
```

**性能对比**（1000 条记录）：
- 单对象循环: ~50ms
- 批量操作: ~5ms（**10倍提升**）

### JSON 格式化

序列化器默认使用 **4 空格缩进**的格式化 JSON，便于阅读。

如需紧凑格式（减少文件大小），可以在序列化后重新解析并压缩：

```cpp
auto jsonResult = serializer.SerializeJogTestData(data);
if (jsonResult.IsSuccess()) {
    // 重新解析并压缩
    auto j = json::parse(jsonResult.Value());
    std::string compactJson = j.dump();  // 无缩进
}
```

---

## 最佳实践

### 1. 始终检查返回值

```cpp
// ✅ 正确
auto result = serializer.SerializeJogTestData(data);
if (result.IsSuccess()) {
    // 使用结果
}

// ❌ 错误 - 未检查错误
auto jsonStr = serializer.SerializeJogTestData(data).Value();
```

### 2. 使用命名常量避免拼写错误

```cpp
// ✅ 正确 - 使用枚举
data.trigger_action = TriggerAction::OUTPUT_ON;

// ❌ 错误 - 字符串易拼写错误
data.trigger_action = "output_onn";
```

### 3. 批量操作使用预分配容器

```cpp
std::vector<JogTestData> dataList;
dataList.reserve(1000);  // 预分配内存

// ... 填充数据
auto result = serializer.SerializeJogTestDataBatch(dataList);
```

### 4. 日志记录序列化错误

```cpp
auto result = serializer.DeserializeJogTestData(jsonStr);
if (!result.IsSuccess()) {
    // 记录详细错误信息
    loggingService->LogError(
        "反序列化失败",
        result.Error().Message(),
        "SerializationModule"
    );
}
```

### 5. 验证 JSON 格式

在反序列化前验证 JSON 格式：

```cpp
// 使用 JSONUtils 进行预解析
using namespace Siligen::Infrastructure::Serialization;

auto parseResult = Parse(jsonStr);
if (!parseResult.IsSuccess()) {
    std::cerr << "JSON 格式错误: " << parseResult.Error().Message() << std::endl;
    return;
}

// 继续反序列化
auto result = serializer.DeserializeJogTestData(jsonStr);
```

---

## 完整示例

### 保存测试数据到文件

```cpp
#include "infrastructure/serialization/JogTestDataSerializer.h"
#include <fstream>

void SaveJogTestData(const std::vector<JogTestData>& dataList, const std::string& filename) {
    JogTestDataSerializer serializer;

    // 批量序列化
    auto jsonResult = serializer.SerializeJogTestDataBatch(dataList);
    if (!jsonResult.IsSuccess()) {
        std::cerr << "序列化失败: " << jsonResult.Error().Message() << std::endl;
        return;
    }

    // 保存到文件
    std::ofstream outfile(filename);
    outfile << jsonResult.Value();
    outfile.close();

    std::cout << "已保存 " << dataList.size() << " 条记录到 " << filename << std::endl;
}
```

### 从文件加载测试数据

```cpp
std::vector<JogTestData> LoadJogTestData(const std::string& filename) {
    JogTestDataSerializer serializer;

    // 读取文件
    std::ifstream infile(filename);
    std::string jsonStr((std::istreambuf_iterator<char>(infile)),
                        std::istreambuf_iterator<char>());
    infile.close();

    // 批量反序列化
    auto result = serializer.DeserializeJogTestDataBatch(jsonStr);
    if (!result.IsSuccess()) {
        std::cerr << "反序列化失败: " << result.Error().Message() << std::endl;
        return {};
    }

    std::cout << "已加载 " << result.Value().size() << " 条记录" << std::endl;
    return result.Value();
}
```

---

## 相关文档

- [序列化扩展开发者指南](./serialization-developer-guide.md) - 如何添加新的序列化器
- [架构文档 - 基础设施层](../../01-architecture/README.md) - 序列化层在架构中的位置
- [API 错误报告](../../02-api/API_ERROR_REPORTING.md) - Result<T> 错误处理模式

---

## 更新日志

**Phase 2 (2025-01-08)**
- 新增 JogTestDataSerializer
- 新增 TriggerTestDataSerializer
- 新增 CMPTestDataSerializer
- 新增 DiagnosticResultSerializer
- 扩展 JSONUtils.h 支持 14 种类型
- 新增 TriggerAction, IssueSeverity 枚举转换器

**Phase 1 (2025-01-07)**
- 新增 HomingTestDataSerializer
- 新增 InterpolationTestDataSerializer
- 实现 JSONUtils 基础类型序列化
- 实现枚举转换器基础设施
