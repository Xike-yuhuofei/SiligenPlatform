# 模块边界说明（仓库内模块化阶段）

## 背景

当前 `control-core` 已采用六边形架构组织代码，但项目边界仍然过宽：同一仓库中的 `src/domain`、`src/application`、`src/infrastructure`、`src/adapters` 同时承载了点胶工艺、运动控制、硬件适配、运行时调度以及 HMI/TCP 入口。

本轮模块化的目标不是立刻拆出独立仓库，而是在 `control-core` 内先建立稳定的能力边界，为后续渐进迁移提供落点。

目标模块如下：

- `modules/shared-kernel`
- `modules/process-core`
- `modules/motion-core`
- `modules/device-hal`
- `modules/control-gateway`
- `apps/control-tcp-server`
- `apps/control-runtime`

## 模块职责总览

### 1. shared-kernel

定位：最小公共内核，只容纳跨模块共享且没有业务倾向的基础能力。

包含：

- `Result<T>` / 错误基础模型
- 通用基础类型
- 小型、稳定、低业务语义的工具函数
- 时间、字符串、基础错误码等公共能力

不包含：

- 配方对象
- 点胶工艺对象
- 运动命令、轨迹对象
- TCP/JSON DTO
- 控制卡配置、设备配置、驱动包装

一句话：`shared-kernel` 解决“大家都要用的基础能力”，不解决“任何一个业务子域的专属问题”。

### 2. process-core

定位：点胶工艺执行核心，负责“做什么工艺、按什么顺序执行”。

包含：

- 配方与配方版本模型
- 点胶工艺模型与约束
- 工艺执行计划
- 点胶步骤编排
- 工艺执行状态流转
- 与工艺相关的校验、补偿、策略

不包含：

- 控制卡 SDK 细节
- TCP 协议对象
- Socket、线程、DLL 装载等基础设施细节
- 厂商错误码

一句话：`process-core` 负责定义“点胶工艺是什么、怎么执行”。

### 3. motion-core

定位：运动语义核心，负责“设备如何安全、可预测地运动”。

包含：

- 轴/坐标/位置/速度/加速度等值对象
- Jog / PTP / Homing / 插补等运动命令语义
- 轨迹与插补语义模型
- 运动状态机
- 安全、互锁、限位规则
- 运动监控所需的领域语义

不包含：

- 控制卡连接、复位、下载、运行流程的具体实现
- TCP/HMI 命令结构
- 配方业务规则
- 设备 SDK 头文件与底层调用细节

一句话：`motion-core` 负责定义“运动语义和规则”，不负责“如何调用某张控制卡”。

### 4. device-hal

定位：设备抽象与硬件适配层，负责把上层端口映射到具体硬件与运行时。

包含：

- 控制卡适配器
- 阀、供胶、IO、传感器适配器
- 设备连接、初始化、监控、健康检查
- 厂商错误码到领域错误的映射
- 运行时资源管理
- 真机 / Mock / Simulator 的适配实现

不包含：

- 配方规则
- 点胶工艺编排
- 运动领域语义定义
- TCP 协议处理

一句话：`device-hal` 负责回答“这台设备怎么驱动起来”。

### 5. control-gateway

定位：对外控制入口层，负责“外部请求如何进入内核”。

包含：

- TCP/JSON 协议解析与序列化
- 会话、连接、分发、错误映射
- 调用 `process-core` / `motion-core` 应用入口

不包含：

- 工艺规则
- 运动领域规则
- 控制卡驱动细节
- 线程化硬件轮询逻辑

一句话：`control-gateway` 负责“协议适配”，不负责“业务定义”。

### 6. apps

定位：最终可执行入口和装配层。

包含：

- `apps/control-tcp-server`
- `apps/control-runtime`

职责：

- 配置加载
- 容器装配
- target 组织
- 最终启动入口

不包含：

- 领域规则
- 硬件实现细节本身

## 当前 canonical 归属建议

### shared-kernel

- canonical 路径：`modules/shared-kernel`
- 主要承接无业务语义、无设备耦合的共享基础能力

### process-core

- canonical 路径：`modules/process-core`
- 主要承接配方、工艺规划、点胶执行语义
- 历史来源主要包括 recipes / dispensing / planning 相关的 domain 与 application 实现

### motion-core

- canonical 路径：`modules/motion-core`
- 主要承接运动语义、轨迹、安全与互锁规则
- 历史来源主要包括 motion / trajectory / safety 相关的 domain 与 application 实现

### device-hal

- canonical 路径：`modules/device-hal/src` 与 `modules/device-hal/vendor`
- 主要承接硬件适配器、厂商驱动源码、厂商 SDK 二进制和设备级错误映射
- 历史来源主要包括 infrastructure 下的 motion / dispensing / diagnostics / system 适配器以及 multicard 驱动相关实现

### control-gateway

- canonical 路径：`modules/control-gateway/src` 与 `apps/control-tcp-server`
- 主要承接 TCP/JSON 协议、会话、命令分发与 TCP 入口

## 迁移判定原则

在迁移某个类或目录前，先回答以下问题：

1. 它描述的是工艺规则，还是设备实现？
2. 它表达的是运动语义，还是控制卡调用细节？
3. 它是对外协议对象，还是内部领域对象？
4. 它是否可以在没有真实硬件、没有 TCP 的情况下独立测试？

若答案偏向：

- 工艺规则 -> `process-core`
- 运动语义 -> `motion-core`
- 硬件实现 -> `device-hal`
- 协议适配 -> `control-gateway`
- 纯基础能力 -> `shared-kernel`

## 当前最可能的高风险耦合点

以下区域在迁移时需要重点审查：

- `src/application/services/dxf`
  - 可能同时混有文件解析、几何处理、工艺规划和执行编排
- `src/application/usecases/dispensing/dxf`
  - 可能把工艺流程与底层执行行为绑得过紧
- `src/application/usecases/motion`
  - 可能直接感知设备控制器行为
  - 可能承担过多流程编排与交互状态控制
- `modules/control-gateway/src/tcp`
  - 可能混入业务对象映射和运行时逻辑
- `apps/control-runtime/runtime`
  - 容易与 motion / machine 的领域状态机混用

## 本轮模块化的非目标

本轮仅建立仓库内模块边界，不作为以下事项的落地阶段：

- 不拆出独立仓库
- 不一次性迁移所有旧代码
- 不重写控制卡集成
- 不大规模改动 TCP/HMI 对外契约
- 不做全仓命名空间重命名

## 阶段 0 验收标准

阶段 0 完成时，应满足：

- 每个目标模块都有明确的一句话职责
- 当前职责可以直接映射到 canonical 模块路径
- 存在明确的依赖禁止规则
- 能在不迁移代码的前提下为下一阶段创建骨架
