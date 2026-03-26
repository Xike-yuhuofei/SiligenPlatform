# Quickstart: HMI 在线点胶轨迹预览一致性验证

## 1. 目标

在机台已经准备就绪的前提下，使用 HMI `online` 模式加载真实 DXF，生成可追溯的权威在线预览，并验证其与上位机准备下发到运动控制卡的数据在业务语义上保持一致。

## 2. 必备输入

开始前确认以下资产存在：

1. 规格与计划资产
   `D:\Projects\SiligenSuite\specs\test\hmi\TASK-001-offline-dxf-preview-consistency\spec.md`
   `D:\Projects\SiligenSuite\specs\test\hmi\TASK-001-offline-dxf-preview-consistency\plan.md`
2. 强制 DXF 基线
   `D:\Projects\SiligenSuite\samples\dxf\rect_diag.dxf`
3. 现有几何基线
   `D:\Projects\SiligenSuite\tests\baselines\preview\rect_diag.preview-snapshot-baseline.json`
4. 正式协议源
   `D:\Projects\SiligenSuite\shared\contracts\application\commands\dxf.command-set.json`
5. 现有真实预览验收清单
   `D:\Projects\SiligenSuite\docs\validation\dispense-trajectory-preview-real-acceptance-checklist.md`

## 3. 最小准备

在仓库根目录 `D:\Projects\SiligenSuite\` 执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite apps
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite contracts,protocol-compatibility
python -m pytest .\tests\contracts\test_protocol_compatibility.py -q
python -m pytest .\tests\e2e\first-layer\test_real_preview_snapshot_geometry.py -q
```

## 4. 在线预览验证步骤

完成实现后，按以下顺序执行：

1. 使用在线 smoke 证明 HMI 已进入 `online_ready`：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\apps\hmi-app\scripts\online-smoke.ps1 `
  -GatewayExe '<path-to-built-siligen_runtime_gateway.exe>' `
  -GatewayConfig 'config\machine\machine_config.ini'
```

2. 使用真实机台配置跑 canonical DXF 快照证据脚本：

```powershell
python .\tests\e2e\hardware-in-loop\run_real_dxf_preview_snapshot.py --config-mode real
```

脚本输出至少应包含：

- `plan-prepare.json`
- `snapshot.json`
- `trajectory_polyline.json`
- `preview-verdict.json`
- `preview-evidence.md`

3. 启动 HMI 在线模式：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\apps\hmi-app\run.ps1 -Mode online
```

4. 在 HMI 中加载 `D:\Projects\SiligenSuite\samples\dxf\rect_diag.dxf`。
5. 触发在线预览生成，并确认当前上下文属于在线就绪状态。
6. 确认预览来源标签为 `runtime_snapshot`。
7. 确认当前预览可回链到同一 `plan_id / plan_fingerprint / snapshot_hash`，并且与准备下发到运动控制卡的数据在语义上无无法解释差异。
8. 检查 `preview-verdict.json`，确认其结论为 `passed`，且三项语义匹配均为 `true`。
9. 保存本次在线预览证据。若复用 HIL 脚本，默认报告目录为 `tests\reports\adhoc\real-dxf-preview-snapshot-canonical\<timestamp>\`；若补充人工验收包，推荐保存到 `tests\reports\online-preview\<timestamp>\`。

## 5. 一致性判定

本特性完成后的在线预览通过标准：

1. 预览输入来自真实 DXF，而不是 mock / synthetic 几何。
2. `preview_source` 明确存在，且等于 `runtime_snapshot`。
3. 快照可回链到同一 `plan_id / plan_fingerprint / snapshot_hash`。
4. 与执行准备结果在几何路径、路径顺序和点胶相关运动语义上无无法解释差异。
5. 对 `rect_diag.dxf`，应能辨认矩形边段与对角线段，并与 `rect_diag.preview-snapshot-baseline.json` 保持容差内一致。
6. 对 `mock_synthetic`、来源缺失、证据缺失或上下文不匹配场景，`preview-verdict.json` 必须分别落为 `invalid-source`、`not-ready`、`incomplete` 或 `mismatch`，不得出现 `passed`。

## 6. 进入 `speckit-tasks` 的条件

满足以下条件后即可进入 `speckit-tasks`：

1. `plan.md`、`research.md`、`data-model.md`、`contracts\*` 已冻结。
2. 已明确在线就绪前提与预览权威来源的边界，不再混淆 `runtime_snapshot`、`mock_synthetic` 与历史残留结果。
3. 验证命令与证据目录已固定到 canonical roots。
