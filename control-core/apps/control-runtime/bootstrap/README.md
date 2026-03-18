# runtime bootstrap

本目录已降级为 legacy 文档壳。

真实实现位置：

- `D:\Projects\SiligenSuite\packages\runtime-host\src\bootstrap\InfrastructureBindings.h`
- `D:\Projects\SiligenSuite\packages\runtime-host\src\bootstrap\InfrastructureBindingsBuilder.cpp`

职责：

- 组装基础设施适配器
- 创建 runtime 所需的绑定集合
- 供 `ContainerBootstrap` 统一接入 `ApplicationContainer`
