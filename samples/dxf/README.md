# samples/dxf

`samples/dxf/` 是 `M2 dxf-geometry` 相关静态样本的正式承载面（canonical root）。

## 正式职责

- 承载可复用、可回归、可审计的 DXF 输入样本。
- 作为几何解析与上游链验证的稳定输入事实源。
- 为 `dsp-e2e-spec-s04` 与上游链检查提供统一样本入口。

## 输入范围

- 原始几何输入：`.dxf`
- 可复现场景输入：与 DXF 直接绑定的解析前置素材（若有）
- 样本索引与说明文档

## 样本分层

### canonical performance samples

- `rect_diag.dxf`：默认 `small` regression sample
- `rect_medium_ladder.dxf`：默认 `medium` nightly-performance sample，line-only serpentine，`383` segments
- `rect_large_ladder.dxf`：默认 `large` nightly-performance sample，line-only serpentine，`1919` segments

### importer canonical truth / geometry coverage samples

- `bra.dxf`：闭合 `LWPOLYLINE` 轮廓样本，用于覆盖有机轮廓与闭合 polyline 导入；已升级为 full-chain canonical producer case
- `Demo.dxf`：`LINE` + `LWPOLYLINE` + `POINT` 混合实体样本，用于覆盖混合实体导入与点噪声容忍
- `arc_circle_quadrants.dxf`：仓内最小 `ARC` 闭合轮廓样本，由四段四分之一圆弧构成，用于覆盖 arc 解析与闭环归一化；已升级为 full-chain canonical producer case
- `geometry_zoo.dxf`：仓内综合 primitive 样本，覆盖 `LINE` / `ARC` / `CIRCLE` / `POINT` / `LWPOLYLINE` / `SPLINE` / `ELLIPSE` 导入

### diagnostic / exploratory samples

- `Demo-1.dxf`：闭合轮廓方向与折返问题的诊断样本；不自动进入默认 performance 或 shared-asset registry
- `I_LOVE_YOU.dxf`：轻量 `LINE` 文字样本，保留用于人工浏览与轻量 smoke；不作为默认回归输入

## shared truth matrix 对接

- `shared/contracts/engineering/fixtures/dxf-truth-matrix.json` 是当前 full-chain canonical producer case 的正式 owner manifest
- 当前被 `L3/L4/L5` runner 消费的 full-chain case 只有 `rect_diag`、`bra`、`arc_circle_quadrants`
- `rect_diag.dxf` 仍是默认 runtime / HIL canonical producer case
- `Demo.dxf` 与 `geometry_zoo.dxf` 仍是 importer truth sample，不会自动进入 preview / simulation / HIL full-chain runner
- 是否进入 full-chain case，不由文件名或样本位置隐式决定，而由 truth matrix manifest 明确声明

## runner 用法

- `L3` engineering regression 默认枚举全部 full-chain case；可用 `--case-id <case>` 缩小范围
- `L4` simulated-line compat 默认枚举全部 full-chain case；可用 `--compat-case-id <case>` 缩小范围
- `L5` HIL runner 默认仍走 `rect_diag`；只有显式传 `--dxf-case-id <case>` 时才切到其他 full-chain case

正式示例：

```powershell
python .\tests\integration\scenarios\run_engineering_regression.py
```

```powershell
python .\tests\integration\scenarios\run_engineering_regression.py --case-id bra
```

```powershell
python .\tests\e2e\simulated-line\run_simulated_line.py --mode compat --compat-case-id arc_circle_quadrants
```

```powershell
python .\tests\e2e\hardware-in-loop\run_hil_closed_loop.py --dxf-case-id rect_diag
```

## 生成与维护规则

- `rect_medium_ladder.dxf` 与 `rect_large_ladder.dxf` 由 `tests/performance/generate_canonical_dxf_samples.py` 生成。
- 生成规则固定为 line-only serpentine，不允许引入 arc/spline 或外部未审计 DXF 来源。
- `arc_circle_quadrants.dxf` 为仓内手工维护的最小 `ARC` 样本，不依赖外部未审计 DXF 来源。
- `geometry_zoo.dxf` 为 importer-only truth sample，不自动进入 preview / simulation / HIL canonical producer。
- `bra.dxf`、`arc_circle_quadrants.dxf` 已冻结 preview / simulation consumer，额外在 `shared/contracts/engineering/fixtures/cases/` 下维护 full-chain canonical fixture 副本。
- `Demo.dxf` 作为 importer truth sample 维护在本目录中，但不会在未冻结 consumer 之前自动进入默认 performance gate 或 full-chain shared contract fixture。

## 来源与归位约束

- 历史来源 `examples/dxf/` 仅作为迁移来源，不再是正式 owner 面。
- 需要长期复用或参与验收回归的 DXF 样本必须落在 `samples/dxf/`。
- 禁止新增稳定样本回流到 `examples/dxf/`。
- DXF 样本进入正式长期使用前，必须至少在本 README 或对应 manifest 中可回链，不允许无 owner 的孤立文件。
- `nightly-performance` 默认只认 `canonical performance samples`；importer truth 或诊断样本不得被隐式提升为正式 performance 输入。
