# QA Report

## 1. 环境信息

1. 分支：`feat/dispense/NOISSUE-e2e-flow-spec-alignment`
2. 基线提交：`f6e5668d`
3. 执行时间：`2026-03-22`
4. 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-212031`
5. 说明：本轮只执行 `Wave 4C` 的观察命令和阶段前后 `legacy-exit` 复核

## 2. 执行命令与退出码

1. `powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4c-closeout\legacy-exit-pre`
   - exit code：`0`
2. `where.exe engineering-data-dxf-to-pb`
   - exit code：`1`
3. `where.exe dxf-to-pb`
   - exit code：`1`
4. `powershell -NoProfile -Command "Get-Command engineering-data-dxf-to-pb -ErrorAction SilentlyContinue | Format-List *"`
   - exit code：`1`
5. `powershell -NoProfile -Command "Get-Command dxf-to-pb -ErrorAction SilentlyContinue | Format-List *"`
   - exit code：`1`
6. `rg -n "dxf-to-pb|path-to-trajectory|export-simulation-input|generate-dxf-preview|dxf_pipeline|proto\.dxf_primitives_pb2|config\\machine_config\.ini" apps integration tools -g '*.ps1' -g '*.py' -g '*.bat' -g '*.cmd'`
   - exit code：`0`
7. `powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4c-closeout\legacy-exit-post`
   - exit code：`0`

## 3. 观察结论

1. 当前可访问环境中 canonical `engineering-data-*` launcher 不可见
   - 结论：`No-Go`
2. 当前仓内没有可观察的发布包、回滚包、现场脚本快照
   - 结论：`input-gap`
3. 当前仓内脚本扫描只命中 `tools/scripts/legacy_exit_checks.py`
   - 结论：不构成 live legacy 回流

## 4. 风险接受结论

1. 不接受把当前状态写成“仓外迁移完成”。
2. 不接受在缺少外部输入的情况下直接进入下一波次规划。
3. 接受把本轮结果固化为 `Wave 4C` 阻断 closeout，并等待补充输入后重跑。
