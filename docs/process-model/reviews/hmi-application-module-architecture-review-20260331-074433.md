# hmi-application 模块架构审查

- 审查时间：2026-03-31 07:44:33
- 审查范围：`modules/hmi-application/`
- 少量关联查看：根 `README.md`、`modules/README.md`、`modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md`、根级 `CMakeLists.txt`、`apps/hmi-app/`、`apps/planner-cli/`、`shared/contracts/application/`
- 审查方式：先读声明性边界，再读实现；只在判断边界所必需时查看少量关联代码
- 说明：未执行测试；结论基于限定范围内的静态结构审查

## 1. 模块定位

- 从文档和代码共同看，`modules/hmi-application` 当前真实核心职责不是“完整 HMI 应用层”，而是更窄的两块：启动/恢复会话状态收敛、预览会话与生产前置门禁收敛。
- 它在业务链中的位置是：`apps/hmi-app` 宿主接收用户操作后，调用 M11 生成 `LaunchUiState`、`PreflightDecision`、`PreviewPayloadResult`、`SessionSnapshot` 等，再通过 app 侧协议客户端访问 runtime/gateway。
- 主要输入：启动模式、Supervisor 快照/阶段事件、preview payload、机器状态对象、操作员确认动作。
- 主要输出：UI 状态、恢复动作判定、预览判定、预检判定、启动结果、阶段事件。
- 按声明，它应依赖稳定契约和自身 adapter，而不应反向依赖宿主 app。
- 按声明，应依赖它的是 `apps/hmi-app` 这类宿主；实际也主要如此，但另有可疑构建依赖来自 `planner-cli`。

## 2. 声明边界 vs 实际实现

### 2.1 文档中的模块定义

- `module.yaml` 将其声明为 `M11 hmi-application`，owner artifact 为 `UIViewModel`。
- `modules/hmi-application/README.md` 将其定义为 canonical owner 入口，并明确：
  - `apps/hmi-app` 仅承担宿主职责
  - 启动恢复、preview 语义校验、preview 重同步与生产前置 block reason 必须落在模块内
  - 模块不应越权承担设备实现或运行时执行 owner 职责
- `contracts/README.md` 声明模块 owner 专属契约应收敛到 `modules/hmi-application/contracts/`

### 2.2 实际代码中的模块职责

- 实际代码主要实现了：
  - 启动/恢复状态机与 Supervisor 会话模型
  - launch result 到 UI state 的映射
  - preview session、preview contract 校验、resync、preflight 阻断
  - runtime degradation 收敛
- 同时它还直接包含：
  - `QThread`/`pyqtSignal`
  - `TcpClient`、`CommandProtocol`、`BackendManager` 的具体协同
  - 直接连 runtime/gateway 的 worker 逻辑

### 2.3 一致性判断

- 结论：部分一致，但整体不一致。
- 一致的部分：
  - 启动/preview owner 语义确实已经从 `apps/hmi-app` 下沉到该模块。
- 不一致的部分：
  - 模块名和 owner artifact 指向的是更宽的“HMI 应用层/UIViewModel”，但当前只收口了启动和 preview 两条子链。
  - 文档要求契约、adapter、服务分层明确，实际 live 实现几乎全部集中在 `application/hmi_application/`。
  - 文档声明模块应依赖 contracts/shared，实际却直接反向依赖 `apps/hmi-app` 的具体客户端实现。

### 2.4 重点偏差

- 存在该模块应有实现却落在别处：
  - `home/jog` 前置判断
  - 生产启动/暂停/恢复/停止入口
  - 多处 HMI 操作判定仍在 `apps/hmi-app/src/hmi_client/ui/main_window.py`
- 存在别的模块或宿主的 owner 事实被放进该模块：
  - `PreviewSnapshotWorker`、`StartupWorker`、`RecoveryWorker` 和 `SupervisorSession` 直接操作 app 侧 `TcpClient/CommandProtocol/BackendManager`
- 不存在明显“第二套同源业务实现”，但存在第二套 compat surface 与重复 owner 测试
- 存在“目录存在，但真实实现未按骨架分层”的情况：
  - `domain/services/adapters/contracts` 基本只有 README
  - 真实实现集中在 `application/hmi_application/`

## 3. 模块内部结构审查

- 内部结构：一般

### 原因

- 文件级职责还算可读，但模块级分层没有真正落地。
- `domain/services/adapters/contracts` 全是占位骨架，没有 live 实现。
- `application/hmi_application/` 同时混放：
  - owner 契约
  - 状态机
  - UI state 组装
  - preview payload 规范化
  - Qt worker
  - 对 app concrete client 的直接调用
- 公共接口与内部实现没有真正分离：
  - `supervisor_contract.py` 这类 owner 契约不在 `contracts/`，而在 `application/`
- CMake 没有真实表达边界：
  - target 声称实现根包括 `domain/services/application/adapters/tests/examples`
  - 但实际 asset 列表只指向 `application/hmi_application/*.py`

### 证据文件

- `modules/hmi-application/CMakeLists.txt`
- `modules/hmi-application/contracts/README.md`
- `modules/hmi-application/application/hmi_application/preview_session.py`
- `modules/hmi-application/application/hmi_application/startup.py`
- `modules/hmi-application/application/hmi_application/supervisor_contract.py`

## 4. 对外依赖与被依赖关系

### 合理依赖

- `apps/hmi-app` 作为宿主消费 M11 owner 结果，这与声明一致。
- 模块消费 `shared/contracts/application/` 中的稳定协议背景信息，这与文档声明一致。

### 可疑依赖

- M11 owner 面直接依赖 `PyQt5.QtCore`
- `planner-cli` 构建上链接 `siligen_module_hmi_application`，但在抽查范围内未看到对应使用证据

### 高风险依赖

- M11 直接依赖宿主 app 的 `BackendManager`、`TcpClient`、`CommandProtocol` 具体实现
- M11 的 worker 逻辑直接建立 TCP 连接并发命令，不经过模块内 adapter/port
- app 侧又通过 `module_paths.py` 和 compat re-export 把模块注回宿主命名空间，形成双向绑定

### 证据文件

- `modules/hmi-application/module.yaml`
- `modules/hmi-application/application/hmi_application/preview_session.py`
- `modules/hmi-application/application/hmi_application/startup.py`
- `modules/hmi-application/application/hmi_application/supervisor_session.py`
- `apps/hmi-app/src/hmi_client/module_paths.py`
- `apps/hmi-app/src/hmi_client/client/preview_session.py`
- `apps/planner-cli/CMakeLists.txt`

### 工程后果

- 模块无法脱离 `apps/hmi-app` 独立演进
- 未来替换宿主、拆分 headless use case、做跨宿主复用时会受阻
- 依赖方向被破坏后，文档、构建和代码评审会持续失真
- 当前未看到明确模块间代码环依赖证据，但 app 与 module 已形成结构性双向依赖

## 5. 结构性问题清单

### 5.1 模块反向依赖宿主 app concrete implementation

- 现象：
  - M11 在 owner 代码里直接 import `hmi_client.client.protocol`、`hmi_client.client.tcp_client`、`hmi_client.client.backend_manager`
  - 并在 worker/session 中直接调用具体连接与启动逻辑
- 涉及文件：
  - `modules/hmi-application/application/hmi_application/preview_session.py`
  - `modules/hmi-application/application/hmi_application/startup.py`
  - `modules/hmi-application/application/hmi_application/supervisor_session.py`
  - `modules/hmi-application/module.yaml`
- 为什么这是结构问题而不是局部实现问题：
  - 被破坏的是依赖方向，不是单个函数写法
  - 模块不再是 owner 根，而变成宿主实现之上的一层脚手架
- 可能后果：
  - 无法独立测试/复用/替换宿主
  - 导入路径脆弱
  - 长期边界治理失败
- 优先级：P0

### 5.2 `hmi-application` 名称与实际 owner 范围不一致，操作入口仍留在宿主

- 现象：
  - 模块只承接启动/preview 语义
  - `home/jog/job start-stop` 等 HMI 操作入口与前置判断仍在 `main_window.py`
- 涉及文件：
  - `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md`
  - `apps/hmi-app/src/hmi_client/ui/main_window.py`
- 为什么这是结构问题而不是局部实现问题：
  - 这决定 M11 到底是不是“HMI 应用层 owner”
  - 当前模块名、owner_artifact、业务文档和真实收口面不一致
- 可能后果：
  - 后续新增 HMI 规则继续堆进宿主
  - 模块长期停留在“半 owner”
- 优先级：P1

### 5.3 声明骨架、契约落点、构建组织与真实实现脱节

- 现象：
  - `contracts/domain/services/adapters` 基本为空
  - `supervisor_contract.py` 却放在 `application/`
  - CMake 宣称多根实现，实际只收 application 文件
  - 业务树文档仍指向不存在的 C++ `src/CommandHandlers.cpp`
- 涉及文件：
  - `modules/hmi-application/contracts/README.md`
  - `modules/hmi-application/CMakeLists.txt`
  - `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md`
- 为什么这是结构问题而不是局部实现问题：
  - 架构声明本身已不可信
  - 团队无法仅靠模块骨架和文档判断 owner 边界
- 可能后果：
  - 新增实现继续放错层
  - 评审和迁移建立在错误地图上
- 优先级：P1

### 5.4 兼容桥、测试面和构建依赖没有随 owner 迁移完成收口

- 现象：
  - app 侧仍保留 compat re-export
  - app 侧仍保留 owner 级单测
  - `planner-cli` 还链接 HMI 模块
- 涉及文件：
  - `apps/hmi-app/src/hmi_client/module_paths.py`
  - `apps/hmi-app/src/hmi_client/client/preview_session.py`
  - `apps/hmi-app/tests/unit/test_supervisor_session.py`
  - `apps/hmi-app/tests/unit/test_startup_sequence.py`
  - `apps/planner-cli/CMakeLists.txt`
- 为什么这是结构问题而不是局部实现问题：
  - 这些桥接资产会持续制造“模块已迁移完成”的错觉
  - 边界无法真正变干净
- 可能后果：
  - 回归面重复
  - 依赖图污染
  - 后续改动成本上升
- 优先级：P2

## 6. 模块结论

- 宣称职责：
  - HMI 启动恢复、preview/preflight、展示状态聚合、owner 契约边界
  - 更宽泛文档还把它描述为 HMI 任务接入与状态查询 owner
- 实际职责：
  - 启动/恢复状态机、preview session/preflight/runtime degradation/UI state 映射
  - 加上具体 Qt worker、TCP/gateway/hardware 协同
- 是否职责单一：否
- 是否边界清晰：否
- 是否被侵入：是，`apps/hmi-app` 仍保留多项 HMI 操作入口与前置规则
- 是否侵入别人：是，直接侵入宿主 app client / Qt 线程 / 启动管理具体实现
- 是否适合作为稳定业务模块继续演进：当前不适合直接按“稳定 canonical 业务模块”继续扩张，需先修复依赖方向与收口面
- 最终评级：明显偏移

## 7. 修复顺序

### 7.1 先收口边界 charter 与依赖方向

- 目标：
  - 明确 M11 只保留 owner 决策与状态对象
  - 外部调用改成 port/adapter
- 涉及目录/文件：
  - `modules/hmi-application/application/hmi_application/startup.py`
  - `modules/hmi-application/application/hmi_application/supervisor_session.py`
  - `modules/hmi-application/application/hmi_application/preview_session.py`
  - `modules/hmi-application/adapters/`
  - `modules/hmi-application/contracts/`
- 收益：
  - 切断 app -> module <- app concrete implementation 的双向耦合
- 风险：
  - 导入路径、线程回调和现有测试会一起调整

### 7.2 再迁移剩余 HMI 操作 owner 规则

- 目标：
  - 把 `home/jog/production` 的前置判断与操作决策从 `main_window.py` 下沉到模块
  - 宿主只保留 Qt 事件、消息框、协议发起和渲染
- 涉及目录/文件：
  - `apps/hmi-app/src/hmi_client/ui/main_window.py`
  - `modules/hmi-application/services/` 或 `application/` 新用例文件
- 收益：
  - 模块名与实际 owner 面一致
- 风险：
  - 如果团队实际只想让 M11 管启动/preview，就必须同步改模块命名和文档，而不是继续保留当前名义

### 7.3 最后统一契约、构建、文档和测试

- 目标：
  - 把 owner 契约放回 `contracts/`
  - 把 `services/adapters` 真正用起来
  - 清理 app compat wrappers、owner 级 app tests 和 `planner-cli` 的无关依赖
  - 修正文档树
- 涉及目录/文件：
  - `modules/hmi-application/CMakeLists.txt`
  - `modules/hmi-application/contracts/README.md`
  - `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md`
  - `apps/planner-cli/CMakeLists.txt`
- 收益：
  - 声明、实现、构建三者一致
- 风险：
  - CI、脚本和测试路径会短期抖动

## 8. 证据索引

| 文件路径 | 得出的判断 | 支撑结论 |
|---|---|---|
| `modules/hmi-application/README.md` | 模块自称是 canonical owner，宿主应保持 thin-host | 模块定位、声明边界、最终评级 |
| `modules/hmi-application/module.yaml` | owner artifact 是 `UIViewModel`，allowed deps 仅 contracts/shared | 模块定位、依赖方向、P0 问题 |
| `modules/hmi-application/CMakeLists.txt` | 构建声明多根实现，但实际只挂 application Python 文件 | 内部结构、P1 问题 |
| `modules/hmi-application/contracts/README.md` | 契约应在 `contracts/`，但目录实际为空壳 | 声明边界、P1 问题 |
| `modules/hmi-application/application/hmi_application/preview_session.py` | 模块内混放 owner 规则、Qt worker、TCP/protocol 具体调用 | 内部结构、依赖漂移、P0 问题 |
| `modules/hmi-application/application/hmi_application/startup.py` | 启动 owner 逻辑直接绑定 Qt worker 与 app concrete types | 依赖方向、P0 问题 |
| `modules/hmi-application/application/hmi_application/supervisor_session.py` | 模块直接控制 backend/tcp/protocol 具体实现 | 依赖方向、P0 问题 |
| `apps/hmi-app/src/hmi_client/module_paths.py` | app 通过 `sys.path` 注入 module，形成桥接面 | 依赖方向、P2 问题 |
| `apps/hmi-app/src/hmi_client/client/preview_session.py` | app 保留 compat re-export，而非纯消费接口 | 声明边界偏差、P2 问题 |
| `apps/hmi-app/src/hmi_client/ui/main_window.py` | 宿主仍保留 home/jog/production precondition 和操作入口 | 实际职责、P1 问题 |
| `modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md` | 仓库级业务文档仍描述不存在的 C++ `src/` 结构，声明层失真 | 声明边界偏差、P1 问题 |
| `apps/planner-cli/CMakeLists.txt` | 非 HMI 宿主对 M11 存在可疑构建依赖 | 依赖关系、P2 问题 |

## 9. 关键证据摘录

- `modules/hmi-application/module.yaml`
  - `allowed_dependencies: workflow/contracts, runtime-execution/contracts, shared`
- `modules/hmi-application/application/hmi_application/preview_session.py`
  - 直接 `from PyQt5.QtCore import QThread, pyqtSignal`
  - 直接引入 `hmi_client.client.protocol`、`hmi_client.client.tcp_client`
- `modules/hmi-application/application/hmi_application/startup.py`
  - 直接引入 `hmi_client.client.backend_manager`、`CommandProtocol`、`TcpClient`
- `apps/hmi-app/src/hmi_client/module_paths.py`
  - 通过 `sys.path.insert` 把 `modules/hmi-application/application` 注入宿主运行路径
- `apps/hmi-app/src/hmi_client/ui/main_window.py`
  - `_check_home_preconditions`
  - `_check_motion_preconditions`
  - `_check_production_preconditions`
  - `_start_production_process`

