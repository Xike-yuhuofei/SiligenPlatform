# NOISSUE Wave 4E Architecture Plan - External Intake to Observation Rerun Flow

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-215937`
- 关联 scope：`docs/process-model/plans/NOISSUE-wave4e-scope-20260322-215937.md`

## 1. 数据流 / 调用流

```text
external artifact (field scripts / release package / rollback package)
  -> operator records source path, timestamp, listing/hash
  -> wave4e-rerun/intake/<scope>.md
  -> operator runs scope-specific rg / command scan on real external path
  -> wave4e-rerun/observation/<scope>.md
  -> observation/summary.md + blockers.md
  -> Go / No-Go decision
```

## 2. 组件职责

1. `docs/runtime/external-migration-observation.md`
   - 定义统一的观察项、判定规则和阶段独立落盘约束。
2. `wave4d-closeout/intake/*.md`
   - 作为复用模板，固定采集字段与推荐命令。
3. `integration/reports/verify/wave4e-rerun/intake/`
   - 存放新的外部输入登记结果，不覆写历史模板或历史观察结果。
4. `integration/reports/verify/wave4e-rerun/observation/`
   - 存放对真实外部输入执行后的扫描结果、summary 和 blockers。

## 3. 失败模式与补救

1. 失败模式：新一轮证据继续写回 `wave4c-closeout` 或 `wave4d-closeout`
   - 补救：停止覆盖历史目录，迁移到 `wave4e-rerun/` 后再继续。
2. 失败模式：只登记仓内文档路径，没有真实外部输入路径
   - 补救：保持 `pending-input` 或 `input-gap`，不进入观察结论。
3. 失败模式：扫描命令误对仓库根执行，导致把 repo-side 结果当仓外结果
   - 补救：所有 scan command 必须显式指向 `source_path`。
4. 失败模式：把 release evidence 当成已部署包本体
   - 补救：在 intake 中区分“发布证据目录”和“真实发布包/解包根”，分别登记。

## 4. 发布与回滚边界

1. 本阶段不发布、不回滚，只准备证据落点和执行口径。
2. 真正的 `Go/No-Go` 仍以后续拿到的真实外部输入为准。
