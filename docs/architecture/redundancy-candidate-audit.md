# 冗余候选审计（初稿）

更新时间：`2026-03-19`

## 1. 目的

本文件不回答“目录是否整齐”，只回答一件事：

**当前工作区里，哪些模块已经证明有必要保留，哪些模块只是镜像、占位、兼容壳，或需要重新证明业务必要性。**

它用于辅助后续做三类决策：

- 保留并继续建设
- 收缩为兼容壳
- 降级为 package / 工具能力
- 候删

## 2. 判定方法

一个模块是否“多余”，不看名字是否高级，也不看它是否已经放进 `apps/` 或 `packages/`。
只看以下四类证据：

1. 真实调用
   - 是否被运行时代码、脚本、CI、集成场景真正调用
   - README、规划文档、架构图不算真实调用
2. 独立边界
   - 是否真有独立进程、独立发布、独立协议、独立数据 owner
   - 只是“为了结构对称放在 apps/”不算独立边界
3. 重复度
   - 是否与另一目录暴露同名包、同一份源码或高度相似的实现
4. 删除影响
   - 删除后是否会让主业务链路、CI、硬件验证、真实用户工作流断掉

## 3. 当前审计结论

### 3.1 明确的镜像冗余

| 模块 | 初判 | 关键证据 | 建议动作 |
|---|---|---|---|
| `dxf-editor` | `候删（先收缩）` | README 已明确定义为 `apps/dxf-editor-app` 的兼容壳；`dxf-editor/src` 与 `apps/dxf-editor-app/src` 共有 `116` 个相对路径文件，`116` 个 hash 全一致 | 先收缩为 `README + scripts + 必要迁移文档`，停止保留重复 `src/tests/pyproject.toml` |
| `hmi-client` | `已删除（已完成）` | HMI 真实源码与入口已统一到 `apps/hmi-app`；legacy `hmi-client/` 目录已删除，历史材料已归档到 `docs/_archive/2026-03/hmi-client/` | 保持 archive 与删除门禁，不再恢复兼容壳 |

### 3.2 需要重新证明业务必要性的模块

| 模块 | 初判 | 关键证据 | 建议动作 |
|---|---|---|---|
| `apps/dxf-editor-app` | `强候选复审` | HMI 生产页运行时预览主链路已切到 gateway `dxf.preview.snapshot` / `dxf.preview.confirm`；未在 `apps/hmi-app/src` 中发现 `dxf_editor`、编辑器启动、notify 监听等真实源码引用；`packages/editor-contracts/README.md` 也明确写到“未看到 HMI notify 监听实现源码” | 不要默认把它当成必须保留的独立 app。先确认产品上是否真存在“独立 DXF 编辑器”用户与工作流；若没有，应考虑降级为内部工具或把必要能力回收为 package 能力 |
| `packages/editor-contracts` | `随同复审` | 该包只在“外部进程编辑器协作”成立时才有强必要性；当前 HMI 代码侧未看到真实消费者，更多体现为文档和冻结说明 | 与 `apps/dxf-editor-app` 一并决策。若独立编辑器不是主链路，可收缩为历史契约文档，甚至进入废弃计划 |

### 3.3 不是冗余，但容易被误判为“已迁完”的占位模块

| 模块 | 初判 | 关键证据 | 建议动作 |
|---|---|---|---|
| `packages/shared-kernel` | `占位锚点，不是已迁完模块` | 当前只有 README；文档中真实迁移来源仍是 `control-core/modules/shared-kernel` | 不删除，但也不要把“目录已建”误当成迁移完成 |
| `packages/traceability-observability` | `占位锚点，不是已迁完模块` | 当前只有 README；真实代码仍散落在 `control-core` 多处 | 不删除，但不要继续围绕它设计二级结构，先落真实代码 |
| `packages/simulation-engine` | `保留` | 已接管原顶层仿真源码、示例、测试，并新增方案 C 框架骨架 | 继续作为运动仿真 canonical owner 建设，不再回退到顶层独立目录 |

### 3.4 不是冗余，而是稳定入口壳

| 模块 | 初判 | 关键证据 | 建议动作 |
|---|---|---|---|
| `apps/control-runtime` | `保留为薄入口` | `run.ps1` 只解析 canonical `siligen_control_runtime.exe`；legacy fallback 已阻断 | 保留，但禁止新增宿主实现 |
| `apps/control-cli` | `保留为薄入口` | `run.ps1` 默认只解析 canonical `siligen_cli.exe`；连接调试、运动、点胶、DXF、recipe 命令面都已迁入 canonical CLI | 保留，但禁止新增 CLI 业务逻辑 |
| `apps/control-tcp-server` | `保留为薄入口` | `run.ps1` 只解析 canonical `siligen_tcp_server.exe`；HIL 默认入口也已切到 canonical | 保留，但禁止新增协议实现 |

### 3.5 当前明确不能删的真实依赖

| 模块 | 初判 | 关键证据 | 建议动作 |
|---|---|---|---|
| `dxf-pipeline` | `已完成兼容退出` | HMI 运行时预览主链路已迁出；legacy CLI / import shim / proto 兼容壳也已退出 | 不再把它当成 HMI 运行时依赖；后续只保留仓外观察与防回流门禁 |
| `control-core` | `保留` | config/data/HIL/alias/source-root fallback 已切走，CLI residual fallback 也已切除；但多处 C++ canonical 包仍以它为真实实现承载，并继续持有 `third_party` | 当前仍是核心源码与 `third_party` owner，不能静默下线 |

## 4. 对 `apps/dxf-editor-app` 的特别说明

`apps/dxf-editor-app` 和 `dxf-editor` 不是同一个问题：

- `dxf-editor` 的问题是“重复实现”。
- `apps/dxf-editor-app` 的问题是“是否真的存在不可替代的独立业务边界”。

目前证据更接近后者：

1. HMI 真实代码里已存在 DXF 预览工作流，但它走的是 `dxf-pipeline`。
2. HMI 文档提到与编辑器做外部进程协作，但当前源码未体现完整闭环。
3. 因此，`apps/dxf-editor-app` 目前更像“被定义出来的边界”，而不是“已被主业务链路证明必须独立存在的 app”。

这类模块最容易在重构时被误保留。

## 5. 推荐动作优先级

### P1：立即处理

1. 把 `dxf-editor` 明确列入收缩计划；`hmi-client` 已完成退场并切到删除后治理。
2. 对 `apps/dxf-editor-app` 发起一次产品/流程复审：
   - 是否存在独立编辑器用户？
   - 是否存在必须单独发布的桌面编辑器？
   - HMI 是否真的需要“编辑器进程 + notify 回调”而不是“预处理/预览能力”？

### P2：如果编辑器不是主链路

1. 将 `apps/dxf-editor-app` 从“默认保留的 app”降为“待合并候选”。
2. 将 `packages/editor-contracts` 从“正式长期契约”降为“待归档候选”。
3. 只保留 DXF 导入、预览、预处理中仍被主链路需要的能力。

### P3：治理加固

1. 后续新增文档和 PR 必须说明：当前改动触碰的是“真实 owner”还是“兼容壳/占位壳”。
2. 对同名 Python 包双暴露目录建立 CI 检查，避免 legacy 镜像再次回流为事实来源。
3. 不再把“目录已建”当成迁移进度；只以“真实代码、真实调用、真实测试”作为迁移完成标准。

## 6. 一句话结论

当前工作区最明确的冗余不是 `control-*` 这类入口壳，而是：

- **镜像冗余**：`dxf-editor`
- **已完成收口**：`hmi-client`
- **概念性冗余候选**：`apps/dxf-editor-app`、`packages/editor-contracts`
- **占位性冗余认知**：`packages/shared-kernel`、`packages/traceability-observability` 这些目录已经存在，但不能被误读为“真实模块已迁完”
