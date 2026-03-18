# 测试目录说明

本目录现在主要保留 `control-core` 自身仍需维护的测试内容与兼容入口。

当前核心内核的 canonical owner 已收口到 `packages/process-runtime-core`。核心单测源码已物理迁移到 `packages/process-runtime-core/tests`，本目录中的 `CMakeLists.txt` 只负责兼容转发到 package 侧 target。

## 保留范围

- `tests/unit`：领域、应用、核心算法与核心流程相关测试。
- `tests/integration`：控制核心、TCP、运行时、Mock 硬件相关测试。
- `tests/contract`：与外部项目的协议/数据契约测试。

## 不应放入本目录的测试

- `hmi-client` 的 UI、ViewModel、TCP client 层测试。
- `dxf-editor` 的编辑器文档模型、保存、notify 测试。
- `dxf-pipeline` 的几何、轨迹、proto 兼容性测试。

## 当前状态

当前迁移后的 `tests` 目录内容仍以 `control-core` 为主，未发现 HMI/编辑器/pipeline 的直接测试混入。

当前核心测试入口：

- `siligen_unit_tests`
- `siligen_pr1_tests`

二者现在由 `packages/process-runtime-core/tests/CMakeLists.txt` 定义，并通过 `siligen_process_runtime_core` 链接核心业务内核；只有 DXF 解析相关测试额外链接 `siligen_parsing_adapter`。
