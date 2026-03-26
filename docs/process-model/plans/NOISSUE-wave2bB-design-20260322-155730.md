# NOISSUE Wave 2B-B 设计：Runtime Asset-Path Cutover

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`branchSafe=feat-dispense-NOISSUE-e2e-flow-spec-alignment`，`timestamp=20260322-155730`

## 范围

1. 只分析 runtime asset-path cutover 的设计边界、桥接策略、fail-fast 规则和验证口径。
2. 只基于以下范围建立当前事实与切换合同：
   - `packages/runtime-host/src/runtime/configuration/WorkspaceAssetPaths.h`
   - `apps/control-runtime`
   - `apps/control-tcp-server`
   - `apps/control-cli`
   - `packages/runtime-host`
   - `packages/transport-gateway`
3. 只覆盖与 runtime 默认资产路径直接相关的配置、recipe、schema、upload、CLI 样例 DXF、CLI preview 脚本路径合同。
4. 输出“当前是否单点解析”“哪里仍有旧路径假设”“切换日允许哪些 fallback”“哪些情况必须 fail-fast”的明确结论。

## 非范围

1. 不修改任何默认路径，不设计新的 canonical 默认路径。
2. 不做代码实现，不执行路径切换。
3. 不把 `run.ps1 -DryRun` 成功等同于真实运行无风险，也不把它作为 cutover 完成条件。
4. 不处理 binary 入口 cutover；`apps/control-runtime/run.ps1`、`apps/control-tcp-server/run.ps1`、`apps/control-cli/run.ps1` 仅作为现状证据。
5. 不覆盖 `logs/*` 默认路径合同；这些路径只作为相邻风险记录，不在本波次改动。
6. 不覆盖配置内容 schema 兼容策略；例如 `[Safety]` 到 `[Interlock]` 的字段 fallback 不是本次路径 cutover 主体。

## 当前事实

1. `WorkspaceAssetPaths.h` 已定义以下 canonical 相对路径：
   - `config/machine/machine_config.ini`
   - `data/recipes`
   - `data/schemas/recipes`
   - `uploads/dxf/`
2. 同一头文件仍保留 `config/machine_config.ini` compatibility 路径，并在 `ResolveConfigFilePath(...)` 内对 canonical/compat 双向候选做存在性探测。
3. `apps/control-runtime/main.cpp`、`apps/control-tcp-server/main.cpp` 在应用入口先解析一次配置路径；`packages/runtime-host/src/container/ApplicationContainer.cpp` 与 `packages/runtime-host/src/bootstrap/InfrastructureBindingsBuilder.cpp` 又各自重复解析一次。
4. `apps/control-cli` 不是单一路径消费方：
   - `main.cpp` 通过 `BuildContainer(...)` 走 runtime-host 容器链。
   - `CommandHandlers.cpp`、`CommandHandlers.Motion.cpp`、`CommandHandlers.Dxf.cpp` 还会直接读取 ini，并直接拼接 DXF / preview / report 路径。
5. `packages/transport-gateway` 当前不自行解析 workspace 资产路径；它通过 app 层传入 `config_path`，真正消费诊断配置时依赖 `IConfigurationPort`。但其 `TcpServerHostOptions` 和 `TcpCommandDispatcher` 仍暴露原始 `config_path` 合同面。
6. 2026-03-22 审计结果：
   - `config/machine/machine_config.ini` 与 `config/machine_config.ini` 两个文件都存在，且哈希相同。
   - `data/recipes/templates.json` 在仓内不存在，但 `TemplateFileRepository` 代码仍保留该 legacy 单文件布局的读取 fallback。
   - `examples/dxf/rect_diag.dxf` 与 `packages/engineering-data/scripts/generate_preview.py` 在仓内存在。
7. `FindWorkspaceRoot()` 当前把 `WORKSPACE.md`、`CMakeLists.txt`、`CLAUDE.md`、`.git` 都视为 workspace marker；按代码逻辑推导，只要从带 `CMakeLists.txt` 的子模块目录启动，就可能在子目录提前停下，而不是回到仓库根。

## 当前路径解析图

```text
apps/control-runtime/main.cpp
  --config 默认值 -> config/machine/machine_config.ini
  -> ResolveConfigFilePath(...)
  -> BuildContainer(config_path)
     -> ApplicationContainer(config_file_path_)
        -> ResolveConfigFilePath(...)        [重复解析]
     -> CreateInfrastructureBindings(config.config_file_path)
        -> ResolveConfigFilePath(...)        [重复解析]
        -> ConfigFileAdapter(resolved_config_path)
        -> ResolveUploadDirectory()          -> uploads/dxf/
        -> ResolveRecipeDirectory()          -> data/recipes
        -> ResolveRecipeSchemaDirectory()    -> data/schemas/recipes
        -> ResolveWorkspaceRelativePath("logs/audit")

apps/control-tcp-server/main.cpp
  --config 默认值 -> config/machine/machine_config.ini
  -> ResolveConfigFilePath(...)
  -> BuildContainer(...)
     -> 同 control-runtime
  -> TcpServerHostOptions{config_path}
     -> TcpCommandDispatcher(config_path)    [保留 config_path 合同面，但当前不再自行解析]

apps/control-cli/main.cpp
  CommandLineConfig.config_path 默认值 -> config/machine/machine_config.ini
  -> BuildCliContainer(ResolveCliConfigPath(config.config_path))
     -> ResolveCliConfigPath(non-empty) -> 直接返回原值   [旁路 1]
     -> BuildContainer(...)
        -> ApplicationContainer / CreateInfrastructureBindings 再解析

apps/control-cli side paths
  -> ReadIniValue/ReadIniDoubleValue(config_path, ...)        [旁路 2：直接读 ini]
  -> LoadDefaultDxfFilePath() -> examples/dxf/rect_diag.dxf   [旁路 3：cwd 相对]
  -> ResolvePreviewScriptPath() -> current_path()/packages/engineering-data/scripts/generate_preview.py
                                                           [旁路 4：cwd 相对]
  -> ResolveVelocityTracePath() / ResolvePlanningReportPath() -> logs/*

WorkspaceAssetPaths.h
  Resolve* -> FindWorkspaceRoot()
           -> marker = WORKSPACE.md | CMakeLists.txt | CLAUDE.md | .git
           -> [风险] 子模块目录也有 CMakeLists.txt，可能提前命中非仓库根
```

## 关键结论

### 当前是否真正单点解析

1. 结论：不是。
2. 原因不是“没有公共 helper”，而是“helper 不是唯一入口，也不是唯一 root 判定源”。
3. 具体表现：
   - config 路径在 app 入口、`ApplicationContainer`、`InfrastructureBindingsBuilder` 三层重复解析。
   - CLI 通过 `ResolveCliConfigPath(...)`、`ReadIniValue(...)`、默认 DXF 样例、preview 脚本路径形成多条旁路。
   - `FindWorkspaceRoot()` 的 marker 集合会把模块级 `CMakeLists.txt` 误当 workspace root 证据，不能视为稳定单点。

### 哪些地方仍隐含旧路径假设

1. `config/machine_config.ini`
   - 仍是 active compatibility alias，不只是历史文档。
   - 仓内实际文件仍存在，因此任何未完成 cutover 的消费者都可能继续命中它。
2. `apps/control-cli/CommandHandlersInternal.h`
   - `ResolveCliConfigPath(...)` 对非空输入直接返回原值，意味着默认 canonical 路径字符串本身并不会在 CLI 侧被真正解析成 workspace 定位后的路径。
3. `apps/control-cli/CommandHandlers.cpp`、`CommandHandlers.Motion.cpp`、`CommandHandlers.Dxf.cpp`
   - 多处 `ReadIniValue(...)` / `ReadIniDoubleValue(...)` 直接依赖传入字符串可被当前 cwd 打开。
4. `apps/control-cli/CommandHandlers.Dxf.cpp`
   - preview 脚本路径默认由 `current_path()/packages/engineering-data/scripts/generate_preview.py` 推导。
   - preview 输出目录、velocity trace、planning report 默认写入 `logs/*`，仍是 cwd 相对。
5. `apps/control-cli/CommandHandlersInternal.h`
   - 默认 DXF 样例是 `examples/dxf/rect_diag.dxf`，当前是字符串常量，不经过 workspace 资产解析。
6. `packages/runtime-host/src/runtime/recipes/TemplateFileRepository.cpp`
   - 仍保留 `data/recipes/templates.json` legacy 单文件布局读取逻辑。
7. `packages/transport-gateway`
   - `config_path` 仍穿透到 gateway 层接口，但 gateway 当前没有成为统一解析 owner；这会让 cutover 后的合同边界继续偏宽。

### 切换日哪些旧路径允许 fallback

1. 允许：`config/machine_config.ini`
   - 仅允许作为 cutover-day 的 inbound read-only alias。
   - 目标是吸收尚未完成切换的调用方，不允许继续把它写进新脚本、新文档或新参数示例。
2. 条件允许：`data/recipes/templates.json`
   - 仅当部署现场确实还存在该文件，且 canonical `data/recipes/templates/*.json` 迁移未完成时，允许 read-only fallback。
   - 当前仓内该文件不存在，因此它不是本仓验证必须依赖的默认资产。
3. 不允许 fallback：
   - `examples/dxf/rect_diag.dxf` 的 cwd 相对猜测
   - preview 脚本的 `current_path()/packages/...` 猜测
   - `data/schemas/recipes` 的第二目录猜测
   - `uploads/dxf/` 的旧路径猜测
   - 任何因为 `FindWorkspaceRoot()` 误判而落到模块子目录的“伪 canonical”路径

### 哪些情况必须 fail-fast

1. workspace root 判定命中模块级 `CMakeLists.txt` 而不是仓库根时，必须 fail-fast。
2. canonical 与 compatibility config 文件同时存在但内容不一致时，必须 fail-fast。
3. 命令需要真实配置文件、真实 DXF、真实 preview 脚本，而解析结果仍依赖 cwd 猜测时，必须 fail-fast。
4. cutover 窗口关闭后仍显式传入或引用 `config/machine_config.ini` 时，必须 fail-fast。
5. 现场若还存在 `data/recipes/templates.json`，但 canonical `templates/` 与 legacy 文件内容不一致且无法证明 canonical 优先时，必须 fail-fast。
6. `run.ps1 -DryRun` 通过但真实启动从非仓库根 cwd 失败时，发布门禁必须判失败；不能接受“dry-run 已成功”的说法。

## 新旧路径桥接矩阵

| 资产/合同 | canonical 目标 | 旧路径 / 旧布局 / 旁路假设 | 当前消费者 | 切换日策略 | 最终状态 |
|---|---|---|---|---|---|
| 机器配置文件 | `config/machine/machine_config.ini` | `config/machine_config.ini` | `apps/control-runtime`、`apps/control-tcp-server`、`apps/control-cli`、`packages/runtime-host` | 允许 read-only alias fallback | cutover 稳定后删除 alias 文件与 alias 解析 |
| recipe 根目录 | `data/recipes` | 无活跃旧目录，但模板仍有 legacy 单文件布局 | `packages/runtime-host`，经 facade 间接被 CLI / TCP 使用 | canonical only | 保持 canonical，不新增第二目录 |
| recipe 模板布局 | `data/recipes/templates/*.json` | `data/recipes/templates.json` | `TemplateFileRepository` | 仅在现场遗留文件存在时保留 read-only fallback | 完成迁移后移除 legacy 文件读取逻辑 |
| recipe schema 目录 | `data/schemas/recipes` | 无当前活跃旧目录；provider 仅保留接口级 fallback 槽位 | `ParameterSchemaFileProvider` | canonical only | 保持 canonical，不启用第二 schema 目录 |
| upload 目录 | `uploads/dxf/` | 无当前活跃旧目录 | `LocalFileStorageAdapter` | canonical only | 保持 canonical |
| CLI 默认 DXF 样例 | `examples/dxf/rect_diag.dxf` | 当前是 cwd 相对字符串常量，不是旧路径桥接 | `apps/control-cli` | 不允许 fallback；必须改成 workspace-rooted 解析 | 保持同一路径，但解析 owner 收敛到统一 resolver |
| CLI preview 脚本 | `packages/engineering-data/scripts/generate_preview.py` | 当前是 `current_path()/packages/...` 猜测 | `apps/control-cli` | 不允许 fallback；必须改成 workspace-rooted 解析或显式环境变量 | 保持同一路径，但禁止 cwd 猜测 |
| gateway config_path 合同面 | app 层传入已解析 config 路径 | 空字符串时回落到 canonical 相对字符串 | `packages/transport-gateway` | 不新增 fallback；保持 pass-through | gateway 不再承担任何二次解析责任 |

## fallback 保留条件

1. `config/machine_config.ini` 只在以下条件同时满足时保留：
   - cutover 首波仍需兼容旧调用方输入。
   - canonical 文件是唯一真实源，compat 文件与 canonical 文件内容一致。
   - 所有新文档、新脚本、新帮助文本只暴露 canonical 路径。
2. `data/recipes/templates.json` 只在以下条件同时满足时保留：
   - 现场或历史数据包仍可能携带 legacy 单文件模板。
   - canonical `templates/` 目录优先，legacy 只做补漏读取。
   - fallback 只读，不允许再向 legacy 文件写回。
3. 任何 fallback 都必须带退出条件，不能以“先留着更稳妥”为理由长期保留。

## fallback 移除条件

1. 配置 alias 移除条件：
   - `apps/control-runtime`、`apps/control-tcp-server`、`apps/control-cli` 从仓库根和非仓库根 cwd 启动都只依赖 canonical 路径成功。
   - 仓内 `rg -n "config/machine_config\.ini"` 只剩设计允许的历史证据点。
   - 部署 inventory 证明无外部脚本继续显式引用旧路径。
2. 模板 legacy 文件移除条件：
   - 部署 inventory 证明不再存在 `data/recipes/templates.json`。
   - 模板 `save/get/list` 全量验证在只有 `templates/*.json` 的情况下通过。
   - 任何回滚方案都不再依赖重新生成 legacy 单文件。
3. preview / DXF 旁路不是“可保留 fallback”，而是必须被消除的解析分叉；验证通过后直接移除分叉，不设长期桥接。

## fail-fast 规则

1. 解析 workspace root 时必须优先证明“这是仓库根”，而不是“这是某个带 `CMakeLists.txt` 的目录”。
2. 任何非绝对路径在进入业务逻辑前都必须被归一化到单一 owner；进入业务逻辑后不允许再次用 `current_path()` 拼接 runtime 资产。
3. 需要真实文件的命令不得静默回退到默认字符串：
   - 配置读取失败
   - preview 脚本不存在
   - 默认 DXF 样例不存在
   - legacy/canonical 同时存在但冲突
4. 对 `config/machine_config.ini` 的 compat fallback 必须是显式、可观测、可删除的过渡行为；一旦进入移除窗口，继续命中直接报错。
5. 干运行门禁只验证 binary 可发现性，不验证资产路径正确性；因此任何以 dry-run 作为 runtime asset-path 签收依据的结论都无效。

## 验证矩阵

| 验证项 | 命令或方式 | 预期结果 | 说明 |
|---|---|---|---|
| root build 基线 | `.\build.ps1 -Profile CI -Suite apps` | 成功结束，生成 canonical app binaries | 只证明 binary 产物存在，不证明资产解析正确 |
| root test 基线 | `.\test.ps1 -Profile CI -Suite packages -FailOnKnownFailure` | 成功结束 | 需要补充针对 `WorkspaceAssetPaths` 的单元覆盖 |
| config alias 一致性 | `Get-FileHash config/machine/machine_config.ini, config/machine_config.ini` | 两者哈希一致；若不一致直接失败 | cutover-day 必选检查 |
| control-runtime 非仓库根启动 | `Push-Location apps/control-runtime; .\run.ps1 -- --version; Pop-Location` | 只能作为 binary 烟测；不得作为资产 cutover 验收 | 真实验收应补充需要读取 config 的命令 |
| control-runtime 真实配置读取 | `Push-Location apps/control-runtime; .\run.ps1 -- --config ..\..\config\machine\machine_config.ini --run-seconds 1; Pop-Location` | 从非仓库根 cwd 也能读取 canonical config 成功 | 这是 asset-path 验收，不是 dry-run |
| control-tcp-server 真实配置读取 | `Push-Location apps/control-tcp-server; .\run.ps1 -- --config ..\..\config\machine\machine_config.ini --port 9555; Pop-Location` | 能完成启动前配置读取；若路径依赖 cwd 猜测则失败 | 需用可控短命令或测试桩避免长驻 |
| CLI bootstrap-check 非仓库根启动 | `Push-Location apps/control-cli; .\run.ps1 -- bootstrap-check; Pop-Location` | 必须成功；不能依赖 cwd 恰好能打开 `config/...` | 当前设计重点验证项 |
| CLI dxf-preview 非仓库根启动 | `Push-Location apps/control-cli; .\run.ps1 -- dxf-preview --file ..\..\examples\dxf\rect_diag.dxf --json; Pop-Location` | preview 脚本和样例 DXF 都能按统一 resolver 命中 | 直接覆盖 `current_path()` 旁路 |
| legacy template 只读桥接 | 新增 unit/integration 用例：仅放置 `data/recipes/templates.json` | `get/list` 成功，`save` 只写 canonical `templates/*.json` | 不允许反向写回 legacy 文件 |
| schema / upload canonical only | 新增 unit 用例：缺失 canonical 目录时启动 | 明确报错或创建 canonical 目录；不去猜第二目录 | 不允许隐式 fallback |
| cutover 关闭后旧路径拦截 | 新增 gate：`rg -n "config/machine_config\.ini|templates\.json"` 针对允许清单以外路径 | 0 非法命中 | 用于退出 bridge 窗口 |

## 风险

1. `FindWorkspaceRoot()` 当前 marker 过宽，最容易把模块级 `CMakeLists.txt` 误判为 workspace root，导致“看起来用了统一 resolver，实际上仍是子目录相对路径”。
2. CLI 当前同时存在容器链解析和直接 ini 读取两条链路；即使 container 启动成功，也不能证明 CLI side path 已完成 cutover。
3. `config/machine_config.ini` 实体文件仍在仓内，会掩盖未切换调用方；如果不加一致性校验和退出门禁，compat 会长期滞留。
4. `TemplateFileRepository` 的 legacy 文件 fallback 当前在仓内不可见、在现场可能可见；如果不做部署 inventory，很容易在测试环境“看起来没问题”，到现场才暴露。
5. `run.ps1 -DryRun` 当前只检查 binary 可发现性；若团队把它误当 cutover gate，会形成明显假阳性。
6. `logs/*` 仍是 cwd 相对，虽然不在本波次改动范围，但它会干扰“从非仓库根启动”的验收观感，实施时必须避免把日志问题和资产路径问题混淆。
7. `packages/transport-gateway` 继续暴露 `config_path` 合同面，会让后续 owner 误以为 gateway 也能独立承担路径解析；应在设计上明确禁止这种扩散。

## 阻断项

1. 缺少专门覆盖 `FindWorkspaceRoot()` 非仓库根 cwd 行为的单元测试；在没有该测试前，无法安全实施 resolver cutover。
2. 缺少针对 CLI side path 的正式回归用例，尤其是：
   - `bootstrap-check`
   - `dxf-preview`
   - `dxf-plan` / `dxf-dispense` 的配置回读
3. 现场部署对 `config/machine_config.ini` 和 `data/recipes/templates.json` 的真实依赖库存尚未冻结；这会直接影响 bridge 窗口长度。
4. `FindWorkspaceRoot()` 的“仓库根判定合同”尚未冻结为单一准则；若不先冻结，就无法判断哪些启动目录应该成功、哪些应该直接 fail-fast。

## 最终回答

1. 当前不是真正单点解析；`WorkspaceAssetPaths.h` 只是局部公共 helper，不是唯一入口，也不是稳定的唯一 root 判定源。
2. 仍隐含旧路径假设的地方主要有：
   - `config/machine_config.ini` compatibility alias
   - CLI 的 `ResolveCliConfigPath(...)` 非空直返
   - CLI 的直接 ini 读取
   - CLI 默认 DXF 样例与 preview 脚本的 cwd 相对拼接
   - `TemplateFileRepository` 对 `data/recipes/templates.json` 的 legacy 读取
3. 切换日允许 fallback 的旧路径只有两类：
   - `config/machine_config.ini`，且仅限 inbound read-only alias
   - `data/recipes/templates.json`，且仅限现场仍有遗留文件时的 read-only fallback
4. 必须 fail-fast 的情况包括：
   - workspace root 误判
   - canonical/legacy 配置冲突
   - 真实命令仍依赖 cwd 猜测
   - cutover 关闭后继续命中旧路径
   - 任何把 dry-run 成功当成真实运行无风险的签收结论
