# Removed Legacy Items

更新时间：`2026-03-19`

本文档记录已经实际执行删除的 legacy 项，并保留对应风险、回滚方式与验证结果。

## 1. 已移除项

| 批次 | 路径 | 类型 | 删除原因 | 风险 | 回滚方式 |
|---|---|---|---|---|---|
| v1 | `hmi-client/tests/` | 重复测试树 | legacy `scripts/test.ps1` 已转发到 canonical app，仓内无活跃自动化引用 | 少量人工习惯会受影响 | 从删除前工作区备份恢复目录 |
| v1 | `hmi-client/pyproject.toml` | 重复安装面 | 避免继续从旧目录做 editable install | 外部个人脚本可能需要改命令 | 从删除前工作区备份恢复文件 |
| v1 | `hmi-client/requirements.txt` | 重复安装面 | 根级安装入口已切到 canonical app | 同上 | 从删除前工作区备份恢复文件 |
| v1 | `hmi-client/packaging/` | 失效打包面 | 旧打包目录已不在当前发布链 | 风险极低 | 从删除前工作区备份恢复目录 |
| v1 | `dxf-editor/tests/` | 重复测试树 | legacy `scripts/test.ps1` 已转发到 canonical app，仓内无活跃自动化引用 | 少量人工习惯会受影响 | 从删除前工作区备份恢复目录 |
| v1 | `dxf-editor/pyproject.toml` | 重复安装面 | 避免继续从旧目录做 editable install | 外部个人脚本可能需要改命令 | 从删除前工作区备份恢复文件 |
| v1 | `dxf-editor/requirements.txt` | 重复安装面 | 根级安装入口已切到 canonical app | 同上 | 从删除前工作区备份恢复文件 |
| v1 | `dxf-editor/packaging/` | 失效打包面 | 旧打包目录已不在当前发布链 | 风险极低 | 从删除前工作区备份恢复目录 |
| v2 | `hmi-client/.github/workflows/ci.yml`、`hmi-client/.github/PULL_REQUEST_TEMPLATE.md` | 重复 CI 壳 | legacy 本地 workflow/template 已不进入当前根级 CI 链路，且内容已与现状脱节 | 极低；主要是历史文档感知变化 | 从删除前工作区备份恢复这两个文件，并回退相关 README/审计文档 |
| v2 | `dxf-editor/.github/workflows/ci.yml`、`dxf-editor/.github/PULL_REQUEST_TEMPLATE.md` | 重复 CI 壳 | legacy 本地 workflow/template 已不进入当前根级 CI 链路，且内容已与现状脱节 | 极低；主要是历史文档感知变化 | 从删除前工作区备份恢复这两个文件，并回退相关 README/审计文档 |
| v3 | `hmi-client/` | legacy HMI 兼容壳整目录 | HMI 真实源码、脚本与测试入口已全部收口到 `apps/hmi-app`；历史材料已迁入根级 archive；legacy-exit 检查已切换为“目录必须不存在” | 工作区外脚本若仍引用旧路径会失效 | 从 `tmp/legacy-removal-backups/20260319-151820/hmi-client` 恢复目录，并重跑 HMI/legacy-exit 验证 |

## 2. 本轮冻结但不删除项

| 路径 | 处理方式 | 原因 | 风险 | 回滚方式 |
|---|---|---|---|---|
| `dxf-editor/scripts/*` | 冻结 | 仍承担历史命令入口 | 误删会切断兼容命令 | 从备份恢复脚本 |
| `dxf-editor/src/` | 冻结 | 重复 Python 包根仍可能被外部 import | 误删会导致残余 import 失败 | 从备份恢复目录 |
| `control-core/src/adapters/tcp/`、`control-core/modules/control-gateway/` | 冻结 | 兼容 target alias 仍在用 | 误删会破坏 transport compatibility | 从备份恢复目录并重跑兼容测试 |

## 3. 仍不可删除项

| 路径 | 原因 | 风险 | 回滚方式 |
|---|---|---|---|
| `dxf-pipeline/` | canonical HMI 预览仍直接依赖 | 删除会打断 DXF 预览与契约验证 | 从备份恢复目录并重跑 engineering/HMI 验证 |
| `control-core/build/bin/**` | 仍是 TCP server/CLI 真实产物来源 | 删除会打断 HIL、CLI、现场链路 | 恢复构建产物或重新构建 |
| `control-core/apps/control-runtime/`、`control-core/apps/control-tcp-server/` | runtime/tcp 迁移未闭环 | 删除会打断运行链路 | 从备份恢复目录并重跑 dry-run/HIL 验证 |
| `control-core/src/domain/`、`src/application/`、`modules/process-core/`、`modules/motion-core/` | process/runtime 真实实现仍在 legacy | 删除会破坏构建与测试 | 从备份恢复目录并重跑相关回归 |
| `control-core/modules/device-hal/`、`modules/shared-kernel/` | device/shared-kernel 仍未脱离 legacy include | 删除会破坏 device 相关构建 | 从备份恢复目录并重跑相关构建/测试 |
| `control-core/config/machine_config.ini`、`control-core/data/recipes/*` | 仍有部署/现场 fallback 价值 | 删除会影响部署与回滚 | 从备份恢复资产并复核部署 |

## 4. 删除后验证

本轮计划并执行以下验证：

```powershell
Set-Location D:\Projects\SiligenSuite
powershell -NoProfile -ExecutionPolicy Bypass -File .\apps\hmi-app\scripts\test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\dxf-editor\scripts\test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\legacy-cleanup-v2-apps -FailOnKnownFailure
powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1
```

验证目标：

- canonical HMI 测试入口保持可用
- canonical app 测试入口保持通过
- 本轮删除没有引入新的 build/test/CI 回归
- `hmi-client/` 删除后不会被 legacy-exit 门禁放回执行链路
