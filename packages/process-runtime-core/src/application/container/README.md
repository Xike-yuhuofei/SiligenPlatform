# ApplicationContainer 兼容入口

`ApplicationContainer` 的真实实现已迁移到：

- `packages/runtime-host/src/container/ApplicationContainer.h`
- `packages/runtime-host/src/container/ApplicationContainer.cpp`

当前目录仅保留兼容 include 入口，避免一次性修改全部旧引用。
