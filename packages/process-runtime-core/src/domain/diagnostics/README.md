# Diagnostics 子域 - 诊断系统

**职责**: 负责系统诊断、健康检查、性能监控、Bug 报告管理，并覆盖工艺结果与标定流程的可追溯记录

## 业务范围

- 健康状态监控
- 性能指标采集
- 诊断报告生成
- Bug 信息收集
- 系统状态查询
- 故障诊断
- 工艺结果采集与追溯
- 标定/校准流程结果记录

## 目录结构

```
diagnostics/
├── aggregates/                 # 聚合
│   ├── TestConfiguration.h
│   └── TestRecord.h
├── domain-services/            # 领域服务
│   ├── ProcessResultSerialization.h
│   └── ProcessResultService.h
├── value-objects/              # 值对象
│   ├── DiagnosticTypes.h
│   ├── TestDataTypes.h
│   └── BugReportTypes.h
└── ports/                      # 端口接口（1个）
    └── IDiagnosticsPort
```

## 命名空间

```cpp
namespace Siligen::Domain::Diagnostics {
    namespace Entities { ... }
    namespace ValueObjects { ... }
    namespace DomainServices { ... }
    namespace Ports { ... }
}
```

## 依赖关系

- ✅ 依赖: `shared/types`, `shared/utils`, `domain/_shared`
- ❌ 不依赖: `infrastructure`, `application`, 其他子域

## 诊断流程

1. 定期采集系统状态与工艺/标定关键指标
2. 计算性能指标
3. 评估健康状态
4. 生成诊断报告
5. 触发告警（如有问题）

## 工艺结果架构规范

- 结构化定义与校验集中在 Domain：
  - 测试/工艺结果结构由 `value-objects/TestDataTypes.h` 定义
  - 记录聚合与基础校验由 `domain-services/ProcessResultService` 完成
- JSON 格式由 `domain-services/ProcessResultSerialization` 统一生成/解析
  - 字段名与结构视为外部契约，不得随意变更
- Infrastructure 仅实现持久化端口 `ITestRecordRepository`
  - 禁止在 Infrastructure 层新增结果结构、校验或聚合逻辑
  - 禁止在持久化层“补字段/修字段/改格式”
