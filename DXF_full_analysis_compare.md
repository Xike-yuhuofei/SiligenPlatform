# DXF 全量梳理 + 对比分析报告

## 1. Executive Summary

1. 两侧 DXF 主链已形成明确“边界分工”：`process-runtime-core` 负责运行时编排/执行，`engineering-data` 负责 DXF 离线预处理与工件产物生成（PB、preview、simulation-input、offline trajectory）。
2. 当前交汇点是 `PathBundle(.pb)` 与脚本入口 `scripts/dxf_to_pb.py`；`process-runtime-core` 通过 `DxfPbPreparationService::EnsurePbReady` 统一桥接到 `engineering-data`。
3. 发现 12 条关键差异（已附证据）：包括错误模型、默认值、触发策略、元数据契约、构建依赖开关、legacy 兼容方式、测试入口等。
4. P0 风险集中在“legacy autopath 占位 PB + 成功返回语义”与“跨侧行为不一致（jerk/strict-r12/错误码）”，会导致线上行为漂移和定位困难。
5. 建议短期先做“契约收口+错误语义对齐+高风险 fallback 下线”，中期再做“边界固化+去重迁移+统一回归门禁”。

## 2. 文件清单总表（按 package）

说明：仅统计任务限定范围；已排除 `third_party/**`、`build/**`、`docs/_archive/**`、`__pycache__`、`.pyc`。

| Package | 分组 | 数量 | 全量范围 |
|---|---:|---:|---|
| process-runtime-core | `src/application/usecases/dispensing` | 21 | DXF 用例主目录全量 |
| process-runtime-core | `src/application/services/dxf` | 2 | PB 预处理服务全量 |
| process-runtime-core | `src/infrastructure/adapters/planning/dxf` | 13 | DXF 适配器/迁移层全量 |
| process-runtime-core | `tests/unit/dispensing` | 4 | DXF 用例单测全量 |
| process-runtime-core | `tests/unit/infrastructure/adapters/planning/dxf` | 2 | DXF 适配层单测全量 |
| engineering-data | `src/engineering_data/{processing,contracts,preview,trajectory,cli,proto}` + `models/trajectory.py` | 17 | canonical DXF 管线全量 |
| engineering-data | `src/dxf_pipeline/**` | 14 | legacy shim 全量 |
| engineering-data | `scripts/*` | 4 | 工作区脚本入口全量 |
| engineering-data | `tests/*` | 2 | DXF 兼容性与 legacy shim 回归全量 |

## 3. process-runtime-core DXF 架构图与职责

```text
DXF输入
  -> UploadFileUseCase
  -> DxfPbPreparationService.EnsurePbReady
      -> 调 engineering-data dxf_to_pb（脚本/命令）
  -> PlanningUseCase 或 DispensingExecutionUseCase
      -> DispensingPlanner.Plan
          -> IPathSourcePort(PbPathSourceAdapter/AutoPathSourceAdapter)
          -> ContourPathOptimizer / ContourAugmenterAdapter
          -> UnifiedTrajectoryPlanner
          -> TriggerPlanner
          -> InterpolationProgramPlanner
  -> 预览输出 或 执行(DispensingProcessService)
```

按“应用用例 / Domain 服务 / 适配器或服务 / 配置读取”归类：

| 类别 | 模块 | 输入 | 输出 | 依赖 | 主要错误路径 |
|---|---|---|---|---|---|
| 应用用例 | `UploadFileUseCase.cpp` | 上传文件字节 | 存储路径 + 已就绪 PB | 文件存储、`EnsurePbReady` | 预处理失败时清理 dxf/pb；格式校验失败 |
| 应用用例 | `PlanningUseCase.cpp` | 规划请求+DXF路径 | 预览轨迹/触发点 | planner、配置口、`EnsurePbReady` | `CMP_TRIGGER_SETUP_FAILED`（禁止回退定时触发） |
| 应用用例 | `DispensingExecutionUseCase.cpp` | 执行请求 | 执行结果 | planner、process service、配置口、`EnsurePbReady` | PB准备失败、规划失败、执行失败 |
| Domain 服务 | `TriggerPlanner.cpp` | 轨迹段长度/速度/触发参数 | 触发距离与命令计划 | 安全边界配置 | 违反安全边界可降级或失败 |
| 适配器/服务 | `DxfPbPreparationService.cpp` | `.dxf/.pb` 路径 | 就绪 `.pb` 路径 | 环境变量、配置口、外部 python 命令 | 命令模板非法、脚本缺失、PB空文件、命令退出码 |
| 适配器/服务 | `PbPathSourceAdapter.cpp` | `.pb` | `PathSourceResult` | protobuf 解析 | Parse失败、空 primitives、`SILIGEN_ENABLE_PROTOBUF=OFF` |
| 适配器/服务 | `AutoPathSourceAdapter.cpp` | `.dxf/.pb` | `DXFPathSourceResult` | migration config、pb adapter | legacy 默认禁用；多处“Success+error_message”；占位 PB 回退 |
| 配置读取 | `IConfigurationPort.h` | 配置端口 | `DxfPreprocessConfig`/`DxfTrajectoryConfig` 等 | 运行时配置系统 | 读取失败走默认参数 |

## 4. engineering-data DXF 架构图与职责

```text
CLI/scripts 输入
  -> engineering_data.cli.*
  -> processing.dxf_to_pb (DXF -> PathBundle.pb)
  -> contracts.simulation_input (PB -> simulation-input.json)
  -> preview.html_preview (DXF -> preview HTML artifact)
  -> trajectory.offline_path_to_trajectory (points -> trajectory JSON)
  -> dxf_pipeline/* legacy shim 全部 forward 到 canonical
```

按“CLI / contracts / scripts / 转换管线”归类：

| 类别 | 模块 | 输入 | 输出 | 依赖 | 主要错误路径 |
|---|---|---|---|---|---|
| CLI | `cli/dxf_to_pb.py` | 参数 | 调 canonical 主函数 | processing | 由 processing 返回码驱动 |
| CLI | `cli/export_simulation_input.py` | PB + 可选 trigger CSV | simulation-input JSON | contracts | PB加载失败/CSV列缺失/无可导出段 |
| CLI | `cli/generate_preview.py` | DXF | preview artifact | preview contract | 文件不存在/无几何 |
| contracts | `contracts/simulation_input.py` | PathBundle | simulation payload dict | proto | `No exportable path segments` |
| contracts | `contracts/preview.py` | PreviewRequest | PreviewArtifact | dataclass | 请求不合法时上层抛异常 |
| scripts | `scripts/dxf_to_pb.py` 等 | 命令行 | 调 cli | Python 环境 | 继承 CLI 行为 |
| 转换管线 | `processing/dxf_to_pb.py` | DXF | PathBundle.pb | ezdxf/geomdl/protobuf | 返回码 1/2/3/4 |
| 转换管线 | `trajectory/offline_path_to_trajectory.py` | 点集 JSON | trajectory JSON | ruckig | 点不足、Ruckig导入/计算失败 |
| legacy | `src/dxf_pipeline/**` | 旧导入/旧CLI | forward 到 canonical | shim | 保留兼容入口，不承载算法 |

## 5. 差异对比矩阵（维度化）

| 维度 | process-runtime-core | engineering-data | 证据（路径 + 关键符号/键） |
|---|---|---|---|
| 1. 功能边界 | 负责运行时规划/执行与触发控制 | 负责离线预处理，不接管实时控制 | `packages/process-runtime-core/src/application/usecases/dispensing/README.md`；`packages/engineering-data/README.md` |
| 2. DXF->PB 调用方式 | C++ 进程外调用 Python 脚本/命令模板 | Python 内部 canonical 实现 | `DxfPbPreparationService.cpp::ResolvePbCommandArgs`；`processing/dxf_to_pb.py::main` |
| 3. 输入契约（strict-r12） | 配置项 `strict_r12`，默认 false；上传链路默认走 `--no-strict-r12` | `--strict-r12/--no-strict-r12`，严格模式失败返回 4 | `IConfigurationPort.h::DxfPreprocessConfig`；`UploadFileUseCaseTest.cpp`；`dxf_to_pb.py::validate_input_contract` |
| 4. 错误模型 | `Result<T> + ErrorCode` 统一 | 异常 + CLI 返回码 | `DxfPbPreparationService.cpp`；`dxf_to_pb.py::return 1/2/3/4` |
| 5. legacy 回退策略 | legacy autopath 默认禁用，需 `SILIGEN_DXF_AUTOPATH_LEGACY=1` | legacy 包全部 forward 到 canonical | `AutoPathSourceAdapter.cpp`；`src/dxf_pipeline/cli/*.py` |
| 6. 数据契约（metadata） | `PathPrimitiveMeta` 仅 id/type/segment/closed | `PrimitiveMeta` 还写入 `layer/color` | `IPathSourcePort.h::PathPrimitiveMeta`；`dxf_to_pb.py::add_meta` |
| 7. metadata 不一致处理 | 元数据数量不匹配时补默认 meta；轮廓优化直接放弃优化 | 导出端每 primitive 都写 meta（设计上对齐） | `PbPathSourceAdapter.cpp`；`ContourOptimizationService.cpp` |
| 8. 触发策略 | `TriggerPlanner` 有 safety downgrade；Web 预览无触发直接失败 | simulation payload 允许 `triggers=[]` | `TriggerPlanner.cpp`；`PlanningUseCase.cpp`；`simulation_input.py` |
| 9. 轨迹 jerk 默认值 | `jmax<=0 -> 5000` | `jmax<=0 -> 1e6` | `DispensingPlannerService.cpp`；`offline_path_to_trajectory.py` |
| 10. 配置项有效性 | 读取 `DxfTrajectoryConfig.python/script` 填入 request，但主规划未消费 | Python 轨迹脚本实际被 CLI 使用 | `DispensingExecutionUseCase.Setup.cpp`；`DispensingPlannerService.h/.cpp`；`cli/path_to_trajectory.py` |
| 11. 边界/行程约束 | 会做 DXF 偏移、负坐标平移、软限位拟合与越界失败 | preview/simulation 不做机台软限位约束 | `DispensingPlannerService.cpp`；`html_preview.py`；`simulation_input.py` |
| 12. 依赖与 fallback 机制 | 构建期开关 `SILIGEN_ENABLE_PROTOBUF/CGAL`，关闭时 `NOT_IMPLEMENTED` | 依赖在 pyproject 固定声明，运行时导入失败抛异常 | `process-runtime-core/CMakeLists.txt`；`ContourAugmenterAdapter.stub.cpp`；`engineering-data/pyproject.toml` |

端到端交汇点与边界责任：

1. 交汇点 A：`EnsurePbReady -> scripts/dxf_to_pb.py -> .pb`，责任在 `process-runtime-core` 负责编排，`engineering-data` 负责转换正确性。
2. 交汇点 B：`.pb` 消费模型，`process-runtime-core` 通过 `PbPathSourceAdapter` 解包到运行时 primitive。
3. 交汇点 C：回归基线，`engineering-data` fixture（pb/preview/simulation）与 runtime 规划执行链路目前尚未统一成单一金标门禁。

## 6. 风险清单（P0/P1/P2）

| 级别 | 风险 | 触发条件 | 建议动作 |
|---|---|---|---|
| P0 | legacy autopath 会写“占位 PB”且上层可能收到 Success 语义 | 开启 legacy/autopath 或误走旧链路 | 立即禁用占位 PB 回退，失败必须显式 `Failure(Error)` |
| P0 | 跨侧默认值不一致（`jmax<=0`、strict-r12 容错）导致离线/在线结果漂移 | 同一输入在两侧跑 | 建立统一参数契约与默认值基线，出厂即对齐 |
| P1 | metadata 契约丢失 `layer/color`，影响后续按图层/颜色策略扩展 | 需要图层级策略时 | 扩展 runtime `PathPrimitiveMeta` 并保持向后兼容 |
| P1 | `python_ruckig_*` 配置疑似“死字段”造成运维误判 | 修改配置无效果 | 明确删除或接入消费路径，补测试 |
| P1 | 错误码映射不透明（Python exit code -> runtime ErrorCode） | 预处理失败排障 | 定义统一错误映射表并输出结构化日志 |
| P2 | 远程适配器与迁移管理仍为壳实现，存在认知噪音 | 团队误以为可切 REMOTE | 文档与代码同时标记实验态，避免默认暴露 |
| P2 | 回归入口分散（runtime 单测 vs engineering-data fixture） | 变更验证成本高 | 收敛到统一回归流水线 |

## 7. 收敛路线图（短期/中期）

| 阶段 | 改动目标 | 影响面 | 验证方式 | 预期收益 |
|---|---|---|---|---|
| 短期 | 下线 `AutoPathSourceAdapter` 占位 PB fallback，统一失败语义 | runtime DXF 旧入口 | 单测补齐 + 人工注入失败场景 | 降低误成功与脏 PB 风险（P0） |
| 短期 | 建立 `dxf_to_pb` 返回码到 `ErrorCode` 的固定映射 | `DxfPbPreparationService`、日志、告警 | 用 1/2/3/4 返回码逐例回归 | 故障定位成本显著下降 |
| 短期 | 对齐关键默认值（`strict_r12`、`jmax`、trigger 间隔） | 预览、执行、离线轨迹 | 同 DXF 输入双侧产物对比测试 | 减少离线/在线行为漂移 |
| 短期 | 增加跨包金标用例（DXF->PB->preview/sim/plan） | 两个 package 测试层 | CI 新增契约回归 job | 变更可回归、可追责 |
| 中期 | 扩展统一 metadata 契约（含 layer/color） | pb schema、runtime port、策略层 | 兼容读写测试 + 历史 PB 回放 | 支撑图层策略/可视化一致性 |
| 中期 | 清理 dead config（`python_ruckig_*`）并文档化边界 | 配置系统、用例构建 | 配置生效性测试 | 降低配置歧义与维护成本 |
| 中期 | 迁移壳代码治理：明确保留/淘汰清单（`dxf_pipeline` vs runtime migration） | 兼容层、文档 | deprecation 计划 + 版本门禁 | 去重，减少长期技术债 |

## 8. 附录：检索命令与关键证据路径

检索命令（执行）：

1. `rg --files packages/process-runtime-core packages/engineering-data | rg -i "dxf|pb|path_to_trajectory|dxf_to_pb|preview|contour|augment|simulation-input|trajectory"`
2. `rg -n -i "dxf|pb|path_to_trajectory|dxf_to_pb|preview|augment|contour|trajectory|simulation-input" packages/process-runtime-core packages/engineering-data`
3. `rg -n "EnsurePbReady|strict-r12|SILIGEN_DXF_PB_COMMAND|SILIGEN_DXF_AUTOPATH_LEGACY|jmax|bundle_to_simulation_payload|generate_preview" ...`

关键证据路径（核心）：

1. `packages/process-runtime-core/src/application/services/dxf/DxfPbPreparationService.cpp`
2. `packages/process-runtime-core/src/application/usecases/dispensing/UploadFileUseCase.cpp`
3. `packages/process-runtime-core/src/application/usecases/dispensing/PlanningUseCase.cpp`
4. `packages/process-runtime-core/src/application/usecases/dispensing/DispensingExecutionUseCase.Setup.cpp`
5. `packages/process-runtime-core/src/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp`
6. `packages/process-runtime-core/src/infrastructure/adapters/planning/dxf/PbPathSourceAdapter.cpp`
7. `packages/process-runtime-core/src/infrastructure/adapters/planning/dxf/AutoPathSourceAdapter.cpp`
8. `packages/process-runtime-core/src/infrastructure/adapters/planning/dxf/DXFMigrationConfig.h`
9. `packages/process-runtime-core/src/domain/configuration/ports/IConfigurationPort.h`
10. `packages/process-runtime-core/src/domain/dispensing/domain-services/TriggerPlanner.cpp`
11. `packages/engineering-data/src/engineering_data/processing/dxf_to_pb.py`
12. `packages/engineering-data/src/engineering_data/contracts/simulation_input.py`
13. `packages/engineering-data/src/engineering_data/preview/html_preview.py`
14. `packages/engineering-data/src/engineering_data/trajectory/offline_path_to_trajectory.py`
15. `packages/engineering-data/src/dxf_pipeline/services/dxf_preprocessing.py`
16. `packages/engineering-data/tests/test_engineering_data_compatibility.py`
17. `packages/engineering-data/tests/test_dxf_pipeline_legacy_shims.py`

---

本报告仅做分析，不包含代码修改。




