# Legacy Deletion Gates

更新时间：`2026-03-18`

## 1. 目的

本文件定义 legacy 目录的“安全删除门禁”与已删除状态记录。

## 2. 状态分级

| 分级 | 含义 |
|---|---|
| `已删除` | 目录已退出工作区默认链路，源码、脚本与契约已移除，只保留历史文档记录。 |
| `可进入删除准备` | 默认构建、运行、测试、CI 已切到 canonical；剩余工作主要是清理镜像源码、旧文档、旧脚本或残留 fallback。 |
| `仅可冻结` | canonical 实现已存在，但 live consumer 仍直接依赖 legacy 兼容壳；当前只能冻结，不允许扩边界。 |
| `尚不可切换` | legacy 目录仍承载真实实现、真实产物或真实资产；不能按目录整体删除。 |

## 3. 当前状态总表

| 目录 | 当前状态分级 | 说明 |
|---|---|---|
| `control-core` | `尚不可切换` | 默认 control app 产物链路已切到 canonical control-apps build root，但库图、shared-kernel、未迁完 CLI 命令面与历史迁移源仍留在本目录。 |
| `dxf-editor` | `已删除` | `apps/dxf-editor-app`、legacy `dxf-editor`、`packages/editor-contracts` 已按“外部编辑器人工流程”方案退出工作区默认链路。 |
| `dxf-pipeline` | `仅可冻结` | 仍被 canonical HMI 直接调用，且契约测试、proto、fixtures 仍显式依赖本目录。 |
| `simulation-engine`（旧顶层目录） | `可进入删除准备` | 顶层旧目录已不存在；当前只剩 residual fallback / 文本残留需要清零并防回流。 |

## 4. `control-core`

- 状态分级：`尚不可切换`

### 4.1 当前真实职责

1. 仍是 control app 共享库图与 CMake source root owner；`runtime-host`、`process-runtime-core`、`device-adapters`、`device-contracts`、`shared-kernel` 仍通过本目录编译。
2. `control-core/apps/CMakeLists.txt` 现在只负责注册根级 canonical app 目录：`../apps/control-runtime`、`../apps/control-tcp-server`、`../apps/control-cli`。
3. `control-runtime`、`control-tcp-server`、`control-cli` 的默认 exe 产物来源已经切到 canonical control-apps build root，而不是 `control-core/build/bin/**`。
4. HIL、workspace validation、根级 build/test/CI 默认都已切到 canonical control-apps build root。

### 4.2 当前阻塞项

1. `control-cli` canonical 承载面当前只覆盖 `bootstrap-check` 与 `recipe` 子命令；运动、点胶、DXF、连接调试命令仍未迁完。
2. `control-core/apps/control-runtime`、`control-core/apps/control-tcp-server` 仍保留 legacy 壳/历史源码，删除前需要先完成 consumer 清零与文档切换。
3. 现场部署、回滚、运行手册仍有 residual legacy 文本，删除 `control-core/apps/*` 前必须清理。
4. `control-core` 顶层目录本体仍不能删除，因为库图与共享头文件并未迁出。

### 4.3 当前结论

1. `control-core/build/bin/**` 已退出三个 control app 的默认运行链路。
2. 当前仍保留显式、临时、可审计的 `-UseLegacyFallback` 回退，用于 CLI 未迁移命令和排障期过渡。
3. `control-core/apps/*` 的删除前置条件见 `docs/architecture/control-apps-realization.md`。

## 5. `dxf-editor`

- 状态分级：`已删除`

### 5.1 当前结论

1. `apps/dxf-editor-app`、legacy `dxf-editor`、`packages/editor-contracts` 已整体移除。
2. 旧 notify / CLI 编辑器协作协议已废弃，不再作为工作区支持能力。
3. 当前 DXF 编辑流程改为外部编辑器人工处理，默认说明见 `docs/runtime/external-dxf-editing.md`。

### 5.2 删除后门禁

以下检查期望 `0` 行：

```powershell
Get-ChildItem .\apps,.\packages,.\integration,.\tools,.\docs -Recurse -File |
  Where-Object { $_.FullName -notmatch '\\build\\|\\reports\\|\\tmp\\|\\__pycache__\\' } |
  Select-String -Pattern 'apps\\dxf-editor-app|apps/dxf-editor-app|dxf-editor\\|dxf-editor/|packages\\editor-contracts|packages/editor-contracts|dxf_editor'
```

## 6. `dxf-pipeline`

- 状态分级：`仅可冻结`

### 6.1 当前真实职责

1. canonical HMI 仍通过 `DxfPipelinePreviewClient` 直接执行 `python -m dxf_pipeline.cli.generate_preview`。
2. `packages/engineering-contracts/tests/test_engineering_contracts.py` 仍直接读取 `dxf-pipeline/proto/dxf_primitives.proto`。
3. `dxf-pipeline/tests/fixtures/...` 仍是 HMI 默认 DXF 打开候选目录之一。

## 7. `simulation-engine`（旧顶层目录）

- 状态分级：`可进入删除准备`

### 7.1 当前真实职责

1. 顶层旧 `simulation-engine/` 目录当前已不存在；canonical owner 已是 `packages/simulation-engine`。
2. 根级 `build.ps1` / `test.ps1` / CI matrix 的 `simulation` suite 都直接指向 `packages/simulation-engine`。

## 8. 执行顺序建议

1. 先完成 `control-core` 与 `dxf-pipeline` 的剩余切换。
2. 持续保持 `dxf-editor` 已删除状态，不再恢复 app、legacy 目录或 editor contracts。
