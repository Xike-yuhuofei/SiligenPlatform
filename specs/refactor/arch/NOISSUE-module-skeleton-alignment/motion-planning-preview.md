# Motion-Planning 统一骨架目标预览

更新时间：`2026-03-25`

## 1. 设计前提

`modules/motion-planning/` 的终态结构不能只是把现有 `src/motion/` 平移成更多目录，而应先按 `M7 motion-planning` 的 owner 边界净化内容。`M7` 的核心职责是运动规划与 `MotionPlan` 产出，不应长期保留运行态执行或设备控制职责。

## 2. 目标目录预览

```text
modules/motion-planning/
├─ README.md
├─ module.yaml
├─ contracts/
│  ├─ README.md
│  ├─ commands/
│  ├─ events/
│  └─ queries/
├─ domain/
│  ├─ value-objects/
│  ├─ algorithms/
│  └─ services/
├─ services/
│  ├─ PlanMotionService/
│  ├─ ValidateMotionConstraintsService/
│  └─ BuildTriggerTimelineService/
├─ application/
│  ├─ facades/
│  ├─ handlers/
│  └─ ports/
├─ adapters/
│  ├─ solver/
│  └─ serialization/
├─ tests/
│  ├─ unit/
│  ├─ contract/
│  ├─ integration/
│  └─ regression/
└─ examples/
```

## 3. 迁移判断原则

- 纯运动规划算法、值对象和规则进入 `domain/`。
- 模块级用例编排进入 `services/` 或 `application/`。
- 与设备控制、回零、手动控制、运行态会话强相关的内容不默认保留在 `M7`，需单独评审归属。
- 现有 `src/` 在迁移期作为 bridge 保留，终态应退出。
