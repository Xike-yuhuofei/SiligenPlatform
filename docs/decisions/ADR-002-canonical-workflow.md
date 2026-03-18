# ADR-002 Canonical Workflow

- 状态：`Accepted`
- 日期：`2026-03-18`

## 决策

自 `2026-03-18` 起，`SiligenSuite` 采用“根级入口 + canonical 路径优先”的唯一推荐工作方式。

### 默认入口

| 场景 | 默认入口 | 规则 |
|---|---|---|
| 日常构建 | `.\build.ps1` | 所有新的构建说明、脚本、自动化都必须先接到根级入口。 |
| 日常测试 | `.\test.ps1` | 所有新的验证说明、测试回归、人工复跑步骤都必须先写根级入口。 |
| CI / 严格验证 | `.\ci.ps1` | CI、预提交流程、发布前自检默认使用根级入口。 |
| HMI 启动 | `.\apps\hmi-app\run.ps1` | 不再把 `hmi-client` 当默认入口。 |
| DXF 编辑 | `docs/runtime/external-dxf-editing.md` | 不再提供 `apps/dxf-editor-app` 或 `dxf-editor` 默认入口。 |
| Runtime/TCP/CLI 启动 | `.\apps\control-runtime\run.ps1`、`.\apps\control-tcp-server\run.ps1`、`.\apps\control-cli\run.ps1` | 即使真实产物仍在 `control-core`，对外默认也先经过根级 `apps/` wrapper。 |

### 禁止新增的默认引用

- `apps/dxf-editor-app`
- `dxf-editor`
- `packages/editor-contracts`
- `dxf_editor`
- `hmi-client/src`
- `dxf-pipeline/src`
- `control-core/build/bin/**`

### 结果

DXF 编辑相关需求统一按外部编辑器人工流程说明处理，不再创建新的 editor app、editor contracts 或兼容壳。
