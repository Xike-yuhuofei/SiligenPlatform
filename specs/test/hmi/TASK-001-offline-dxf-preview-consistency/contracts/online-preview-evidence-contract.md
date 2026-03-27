# Contract: 在线预览验收证据包

## 1. 目的

定义一次 HMI 在线预览验收必须产出的证据包内容，用于证明：

1. 输入确实是真实 DXF
2. HMI 运行在 `online` 模式，且当时已经处于 `online_ready`
3. 预览来源是权威 `runtime_snapshot` 而不是 mock 或历史残留结果
4. 预览与上位机准备下发到运动控制卡的数据在业务语义上保持一致

## 2. 证据目录

推荐目录：

```text
D:\Projects\SiligenSuite\tests\reports\online-preview\<timestamp>\
```

若复用仓内现有自动化脚本，也允许使用其默认输出目录：

```text
D:\Projects\SiligenSuite\tests\reports\adhoc\real-dxf-preview-snapshot-canonical\<timestamp>\
```

## 3. 必需文件

| 文件 | 必需 | 说明 |
|---|---|---|
| `plan-prepare.json` | 是 | `dxf.plan.prepare` 原始响应，必须包含 `plan_id` 与 `plan_fingerprint` |
| `snapshot.json` | 是 | `dxf.preview.snapshot` 原始响应 |
| `trajectory_polyline.json` | 是 | 用于渲染的点列 |
| `preview-verdict.json` | 是 | 本次验收结论 |
| `preview-evidence.md` | 是 | 面向人工审阅的摘要 |
| `hmi-online-screenshot.png` | 否 | 预览画面截图 |
| `baseline-compare.json` | 否 | 与 canonical 基线对比结果 |

## 4. `preview-verdict.json` 最小字段

| 字段 | 必填 | 说明 |
|---|---|---|
| `verdict` | 是 | `passed` / `failed` / `invalid-source` / `mismatch` / `not-ready` / `incomplete` |
| `launch_mode` | 是 | 必须为 `online` |
| `online_ready` | 是 | 必须为 `true` 才允许通过 |
| `dxf_file` | 是 | 被验收的 DXF |
| `artifact_id` | 是 | 当前 DXF 对应 artifact |
| `preview_source` | 是 | 必须为 `runtime_snapshot` |
| `snapshot_hash` | 是 | 快照签名 |
| `plan_id` | 是 | 执行准备标识 |
| `plan_fingerprint` | 是 | 执行准备签名 |
| `geometry_semantics_match` | 是 | 几何是否一致 |
| `order_semantics_match` | 是 | 顺序是否一致 |
| `dispense_motion_semantics_match` | 是 | 点胶相关运动语义是否一致 |
| `failure_reason` | 否 | 失败时必须给出 |

## 5. 通过条件

只有同时满足以下条件，`verdict` 才允许为 `passed`：

1. `launch_mode == online`
2. `online_ready == true`
3. `preview_source == runtime_snapshot`
4. `snapshot_hash`、`plan_id`、`plan_fingerprint` 全部存在
5. `geometry_semantics_match == true`
6. `order_semantics_match == true`
7. `dispense_motion_semantics_match == true`
8. `trajectory_polyline.json` 与 `snapshot.json` 中的点数和摘要字段可互相校验

## 6. 失败边界

以下情况必须落为非通过结论：

1. `preview_source == mock_synthetic`
2. 当前上下文未达到 `online_ready`
3. 证据包缺失 `plan-prepare.json`、`snapshot.json` 或 `trajectory_polyline.json`
4. 预览结果无法回链到同一 DXF、同一 `plan_id`、同一 `plan_fingerprint`
5. 只证明了几何外观相似，但不能证明执行顺序或点胶运动语义一致
6. UI 截图存在，但原始快照、plan 准备结果或 verdict 丢失

## 7. 与现有仓内验证资产的关系

本证据包应与以下仓内资产保持一致：

- `D:\Projects\SiligenSuite\tests\baselines\preview\rect_diag.preview-snapshot-baseline.json`
- `D:\Projects\SiligenSuite\tests\e2e\first-layer\test_real_preview_snapshot_geometry.py`
- `D:\Projects\SiligenSuite\tests\e2e\hardware-in-loop\run_real_dxf_preview_snapshot.py`
- `D:\Projects\SiligenSuite\docs\validation\dispense-trajectory-preview-real-acceptance-checklist.md`

规则：

1. 本特性的在线证据包不能绕开上述 canonical baseline 与 checklist。
2. 若人工验收包字段多于现有自动化脚本输出，必须通过增量文件补齐，不能用口头说明替代原始证据。
