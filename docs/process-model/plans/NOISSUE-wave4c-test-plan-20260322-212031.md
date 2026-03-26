# NOISSUE Wave 4C Test Plan - External Observation Execution

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-212031`
- 关联 scope：`docs/process-model/plans/NOISSUE-wave4c-scope-20260322-212031.md`
- 关联 arch：`docs/process-model/plans/NOISSUE-wave4c-arch-plan-20260322-212031.md`

## 1. 验证原则

1. 只执行观察期命令和 `legacy-exit` 复核，不跑新的全量 build/test。
2. 缺少输入时输出 `input-gap`，不把缺失视作通过。
3. 所有证据必须落到 `integration/reports/verify/wave4c-closeout/`。

## 2. 阶段前后门禁

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4c-closeout\legacy-exit-pre
powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4c-closeout\legacy-exit-post
```

预期：

1. `legacy-exit-pre` 和 `legacy-exit-post` 均为全绿。
2. 文档与证据调整不引入新的仓内 legacy 回流。

## 3. 外部 launcher 观察

```powershell
where.exe engineering-data-dxf-to-pb
where.exe dxf-to-pb
```

预期：

1. canonical launcher 可见时记录为 `Go`。
2. legacy launcher 可见时记录为 `No-Go`。
3. 若当前环境没有 canonical launcher，也按 `No-Go` 处理，而不是跳过。

## 4. 外部脚本 / 发布包 / 回滚包观察

```powershell
rg -n "dxf-to-pb|path-to-trajectory|export-simulation-input|generate-dxf-preview|dxf_pipeline|proto\.dxf_primitives_pb2|config\\machine_config\.ini" <target-root>
```

预期：

1. 目标存在时，根据扫描结果做 `Go / No-Go`。
2. 目标缺失时，记录为 `input-gap`。
3. 汇总文件能直接支撑 closeout 结论。
