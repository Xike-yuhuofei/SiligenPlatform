# HMI-TCP-Application 契约细化

## 目标

在既定冻结条件下，将系统对外交互收敛为以下稳定边界：

- HMI 是唯一人机交互入口
- TCP 是唯一外部程序化入口
- Application facade / 内部 service API 是 HMI 与 TCP 之间的稳定业务边界
- 不再恢复面向人的 CLI 客户端

本文件只细化边界，不改变既有总体方案。

## 冻结约束

- HMI 固定为 PyQt5 GUI 适配器
- HMI 不直接调用硬件适配器，不直连 `device-hal`
- TCP server 负责协议解析、路由、会话与错误映射
- Application facade 负责“用例编排入口”，不承载 JSON/TCP 细节
- 内部 service API 负责复用业务能力，不暴露为人机交互入口

## 分层职责

### HMI 层

职责：

- 呈现界面、采集操作
- 维护页面状态、表单状态、前端交互反馈
- 调用 TCP 请求
- 订阅并显示事件流

禁止：

- 直接拼装硬件命令
- 直接访问配置文件、配方文件、控制卡 SDK
- 在 GUI 内复制业务校验规则

### TCP Gateway 层

职责：

- 建立连接、会话与请求关联 ID
- 解析 JSON 请求
- 路由到对应 facade
- 把 `Result<T>`、领域错误、异常转换成稳定响应
- 推送事件与状态变更

禁止：

- 承载长流程业务编排
- 直接调用 `device-hal` 具体实现
- 在 handler 中写运动/点胶规则

### Application Facade 层

职责：

- 作为 TCP 与应用用例之间的稳定业务入口
- 将一个外部动作映射到一个或一组 use case
- 聚合跨 use case 的返回数据
- 保持参数与返回模型稳定

禁止：

- 关心 TCP 报文结构
- 关心 socket/session 生命周期
- 关心 GUI 页面状态

### Internal Service API 层

职责：

- 承载内部复用能力
- 为 use case、facade、runtime monitor 提供统一服务接口
- 封装跨模块复用逻辑

典型内容：

- 任务调度
- 运行状态查询
- 审计记录服务
- 配方导入导出服务
- DXF 预处理协调服务

## 推荐调用链

```text
HMI ViewModel / Controller
    -> TCP Client
        -> TCP Server / Gateway
            -> Tcp*Facade
                -> UseCase / Application Service
                    -> Domain Service / Port
                        -> device-hal / persistence / runtime adapters
```

设计原则：

- HMI 只知道“命令名 + 参数 + 响应”
- TCP 只知道“协议映射 + 路由”
- Facade 只知道“业务动作”
- UseCase 只知道“业务规则 + 端口”

## 稳定契约形态

### 1. 请求信封

统一请求结构建议保持为：

```json
{
  "id": "req-20260317-0001",
  "method": "motion.home",
  "params": {
    "axes": ["X", "Y"],
    "mode": "auto"
  }
}
```

约束：

- `id`：由 HMI 生成，用于请求/响应关联
- `method`：稳定动作名，不直接暴露 C++ 类名
- `params`：只传业务参数，不传 UI 内部状态

### 2. 响应信封

成功响应：

```json
{
  "id": "req-20260317-0001",
  "success": true,
  "result": {
    "accepted": true,
    "jobId": "motion-home-001"
  },
  "error": null
}
```

失败响应：

```json
{
  "id": "req-20260317-0001",
  "success": false,
  "result": null,
  "error": {
    "code": "AXIS_NOT_HOMED",
    "message": "Axis X is not homed",
    "details": {
      "axis": "X"
    }
  }
}
```

约束：

- `error.code` 使用稳定、可枚举的业务码
- `message` 给 HMI 直接展示
- `details` 用于页面级补充处理

### 3. 事件推送信封

```json
{
  "event": "motion.status.changed",
  "timestamp": "2026-03-17T14:30:00+08:00",
  "data": {
    "axis": "X",
    "state": "Idle",
    "position": 120.5
  }
}
```

适用事件：

- `system.state.changed`
- `motion.status.changed`
- `motion.alarm.raised`
- `dispensing.state.changed`
- `recipe.changed`
- `job.progress.changed`

## Method 命名规范

建议统一使用“业务域.动作”：

- `system.initialize`
- `system.emergency_stop`
- `motion.home`
- `motion.jog.start`
- `motion.jog.stop`
- `motion.move_to`
- `motion.status.get`
- `motion.position.get`
- `dispense.start`
- `dispense.stop`
- `dispense.pause`
- `recipe.list`
- `recipe.get`
- `recipe.create`
- `recipe.draft.create`
- `recipe.draft.update`
- `recipe.publish`
- `recipe.audit.list`
- `recipe.bundle.export`
- `recipe.bundle.import`
- `dxf.upload`
- `dxf.plan`
- `dxf.execute`

规则：

- 方法名稳定，不跟着内部类重命名
- 一个方法只表达一个清晰动作
- 查询与命令分离，避免一个方法兼具读取和修改

## Facade 拆分建议

结合当前代码，建议继续沿用并明确以下 facade：

- `TcpSystemFacade`
- `TcpMotionFacade`
- `TcpDispensingFacade`
- `TcpRecipeFacade`

建议补充或在内部服务层明确：

- `TcpDxfFacade` 或等价的 DXF facade/service
- `RuntimeStatusService`
- `JobTrackingService`
- `AuditQueryService`

拆分原则：

- 一个 facade 对应一个业务子域
- facade 可以编排多个 use case
- facade 不直接管理 socket

## HMI 侧接口适配建议

HMI 内建议按“页面服务”封装 TCP 调用，而不是在控件中直接发 JSON：

- `SystemClient`
- `MotionClient`
- `DispensingClient`
- `RecipeClient`
- `DxfClient`
- `EventSubscriptionClient`

每个 client 职责：

- 负责方法名映射
- 负责请求体构造
- 负责响应解码
- 负责把错误转换成 HMI 可展示对象

不要让 PyQt 页面直接拼接：

- method 字符串
- error code 判断
- 原始 JSON 字段路径

## 错误模型细化

建议把错误分为四层：

### 1. 协议错误

示例：

- `INVALID_JSON`
- `METHOD_NOT_FOUND`
- `INVALID_PARAMS`

由 TCP Gateway 负责返回。

### 2. 应用错误

示例：

- `SYSTEM_NOT_INITIALIZED`
- `AXIS_NOT_HOMED`
- `RECIPE_NOT_FOUND`
- `RECIPE_INVALID_STATE`

由 facade / use case 返回稳定业务码。

### 3. 设备错误

示例：

- `HARDWARE_DISCONNECTED`
- `SERVO_ALARM`
- `LIMIT_TRIGGERED`
- `MULTICARD_CALL_FAILED`

由 `device-hal` 向上翻译，不直接暴露厂商原始码给 HMI。

### 4. 基础设施错误

示例：

- `CONFIG_LOAD_FAILED`
- `FILE_NOT_FOUND`
- `TIMEOUT`

用于配置、存储、运行时资源等问题。

## 同步 / 异步边界

建议明确两类动作：

### 同步动作

特点：

- 短时间内可完成
- 响应直接给出最终结果

示例：

- `recipe.list`
- `recipe.get`
- `motion.position.get`

### 异步动作

特点：

- 会持续执行
- 首次响应只返回 `accepted` / `jobId`
- 后续通过事件流推送状态

示例：

- `system.initialize`
- `motion.home`
- `dxf.plan`
- `dxf.execute`

这样可以避免 HMI 卡死，也避免 TCP handler 内部阻塞过长。

## 会话与状态建议

TCP 会话建议只保留“连接级状态”，不保留“业务真状态”：

- 可保留：订阅列表、客户端标识、最近请求 ID
- 不可保留：当前配方、当前任务真状态、轴状态缓存真源

真实业务状态应统一来自：

- runtime service
- application service
- domain port / monitoring use case

这样可以避免“某个 TCP 会话断开后业务状态丢失”的问题。

## 推荐的最小正式契约

现阶段建议优先固化以下最小契约集：

### System

- `system.initialize`
- `system.shutdown`
- `system.emergency_stop`
- `system.status.get`

### Motion

- `motion.home`
- `motion.jog.start`
- `motion.jog.stop`
- `motion.move_to`
- `motion.status.get`
- `motion.position.get`

### Dispensing

- `dispense.start`
- `dispense.stop`
- `dispense.pause`
- `dispense.status.get`

### Recipe

- `recipe.list`
- `recipe.get`
- `recipe.create`
- `recipe.draft.create`
- `recipe.draft.update`
- `recipe.publish`
- `recipe.audit.list`
- `recipe.bundle.export`
- `recipe.bundle.import`

### DXF

- `dxf.upload`
- `dxf.plan`
- `dxf.execute`
- `dxf.status.get`

## 演进规则

后续新增接口时，必须满足：

1. 不新增第二个人机交互入口
2. 不绕过 facade 直接从 HMI 调用底层适配器
3. 不把业务规则塞回 TCP handler
4. 不把厂商 SDK 细节暴露到 HMI 协议
5. 新接口优先补充到既有子域 facade，而不是散落新增匿名 handler

## 当前推荐落地顺序

1. 先把现有 TCP `method -> facade -> use case` 映射整理成清单
2. 再统一成功/失败/事件三类 JSON 信封
3. 再补 HMI 侧 `*Client` 封装，消除页面直接拼 JSON
4. 最后补 job/event 契约，支撑长流程操作

一句话结论：

当前最稳的“设计细化”落点，不是继续扩入口，而是把 HMI、TCP、Facade、Service 之间的契约固定下来，让人机入口唯一、程序入口唯一、业务入口稳定可演进。

配套的 HMI 客户端分层与接口草案见：

- `docs/architecture/hmi-client-interface-draft.md`
