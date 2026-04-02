# workflow integration tests

本目录是 `workflow` 模块 integration 承载面的正式入口。

## 当前登记目标

- 当前不登记 workflow-owned live integration smoke。
- 已删除的 runtime assembly / motion assembly smoke 不得在本目录恢复。

## 约束

- 本目录只负责 integration 级构建入口、命名与登记。
- 只有出现真正 workflow-owned facade integration case 时，才在本目录新增 live target。
- 后续新增集成测试不得回写 bridge 测试入口，也不得把 runtime-owned assembly 再拉回 workflow。
