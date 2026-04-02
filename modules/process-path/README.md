# process-path

`modules/process-path/` 是 `M6 process-path` 的 canonical owner 入口。

## Owner 范围

- 基于 `CoordinateTransformSet` 与上游规则构建 `ProcessPath`。
- 承接路径段、路径序列与路径校验的模块 owner 边界。
- 为 `M7 motion-planning` 提供正式路径事实，不回退为执行层的临时中间态。
- 仅负责 `Primitive/Path/ProcessPath` 与路径规范化、工艺标注、路径整形，不再承载运动配置、轨迹结果或运动求解语义。
- `ProcessPathFacade` request/result 是当前唯一 public entry 与 public contract 入口。

## Owner 入口

- 构建入口：`CMakeLists.txt`（target：`siligen_module_process_path`）。
- 模块契约入口：`contracts/README.md`。

## 边界约束

- `M4`、`M5` 只提供规划和对齐输入，不承载 `M6` 终态 owner 事实。
- `M8`、`M9` 只能消费 `ProcessPath` 结果，不得反向重排或修补路径事实。
- 跨模块稳定工程契约应维护在 `shared/contracts/engineering/`，本目录只承载 `M6` 专属契约。
- 本阶段不迁移 DXF 专属输入边界；DXF 预处理与适配归属在后续阶段单独收口。

## 当前正式事实来源

- `modules/process-path/contracts/include/process_path/contracts/`
- `modules/process-path/application/include/application/services/process_path/`
- `modules/process-path/domain/trajectory/`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `contracts/` 与 `application/` 已成为当前 `M6` 的 canonical public surface。
- `ProcessPathFacade` 是唯一对外应用入口；新 consumer 不得直连 workflow 下的 trajectory 历史副本。
- `domain/trajectory/` 承载 `M6` 领域规则与值对象，作为 owner implementation root 继续保留。
- 本模块不再对外公开运动配置、规划报告、轨迹别名或运动规划服务。

## 测试基线

- `modules/process-path/tests/` 负责 `process-path` owner 级 `unit + contracts + golden + integration` 证明。
- 当前最小正式矩阵聚焦 `ProcessPathFacade` 的 request/result contract、lead-on/lead-off 语义和到 `motion-planning` 的公共链路交接。
- 仓库级 `tests/` 只消费跨 owner 场景，不替代本模块内 contract。
