# Deployment

更新时间：`2026-03-26`

## 1. 正式入口

部署与交付从仓库根发起：

```powershell
Set-Location <repo-root>
.\build.ps1
.\test.ps1
.\ci.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1
```

## 2. 部署面

| 主题 | 正式根 |
|---|---|
| 进程入口 | `apps/` |
| 自动化脚本 | `scripts/` |
| 部署材料 | `deploy/` |
| 配置源 | `config/` |
| 数据源 | `data/` |
| 样本与回归输入 | `samples/` |

## 3. 发布前统一检查

以下检查是正式发布前的最低执行链，不替代 [docs/runtime/release-readiness-standard.md](/D:/Projects/SiligenSuite/docs/runtime/release-readiness-standard.md) 的全部门禁：

```powershell
.\scripts\validation\install-python-deps.ps1
.\legacy-exit-check.ps1 -Profile Local -ReportDir tests\reports\verify\pre-release-legacy-exit
.\build.ps1 -Profile Local -Suite all
.\test.ps1 -Profile Local -Suite all -ReportDir tests\reports\verify\pre-release
.\ci.ps1 -Suite all
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1 -ReportRoot tests\reports\verify\pre-release-local-gate
```

正式发版前还必须补齐：

- `.\release-check.ps1 -Version <x.y.z> -IncludeHardwareSmoke`
- 仓外交付物观察证据
- HIL / 真机 / 长稳证据

## 4. 交付约束

- 部署说明优先落在 `deploy/`，不再使用历史目录承载正式交付口径。
- 进程入口统一为 `apps/runtime-service`、`apps/runtime-gateway`、`apps/planner-cli`、`apps/hmi-app`。
- 配置和数据默认只认 `config/` 与 `data/`。
- 已删除旧根不得回流。
- `hardware smoke` 不是正式现场验收替代项。
