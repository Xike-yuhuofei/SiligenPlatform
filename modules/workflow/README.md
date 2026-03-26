# workflow

`modules/workflow/` 是 `M0 workflow` 的 canonical owner 入口。

## Owner 范围

- 工作流编排与阶段推进语义
- 规划链（`M4-M8`）触发与编排边界
- 流程状态与调度决策的事实归属

## 边界约束

- `process-runtime-core/` 与 `src/` 在迁移期仅作为 shell-only bridge，不再承担 `M0` 终态 owner 或真实实现承载面。
- `workflow` 仅承载 `M0` 编排职责，不直接承载 `M4-M8` 的模块内事实实现。
- 跨模块稳定公共契约应维护在 `shared/contracts/`，`M0` 专属契约放在 `modules/workflow/contracts/`。

## 迁移来源（当前事实）

- `legacy-archive/process-runtime-core/src/application`
- `legacy-archive/process-runtime-core/src/domain`
- `legacy-archive/process-runtime-core/modules/process-core`
- `legacy-archive/process-runtime-core/modules/motion-core`
- `legacy-archive/workflow/src/application`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- 历史目录当前按 bridge 管理；新增终态实现不得继续进入 bridge 目录。
- `application/include/`、`domain/include/`、`adapters/include/` 现为对外 canonical public surface。
- `domain/`、`application/`、`adapters/`、`tests/` 已成为 `workflow` 的唯一真实实现与验证承载面。
- `src/` 与 `process-runtime-core/` 已完成 shell-only bridge 收敛，不再承载真实 `.h/.cpp` payload。

