# Infrastructure 层文档

## 概述

Infrastructure 层是 Siligen 点胶机控制系统的基础设施层，负责实现 Domain 层定义的端口接口。该层包含硬件适配器、配置管理、持久化、日志等外部系统集成。

## 架构原则

### 核心约束

| 约束项 | 说明 |
|--------|------|
| **依赖方向** | 依赖 Domain 层 |
| **实现原则** | 实现 Domain 层定义的端口接口 |
| **错误处理** | 转换硬件错误为 `Result<T>` |
| **线程安全** | 使用互斥锁保护硬件访问 |
| **资源管理** | 正确的初始化和清理 |
| **命名空间** | `Siligen::Infrastructure::*` |

## 目录结构

```
src/infrastructure/
├── adapters/               # 适配器实现
│   ├── hardware/           # 硬件适配器
│   ├── configuration/      # 配置适配器
│   ├── parsing/            # 解析适配器
│   ├── storage/            # 存储适配器
│   ├── visualization/      # 可视化适配器
│   ├── external/           # 外部服务适配器
│   ├── logging/            # 日志适配器
│   └── security/           # 安全适配器
├── config/                 # 配置管理组件
├── configs/                # 具体配置实现
├── factories/              # 工厂类
├── hardware/               # 硬件抽象层
├── logging/                # 日志系统
├── persistence/            # 持久化层
├── serialization/          # 序列化层 (Phase 1 & 2)
│   ├── JSONUtils.h         # JSON 工具函数
│   ├── EnumConverters.h    # 枚举转换器
│   ├── SerializationTypes.h # 序列化错误类型
│   ├── HomingTestDataSerializer.h/cpp      # 回零测试序列化器
│   ├── InterpolationTestDataSerializer.h/cpp # 插补测试序列化器
│   ├── JogTestDataSerializer.h/cpp         # 点动测试序列化器
│   ├── TriggerTestDataSerializer.h/cpp     # 触发测试序列化器
│   ├── CMPTestDataSerializer.h/cpp         # CMP 协同序列化器
│   └── DiagnosticResultSerializer.h/cpp    # 诊断结果序列化器
├── services/               # 基础设施服务
├── visualizers/            # 可视化组件
├── grpc/                   # gRPC 相关
└── security/               # 安全组件
```

## 硬件适配器

### HardwareTestAdapter

硬件测试适配器，实现 `IHardwareTestPort` 接口。

```cpp
namespace Siligen::Infrastructure::Adapters {

class HardwareTestAdapter : public Domain::Ports::IHardwareTestPort {
public:
    explicit HardwareTestAdapter(
        std::shared_ptr<IMultiCardWrapper> card_wrapper
    );

    // 连接管理
    Result<void> Connect(const std::string& config) override;
    Result<void> Disconnect() override;
    Result<bool> IsConnected() const override;

    // 点动测试
    Result<JogTestResult> JogTest(
        int axis,
        float velocity,
        float distance
    ) override;

    // 回零测试
    Result<HomingTestResult> HomingTest(int axis) override;

    // IO 测试
    Result<IOTestResult> IOTest(
        int channel,
        IOType type,
        bool value
    ) override;

    // 位置触发测试
    Result<TriggerTestResult> PositionTriggerTest(
        int axis,
        float trigger_position,
        float tolerance
    ) override;

    // CMP 测试
    Result<CMPTestResult> CMPTest(
        int axis,
        const CMPulseConfig& config
    ) override;

    // 插补测试
    Result<InterpolationTestResult> InterpolationTest(
        const std::vector<InterpolationPoint>& points
    ) override;

private:
    std::shared_ptr<IMultiCardWrapper> card_wrapper_;
    mutable std::mutex hardware_mutex_;
};

} // namespace Siligen::Infrastructure::Adapters
```

**特性：**
- 线程安全，使用互斥锁保护硬件访问
- 封装 MultiCard 硬件控制 API
- 支持点动、回零、I/O、位置触发、CMP 测试、插补测试

### MultiCardMotionAdapter

运动控制适配器，实现 `MotionControllerPortBase` 接口。

```cpp
namespace Siligen::Infrastructure::Adapters {

class MultiCardMotionAdapter : public Domain::Ports::MotionControllerPortBase {
public:
    MultiCardMotionAdapter(
        std::shared_ptr<IMultiCardWrapper> card_wrapper
    );

    // 轴控制
    Result<void> EnableAxis(int axisId) override;
    Result<void> DisableAxis(int axisId) override;
    Result<void> ClearPosition(int axisId) override;

    // 位置控制
    Result<void> SetVelocity(int axisId, float velocity) override;
    Result<void> SetAcceleration(int axisId, float accel) override;

    // 运动控制
    Result<void> MoveToPosition(int axisId, float position) override;
    Result<void> StopAxis(int axisId) override;
    Result<void> EmergencyStop() override;

    // 状态查询
    Result<double> GetCurrentPosition(int axisId) override;
    Result<AxisStatus> GetAxisStatus(int axisId) override;

private:
    // 单位转换 (mm <-> pulses)
    double pulsesTo_mm(int32_t pulses, int axisId);
    int32_t mmToPulses(double mm, int axisId);

    std::shared_ptr<IMultiCardWrapper> card_wrapper_;
    std::array<double, MAX_AXES> pulse_equivalents_;
};

} // namespace Siligen::Infrastructure::Adapters
```

**特性：**
- 处理轴位置控制、速度设置、状态监控、紧急停止
- 单位转换（mm <-> pulses）

### ValveAdapter

阀门控制适配器，实现 `IValvePort` 接口。

```cpp
namespace Siligen::Infrastructure::Adapters {

class ValveAdapter : public Domain::Ports::IValvePort {
public:
    ValveAdapter(
        std::shared_ptr<IMultiCardWrapper> card_wrapper
    );

    // 点胶阀控制
    Result<void> OpenDispenserValve() override;
    Result<void> CloseDispenserValve() override;
    Result<bool> GetDispenserValveState() override;

    // 供胶阀控制
    Result<void> OpenSupplyValve() override;
    Result<void> CloseSupplyValve() override;
    Result<bool> GetSupplyValveState() override;

    // CMP 触发控制
    Result<void> ConfigureCMPTrigger(
        const CMPTriggerConfig& config
    ) override;
    Result<void> EnableCMPTrigger(bool enable) override;

private:
    std::shared_ptr<IMultiCardWrapper> card_wrapper_;
    std::mutex valve_mutex_;
};

} // namespace Siligen::Infrastructure::Adapters
```

**特性：**
- 控制点胶阀和供胶阀
- 支持位置触发和脉冲控制
- 使用 CMP 触发 API

### HardwareConnectionAdapter

硬件连接适配器。

```cpp
namespace Siligen::Infrastructure::Adapters {

class HardwareConnectionAdapter :
    public Domain::Ports::IHardwareConnectionPort {

public:
    Result<void> Connect(const ConnectionConfig& config) override;
    Result<void> Disconnect() override;
    Result<bool> IsConnected() const override;
    Result<HardwareInfo> GetHardwareInfo() override;
};

} // namespace Siligen::Infrastructure::Adapters
```

## 配置适配器

### ConfigFileAdapter

INI 配置文件适配器，实现 `IConfigurationPort` 接口。

```cpp
namespace Siligen::Infrastructure::Adapters {

class ConfigFileAdapter : public Domain::Ports::IConfigurationPort {
public:
    explicit ConfigFileAdapter(const std::string& config_path);

    // 系统配置
    Result<SystemConfig> LoadSystemConfig() override;
    Result<void> SaveSystemConfig(const SystemConfig& config) override;

    // 点胶配置
    Result<DispensingConfig> LoadDispensingConfig() override;
    Result<void> SaveDispensingConfig(const DispensingConfig& config) override;

    // 机器配置
    Result<MachineConfig> LoadMachineConfig() override;
    Result<void> SaveMachineConfig(const MachineConfig& config) override;

    // 配置验证
    Result<bool> ValidateConfig(const std::string& config_path) override;

    // 备份和恢复
    Result<void> BackupConfig(const std::string& backup_path) override;
    Result<void> RestoreConfig(const std::string& backup_path) override;

private:
    std::string config_path_;
    std::mutex config_mutex_;
};

} // namespace Siligen::Infrastructure::Adapters
```

**特性：**
- 读写系统配置、点胶配置、机器配置
- 支持配置验证、备份和恢复
- 管理 DispenserValveConfig 和 SupplyValveConfig

## 解析适配器

### DXFFileParsingAdapter

DXF 文件解析适配器，实现 `IDXFFileParsingPort` 接口。

```cpp
namespace Siligen::Infrastructure::Adapters {

class DXFFileParsingAdapter : public Domain::Ports::IDXFFileParsingPort {
public:
    Result<DXFData> ParseFile(const std::string& file_path) override;
    Result<std::vector<Geometry>> ExtractGeometry(
        const DXFData& dxf_data
    ) override;
};

} // namespace Siligen::Infrastructure::Adapters
```

## 存储适配器

### LocalFileStorageAdapter

本地文件存储适配器。

```cpp
namespace Siligen::Infrastructure::Adapters {

class LocalFileStorageAdapter : public Domain::Ports::IFileStoragePort {
public:
    Result<void> SaveFile(
        const std::string& path,
        const std::vector<uint8_t>& data
    ) override;

    Result<std::vector<uint8_t>> LoadFile(
        const std::string& path
    ) override;

    Result<bool> FileExists(const std::string& path) override;
    Result<void> DeleteFile(const std::string& path) override;
};

} // namespace Siligen::Infrastructure::Adapters
```

## 硬件抽象层

### IMultiCardWrapper

MultiCard 包装器接口。

```cpp
namespace Siligen::Infrastructure::Hardware {

class IMultiCardWrapper {
public:
    virtual ~IMultiCardWrapper() = default;

    // 卡操作
    virtual Result<int> OpenCard(int card_id) = 0;
    virtual Result<void> CloseCard(int card_id) = 0;

    // 轴操作
    virtual Result<void> EnableAxis(int card, int axis) = 0;
    virtual Result<void> DisableAxis(int card, int axis) = 0;
    virtual Result<void> MoveAbsolute(int card, int axis, int32_t pulses) = 0;

    // IO 操作
    virtual Result<bool> ReadInput(int card, int channel) = 0;
    virtual Result<void> WriteOutput(int card, int channel, bool value) = 0;

    // CMP 操作
    virtual Result<void> ConfigureCMP(int card, int axis, const CMPConfig& config) = 0;
    virtual Result<void> EnableCMP(int card, int axis, bool enable) = 0;
};

} // namespace Siligen::Infrastructure::Hardware
```

### RealMultiCardWrapper

真实 MultiCard 实现。

### MockMultiCardWrapper

Mock MultiCard 实现，用于测试。

## 持久化层

### SQLiteTestRecordRepository

SQLite 测试记录仓储。

```cpp
namespace Siligen::Infrastructure::Persistence {

class SQLiteTestRecordRepository {
public:
    explicit SQLiteTestRecordRepository(const std::string& db_path);

    Result<void> Initialize();
    Result<void> SaveRecord(const TestRecord& record);
    Result<std::vector<TestRecord>> LoadRecords(
        const std::string& test_type,
        const TimeRange& time_range
    );
};

} // namespace Siligen::Infrastructure::Persistence
```

## 日志系统

### Logger

主日志记录器。

```cpp
namespace Siligen::Infrastructure::Logging {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

class Logger {
public:
    void Log(LogLevel level, const std::string& message);
    void LogInfo(const std::string& message);
    void LogError(const std::string& message);
    void LogCritical(const std::string& message);
};

} // namespace Siligen::Infrastructure::Logging
```

### ConsoleLogger

控制台日志器。

### UILogProvider

UI 日志提供者。

## 工厂模式

### InfrastructureAdapterFactory

基础设施工厂类。

```cpp
namespace Siligen::Infrastructure::Factories {

class InfrastructureAdapterFactory {
public:
    // 创建硬件适配器
    static std::shared_ptr<HardwareTestAdapter> CreateHardwareTestAdapter(
        bool use_mock = false
    );

    // 创建运动控制适配器
    static std::shared_ptr<MultiCardMotionAdapter> CreateMotionAdapter(
        bool use_mock = false
    );

    // 创建配置管理器
    static std::shared_ptr<ConfigFileAdapter> CreateConfigAdapter(
        const std::string& config_path
    );

    // 创建 MultiCard 包装器
    static std::shared_ptr<IMultiCardWrapper> CreateMultiCardWrapper(
        bool use_mock
    );
};

} // namespace Siligen::Infrastructure::Factories
```

**特性：**
- 创建所有基础设施层的对象实例
- 支持真实硬件和 Mock 测试两种模式
- 管理依赖注入，避免直接依赖

## 服务

### CMPTestPresetService

CMP 测试预设服务。

```cpp
namespace Siligen::Infrastructure::Services {

class CMPTestPresetService {
public:
    Result<void> SavePreset(const std::string& name, const CMPTestConfig& config);
    Result<CMPTestConfig> LoadPreset(const std::string& name);
    Result<std::vector<std::string>> ListPresets();
    Result<void> DeletePreset(const std::string& name);
};

} // namespace Siligen::Infrastructure::Services
```

## 可视化组件

### DXFVisualizer

DXF 可视化器。

### DispensingVisualizer

点胶可视化器。

## 序列化层

序列化层提供测试数据的 JSON 序列化/反序列化功能，支持数据持久化、网络传输和配置管理。

### 核心组件

#### JSONUtils.h

JSON 工具函数库，提供通用的 JSON 操作。

```cpp
namespace Siligen::Infrastructure::Serialization {

// JSON 解析和序列化
Result<json> Parse(const std::string& jsonStr);
Result<std::string> Dump(const json& j);

// 字段获取辅助函数
Result<int> GetIntField(const json& j, const std::string& fieldName);
Result<double> GetDoubleField(const json& j, const std::string& fieldName);
Result<bool> GetBoolField(const json& j, const std::string& fieldName);
Result<std::string> GetStringField(const json& j, const std::string& fieldName);

// 数组字段获取
Result<std::vector<T>> GetArrayField<T>(const json& j, const std::string& fieldName);

// 嵌套对象获取
Result<json> GetObjectField(const json& j, const std::string& fieldName);

} // namespace Siligen::Infrastructure::Serialization
```

**支持的类型** (Phase 2 扩展)：
- 基础类型: `int`, `double`, `bool`, `std::string`
- 复杂类型: `Point2D`, `Point3D`, `TriggerConfig`, `CMPTriggerPoint`
- 枚举类型: `TriggerAction`, `IssueSeverity`, `TestStatus`
- 嵌套数组: `std::vector<T>`, `std::vector<TriggerPoint>`

#### EnumConverters.h

枚举转换器，提供枚举类型与字符串之间的双向转换。

```cpp
namespace Siligen::Infrastructure::Serialization {

// TriggerAction 转换
std::string TriggerActionToString(TriggerAction action);
Result<TriggerAction> StringToTriggerAction(const std::string& str);

// IssueSeverity 转换
std::string IssueSeverityToString(IssueSeverity severity);
Result<IssueSeverity> StringToIssueSeverity(const std::string& str);

// TestStatus 转换
std::string TestStatusToString(TestStatus status);
Result<TestStatus> StringToTestStatus(const std::string& str);

} // namespace Siligen::Infrastructure::Serialization
```

#### SerializationTypes.h

序列化错误类型定义。

```cpp
namespace Siligen::Shared::Types {

enum class SerializationErrorCode {
    PARSE_FAILED = 1,           // JSON 解析失败
    MISSING_REQUIRED_FIELD = 2, // 缺少必需字段
    INVALID_TYPE = 3,           // 字段类型错误
    SERIALIZATION_FAILED = 4,   // 序列化失败
    INVALID_ENUM_VALUE = 5      // 无效的枚举值 (Phase 2)
};

// 错误创建函数
Error CreateJsonParseError(const std::string& message);
Error CreateSerializationError(SerializationErrorCode code, const std::string& message);
Error CreateMissingFieldError(const std::string& fieldName);
Error CreateInvalidTypeError(const std::string& fieldName, const std::string& expectedType);

} // namespace Siligen::Shared::Types
```

### 序列化器

所有序列化器遵循统一的 API 设计，支持单对象和批量操作。

#### JogTestDataSerializer

点动测试数据序列化器。

```cpp
namespace Siligen::Infrastructure::Serialization {

class JogTestDataSerializer {
public:
    // 单对象序列化
    Result<std::string> SerializeJogTestData(const JogTestData& data);

    // 单对象反序列化
    Result<JogTestData> DeserializeJogTestData(const std::string& jsonStr);

    // 批量序列化
    Result<std::string> SerializeJogTestDataBatch(const std::vector<JogTestData>& dataList);

    // 批量反序列化
    Result<std::vector<JogTestData>> DeserializeJogTestDataBatch(const std::string& jsonArrStr);
};

} // namespace Siligen::Infrastructure::Serialization
```

**JSON 格式示例**：

```json
{
    "axis_id": 1,
    "target_position": 100.5,
    "max_velocity": 500.0,
    "acceleration": 1000.0,
    "deceleration": 800.0,
    "jog_mode": "velocity"
}
```

#### TriggerTestDataSerializer

触发测试数据序列化器。

```cpp
class TriggerTestDataSerializer {
public:
    Result<std::string> SerializeTriggerTestData(const TriggerTestData& data);
    Result<TriggerTestData> DeserializeTriggerTestData(const std::string& jsonStr);
    Result<std::string> SerializeTriggerTestDataBatch(const std::vector<TriggerTestData>& dataList);
    Result<std::vector<TriggerTestData>> DeserializeTriggerTestDataBatch(const std::string& jsonArrStr);
};
```

**JSON 格式示例**：

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

#### CMPTestDataSerializer

CMP 协同测试数据序列化器。

```cpp
class CMPTestDataSerializer {
public:
    Result<std::string> SerializeCMPTestData(const CMPTestData& data);
    Result<CMPTestData> DeserializeCMPTestData(const std::string& jsonStr);
    Result<std::string> SerializeCMPTestDataBatch(const std::vector<CMPTestData>& dataList);
    Result<std::vector<CMPTestData>> DeserializeCMPTestDataBatch(const std::string& jsonArrStr);
};
```

**JSON 格式示例**：

```json
{
    "cmp_mode": "coordinated",
    "trigger_points": [
        {
            "x_position": 10.5,
            "y_position": 20.3,
            "z_position": 0.0,
            "output_mask": 1,
            "output_state": true
        }
    ],
    "output_config": {
        "polarity": "active_high",
        "initial_state": "all_off"
    }
}
```

#### DiagnosticResultSerializer

诊断结果数据序列化器。

```cpp
class DiagnosticResultSerializer {
public:
    Result<std::string> SerializeDiagnosticResult(const DiagnosticResult& data);
    Result<DiagnosticResult> DeserializeDiagnosticResult(const std::string& jsonStr);
    Result<std::string> SerializeDiagnosticResultBatch(const std::vector<DiagnosticResult>& dataList);
    Result<std::vector<DiagnosticResult>> DeserializeDiagnosticResultBatch(const std::string& jsonArrStr);
};
```

**JSON 格式示例**：

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

#### HomingTestDataSerializer (Phase 1)

回零测试数据序列化器。

```cpp
class HomingTestDataSerializer {
public:
    Result<std::string> SerializeHomingTestData(const HomingTestData& data);
    Result<HomingTestData> DeserializeHomingTestData(const std::string& jsonStr);
    Result<std::string> SerializeHomingTestDataBatch(const std::vector<HomingTestData>& dataList);
    Result<std::vector<HomingTestData>> DeserializeHomingTestDataBatch(const std::string& jsonArrStr);
};
```

#### InterpolationTestDataSerializer (Phase 1)

插补测试数据序列化器。

```cpp
class InterpolationTestDataSerializer {
public:
    Result<std::string> SerializeInterpolationTestData(const InterpolationTestData& data);
    Result<InterpolationTestData> DeserializeInterpolationTestData(const std::string& jsonStr);
    Result<std::string> SerializeInterpolationTestDataBatch(const std::vector<InterpolationTestData>& dataList);
    Result<std::vector<InterpolationTestData>> DeserializeInterpolationTestDataBatch(const std::string& jsonArrStr);
};
```

### 错误处理

所有序列化器使用 `Result<T>` 模式，**不会抛出异常**。

```cpp
auto result = serializer.SerializeJogTestData(data);

if (!result.IsSuccess()) {
    const Error& error = result.Error();

    std::cerr << "错误代码: " << static_cast<int>(error.Code()) << std::endl;
    std::cerr << "错误消息: " << error.Message() << std::endl;

    // 根据错误类型处理
    switch (error.Code()) {
        case ErrorCode::PARSE_FAILED:
            // 处理解析错误
            break;
        case ErrorCode::MISSING_REQUIRED_FIELD:
            // 处理缺失字段
            break;
        case ErrorCode::INVALID_TYPE:
            // 处理类型错误
            break;
        default:
            // 处理其他错误
    }
}
```

### 性能优化

#### 批量操作优先

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

#### JSON 格式选择

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

### 依赖关系

```
序列化层依赖关系：

JSONUtils.h
    ↓
EnumConverters.h
    ↓
SerializationTypes.h (shared/types/)
    ↓
具体序列化器
    ↓
Domain Models (domain/models/)
```

**依赖原则**：
- ✅ Infrastructure → Domain (正确方向)
- ✅ 使用 shared/types/Result<T> 错误处理
- ✅ 不依赖 Application 或 Adapters 层

### 使用示例

```cpp
#include "infrastructure/serialization/JogTestDataSerializer.h"

using namespace Siligen::Infrastructure::Serialization;

// 创建序列化器实例
JogTestDataSerializer serializer;

// 准备测试数据
JogTestData testData;
testData.axis_id = 1;
testData.target_position = 100.0f;

// 序列化
auto jsonResult = serializer.SerializeJogTestData(testData);
if (jsonResult.IsSuccess()) {
    std::cout << "JSON: " << jsonResult.Value() << std::endl;
}

// 反序列化
std::string jsonStr = R"({"axis_id": 1, "target_position": 100.0})";
auto dataResult = serializer.DeserializeJogTestData(jsonStr);
if (dataResult.IsSuccess()) {
    JogTestData data = dataResult.Value();
    std::cout << "轴 ID: " << data.axis_id << std::endl;
}
```

## 架构特点

1. **分层架构**：遵循六边形架构模式，领域层定义接口，基础设施层实现
2. **硬件抽象**：通过 `IMultiCardWrapper` 接口抽象硬件
3. **适配器模式**：每个外部系统都有对应的适配器
4. **工厂模式**：使用 `InfrastructureAdapterFactory` 统一创建对象
5. **配置管理**：分层配置系统，支持多种配置格式
6. **日志系统**：统一的日志接口，多种输出目标
