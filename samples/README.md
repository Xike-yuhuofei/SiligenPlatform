# samples

`samples/` 是稳定样本、golden cases 与 fixtures 的正式承载根（canonical root）。

## 归位入口

- `samples/README.md` 是稳定样本与 fixtures 的总入口（sample index）。
- `samples/golden/README.md` 是 golden 输入与基准样本的正式入口。
- 领域子目录（如 `samples/dxf/`）仅作为分域说明入口，其承载关系统一回链到本页。

## 正式职责

- 承载可复用、可回归、可审计的样本输入与基准样本。
- 为上游工程链、集成验证与验收基线提供稳定样本事实源。
- 提供 `examples/` 历史样本根的目标收敛面。

## 分层测试体系下的 canonical taxonomy

- `samples/dxf/`：DXF 输入样本，按 `canonical performance`、`importer canonical truth / geometry coverage`、`diagnostic / exploratory` 三类维护；详见 `samples/dxf/README.md`
- `samples/simulation/`：simulated-line / fault scenario 输入
- `samples/replay-data/`：recording / preview 相关回放工件

## 当前 inventory

- `samples/dxf/rect_diag.dxf`：默认 DXF regression sample
- `samples/dxf/rect_medium_ladder.dxf`：canonical medium nightly-performance DXF sample
- `samples/dxf/rect_large_ladder.dxf`：canonical large nightly-performance DXF sample
- `samples/dxf/bra.dxf`：闭合 `LWPOLYLINE` importer truth 样本；已升级为 full-chain canonical producer case
- `samples/dxf/Demo.dxf`：混合实体与点噪声几何覆盖样本
- `samples/dxf/arc_circle_quadrants.dxf`：最小 `ARC` 闭合轮廓 importer truth 样本；已升级为 full-chain canonical producer case
- `samples/dxf/geometry_zoo.dxf`：supported primitive 矩阵 importer truth 样本
- `samples/simulation/rect_diag.simulation-input.json`：compat / scheme C 主输入
- `samples/simulation/sample_trajectory.json`：混合轨迹输入
- `samples/simulation/invalid_empty_segments.simulation-input.json`：结构化失败输入
- `samples/simulation/following_error_quantized.simulation-input.json`：跟随误差输入
- `samples/replay-data/rect_diag.scheme_c.recording.json`：rect_diag replay canonical export
- `samples/replay-data/sample_trajectory.scheme_c.recording.json`：sample_trajectory replay canonical export

## 迁移治理

- `samples/` 是正式样本真值根，禁止再把长期样本落回 `examples/` 或 `data/baselines/`。
- replay / simulation / dxf 样本必须在 README 或 manifest 中可回链，不允许无 owner 的孤立文件。

## 非职责边界

- 不承载临时实验产物与一次性调试输出。
- 不承载业务 owner 实现代码。
- 不承载运行期自动生成缓存或报告文件。

## 迁移与回流约束

- `examples/` 当前仅作为迁移来源与 redirect/tombstone 承载面，不再作为正式样本 owner。
- 新增长期样本必须优先写入 `samples/`，禁止回流到 `examples/`。
- 样本迁移完成后，应通过根级验证与文档回链（含 `examples/README.md` 退出口径）确认退出条件。
