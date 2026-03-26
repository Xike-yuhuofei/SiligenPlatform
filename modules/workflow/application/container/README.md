# ApplicationContainer 兼容入口

`ApplicationContainer` 的真实实现已迁移到：

- `modules/runtime-execution/runtime/host/container/ApplicationContainer.h`
- `modules/runtime-execution/runtime/host/container/ApplicationContainer.cpp`

当前目录仅保留 workflow 侧装配说明，不再承载 runtime host 的真实实现。
