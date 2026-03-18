# 项目边界规则

## 项目职责
- 承载 TCP 入口、runtime 兼容壳、基础设施、硬件适配，以及当前仍留在 `control-core` 的 shared 兼容层。
- `packages/process-runtime-core` 是业务内核 canonical owner；本项目不再作为 `src/domain`、`src/application`、`modules/process-core`、`modules/motion-core` 的默认构建 owner。
- 作为系统实时控制工程的一部分，继续负责真实硬件或 Mock 硬件的统一接入与宿主外围编排。
- 面向人的独立 CLI 已正式退役，不属于当前仓库受支持入口。

## 允许依赖
- 可以依赖本项目内部 `src/shared`、`src/infrastructure`、`src/adapters/tcp`、`modules/device-hal` 和 `apps/*` 入口壳。
- 可以消费 `packages/process-runtime-core`、`packages/runtime-host`、`packages/transport-gateway`、`packages/device-contracts`、`packages/device-adapters` 暴露的公开 target 或契约边界。
- 可以消费 `dxf-pipeline` 输出的 PB/JSON 结果，但只能通过明确的文件/契约格式接入。

## 禁止事项
- 禁止新增对 `src/domain`、`src/application`、`modules/process-core`、`modules/motion-core` 的真实构建依赖；这些目录已转为迁移镜像/兼容语义，不再是默认 owner。
- 禁止绕过 `packages/process-runtime-core`，把业务规则重新吸回 `control-core`。
- 禁止直接依赖 `hmi-client` 的 Python 代码或 UI 组件。
- 禁止直接依赖已删除的 `dxf-editor` / `apps/dxf-editor-app` / `packages/editor-contracts` 能力边界。
- 禁止把桌面 UI 逻辑、交互状态或编辑器流程写入控制核心。

## 对外交互规则
- 对 HMI 只暴露 TCP/JSON 协议，不暴露内部类库细节。
- 对 DXF 预处理只接受稳定的 proto/json 契约，不读取其内部模块。
- 对真实硬件和 Mock 硬件统一通过基础设施适配器接入。

## 测试规则
- 核心内核单测优先落在 `packages/process-runtime-core/tests`，由 package 侧 target 挂接。
- `control-core` 侧测试重点覆盖 TCP、运行时流程、基础设施、Mock 硬件和配方联调链路。
- 所有跨项目联调必须走公开契约，而不是内部文件引用。
