# NOISSUE Wave 2B Readiness 判定

- 状态：No-Go
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-144819`
- 关联证据：`integration/reports/verify/wave2b-readiness/static-evidence.md`

## 1. 判定目标

本文件只回答一个问题：当前工作树是否可以直接进入 `Wave 2B` 的物理切换实施。

## 2. 已确认事实

1. `Wave 2A` 已收口通过，离线工程链默认入口不再直接写死 `packages/engineering-data`
2. 根级 build/test/source-root 仍以 `packages/process-runtime-core`、`packages/runtime-host`、`packages/transport-gateway` 为 canonical root
3. runtime 默认资产路径仍是 `config/machine/machine_config.ini`、`data/recipes`、`data/schemas/recipes`
4. `process-runtime-core`、`device-adapters`、`device-contracts` 仍存在与 legacy `device-hal` / `third_party` 相关的 residual dependency
5. `integration/reports` 仍是唯一统一证据根，任何切换都必须保持该根不分裂

## 3. 判定结果

当前结论：`Wave 2B Readiness = No-Go`

原因固定为：

1. build/test 图还没有桥接或替代方案
2. runtime 默认资产路径还没有桥接或替代方案
3. residual dependency 还没有逐项清零或替代 owner
4. 现有证据链已经证明问题所在，但还没有证明切换后的回滚单元和门禁映射

## 4. 当前允许的下一步

1. 拆 `Wave 2B-A`：root CMake / tests / build / test / CI source-root cutover 设计
2. 拆 `Wave 2B-B`：runtime 默认资产路径 bridge 设计
3. 拆 `Wave 2B-C`：`control-core` residual dependency 与 `device-hal` / `third_party` 迁移账本
4. 拆 `Wave 2B-D`：把未来切换动作映射到根级 gate、回滚单元和报告路径

## 5. 明确禁止

1. 禁止直接修改根级 `CMakeLists.txt` 以翻转 canonical source root
2. 禁止直接改写 `config/machine/machine_config.ini`、`data/recipes`、`data/schemas/recipes` 的默认发现链
3. 禁止在没有替代证据根的情况下拆分 `integration/reports`
4. 禁止把 `Wave 2A` 已通过误解为 `Wave 2B` 可实施
