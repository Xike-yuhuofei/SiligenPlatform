# workflow canonical tests: process-runtime-core

本目录是 `workflow` 模块中历史 `process-runtime-core` 测试的 canonical 承载面。

## 当前职责

- 承载 `siligen_unit_tests` 与 `siligen_pr1_tests` 的真实测试源码。
- 通过 `modules/workflow/tests/process-runtime-core/CMakeLists.txt` 作为正式构建入口。
- 依赖 `workflow` canonical implementation roots，而不是 `process-runtime-core/src` 或 `workflow/src/application`。

## 迁移约束

- 新增测试必须写入本目录，不得回写任何已清除的历史测试壳层。
- 历史 `process-runtime-core` 测试目录已退出 live owner graph。
- 测试 include/link 只能面向 canonical `domain/`、`application/`、`adapters/` 与公开头根。
