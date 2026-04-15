# Fixtures

本目录下的 fixture 分成三层真值：

- full-chain canonical producer case：为工程契约包提供跨 owner 稳定基线，可复用到 preview / simulation / HMI / e2e
- topology contract case：只服务拓扑 / process-path owner 级回归，不承担 full-chain canonical producer 语义
- importer canonical sample：保留在 `samples/dxf/`，只冻结 `DXF -> PB` 导入支持面，不自动提升为 full-chain canonical producer

正式分层清单见 `fixtures/dxf-truth-matrix.json`。

## full-chain canonical producer case

当前 full-chain canonical case 矩阵为：

- `cases/rect_diag`
  - 拓扑族：`branch_or_revisit`
  - 角色：默认 runtime / HIL canonical producer case
- `cases/bra`
  - 拓扑族：`closed_loop_polyline`
  - 角色：闭合有机 polyline canonical producer case
- `cases/arc_circle_quadrants`
  - 拓扑族：`closed_loop_arc`
  - 角色：最小 arc-only canonical producer case

每个 full-chain canonical case 都冻结以下工件：

- `<case>.dxf`
- `<case>.pb`
- `preview-artifact.json`
- `simulation-input.json`

这些 fixture 的目的不是替代生产实现，而是为契约包提供：

- 稳定的回归样本
- canonical producer 输出基线
- `apps/hmi-app` / `tests/e2e/simulated-line` 兼容验证输入

再生与校验入口：

- `python scripts/engineering-data/generate_dxf_truth_matrix_fixtures.py --check`
- `python scripts/engineering-data/generate_dxf_truth_matrix_fixtures.py --write`
- 可叠加 `--case-id <case>` 只处理指定 full-chain canonical case

## topology contract case

以下 case 只服务 `M6 process-path` owner 回归，不参与 canonical producer 输出：

- `cases/point_mixed/point_mixed.pb`
- `cases/fragmented_chain/fragmented_chain.pb`
- `cases/multi_contour_reorder/multi_contour_reorder.pb`
- `cases/intersection_split/intersection_split.pb`

约束：

- 这些 case 默认只提交 `.pb` 与 case README，不补 `preview-artifact.json` / `simulation-input.json`
- 每个 case README 都必须写清楚用途、期望 diagnostics、是否属于 canonical producer
- 新增 process-path regression case 时，优先复用 `scripts/engineering-data/generate_process_path_regression_cases.py` 生成，避免手写二进制样本漂移

## importer canonical sample

以下 importer 真值样本保留在 `samples/dxf/`，不直接复制到 `shared/contracts`：

- `Demo.dxf`：混合 `LINE` / `LWPOLYLINE` / `POINT` importer truth
- `geometry_zoo.dxf`：支持 primitive 矩阵 importer truth

约束：

- importer canonical sample 负责冻结 `DXF -> PB` 支持面
- 只有当某个样本的 preview / simulation / execution 语义已冻结，才允许升级为 full-chain canonical producer case
