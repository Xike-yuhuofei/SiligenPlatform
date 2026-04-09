# Refactor Guard Rules

更新时间：2026-04-07

## 1. 门禁总览

| 门禁 | 实现 | 当前级别 | 报告位置 |
|---|---|---|---|
| Legacy Exit | `legacy-exit-check.ps1` + `scripts/migration/legacy-exit-checks.py` | hard-fail | `tests/reports/legacy-exit/` |
| Semgrep CE | `scripts/validation/invoke-semgrep.ps1` + `scripts/validation/semgrep/arch-rules.yml` | hard-fail | `tests/reports/semgrep/` |
| Import Linter | `scripts/validation/invoke-import-linter.ps1` + `.importlinter` | hard-fail on hard contracts；advisory on advisory contracts | `tests/reports/import-linter/` |
| Python Coverage | `scripts/validation/invoke-python-coverage.ps1` | report-only | `tests/reports/coverage/python/` |
| C/C++ Coverage | `scripts/validation/invoke-cpp-coverage.ps1` | report-only | `tests/reports/coverage/cpp/` |
| Cppcheck | `scripts/validation/invoke-cppcheck.ps1` | report-only | `tests/reports/static-analysis/cppcheck/` |
| Dependency Graph | `scripts/validation/invoke-dependency-graph-export.ps1` | report-only | `tests/reports/dependency-graphs/` |

## 2. Semgrep 规则

规则文件：`scripts/validation/semgrep/arch-rules.yml`

### 2.1 已落地规则

| 规则 ID | 拦截内容 | 失败条件 |
|---|---|---|
| `refactor.guard.no-retired-root-reference-live-files` | 活跃源码/脚本/配置/构建文件引用退休根 `packages/`、`integration/`、`tools/`、`examples/` | 出现 1 条 finding 即失败 |
| `refactor.guard.no-retired-entry-reference-in-entrypoints` | 根入口、build/deploy/config 脚本继续调用已退出入口或旧应用命名 | 出现 1 条 finding 即失败 |
| `refactor.guard.hmi-no-runtime-internal-paths` | `apps/hmi-app` 直达 runtime-service/runtime-gateway/planner-cli 内部路径 | 出现 1 条 finding 即失败 |
| `refactor.guard.apps-no-module-internal-paths` | `apps/*` 直接引用 `modules/*/(internal|private|detail|impl)` | 出现 1 条 finding 即失败 |
| `refactor.guard.shared-no-apps-reverse-dependency` | `shared/kernel`、`shared/contracts` 反向引用 `apps/*` | 出现 1 条 finding 即失败 |
| `refactor.guard.no-legacy-named-runtime-config-paths` | 在 entry/config/build/deploy 文件中新引入 `legacy/compat/old/v1/deprecated/shim/bridge/temp` 路径段 | 出现 1 条 finding 即失败 |

### 2.2 当前状态

- 规则与 CI 接线已落地。
- wrapper 已支持：
  - JSON 报告
  - Markdown 摘要
  - 工具原始输出留档
  - tool execution failure 按 hard-fail 处理
- 当前本机 Windows 环境中 `semgrep-core` 失败，报告状态为 `tool-error`，仍视为 gate failure。

## 3. Import Linter 契约

配置文件：`.importlinter`

### 3.1 Hard contracts

| 契约 ID | 含义 | 当前结果 |
|---|---|---|
| `production-no-test-kit` | 生产包不得 import `test_kit` | passed |
| `hmi-ui-no-direct-tools` | `hmi_client.ui` / `hmi_client.features` 不得直连 `hmi_client.tools` | passed |
| `hmi-ui-no-direct-runtime-session-internals` | UI 不得直连 `backend_manager` / `tcp_client` / `gateway_launch` | passed |

### 3.2 Advisory contracts

| 契约 ID | 含义 | 当前结果 |
|---|---|---|
| `canonical-hmi-application-no-compat-shell` | `hmi_application` 不应依赖 `hmi_client.client` / `hmi_client.features` | failed |
| `hmi-ui-no-owner-direct-import` | UI 不应直接 import `hmi_application` owner | failed |

### 3.3 分层语义

- 当前 hard contracts 用来阻断继续恶化。
- 当前 advisory contracts 用来显影已存在耦合，不直接阻断。
- 后续若 owner/client 边界完成收敛，可把 advisory 提升为 hard。

## 4. Legacy Exit 增强项

真值表：`scripts/migration/legacy_fact_catalog.json`

### 4.1 已增强检查

- 物理残留：根级 `packages/`、`integration/`、`tools/`、`examples/`
- 文本残留：
  - `*.ps1`
  - `CMakeLists.txt`
  - `*.cmake`
  - `*.json`
  - `*.yaml`
  - `*.yml`
  - `*.toml`
  - `*.ini`
  - `*.py`
  - `*.cpp`
  - `*.h`
  - `*.md`
- 分类输出：
  - `build`
  - `config`
  - `runtime-entry`
  - `script`
  - `source`
  - `documentation`
  - `other`
- `IncludeDocs` 开关：活动文档降级为 `report-only`

### 4.2 当前结果

- blocker 已清零。
- 保留 8 条 `controlled-exception`，原因是模块内 canonical `tests/integration` 被 legacy regex 命中。

## 5. Coverage / Static Analysis / Graph

### 5.1 Python Coverage

- 报告：
  - `coverage.json`
  - `coverage.xml`
  - `html/index.html`
  - `coverage-summary.md`
- 当前阈值策略：
  - global branch min: `10%`
  - critical path:
    - `apps/hmi-app/src/hmi_client` -> `20%`
    - `modules/hmi-application/application/hmi_application` -> `20%`
    - `modules/dxf-geometry/application/engineering_data` -> `15%`
- 当前级别：report-only

### 5.2 C/C++ Coverage

- 开关：
  - `build.ps1 -EnableCppCoverage`
  - `test.ps1 -EnableCppCoverage`
  - `ci.ps1 -EnableCppCoverage`
- 当前策略：
  - 优先 LLVM source-based coverage / `llvm-cov`
  - 若未采集到 `.profraw`，输出 `not-collected` 报告而不是直接崩溃

### 5.3 Cppcheck

- 当前规则集以低噪声为先：
  - `warning`
  - `style`
  - `performance`
  - `portability`
- 当前环境缺 `cppcheck`，已保留 CI 接线与报告位。

### 5.4 Dependency Graph

- CMake：导出 `workspace-targets.dot` 与关键 target 子图
- Python：导出 `*.dot` 与 `*.deps.json`
- `dot` 缺失时仍保留 `.dot` / `.deps.json`，不阻断

## 6. 当前 hard fail / report-only 冻结

### Hard fail

- `legacy-exit-check.ps1`
- `invoke-semgrep.ps1`
- `invoke-import-linter.ps1` 中的 hard contracts

### Report-only

- Import Linter advisory contracts
- Python coverage 阈值
- C/C++ coverage 阈值 / not-collected
- Cppcheck
- Dependency graph export
