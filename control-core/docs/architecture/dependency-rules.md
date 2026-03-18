# 模块依赖规则（仓库内模块化阶段）

## 目标

在 `control-core` 内引入模块化时，优先约束依赖方向，避免“目录拆了、耦合还在”。

本文件定义模块之间的允许依赖、禁止依赖和检查清单，作为后续目录迁移与 CMake target 拆分的约束依据。

## 模块依赖总图

```text
apps
  -> control-gateway
  -> process-core
  -> motion-core
  -> shared-kernel

control-gateway
  -> process-core
  -> motion-core
  -> shared-kernel

process-core
  -> motion-core（仅限稳定运动语义或端口抽象，能少则少）
  -> shared-kernel

motion-core
  -> shared-kernel

device-hal
  -> process-core（仅在实现其端口时依赖抽象定义）
  -> motion-core（仅在实现其端口时依赖抽象定义）
  -> shared-kernel

shared-kernel
  -> 不依赖其他业务模块
```

## 允许依赖

### shared-kernel

允许依赖：

- C++ STL
- 与基础能力直接相关的第三方库

禁止依赖：

- `process-core`
- `motion-core`
- `device-hal`
- `control-gateway`
- `apps`

### motion-core

允许依赖：

- `shared-kernel`
- 必要的数学/算法类第三方库（需审查）

禁止依赖：

- `device-hal`
- `control-gateway`
- `apps`
- 任何具体控制卡 SDK
- TCP/HMI DTO

### process-core

允许依赖：

- `shared-kernel`
- `motion-core` 中稳定的运动语义模型或端口抽象

禁止依赖：

- `device-hal` 的具体实现
- `control-gateway`
- `apps`
- 控制卡 SDK
- TCP/HMI 协议对象

说明：

- `process-core` 可以依赖 `motion-core` 的稳定语义，但不应直接依赖某个运动适配器实现。
- 若某些工艺逻辑仅需“执行运动”能力，应通过端口抽象表达，而不是直接引用设备适配类。

### device-hal

允许依赖：

- `shared-kernel`
- `motion-core` 的端口与语义抽象
- `process-core` 的端口与语义抽象（仅在确有必要时）
- 厂商 SDK、操作系统 API、线程、IO、网络等基础设施能力

禁止依赖：

- `control-gateway`
- `apps`
- 配方编排逻辑
- 点胶工艺规则

说明：

- `device-hal` 是实现层，不是业务规则层。
- 一旦发现适配器中出现“根据配方内容决定业务流程”的逻辑，应回收至 `process-core`。

### control-gateway

允许依赖：

- `shared-kernel`
- `process-core`
- `motion-core`
- 少量用于序列化/网络接入的第三方库

禁止依赖：

- `device-hal` 的具体适配器与驱动实现
- 厂商 SDK
- 业务流程编排逻辑

说明：

- `control-gateway` 只负责 parse / call / serialize。
- 不允许在 handler 中写大段流程控制、轮询控制或硬件恢复逻辑。

### apps

允许依赖：

- `control-gateway`
- `process-core`
- `motion-core`
- `device-hal`
- `shared-kernel`

职责限制：

- 只负责装配、配置、启动
- 不承载领域规则

## 典型禁止关系

以下依赖关系视为架构违规：

- `process-core -> device-hal`
- `process-core -> control-gateway`
- `motion-core -> device-hal`
- `motion-core -> control-gateway`
- `control-gateway -> device-hal`
- `shared-kernel -> 任意业务模块`
- `device-hal -> control-gateway`

## 代码级判断规则

### 一个类属于 process-core，如果：

- 它表达配方、工艺步骤、工艺状态、工艺执行计划
- 它的测试不需要真实硬件
- 它不需要知道 TCP/HMI 协议格式

### 一个类属于 motion-core，如果：

- 它表达位置、速度、轨迹、插补、安全、互锁、状态机等运动语义
- 它不需要知道具体控制卡 API

### 一个类属于 device-hal，如果：

- 它封装控制卡、IO、阀、供胶、传感器等具体设备
- 它处理厂商错误码、连接、轮询、驱动初始化等问题

### 一个类属于 control-gateway，如果：

- 它面向用户输入或网络报文
- 它负责请求解析、参数校验、命令分发、结果序列化

### 一个类属于 shared-kernel，如果：

- 它不携带工艺、运动、设备、协议中的任一专属语义
- 它在多个模块中都成立，且不会使公共层膨胀

## CMake target 规则

在后续 target 拆分时，建议遵守：

- `siligen_shared_kernel`
- `siligen_motion_core`
- `siligen_process_core`
- `siligen_device_hal`
- `siligen_control_gateway`
- `siligen_control_tcp_server`
- `siligen_control_runtime`

链接方向建议：

- `siligen_motion_core` -> `siligen_shared_kernel`
- `siligen_process_core` -> `siligen_motion_core`, `siligen_shared_kernel`
- `siligen_device_hal` -> `siligen_process_core`, `siligen_motion_core`, `siligen_shared_kernel`
- `siligen_control_gateway` -> `siligen_process_core`, `siligen_motion_core`, `siligen_shared_kernel`
- `siligen_control_tcp_server` -> `siligen_control_gateway`, `siligen_device_hal`
- `siligen_control_runtime` -> 负责统一装配

注：`siligen_device_hal` 对 `siligen_process_core` 的依赖，应尽量收敛为仅依赖端口定义，不把流程实现倒灌到 HAL。

## 审查清单

每次迁移文件或新增依赖时，都应回答：

1. 这个改动是否让上层模块直接看见了底层实现？
2. 这个改动是否把协议对象带进了领域或应用内核？
3. 这个改动是否把厂商错误码或设备行为直接暴露给业务逻辑？
4. 这个改动是否让 `shared-kernel` 背上了业务语义？
5. 这个改动是否让 gateway/TCP/HMI 承担了本应属于 process 或 motion 的规则？

若任一答案为“是”，则应停止迁移并重新审查边界。

## 阶段 0 验收标准

阶段 0 完成时，至少应达到：

- 模块依赖方向书面化
- 典型禁止关系明确化
- 后续 CMake target 命名与链接方向可直接套用
- 迁移时能通过本文件快速判断“某段代码该去哪一层”
