# process-path

`modules/process-path/` 是 `M6 process-path` 的 canonical owner 入口。

## Owner 范围

- 基于 `CoordinateTransformSet` 与上游规则构建 `ProcessPath`。
- 承接路径段、路径序列与路径校验的模块 owner 边界。
- 为 `M7 motion-planning` 提供正式路径事实，不回退为执行层的临时中间态。
- 仅负责 `Primitive/Path/ProcessPath` 与路径规范化、工艺标注、路径整形，不再承载运动配置、轨迹结果或运动求解语义。
- `ProcessPathFacade` 是当前唯一 public application entry；`process_path/contracts/*` 是唯一 live contract surface。
- `PathGenerationRequest.alignment` 当前仅承载来自 `M5` 的 provenance / owner 证据，不在 `M6` 内作为 live 几何变换执行。
- `PathGenerationRequest.topology_repair.policy` 默认值为 `Off`；只有显式 `Auto` 才进入 repair/reorder 主链。
- `TopologyRepairPolicy::Auto` 要求 `metadata` 与 `primitives` 一一对应，否则在输入校验阶段直接失败。
- `PathGenerationResult.shaped_path` 是对下游负责的 final authority；`process_path` 仅保留为 pre-shape 内部工艺事实。

## Owner 入口

- 构建入口：`CMakeLists.txt`（target：`siligen_module_process_path`）。
- 模块契约入口：`contracts/README.md`。

## 边界约束

- `M4`、`M5` 只提供规划和对齐输入，不承载 `M6` 终态 owner 事实。
- `M8`、`M9` 只能消费 `ProcessPath` 结果，不得反向重排或修补路径事实。
- 跨模块稳定工程契约应维护在 `shared/contracts/engineering/`，本目录只承载 `M6` 专属契约。
- `M6` live 输入面只保留 `Siligen::ProcessPath::Contracts::IPathSourcePort`；DXF 专属 legacy contract 不再暴露在本模块 public surface。

## 当前正式事实来源

- `modules/process-path/contracts/include/process_path/contracts/`
- `modules/process-path/application/include/application/services/process_path/`
- `modules/process-path/domain/trajectory/`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `contracts/` 与 `application/` 已成为当前 `M6` 的 canonical public surface。
- `IPathSourcePort` 已收口到 `contracts/include/process_path/contracts/`，并统一使用 `Siligen::ProcessPath::Contracts` 语义命名空间。
- `domain/trajectory/ports/*.h` bridge 与 DXF legacy contract 已退出 live build / public surface。
- `ProcessPathFacade` 是唯一对外应用入口；新 consumer 不得直连 workflow 下的 trajectory 历史副本。
- `domain/trajectory/` 承载 `M6` 领域规则与值对象，作为 owner implementation root 继续保留。
- 本模块不再对外公开运动配置、规划报告、轨迹别名、legacy DXF port 或运动规划服务。

## 测试基线

- `modules/process-path/tests/` 负责 `process-path` owner 级 `unit + contracts + golden + integration` 证明。
- 当前最小正式矩阵聚焦 `ProcessPathFacade` request/result、`IPathSourcePort` contract、lead-on/lead-off 语义和到 `motion-planning` 的公共链路交接。
- 仓库级 `tests/` 只消费跨 owner 场景，不替代本模块内 contract。
