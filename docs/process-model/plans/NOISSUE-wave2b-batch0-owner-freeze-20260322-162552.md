# NOISSUE Wave 2B Batch 0 - Owner Freeze

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-162552`
- 输入：
  - `docs/process-model/plans/NOISSUE-wave2bC-design-20260322-155805.md`
  - `docs/architecture/device-hal-cutover.md`
  - `docs/architecture/third-party-ownership-cutover.md`

## 1. 冻结目标

冻结 `Wave 2B` 中 recipes/logging/third-party 三类剩余 debt 的最终 canonical owner，不使用过渡 owner。

## 2. 冻结结论

1. diagnostics logging canonical owner 冻结为 `packages/traceability-observability`。
2. recipes persistence / serializer canonical owner 冻结为 `packages/runtime-host`。
3. third-party live consumer ownership 冻结为“由真实消费者显式声明依赖”：
   - `packages/process-runtime-core` 负责其 Boost / Ruckig / Protobuf consumer debt
   - `packages/runtime-host` 负责 recipe/json 相关 consumer debt
   - `packages/transport-gateway` 作为协作 consumer，不再借 legacy owner 透传 include/link 语义
4. `packages/device-adapters` 只继续承担设备边界守门与 vendor 见证，不接收 recipes 或 logging 实现 owner。
5. `packages/device-contracts` 明确不是本轮 residual payoff owner。

## 3. 退出条件冻结

1. logging 退出条件：
   - `SpdlogLoggingAdapter.*` 不再位于 legacy `modules/device-hal`
   - runtime-host include 改到 canonical owner
2. recipes 退出条件：
   - `Recipe*` / `Template*` / `Audit*` / `ParameterSchema*` / `RecipeBundleSerializer*` 不再位于 legacy `modules/device-hal`
   - runtime-host / transport-gateway include 改到 canonical owner
3. third-party 退出条件：
   - 真实消费者不再依赖 `control-core/third_party` 作为临时 owner 语义
   - 不复制 vendor 源码，不引入第二份 third-party materialization

## 4. 后续实现约束

1. `Batch 1` 只能围绕上述最终 owner 建 gate 和可观测性，不能再把 owner 改为“runtime-host 或独立 package”这类开放表述。
2. `Batch 2` 的 bridge 和 `Batch 3` 的 payoff 必须严格按本文件 owner 施工。
3. 任何试图把 logging/recipes 重新塞回 `device-adapters` 或 `device-contracts` 的方案都视为违约。

## 5. 验收

1. 本批次是设计冻结，不涉及代码回滚。
2. 唯一验收标准是：后续实现者不需要再决定 recipes/logging/third-party 的最终 owner。
