# Quickstart: 工作区架构模板对齐重构

## 1. 目标

用最短路径完成本特性的实施与验证，确保 9 轴正式文档、仓库边界收敛和根级门禁证据同步闭环。

## 2. 准备输入

确认以下输入已经可读:

1. 参考模板: `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\`
2. 当前规格目录: `D:\Projects\SS-dispense-align\specs\refactor\arch\NOISSUE-architecture-refactor-spec\`
3. 当前工作区事实来源: `apps\`, `packages\`, `integration\`, `tools\`, `docs\`, `config\`, `data\`, `examples\`

## 3. 执行顺序

1. 在 `D:\Projects\SS-dispense-align\docs\architecture\dsp-e2e-spec\` 建立 S01-S09 正式文档和 S10 冻结目录索引，文件名遵循 `dsp-e2e-spec-s0x-*.md`。
2. 按 `contracts\frozen-docset-contract.md` 将参考模板轴映射到当前工作区事实来源，补齐阶段、对象、模块、状态机、测试矩阵。
3. 按 `contracts\workspace-boundary-contract.md` 为 M0-M11 建立当前 path cluster、owner、禁止越权和 support/vendor/generated 分类。
4. 对非主链模块补齐 `keep / migrate / merge / freeze / remove / archive` 处置决策，不能留下灰区。
5. 确认历史术语只出现在迁移映射节一次，不再作为正式别名并存。

## 4. 最低验证命令

在仓库根目录 `D:\Projects\SS-dispense-align\` 执行:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite apps,packages
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile Local -Suite apps,packages,integration,protocol-compatibility
powershell -NoProfile -ExecutionPolicy Bypass -File .\ci.ps1 -Suite apps,packages,protocol-compatibility
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\run-local-validation-gate.ps1
```

补充证据按需执行:

```powershell
python .\integration\simulated-line\run_simulated_line.py
python .\integration\hardware-in-loop\run_hardware_smoke.py
```

## 5. 文档一致性检查

建议在仓库根执行以下检查:

```powershell
rg -n "ExecutionPackageBuilt|ExecutionPackageValidated|PreflightBlocked|ArtifactSuperseded|WorkflowArchived" docs\architecture\dsp-e2e-spec
rg -n "control-core|hmi-client|dxf-pipeline" docs\architecture\dsp-e2e-spec
rg -n "shared\\\\|third_party\\\\|build\\\\|logs\\\\|uploads\\\\" docs\architecture\dsp-e2e-spec-s06-repo-structure-guide.md
```

判定标准:

1. 五个关键术语在各轴中的语义一致。
2. legacy 名称仅出现在迁移映射节，不能成为正式默认入口。
3. support/vendor/generated 目录被正确分类，未被声明为新 owner 根。

## 6. 完成定义

当以下条件同时满足时，本特性可进入 `speckit-tasks`:

1. `plan.md`、`research.md`、`data-model.md`、`quickstart.md`、`contracts\*` 已冻结。
2. 9 轴正式文档与 S10 索引的目标路径和责任分配明确。
3. 根级验证入口、必要的补充证据和 legacy 隔离规则都已经写入正式文档。
