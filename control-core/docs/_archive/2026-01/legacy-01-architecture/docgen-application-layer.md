# Application 层文档

## 概述

Application 层是 Siligen 点胶机控制系统的应用逻辑层，负责编排业务流程。该层通过 Use Case 模式封装业务用例，协调领域服务完成业务目标。

## 架构原则

### 核心约束

| 约束项 | 说明 |
|--------|------|
| **依赖方向** | 依赖领域层，禁止依赖适配器实现 |
| **编排模式** | 用例编排，调用端口完成业务流程 |
| **请求响应** | Request/Response 模式 |
| **依赖注入** | 通过构造函数注入端口 |
| **命名空间** | `Siligen::Application::*` |
| **业务逻辑** | 禁止包含业务逻辑（下沉到领域层） |

### 用例实现规范

```cpp
class SomeUseCase {
public:
    explicit SomeUseCase(std::shared_ptr<IPort> port);

    Result<Response> Execute(const Request& req);
};
```

## 目录结构

```
src/application/
├── cli/                    # 命令行界面组件
├── container/              # 依赖注入容器
├── di/                     # 依赖注入框架
├── dispenser/              # 点胶机控制相关
├── DTOs/                   # 数据传输对象
├── parsers/                # 轨迹和DXF文件解析器
├── ports/                  # 应用层端口接口
├── services/               # 应用层服务
├── types/                  # 应用层类型定义
└── usecases/               # 业务用例
```

## 业务用例 (Use Cases)

### 系统控制用例

#### InitializeSystemUseCase

系统初始化用例，负责启动系统的初始化流程。

```cpp
namespace Siligen::Application::UseCases {

class InitializeSystemUseCase {
public:
    explicit InitializeSystemUseCase(
        std::shared_ptr<Domain::Ports::IHardwareConnectionPort> hw_port,
        std::shared_ptr<Domain::Ports::IConfigurationPort> config_port
    );

    Result<InitializeSystemResponse> Execute(
        const InitializeSystemRequest& request
    );
};

} // namespace Siligen::Application::UseCases
```

**业务流程：**
1. 验证配置文件
2. 建立硬件连接
3. 初始化轴参数
4. 加载点胶配置
5. 更新系统状态

#### EmergencyStopUseCase

紧急停止用例，关键安全用例，必须在 100ms 内完成。

```cpp
namespace Siligen::Application::UseCases {

enum class EmergencyStopReason {
    USER_REQUEST,       // 用户请求
    POSITION_ERROR,     // 位置误差过大
    HARDWARE_ERROR,     // 硬件错误
    SAFETY_VIOLATION,   // 安全违规
    TIMEOUT,            // 操作超时
    EXTERNAL_SIGNAL,    // 外部信号
    UNKNOWN             // 未知原因
};

struct EmergencyStopRequest {
    EmergencyStopReason reason;
    std::string detail_message;
    bool disable_hardware = true;
    bool clear_task_queue = true;
    bool disable_cmp = true;
    std::chrono::system_clock::time_point timestamp;
};

struct EmergencyStopResponse {
    bool motion_stopped;
    bool cmp_disabled;
    bool hardware_disabled;
    int32_t tasks_cleared;
    Point2D stop_position;
    std::chrono::milliseconds response_time;
    std::vector<std::string> actions_taken;
    std::string status_message;
};

class EmergencyStopUseCase {
public:
    explicit EmergencyStopUseCase(
        std::shared_ptr<Domain::Services::MotionService> motion_service,
        std::shared_ptr<Domain::Services::CMPService> cmp_service,
        std::shared_ptr<Domain::Models::DispenserModel> dispenser_model,
        std::shared_ptr<Shared::Interfaces::ILoggingService> logging_service
    );

    Result<EmergencyStopResponse> Execute(
        const EmergencyStopRequest& request
    );

    Result<bool> IsInEmergencyStop() const;
    Result<void> RecoverFromEmergencyStop();
};

} // namespace Siligen::Application::UseCases
```

**性能约束：**
- 响应时间: <100ms (关键安全要求)
- 所有操作串行执行，确保可靠性
- 错误不影响紧急停止的执行

**业务流程：**
1. 记录紧急停止请求和时间戳
2. 立即执行运动紧急停止
3. 禁用 CMP 触发输出
4. (可选) 清空任务队列
5. (可选) 禁用硬件
6. 更新点胶机状态为 EMERGENCY_STOP
7. 记录停止位置和响应时间

#### HomeAxesUseCase

轴回零用例，控制轴执行回零操作。

```cpp
namespace Siligen::Application::UseCases {

class HomeAxesUseCase {
public:
    Result<HomingResponse> Execute(const HomingRequest& request);
};

} // namespace Siligen::Application::UseCases
```

### 硬件控制用例

#### HardwareConnectionUseCase

硬件连接管理用例。

```cpp
namespace Siligen::Application::UseCases {

class HardwareConnectionUseCase {
public:
    Result<ConnectionResponse> Connect(const ConnectionRequest& request);
    Result<void> Disconnect();
    Result<ConnectionStatus> GetStatus();
};

} // namespace Siligen::Application::UseCases
```

### 点胶控制用例

| 用例 | 说明 |
|------|------|
| `StartDispenserUseCase` | 启动点胶 |
| `StopDispenserUseCase` | 停止点胶 |
| `PauseDispenserUseCase` | 暂停点胶 |
| `ResumeDispenserUseCase` | 恢复点胶 |
| `GetDispenserStatusUseCase` | 获取点胶状态 |

### 阀门控制用例

| 用例 | 说明 |
|------|------|
| `OpenSupplyValveUseCase` | 打开供给阀 |
| `CloseSupplyValveUseCase` | 关闭供给阀 |
| `GetSupplyStatusUseCase` | 获取供给状态 |

### 运动控制用例

| 用例 | 说明 |
|------|------|
| `ExecuteTrajectoryUseCase` | 执行轨迹 |
| `MoveToPositionUseCase` | 移动到位置 |
| `UIMotionControlUseCase` | UI 运动控制 |

### DXF 处理用例

| 用例 | 说明 |
|------|------|
| `DXFWebPlanningUseCase` | DXF 网络规划 |
| `DXFDispensingExecutionUseCase` | DXF 点胶执行 |
| `UploadDXFFileUseCase` | 上传 DXF 文件 |

### 测试验证用例

| 用例 | 说明 |
|------|------|
| `CMPTestUseCase` | CMP (点胶阀控制) 测试 |
| `DiagnosticTestUseCase` | 诊断测试 |
| `HomingTestUseCase` | 回零测试 |
| `InterpolationTestUseCase` | 插补测试 |
| `IOTestUseCase` | IO 测试 |
| `JogTestUseCase` | 点动测试 |
| `TriggerTestUseCase` | 触发测试 |

### 数据服务用例

#### DataQueryUseCase

数据查询用例，支持微信公众号文章数据查询。

```cpp
namespace Siligen::Application::UseCases {

class DataQueryUseCase {
public:
    // 异步查询
    Result<std::future<QueryResult>> QueryAsync(const QueryRequest& request);

    // 同步查询
    Result<QueryResult> QuerySync(const QueryRequest& request);

    // 批量查询
    Result<std::vector<QueryResult>> QueryBatch(
        const std::vector<QueryRequest>& requests
    );
};

} // namespace Siligen::Application::UseCases
```

## 应用服务 (Services)

### DataQueryService

数据查询服务，提供多数据源查询能力。

```cpp
namespace Siligen::Application::Services {

class DataQueryService {
public:
    Result<std::string> QueryArticle(const std::string& url);
    Result<std::vector<ArticleInfo>> QueryArticlesByKeyword(
        const std::string& keyword
    );
};

} // namespace Siligen::Application::Services
```

### BugRecorderService

错误记录服务。

## 依赖注入框架

### ServiceContainer

轻量级 DI 容器，支持服务注册和解析。

```cpp
namespace Siligen::Application::DI {

enum class ServiceLifetime {
    Singleton,   // 单例
    Transient    // 瞬态
};

class ServiceContainer {
public:
    // 注册服务
    template<typename TInterface, typename TImplementation>
    void RegisterService(ServiceLifetime lifetime = ServiceLifetime::Singleton);

    // 解析服务
    template<typename TInterface>
    std::shared_ptr<TInterface> ResolveService();

    // 检测循环依赖
    bool HasCircularDependency();
};

} // namespace Siligen::Application::DI
```

### ApplicationContainer

主依赖注入容器，管理所有用例和端口实例。

```cpp
namespace Siligen::Application::Container {

class ApplicationContainer {
public:
    // 注册所有服务
    void RegisterServices();

    // 创建用例实例
    template<typename TUseCase>
    std::shared_ptr<TUseCase> CreateUseCase();

    // 创建端口实例
    template<typename TPort>
    std::shared_ptr<TPort> CreatePort();
};

} // namespace Siligen::Application::Container
```

### ServiceLocator

服务定位器模式实现。

```cpp
namespace Siligen::Application::DI {

class ServiceLocator {
public:
    template<typename TInterface>
    static void Register(std::shared_ptr<TInterface> instance);

    template<typename TInterface>
    static std::shared_ptr<TInterface> Resolve();

    static void Reset();
};

} // namespace Siligen::Application::DI
```

## 应用层端口接口

| 端口接口 | 职责 |
|---------|------|
| `ICMPTestPresetPort` | CMP 测试预设端口 |
| `IConfigPort` | 配置管理端口 |
| `IHardwarePort` | 硬件操作端口 |
| `IHardwareTestPort` | 硬件测试端口 |
| `IMotionPort` | 运动控制端口 |
| `ITestConfigManager` | 测试配置管理端口 |
| `ITestRecordRepository` | 测试记录仓储端口 |

## 解析器

### DispensingTrajectoryGenerator（已移除）

旧版点胶轨迹生成器，已于 2026-01-31 移除，替换为新的点胶轨迹流水线（`DXFDispensingPlanner` + 轨迹分层流程）。

### DXFParser

DXF 文件解析器。

### DXFTrajectoryGenerator（已移除）

旧版 DXF 轨迹生成器，已于 2026-01-31 移除，替换为新的点胶轨迹流水线（`DXFDispensingPlanner` + 轨迹分层流程）。

## CLI 组件

### CommandLineParser

命令行参数解析器。

```cpp
namespace Siligen::Application::CLI {

class CommandLineParser {
public:
    struct ParsedArguments {
        std::string command;
        std::map<std::string, std::string> options;
        std::vector<std::string> positional_args;
    };

    Result<ParsedArguments> Parse(int argc, char* argv[]);
};

} // namespace Siligen::Application::CLI
```

### HelpPrinter

帮助信息打印器。

### MenuHandler

菜单处理器。

## 类型定义

### CMPTestTypes

CMP 测试相关类型。

### CMPTestPresetTypes

CMP 测试预设类型。

## 使用示例

### 创建用例

```cpp
// 创建紧急停止用例
auto emergencyStopUseCase = std::make_shared<EmergencyStopUseCase>(
    motionService,
    cmpService,
    dispenserModel,
    loggingService
);

// 执行用例
EmergencyStopRequest request;
request.reason = EmergencyStopReason::USER_REQUEST;
request.detail_message = "User pressed emergency stop button";

auto result = emergencyStopUseCase->Execute(request);
if (result.IsSuccess()) {
    auto response = result.GetValue();
    // 处理响应
}
```

### 请求验证

```cpp
struct SomeRequest {
    int axis_id;
    float position;

    bool Validate() const {
        return axis_id >= 0 && axis_id < MAX_AXES &&
               position >= MIN_POSITION && position <= MAX_POSITION;
    }
};
```

### 使用 DI 容器

```cpp
// 创建容器
auto container = std::make_shared<ApplicationContainer>();
container->RegisterServices();

// 解析用例
auto useCase = container->CreateUseCase<EmergencyStopUseCase>();

// 执行
auto result = useCase->Execute(request);
```

## 约束检查清单

- [ ] 禁止直接依赖适配器实现
- [ ] 禁止包含业务逻辑（下沉到领域层）
- [ ] 只依赖端口接口
- [ ] Request 包含 `Validate()` 方法
- [ ] 使用 `Result<T>` 返回值
- [ ] 通过构造函数注入依赖
