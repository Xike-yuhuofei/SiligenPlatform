# workflow canonical tests

本目录是 `workflow` 模块 canonical 测试源码承载面。

## 当前职责

- 承载 `siligen_unit_tests`、`siligen_pr1_tests` 与 `siligen_dispensing_semantics_tests` 的真实测试源码。
- 通过 `modules/workflow/tests/canonical/CMakeLists.txt` 作为正式构建入口。
- 依赖 `workflow` canonical implementation roots，而不是历史 bridge 测试壳层。

## 迁移约束

- 新增测试必须写入本目录，不得回写任何已清除的历史测试壳层。
- 测试 include/link 只能面向 canonical `domain/`、`application/`、`adapters/` 与公开头根。
