# 仓库内模块化落地图（阶段 1）

## 目标目录结构

```text
control-core/
├─ modules/
│  ├─ shared-kernel/
│  ├─ process-core/
│  ├─ motion-core/
│  ├─ device-hal/
│  └─ control-gateway/
├─ apps/
│  ├─ control-tcp-server/
│  └─ control-runtime/
├─ tests/
│  ├─ shared-kernel/
│  ├─ process-core/
│  ├─ motion-core/
│  ├─ device-hal/
│  ├─ control-gateway/
│  └─ integration/
└─ src/
   ├─ shared/
   ├─ domain/
   ├─ application/
   ├─ infrastructure/
   └─ adapters/
```

## 过渡期原则

当前采用“新骨架 + 旧实现并存”的过渡方式：

- `src/` 仍是现有可构建实现路径
- `modules/` 是未来模块落点
- `apps/` 是未来启动入口与装配落点
- `tests/` 中新增模块测试目录，仅用于后续模块化测试分层

## 阶段 1 已落地内容

- 已创建 `modules/` 及 5 个模块骨架
- 已创建 `apps/` 及运行时 / TCP 服务 app 骨架
- 已创建模块测试与集成测试目录骨架
- 顶层 `CMakeLists.txt` 已注册 `modules/` 与 `apps/`
- 每个模块已注册最小 target：
  - `siligen_shared_kernel`
  - `siligen_process_core`
  - `siligen_motion_core`
  - `siligen_device_hal`
  - `siligen_control_gateway`

## 当前约束

阶段 1 只建立结构，不执行以下操作：

- 不大规模迁移 `.cpp/.h`
- 不修改现有 TCP/HMI 对外契约
- 不替换现有 `src/*` 构建路径
- 不重写驱动与运行时逻辑

## 阶段 2 起点

从阶段 2 开始，优先围绕以下内容做最小迁移：

1. `shared-kernel`：从 `src/shared` 提炼稳定公共能力
2. `process-core`：收口配方、点胶工艺、执行编排入口
3. `motion-core`：建立运动语义与安全规则骨架
