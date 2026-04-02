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

## 当前 canonical samples

- `rect_diag.dxf`：默认 `small` regression sample
- `rect_medium_ladder.dxf`：默认 `medium` nightly-performance sample，line-only serpentine，`383` segments
- `rect_large_ladder.dxf`：默认 `large` nightly-performance sample，line-only serpentine，`1919` segments

## 生成规则

- `rect_medium_ladder.dxf` 与 `rect_large_ladder.dxf` 由 `tests/performance/generate_canonical_dxf_samples.py` 生成。
- 生成规则固定为 line-only serpentine，不允许引入 arc/spline 或外部未审计 DXF 来源。

## 来源与归位约束

- 历史来源 `examples/dxf/` 仅作为迁移来源，不再是正式 owner 面。
- 需要长期复用或参与验收回归的 DXF 样本必须落在 `samples/dxf/`。
- 禁止新增稳定样本回流到 `examples/dxf/`。
