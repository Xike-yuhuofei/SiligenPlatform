# Refactor Guard Baseline

更新时间：2026-04-07

## 1. 仓库探测结果

### 1.1 根脚本

| 路径 | 结果 |
|---|---|
| `build.ps1` | 存在 |
| `test.ps1` | 存在 |
| `ci.ps1` | 存在 |
| `legacy-exit-check.ps1` | 存在 |

### 1.2 Apps 与入口

| 路径 | 结果 |
|---|---|
| `apps/hmi-app` | 存在，Python HMI 应用入口 |
| `apps/runtime-service` | 存在，含 `CMakeLists.txt` |
| `apps/runtime-gateway` | 存在，含 `CMakeLists.txt` |
| `apps/planner-cli` | 存在，含 `CMakeLists.txt` |
| `apps/trace-viewer` | 目录存在，但当前无 `CMakeLists.txt`，仅 README |

### 1.3 Python 包与真实包名

| 真实路径 | 包名 / 角色 |
|---|---|
| `apps/hmi-app/src/hmi_client` | HMI UI / client 包 |
| `modules/hmi-application/application/hmi_application` | HMI owner / application 包 |
| `modules/dxf-geometry/application/engineering_data` | DXF/engineering_data 包 |
| `shared/testing/test-kit/src/test_kit` | tests helper / fixture / validation 包 |

说明：
- 当前未探测到独立的 runtime Python package；HMI 侧 runtime 通信接口主要落在 `hmi_client.client.*`。
- Import Linter 契约已按以上真实包名绑定。

### 1.4 C++ / CMake 入口

| 路径 | 结果 |
|---|---|
| `CMakeLists.txt` | 顶层入口存在 |
| `modules/*` | 真实模块根为 `workflow`、`job-ingest`、`dxf-geometry`、`topology-feature`、`process-planning`、`coordinate-alignment`、`process-path`、`motion-planning`、`dispense-packaging`、`runtime-execution`、`trace-diagnostics`、`hmi-application` |

补充：
- 顶层 `CMakeLists.txt` 当前在 Windows 下强制使用 `clang-cl + lld-link` 的 canonical 配置。
- `CMAKE_EXPORT_COMPILE_COMMANDS` 已在 Clang 下开启，供后续 `cppcheck`/coverage 使用。

### 1.5 tests / 文档 / 脚本目录

| 路径 | 结果 |
|---|---|
| `tests/contracts` | 存在 |
| `tests/e2e` | 存在 |
| `tests/integration` | 存在，且是 canonical 测试根，不是已退休仓库根 `integration/` |
| `tests/performance` | 存在 |
| `tests/baselines` | 存在 |
| `scripts/` | 存在 |
| `config/` | 存在 |
| `deploy/` | 存在 |
| `docs/` | 存在 |

### 1.6 退休 / 疑似 legacy 根目录探测

根级目录当前探测结果：

| 名称 | 根级目录是否存在 | 备注 |
|---|---|---|
| `packages` | 否 | 已纳入 blocker 物理残留检查 |
| `integration` | 否 | 仅根级目录退休，`tests/integration` 仍为 canonical |
| `tools` | 否 | 本次已收敛删除，脚本迁移到 `scripts/validation/` |
| `examples` | 否 | 仅根级目录退休，模块内 `modules/*/examples` 仍为 canonical |
| `legacy` | 否 | 未探测到根级目录 |
| `compat` | 否 | 未探测到根级目录 |
| `old` | 否 | 未探测到根级目录 |
| `v1` | 否 | 未探测到根级目录 |
| `deprecated` | 否 | 未探测到根级目录 |
| `shim` | 否 | 未探测到根级目录 |
| `bridge` | 否 | 未探测到根级目录 |
| `temp` | 否 | 未探测到根级目录 |

## 2. 本轮规则绑定的真实位置

### 2.1 Semgrep

- 主规则文件：`scripts/validation/semgrep/arch-rules.yml`
- 绑定扫描面：
  - `apps/`
  - `modules/`
  - `shared/`
  - `scripts/`
  - `config/`
  - `deploy/`
  - `cmake/`
  - `tests/`
  - 根入口：`build.ps1`、`test.ps1`、`ci.ps1`、`legacy-exit-check.ps1`、`CMakeLists.txt`

### 2.2 Import Linter

- 配置文件：`.importlinter`
- 绑定根包：
  - `hmi_client`
  - `hmi_application`
  - `engineering_data`
  - `test_kit`

### 2.3 Dependency Graph

- CMake 图输出：`tests/reports/dependency-graphs/cmake/`
- CMake 关键 target 子图：`siligen_runtime_service.dot`、`siligen_runtime_gateway.dot`、`siligen_planner_cli.dot`、`siligen_runtime_host_unit_tests.dot`、`siligen_dxf_geometry_unit_tests.dot`、`siligen_job_ingest_unit_tests.dot`
- Python 图输出：`tests/reports/dependency-graphs/python/`
- CMake scratch build：`build/dependency-graphs-cmake/`

### 2.4 Coverage

- Python coverage 输出：`tests/reports/coverage/python/`
- C/C++ coverage 输出：`tests/reports/coverage/cpp/`
- 阈值配置：`scripts/validation/coverage-thresholds.json`

### 2.5 Static Analysis

- Cppcheck 输出：`tests/reports/static-analysis/cppcheck/`

### 2.6 Legacy Exit

- 根入口：`legacy-exit-check.ps1`
- 核心实现：`scripts/migration/legacy-exit-checks.py`
- 真值表：`scripts/migration/legacy_fact_catalog.json`
- 输出：`tests/reports/legacy-exit/`

## 3. 首次基线结果

### 3.1 已跑通的 smoke 结果

| 能力 | 结果 |
|---|---|
| `legacy-exit-check.ps1` | 已跑通，`blocking_count=0` |
| Import Linter | 已跑通，hard contract 全通过，2 个 advisory 失败 |
| Dependency Graph | 已跑通，CMake/Python 图均可导出 |
| Python branch coverage | 已跑通，global branch `19.69%` |
| C/C++ coverage | 已接线，当前 `not-collected` |
| Cppcheck | 已接线，当前环境缺工具，`tool-missing` |
| `ci.ps1` 前半段 | 已验证串起 `legacy-exit -> semgrep`，当前在 Semgrep tool-error 处阻断 |

### 3.2 当前已知基线偏差

- Semgrep CE 规则、wrapper、CI 接线已落地，但当前 Windows 本机的 `semgrep-core` 返回 `RPC input error: Expected a number, got ''`，因此以 `tool-error` 形式 hard-fail，并将原始输出写入报告。
- `legacy-exit` 已收敛到 8 条 `controlled-exception`，均为模块内 `tests/integration` 文本命中导致的历史误报，不再阻断。
- Import Linter 当前保留 2 条 advisory 失败，用于暴露 HMI owner/UI 的既有耦合：
  - `hmi_application -> hmi_client.client.*`
  - `hmi_client.ui.main_window -> hmi_application.preview_session`
- Python coverage 已出报告，但 `apps/hmi-app/src/hmi_client` 分支覆盖率仅 `10.13%`，低于当前关键路径阈值 `20%`。

## 4. 本轮最小闭环结论

- 根脚本、规则、配置、报告目录、文档已经全部入仓。
- `tools/` 根目录已删除，相关 loose mock 脚本已迁移到 `scripts/validation/check_no_loose_mock.py`。
- 本轮以“先阻断继续恶化”为目标，优先冻结退休根路径、HMI/Python 导入边界、legacy 文本残留和报告出口。
