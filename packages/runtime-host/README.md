# runtime-host

目标定位：运行时宿主、容器装配、配置加载、生命周期、资源、安全初始化和后台任务。

## owner 边界

- `packages/runtime-host` 承接宿主和装配能力。
- `packages/process-runtime-core` 承接业务内核规则与应用编排。
- `apps/control-runtime` 继续作为 runtime 兼容入口壳。

## 当前已收口到本 package 的源码区域

- `src/ContainerBootstrap.*`
- `src/bootstrap/*`
- `src/container/*`
- `src/runtime/configuration/*`
- `src/runtime/events/*`
- `src/runtime/scheduling/*`
- `src/runtime/storage/files/*`
- `src/security/*`
- `src/services/motion/*LimitMonitorService*`

## 依赖规则

- 依赖业务内核时，优先 link `siligen_process_runtime_core*` 公开 target。
- 允许继续通过 `control-core/src` 获取 `shared/*` 兼容头。
- 不得新增对 `control-core/src/domain`、`control-core/src/application`、`control-core/modules/process-core`、`control-core/modules/motion-core` 的直接 include 或 link。
- 不把宿主线程、后台任务、配置装配、TCP 协议或设备实现重新卷回 `process-runtime-core`。

## 当前不放入本 package 的内容

- `InitializeSystemUseCase` / `EmergencyStopUseCase`
- 任何配方、运动、点胶、规划等业务规则
- TCP 协议 facade 与会话处理
