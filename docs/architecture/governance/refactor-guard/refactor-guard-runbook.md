# Refactor Guard Runbook

更新时间：2026-04-07

## 1. 本地执行入口

所有命令均在仓库根执行。

### 1.1 安装 Python 侧依赖

```powershell
.\scripts\validation\install-python-deps.ps1
```

### 1.2 单项执行

```powershell
.\legacy-exit-check.ps1 -Profile Local -ReportDir tests\reports\legacy-exit
```

```powershell
.\scripts\validation\invoke-semgrep.ps1 -ReportDir tests\reports\semgrep
```

```powershell
.\scripts\validation\invoke-import-linter.ps1 -ReportDir tests\reports\import-linter
```

```powershell
.\scripts\validation\invoke-dependency-graph-export.ps1 -ReportDir tests\reports\dependency-graphs -SoftFail
```

```powershell
.\scripts\validation\invoke-python-coverage.ps1 -ReportDir tests\reports\coverage\python
```

```powershell
.\scripts\validation\invoke-cpp-coverage.ps1 -ReportDir tests\reports\coverage\cpp
```

```powershell
.\scripts\validation\invoke-cppcheck.ps1 -ReportDir tests\reports\static-analysis\cppcheck
```

## 2. 根脚本执行

### 2.1 构建

```powershell
.\build.ps1 -Profile Local -Suite apps
```

启用 C/C++ coverage：

```powershell
.\build.ps1 -Profile Local -Suite apps -EnableCppCoverage
```

### 2.2 测试

```powershell
.\test.ps1 -Profile Local -Suite apps -ReportDir tests\reports
```

启用 coverage：

```powershell
.\test.ps1 -Profile Local -Suite apps -ReportDir tests\reports -EnablePythonCoverage -EnableCppCoverage
```

### 2.3 CI 编排

```powershell
.\ci.ps1 -Suite all -ReportDir tests\reports\ci
```

启用 coverage：

```powershell
.\ci.ps1 -Suite all -ReportDir tests\reports\ci -EnablePythonCoverage -EnableCppCoverage
```

## 3. CI 阶段顺序

`ci.ps1` 当前顺序：

1. `legacy-exit-check.ps1`
2. `scripts/validation/invoke-semgrep.ps1`
3. `scripts/validation/invoke-import-linter.ps1`
4. `build.ps1`
5. `test.ps1`
6. `scripts/validation/invoke-cppcheck.ps1`
7. `scripts/validation/invoke-dependency-graph-export.ps1`
8. `scripts/validation/run-local-validation-gate.ps1`

失败原则：

- `legacy-exit` / `Semgrep` / Import Linter hard contracts：默认失败即终止
- `Cppcheck` / dependency graph / coverage 阈值：当前为 report-only

## 4. 报告目录

| 能力 | 报告目录 |
|---|---|
| Legacy Exit | `tests/reports/legacy-exit/` |
| Semgrep | `tests/reports/semgrep/` |
| Import Linter | `tests/reports/import-linter/` |
| Python coverage | `tests/reports/coverage/python/` |
| C/C++ coverage | `tests/reports/coverage/cpp/` |
| Cppcheck | `tests/reports/static-analysis/cppcheck/` |
| Dependency graphs | `tests/reports/dependency-graphs/` |

Dependency graph 关键文件：

- CMake 总图：`tests/reports/dependency-graphs/cmake/workspace-targets.dot`
- CMake 关键 target 子图：`siligen_runtime_service.dot`、`siligen_runtime_gateway.dot`、`siligen_planner_cli.dot`、`siligen_runtime_host_unit_tests.dot`、`siligen_dxf_geometry_unit_tests.dot`、`siligen_job_ingest_unit_tests.dot`
- Python 包图：`tests/reports/dependency-graphs/python/*.dot` 与 `*.deps.json`

## 5. 当前 hard fail / report-only

### Hard fail

- `legacy-exit-check.ps1`
- `invoke-semgrep.ps1`
- Import Linter hard contracts

### Report-only

- Import Linter advisory contracts
- Python coverage 阈值
- C/C++ coverage 阈值
- Cppcheck
- dependency graph export

## 6. 已知限制

- 当前 Windows 本机的 `semgrep-core` 扫描时会返回 `RPC input error: Expected a number, got ''`。wrapper 已将其收敛为：
  - `semgrep-results.json`
  - `semgrep-summary.md`
  - `semgrep-tool-output.txt`
  - CI 视为 hard-fail
- `cppcheck` 当前环境未安装，因此只能输出 `tool-missing` 报告。
- C/C++ coverage 已完成开关与汇总脚本，但当前尚未采集 `.profraw`，因此报告为 `not-collected`。
- `legacy-exit` 仍保留 8 条 `controlled-exception`，根因是模块内 canonical `tests/integration` 文本命中 legacy regex。

## 7. 下一步收敛方向

1. 修复或替换当前 Windows Semgrep CE 运行时，恢复真实 finding 扫描。
2. 将 `legacy-exit` 的 retired-root regex 从“路径片段命中”收敛到“仓库根引用命中”，移除现有 controlled exception。
3. 收敛 `hmi_application -> hmi_client.client.*` 与 `hmi_client.ui -> hmi_application`，再把 advisory contracts 升级为 hard。
4. 在一次 `build.ps1 -EnableCppCoverage` + `test.ps1 -EnableCppCoverage` 后补齐 `.profraw` 采集闭环。
5. 安装 `cppcheck` 后根据真实噪声水平决定首批 blocking 等级。
