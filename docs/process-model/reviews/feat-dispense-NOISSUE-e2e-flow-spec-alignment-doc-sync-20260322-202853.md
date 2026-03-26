# Doc Sync Review

- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 时间戳：20260322-202853
- 范围：Wave 3C control-cli workspace entry tightening + engineering-data bridge normalization
- 上游收口：`docs/process-model/plans/NOISSUE-wave3b-closeout-20260322-200303.md`

## 1. 本次同步的文件

1. `apps/control-cli/README.md`
   - 明确 `dxf-preview` 默认脚本入口是 bridge，而不是 owner 脚本。
2. `docs/architecture/control-cli-cutover.md`
   - 补录 `Wave 3C` 对默认 config 解析与 preview 默认脚本入口的 current-fact。
3. `docs/architecture/dxf-pipeline-strangler-progress.md`
   - 把 preview canonical CLI 口径补齐到 bridge 稳定入口 + owner 维护入口的双层描述。
4. `packages/engineering-data/README.md`
   - 区分工作区桥接入口与 owner 入口。
5. `packages/engineering-data/docs/compatibility.md`
   - 明确工作区内默认入口收口到 bridge，但仓外兼容窗口不变。

## 2. 代码与测试写集

1. `apps/control-cli/CommandHandlersInternal.h`
2. `integration/scenarios/run_engineering_regression.py`
3. `integration/performance/collect_baselines.py`
4. `packages/engineering-data/README.md`
5. `packages/engineering-data/docs/compatibility.md`

## 3. 新增治理工件

1. `docs/process-model/plans/NOISSUE-wave3c-scope-20260322-202337.md`
2. `docs/process-model/plans/NOISSUE-wave3c-arch-plan-20260322-202337.md`
3. `docs/process-model/plans/NOISSUE-wave3c-test-plan-20260322-202337.md`
4. `docs/process-model/plans/NOISSUE-wave3c-closeout-20260322-202853.md`

## 4. 本次同步的原因

1. 如果不更新 current-fact 文档，代码已经默认走 bridge，但文档仍会把 owner 脚本误写成工作区默认入口。
2. `control-cli` 的默认 config 解析和 preview 默认脚本入口都已完成 Wave 3C 收口，README 与 architecture 文档必须同步。
3. `integration/reports/verify/wave3c-closeout/` 已形成验证证据，文档需要与实际验证终态一致。

## 5. 刻意未改的内容

1. 未改历史 scope / arch / closeout 工件，因为它们属于当时事实的归档证据。
2. 未把 `packages/engineering-data/scripts/*` 从文档中删除，因为这些入口仍是实现 owner 和维护入口。
3. 未把 `dxf-pipeline` 仓外兼容窗口判定为退出，因为本阶段只做工作区内入口收口。

## 6. 结论

1. 当前事实文档、README 与实现终态已对齐到同一结论：
   - bridge 是工作区稳定脚本入口
   - owner 脚本继续保留但不是 workspace 默认入口
   - `control-cli` 配置解析与 preview 默认脚本入口都已完成 Wave 3C 收口
2. 本轮文档同步足以支撑 `Wave 3C closeout` 和后续“仓外兼容窗口退出”独立规划。
