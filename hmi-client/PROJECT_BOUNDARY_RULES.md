# 项目边界规则

## 项目职责
- 承载桌面 HMI 界面、用户工作流、状态展示与操作引导。
- 通过 TCP/JSON 协议访问 `control-core`，通过外部进程方式集成 `dxf-editor`。

## 允许依赖
- 可以依赖本项目内部 UI、ViewModel、workflow、client 与 common 代码。
- 可以依赖 `control-core` 发布的 TCP 协议文档。
- 可以依赖 `dxf-editor` 的启动参数和 notify 文件契约。

## 禁止事项
- 禁止直接导入或链接 `control-core` 的 C++ 源码、内部头文件或构建产物实现细节。
- 禁止直接操作运动控制卡、点胶阀或其他硬件驱动。
- 禁止绕过 TCP 协议直接读写控制核心内部状态。

## 对外交互规则
- 与 `control-core` 仅通过 TCP/JSON 协议通信。
- 与 `dxf-editor` 仅通过命令行参数、DXF 文件和 notify 文件交互。
- 需要共享的数据结构必须沉淀为协议文档，而不是复制内部实现。

## 测试规则
- 单元测试覆盖 UI 逻辑、ViewModel 和客户端协议封装。
- 集成测试优先使用 mock TCP server 和 mock DXF editor。
- 不在 HMI 仓库内承担真实硬件联调责任。
