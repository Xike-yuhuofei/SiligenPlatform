# NOISSUE Wave 4C Scope - External Migration Observation Execution

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-212031`
- 上游收口：`docs/process-model/plans/NOISSUE-wave4b-closeout-20260322-210606.md`

## 1. 阶段目标

1. 实际执行仓外迁移观察期，而不再停留在 repo-side 准备阶段。
2. 对外部 DXF launcher、现场脚本、发布包、回滚包四类对象生成 authoritative 观察记录。
3. 根据实际证据输出 `Go / No-Go / input-gap` 裁决，并决定是否允许进入下一波次规划。

## 2. in-scope

1. `docs/runtime/external-migration-observation.md`
2. `docs/process-model/plans/`、`docs/process-model/reviews/` 中本阶段新增工件
3. `integration/reports/verify/wave4c-closeout/observation/` 下的四类观察记录、汇总与阻塞清单
4. `integration/reports/verify/wave4c-closeout/legacy-exit-pre/` 与 `.../legacy-exit-post/` 门禁复核

## 3. out-of-scope

1. 恢复任何 compat shell、legacy CLI alias 或 root config alias
2. 新的产品代码改动
3. 新的全量 build/test 回归
4. 对缺失的仓外交付物做推定通过

## 4. 完成标准

1. 四类观察记录都已落盘：
   - `external-launcher.md`
   - `field-scripts.md`
   - `release-package.md`
   - `rollback-package.md`
2. 汇总文件 `summary.md` 与 `blockers.md` 已能直接支持 closeout 裁决。
3. `legacy-exit` 在本阶段前后均保持通过。
4. 若任一对象缺失，只能记为 `input-gap`，不能写成 `Go`。

## 5. 阶段退出门禁

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4c-closeout\legacy-exit-pre`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4c-closeout\legacy-exit-post`
3. `where.exe engineering-data-dxf-to-pb`
4. `where.exe dxf-to-pb`
5. `rg -n "dxf-to-pb|path-to-trajectory|export-simulation-input|generate-dxf-preview|dxf_pipeline|proto\.dxf_primitives_pb2|config\\machine_config\.ini" <target-root>`
