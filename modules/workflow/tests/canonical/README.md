# workflow canonical tests

本目录是 `workflow` 模块 canonical 测试源码承载面。

## 当前职责

- 只承载 `workflow` 自有的 canonical 单元测试源码。
- 当前正式构建目标为 `siligen_unit_tests`。
- 通过 `modules/workflow/tests/canonical/CMakeLists.txt` 作为正式构建入口。
- 不再承载 runtime-execution、dispense-packaging、dxf-geometry 等 owner 模块测试。

## 迁移约束

- 新增 canonical 测试必须保持 `workflow` owner 语义，不能把 foreign owner 单测再放回本目录。
- integration / regression 级 workflow 测试应落到各自目录，不再复用本目录源码。
