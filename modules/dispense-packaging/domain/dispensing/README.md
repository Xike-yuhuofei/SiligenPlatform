# Dispensing 子域 - 当前 live domain 面

**当前事实**: 本目录仍是 `dispense-packaging` 的主 domain 承载面，但尚未完成 packaging owner 与 planning / process-control residual 的拆分。

## 当前 live 事实

- 当前 target `siligen_dispense_packaging_domain_dispensing` 同时编译 packaging、trigger/timing planning、拓扑/曲线辅助以及部分 process-control residual。
- 目录中的全部源文件不等于 live owner 面；存在未进入 target 的 parked / legacy-candidate 文件。
- 具体清单见 `LIVE_OWNER_BOUNDARY_INVENTORY.md`。

## 结构判断

- `domain-services/` 当前同时包含 live residual 与 parked residual。
- `planning/domain-services/` 当前同时包含 live planner residual 与未编译的 parked planner residual。
- 本 README 不再把这些目录误写成“已完成收口后的单一工艺 domain”。

## 当前边界约束

- 本阶段不在本目录内继续扩张新的 planning / process-control 语义。
- 若需搬迁 residual 源文件，必须先在后续阶段冻结目标 owner 与 consumer 迁移路径。
- 应用层和外部 consumer 只能依赖当前显式公开 surface，不得把本目录中的 residual 文件默认视为稳定 public contract。

## 目录结构

```
dispensing/
├── domain-services/                      # 领域服务
│   ├── DispensingProcessService          # 点胶过程编排与校验统一入口
│   ├── DispensingController              # 触发点/规划位置触发计算
│   ├── CMPTriggerService                 # CMP触发服务
│   ├── PurgeDispenserProcess             # 排胶/建压稳压流程
│   ├── SupplyStabilizationPolicy         # 供胶稳压策略（统一入口）
│   ├── PositionTriggerController         # 位置触发控制
│   ├── ValveCoordinationService          # 阀门协调
│   ├── PathOptimizationStrategy          # 路径优化
├── value-objects/                        # 值对象
│   ├── DispensingExecutionTypes          # 运行参数/执行计划/执行报告
└── ports/                                # 端口接口
    ├── IValvePort
    ├── ITriggerControllerPort
    ├── ITaskSchedulerPort
    └── IDispensingExecutionObserver
```

## 备注

- 以上目录树仅表示目录中存在的文件，不代表全部都是当前 live owner 面。
- 若需要判断哪些文件当前参与编译，请优先查看 `domain/dispensing/CMakeLists.txt`。
- 若需要判断哪些外部模块直接消费本模块，请查看 `LIVE_OWNER_BOUNDARY_INVENTORY.md`。
