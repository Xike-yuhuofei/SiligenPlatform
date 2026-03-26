# NOISSUE Wave 4B Scope - External Migration Observation and Runtime Doc Canonicalization

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-210606`
- 上游收口：`docs/process-model/plans/NOISSUE-wave4a-closeout-20260322-205132.md`

## 1. 阶段目标

1. 把活跃 current-fact 文档统一到当前仓内事实，不再保留绝对路径、root config alias 和已退出 DXF compat shell 的现行表述。
2. 补齐发布、部署、回滚、排障的 canonical 口径，并把“仓内已验证”和“仓外需人工确认”明确分层。
3. 新增仓外迁移观察期文档和证据落位，作为后续人工确认的唯一执行入口。

## 2. in-scope

1. `README.md`、`WORKSPACE.md`
2. `docs/onboarding/`、`docs/runtime/`、`docs/troubleshooting/` 下的活跃运行文档
3. `integration/hardware-in-loop/README.md`
4. `docs/runtime/external-migration-observation.md`
5. `docs/process-model/plans/`、`docs/process-model/reviews/` 中本阶段新增工件
6. `integration/reports/verify/wave4b-closeout/` 下的文本扫描、观察期准备和门禁报告

## 3. out-of-scope

1. 产品代码、构建图、测试逻辑、门禁规则本身的功能性改动
2. 仓外环境上的真实执行、发布包替换或现场脚本修复
3. 新一波次的功能迁移或 owner 重划分
4. 为适配仓外残留问题而恢复任何 legacy 兼容壳

## 4. 完成标准

1. 活跃文档不再硬编码 `D:\Projects\SiligenSuite`。
2. 活跃文档不再把 `config\machine_config.ini` 当成当前 alias / bridge。
3. 活跃文档不再把 `dxf-pipeline` sibling 依赖写成当前仓内事实。
4. `docs/runtime/external-migration-observation.md` 已提供 `Go/No-Go` 规则、证据落位和命中 legacy 入口时的处理动作。
5. `integration/reports/verify/wave4b-closeout/` 已落盘文本扫描与 `legacy-exit` 复核证据。

## 5. 阶段退出门禁

1. `rg -n "D:\\Projects\\SiligenSuite|config\\machine_config\\.ini|sibling 目录依赖|sibling 运行方式|No commits yet" README.md WORKSPACE.md docs/onboarding docs/runtime docs/troubleshooting integration/hardware-in-loop/README.md`
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4b-closeout\legacy-exit`
3. `git diff -- README.md WORKSPACE.md docs/onboarding docs/runtime docs/troubleshooting integration/hardware-in-loop/README.md`
