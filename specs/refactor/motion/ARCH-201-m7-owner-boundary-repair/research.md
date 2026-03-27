# Research: M7 Owner Boundary Repair

## Decision 1: MotionPlanner owner 归位到 M7

- Decision: `MotionPlanner` canonical 实现迁移到 `modules/motion-planning/domain/motion/domain-services/`
- Rationale: 对齐 M7 owner 声明，消除“源码在 M6、构建在 M7”的结构错位
- Alternatives considered:
  - 维持现状，仅改 facade：不能解决 owner/构建错位
  - 新建中间模块：增加迁移复杂度，短期收益低

## Decision 2: 旧路径保留兼容包装（限时）

- Decision: `modules/process-path/domain/trajectory/domain-services/MotionPlanner.h/.cpp` 仅保留兼容层
- Rationale: 降低并行分支与旧 include 的一次性冲击
- Alternatives considered:
  - 立即删除旧路径：会触发多模块连锁编译失败，不利于阶段验收

## Decision 3: 边界门禁加固

- Decision: 扩展 `assert-module-boundary-bridges.ps1`，新增 M7 旧路径引用检测
- Rationale: 防止后续回退到 `process-path` 实现或 facade 旧 include
- Alternatives considered:
  - 仅靠 code review：不可自动化，容易漏检
