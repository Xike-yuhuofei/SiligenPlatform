# topology-feature 模块架构审查

- 审查对象：`modules/topology-feature/`
- 审查时间：`2026-03-31 07:52:00 +08:00`
- 审查方式：先读声明性边界，再读实现；只查看判断边界所需的少量关联文件

## 1. 模块定位

- 从声明性文档看，`modules/topology-feature/` 被定义为 `M3`，负责 `S3-S4`：拓扑修复、连通性建模、制造特征提取，owner 对象是 `TopologyModel` 和 `FeatureGraph`。证据：`docs/architecture/modules/topology-feature.md`、`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s01-stage-io-matrix.md`
- 从代码看，它的实际 payload 是一个离线 `DXF` 轮廓增广器 `ContourAugmenterAdapter`，输入 `input_path/output_path`，输出增广后的胶路/点阵 `DXF`，并被 `planner-cli` 的 `dxf-augment` 命令直接调用。证据：`modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.h`、`modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.cpp`、`apps/planner-cli/CommandHandlers.Dxf.cpp`
- 它在业务链中的宣称位置应当是 `M2 dxf-geometry` 之后、`M4 process-planning` 之前；但可见代码中的实际位置更像 `planner-cli` 的旁路工程工具，而不是 `BuildTopology/ExtractFeatures` 的正式 owner。
- 主要输入：
  - 宣称输入应是 `CanonicalGeometry` / `topology_model_ref`。证据：`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s01-stage-io-matrix.md`
  - 实际输入是原始 `DXF` 文件路径和 `ContourAugmentConfig`。证据：`modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.h`
- 主要输出：
  - 宣称输出应是 `TopologyModel` 和 `FeatureGraph`。证据：`docs/architecture/modules/topology-feature.md`
  - 实际输出是 `outer/inner loop`、三条胶路 `path_far/path_mid/path_near` 和 `grid_points` 写回 `DXF`。证据：`modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.cpp`
- 它应当依赖谁：按声明应主要依赖 `dxf-geometry/contracts` 与 `shared` 抽象。证据：`modules/topology-feature/module.yaml`
- 谁应当依赖它：按声明应主要是 `process-planning` 通过 `FeatureGraph` 契约消费，而不是 `apps/` 直接 include 它的 infrastructure adapter。

结论：代码职责本身是清楚的，但“模块定位”不清楚，因为宣称职责与实际 payload 明显不是一回事。

## 2. 声明边界 vs 实际实现

- 仓库文档把它定义为 `TopologyModel/FeatureGraph` 的 owner，并承担 `BuildTopology`、`ExtractFeatures` 命令与对应事件。证据：`docs/architecture/modules/topology-feature.md`、`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s05-module-boundary-interface-contract.md`
- 实际代码里，模块根 README 直接写明“当前正式事实来源”是 `adapters/infrastructure/adapters/planning/geometry/`；模块根 target 也只包装 `siligen_topology_feature_contour_augment_adapter`。证据：`modules/topology-feature/README.md`、`modules/topology-feature/CMakeLists.txt`
- 实际公开“契约”不是 `TopologyModel/FeatureGraph`，而是一个只转发具体 adapter 头文件的 `ContourAugmentContracts.h`。证据：`modules/topology-feature/contracts/include/topology_feature/contracts/ContourAugmentContracts.h`
- 一致性判断：不一致，且是根本性不一致。

偏差点：

- 该模块宣称应有的 `TopologyModel/FeatureGraph` 代码承载，在本次范围内未发现；实际代码层也未发现这两个对象的非文档实现。证据不足以证明仓库未检范围绝对不存在，但在本次审查范围内，它们是缺席的。
- 该模块实际承载的是“DXF 文件解析 + 轮廓修复/偏移 + 胶路生成 + 点阵生成 + DXF 写回”的单一工程工具，不是 `S3-S4` 对象 owner。证据：`modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.cpp`
- `workflow` 目录下仍保留同名同内容的 `ContourAugmenterAdapter.cpp/.stub.cpp`；本次文件哈希检查显示两份 `.cpp` 与 `.stub.cpp` 的 SHA256 完全一致，说明存在 shadow copy，而不是纯头转发。具体哈希值见附录。证据：`modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.cpp`、`modules/workflow/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.cpp`
- `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md` 仍把 `topology-feature` 的实现记在 `domain/geometry/`，同时又把同名 payload 记在 `workflow` 下，和模块 README 的“唯一真实实现承载面”不一致。证据：`modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md`、`modules/topology-feature/README.md`

判断：

- “该模块应有实现却落在别处”：是。对宣称的 `TopologyModel/FeatureGraph` 来说，模块内没有看到对应实现。
- “别的模块的 owner 事实被放进该模块”：是。至少放进了 DXF 文件级增广工具；它直接生成胶路/点阵，已经越过纯拓扑/特征 owner。
- “该模块只是目录存在，真实实现散落外部”：对宣称 owner 来说，基本是；对 `ContourAugmenterAdapter` 这个实际 payload 来说，不是，它有 live 实现，但同时有外部 shadow copy。
- “与其它模块重复建模、重复流程、重复数据定义”：是，最明显的是 `workflow` 里保留了相同 payload。

## 3. 模块内部结构审查

- 内部结构：混乱。

原因：

- 模块根 target 不是业务 owner 聚合，而是一个对单个 adapter target 的 `INTERFACE` 包装。证据：`modules/topology-feature/CMakeLists.txt`
- `contracts` 层直接 include 具体 adapter，并把 public contract target 反向链接到实现 target，公共接口与内部实现没有分离。证据：`modules/topology-feature/contracts/CMakeLists.txt`、`modules/topology-feature/contracts/include/topology_feature/contracts/ContourAugmentContracts.h`
- `domain/geometry/` 不是领域实现，而是兼容壳；`.cpp` 甚至直接 include adapter 实现文件，层次语义失真。证据：`modules/topology-feature/domain/geometry/ContourAugmenterAdapter.h`、`modules/topology-feature/domain/geometry/ContourAugmenterAdapter.cpp`
- 唯一真实实现文件把文件 I/O、DXF 解析、几何处理、胶路生成、点阵生成、DXF 序列化全部压进一个 adapter 文件，属于领域逻辑、应用行为、适配器行为混放。证据：`modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.cpp`
- `application/`、`services/`、`tests/`、`examples/` 只有骨架，未形成真实模块层次；`tests/` 当前只有 `README.md` 和 `.gitkeep`。证据：`modules/topology-feature/tests/`

CMake / 构建判断：不反映真实业务边界。它反映的是“一个 contour augment adapter 的包装模块”，不是“拓扑/特征 owner 模块”。

## 4. 对外依赖与被依赖关系

### 合理依赖

- `shared` 的类型与几何工具：`Error`、`Point2D`、`Result`、`siligen_types`、`siligen_utils`、`siligen_geometry`。证据：`modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.h`、`modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/CMakeLists.txt`
- 可选 `CGAL`：如果该 payload 被视为几何算法 adapter，这是合理的实现依赖。证据：`modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/CMakeLists.txt`

### 可疑依赖

- `module.yaml` 允许依赖 `dxf-geometry/contracts`，但实际实现完全没有消费该契约，而是直接吃文件路径；这说明应有的 `M2 -> M3` 对象边界没有落地。证据：`modules/topology-feature/module.yaml`、`modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.h`
- `domain/geometry` 反向桥接 `adapters/infrastructure`，说明分层只是壳，不是边界。证据：`modules/topology-feature/domain/geometry/CMakeLists.txt`

### 高风险依赖

- `contracts` public target 直接链接具体实现 target，导致“依赖契约”同时“依赖实现”。证据：`modules/topology-feature/contracts/CMakeLists.txt`
- `workflow` 不仅保留了 shadow copy，还把 `siligen_contour_augment_adapter` 挂进自己的 `siligen_workflow_adapters_public` surface，对外再导出 `M3` 细节。证据：`modules/workflow/adapters/CMakeLists.txt`、`modules/workflow/CMakeLists.txt`
- `apps/planner-cli` 直接 include 并实例化具体 adapter，耦合到了 infrastructure 层，而不是应用/服务/契约面。证据：`apps/planner-cli/CMakeLists.txt`、`apps/planner-cli/CommandHandlers.Dxf.cpp`

### 工程后果

- 未见直接 CMake 环依赖证据。
- 但存在明显的跨层直连、public surface 反向再导出、契约反向依赖实现。
- 结果是：任何想把 `M3` 做成真正业务 owner 的改造，都会被 `workflow` 兼容面和 `planner-cli` 直连一起放大。

## 5. 结构性问题清单

### 1. `M3` owner 名义存在，真实 owner 缺位

- 现象：文档把模块定义为 `TopologyModel/FeatureGraph` owner，但模块实际只有 `ContourAugmenterAdapter`，并且直接做 `DXF -> 胶路/点阵 DXF` 转换；在本次范围内未见 `TopologyModel/FeatureGraph` 的源码定义或处理器。
- 涉及文件：`docs/architecture/modules/topology-feature.md`、`docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s05-module-boundary-interface-contract.md`、`modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.h`
- 为什么这是结构问题而不是局部实现问题：它破坏的是模块身份、对象链和上下游契约，不是某个函数写法。
- 可能后果：团队会把一个旁路工程工具误当成 `S3-S4` 正式 owner，后续特征提取、工艺规划、回退链都没有稳定落点。
- 优先级：`P0`

### 2. 公共契约反向依赖具体实现，层次倒置

- 现象：`ContourAugmentContracts.h` 只转发具体 adapter；`siligen_topology_feature_contracts_public` 直接链接 `siligen_topology_feature_contour_augment_adapter`；唯一真实实现文件同时承担解析、计算、序列化。
- 涉及文件：`modules/topology-feature/contracts/include/topology_feature/contracts/ContourAugmentContracts.h`、`modules/topology-feature/contracts/CMakeLists.txt`、`modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.cpp`
- 为什么这是结构问题而不是局部实现问题：它决定了消费者只能依赖实现细节，无法在不破坏调用方的前提下引入真正的 domain/application/contracts 分层。
- 可能后果：未来一旦引入真实 `TopologyModel/FeatureGraph` 契约，现有消费者需要一起改 include、link 和调用路径。
- 优先级：`P1`

### 3. `workflow` 与 `planner-cli` 仍持有/再导出 `M3` 具体适配器

- 现象：`workflow` 保留同名 shadow copy，并把 `siligen_contour_augment_adapter` 纳入 `siligen_workflow_adapters_public`；`planner-cli` 直接实例化 `ContourAugmenterAdapter`。
- 涉及文件：`modules/workflow/adapters/infrastructure/adapters/planning/geometry/CMakeLists.txt`、`modules/workflow/CMakeLists.txt`、`apps/planner-cli/CommandHandlers.Dxf.cpp`
- 为什么这是结构问题而不是局部实现问题：这不是“多一个 include”这么简单，而是 owner 边界没有做到排他，旧模块还在对外提供同一能力入口。
- 可能后果：收口时必须同时处理 `workflow`、`planner-cli`、legacy bridge 和文档库存，改造成本成倍增加。
- 优先级：`P1`

### 4. 骨架、文档库存、测试入口三者不一致

- 现象：README 说真实实现已在 `adapters/...`，`MODULES_BUSINESS_FILE_TREE_AND_TABLES.md` 仍把实现登记在 `domain/geometry/`；模块 tests 只有占位。
- 涉及文件：`modules/topology-feature/README.md`、`modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md`、`modules/topology-feature/tests/README.md`
- 为什么这是结构问题而不是局部实现问题：它会持续制造错误的 owner 认知，并让“是否已收口”无法通过目录和验证面判断。
- 可能后果：后续任何 review、迁移、测试补齐都会先花时间辨认“哪份才是真的”。
- 优先级：`P2`

## 6. 模块结论

- 宣称职责：`S3-S4` 的拓扑修复、连通性建模、制造特征提取，产出 `TopologyModel`、`FeatureGraph`
- 实际职责：`planner-cli dxf-augment` 旁路命令所用的 `DXF` 轮廓增广 adapter，输出胶路和点阵 `DXF`，并附带 legacy bridge
- 是否职责单一：否
- 是否边界清晰：否
- 是否被侵入：是。`workflow` 和 `planner-cli` 仍保留或直接消费它的具体 adapter surface
- 是否侵入别人：是。它直接承担 `DXF` 文件 I/O，并把轮廓直接转成胶路/点阵，越过了 `CanonicalGeometry -> TopologyModel -> FeatureGraph -> ProcessPlan/ProcessPath` 正式链。精确最终应落在 `M4` 还是 `M6`，当前证据不足以二选一
- 是否适合作为稳定业务模块继续演进：不适合
- 最终评级：`高风险`

## 7. 修复顺序

### 第一步：先收口模块定义

- 目标：先决定 `topology-feature` 到底是 `M3` 真实 owner，还是“轮廓增广工具”的临时落点
- 涉及目录/文件：`modules/topology-feature/README.md`、`modules/topology-feature/module.yaml`、`modules/topology-feature/contracts/README.md`、`docs/architecture/modules/topology-feature.md`
- 收益：先恢复“说真话”的模块边界
- 风险：会暴露出下游 `M4` 目前也未承接完整 `FeatureGraph -> ProcessPlan` 链

### 第二步：再迁移消费者和兼容桥

- 目标：让 `workflow` 停止 public re-export，让 `planner-cli` 停止直接 include infrastructure adapter；只保留一条明确消费面
- 涉及目录/文件：`modules/workflow/adapters/CMakeLists.txt`、`modules/workflow/CMakeLists.txt`、`apps/planner-cli/CMakeLists.txt`、`apps/planner-cli/CommandHandlers.Dxf.cpp`
- 收益：恢复 owner 排他性
- 风险：会引起 include 路径和构建 target 的调整

### 第三步：最后统一真实实现归属

- 目标：如果保留 `M3`，就补真实 `TopologyModel/FeatureGraph` contracts、application/service 入口和测试；如果不保留，就把当前 `ContourAugmenterAdapter` 迁成明确的工程工具/adapter，并从 `topology-feature` 剥离
- 涉及目录/文件：`modules/topology-feature/contracts`、`modules/topology-feature/application`、`modules/topology-feature/services`、`modules/topology-feature/tests`
- 收益：长期演进才有真实落点
- 风险：这是唯一需要动到业务对象链的步骤，必须和相邻模块一起校准

## 8. 证据索引

| 文件路径 | 得出的判断 | 支撑结论 |
|---|---|---|
| `docs/architecture/modules/topology-feature.md` | `M3` 宣称承担 `S3-S4`、产出 `TopologyModel/FeatureGraph` | §1, §2, §6 |
| `docs/architecture/dsp-e2e-spec/dsp-e2e-spec-s01-stage-io-matrix.md` | `M3` 在正式链上应位于 `M2` 与 `M4` 之间 | §1, §4 |
| `modules/topology-feature/README.md` | 模块 README 自认真实实现只在 `adapters/.../planning/geometry` | §2, §3 |
| `modules/topology-feature/module.yaml` | `owner_artifact` 写的是 `TopologyModel`，但 `notes` 写的却是 contour augment adapter | §1, §2, §6 |
| `modules/topology-feature/contracts/include/topology_feature/contracts/ContourAugmentContracts.h` | 公共契约头直接暴露具体 adapter | §2, §4, §5 |
| `modules/topology-feature/contracts/CMakeLists.txt` | public contract target 直接链接实现 target | §3, §4, §5 |
| `modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.h` | 实际 API 是 `input_path/output_path` 文件级接口 | §1, §2 |
| `modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.cpp` | 实现直接读写 DXF，并生成胶路/点阵，而不是产出 `TopologyModel/FeatureGraph` | §1, §2, §5 |
| `modules/topology-feature/domain/geometry/ContourAugmenterAdapter.h` | `domain/geometry` 只是兼容桥，不是领域 owner 实现 | §3, §5 |
| `modules/workflow/CMakeLists.txt` | `workflow` public adapters 仍对外暴露 contour augment adapter | §4, §5, §6 |
| `apps/planner-cli/CommandHandlers.Dxf.cpp` | `planner-cli` 直接实例化具体 adapter，实际位置是旁路命令工具 | §1, §4, §6 |
| `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md` | 文档库存仍把实现记在 `domain/geometry/`，并同时记录 `workflow` 同名 payload | §2, §5 |

## 9. 复核附录

- 2026-03-31 二次复核补充了 shadow copy 的 SHA256 证据，使用命令：
  - `Get-FileHash <path> -Algorithm SHA256`
- 哈希结果：
  - `modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.cpp`
    - `19F82FE6E8B15BCE4C60B3D3CF99124E02CB0CFB3BB30BC3A7C472D8A1CAE741`
  - `modules/workflow/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.cpp`
    - `19F82FE6E8B15BCE4C60B3D3CF99124E02CB0CFB3BB30BC3A7C472D8A1CAE741`
  - `modules/topology-feature/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.stub.cpp`
    - `8949C44DDFE833044FCF1CE29C08BBDB926E4E468CFB9884168DD5108F9CA6B2`
  - `modules/workflow/adapters/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.stub.cpp`
    - `8949C44DDFE833044FCF1CE29C08BBDB926E4E468CFB9884168DD5108F9CA6B2`
