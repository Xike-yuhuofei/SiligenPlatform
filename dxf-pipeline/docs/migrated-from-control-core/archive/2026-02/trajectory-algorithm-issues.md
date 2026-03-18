# 轨迹算法审查问题工单清单

来源：`docs/_archive/2026-02/trajectory-algorithm-review.md`  
生成日期：2026-02-02  
范围：019-trajectory-unification（轨迹规划相关 Domain/Application/Infrastructure）

## P1（应该修复）

### P1-001 开环触发缺少运行时速度偏离保护
- 位置：`src/application/usecases/dispensing/dxf/DXFDispensingExecutionUseCase.cpp:480`
- 准确性：基本准确
- 建议动作：在执行环节引入速度偏离阈值检测与报警/降级/停止策略，并提供可配置阈值
- 验收标准：实际速度偏离超阈值时可触发报警，且系统执行明确的保护动作

### P1-002 点距复核阈值硬编码
- 位置：`src/application/usecases/dispensing/dxf/DXFWebPlanningUseCase.cpp:1142-1144`
- 准确性：准确
- 建议动作：将点距复核阈值参数化（配置或请求参数），保留默认值
- 验收标准：运行时可调整阈值，默认行为不变

### P1-004 DXF 解析异常被静默忽略
- 位置：`src/infrastructure/adapters/planning/dxf/internal/DXFParser.cpp:592`
- 准确性：准确
- 建议动作：记录异常或计数，并在解析完成后汇总报告
- 验收标准：解析结束后可获知被跳过的实体数量且有日志提示

### P1-005 默认触发间距硬编码
- 位置：`src/domain/motion/CMPCoordinatedInterpolator.cpp:144`
- 准确性：准确
- 建议动作：改为配置或入参注入默认触发间距，保持兼容默认值
- 验收标准：调用方可指定触发间距；未指定时保持原默认行为

## P2（建议改进）

### P2-001 ContourPathOptimizer 测试覆盖不足
- 位置：`tests/unit/application/dispensing/dxf/ContourPathOptimizerTest.cpp`
- 准确性：准确
- 建议动作：补充空输入、单元素、混合开闭轮廓、椭圆起点、元数据不匹配、大规模用例
- 验收标准：新增用例覆盖上述场景并通过

### P2-002 kPi/PI 常量重复定义
- 位置：`src/application/usecases/dispensing/dxf/ContourPathOptimizer.cpp:23`，`src/application/usecases/dispensing/dxf/DXFWebPlanningUseCase.cpp:31`，`src/domain/trajectory/domain-services/GeometryNormalizer.cpp:24`，`src/infrastructure/adapters/planning/dxf/internal/DXFParser.cpp:18`
- 准确性：准确
- 建议动作：抽取共享数学常量头文件并统一引用
- 验收标准：重复定义移除，统一来源使用

### P2-003 EllipsePoint 重复实现
- 位置：`src/application/usecases/dispensing/dxf/ContourPathOptimizer.cpp:25`，`src/domain/trajectory/domain-services/GeometryNormalizer.cpp:90`
- 准确性：准确
- 建议动作：抽取公共几何工具函数供两处复用
- 验收标准：仅保留一个实现并被两处引用

### P2-004 Domain 公共 API 缺少 noexcept
- 位置：`src/domain/trajectory/domain-services/GeometryNormalizer.h:38-39`，`src/domain/trajectory/domain-services/GeometryNormalizer.cpp:118`
- 准确性：准确
- 建议动作：为 `Normalize` 补 `noexcept`，符合 `src/domain/CLAUDE.md` 规则
- 验收标准：Domain 公共 API 满足 `noexcept` 要求

### P2-005 椭圆离散步长缺少最小值钳制
- 位置：`src/domain/trajectory/domain-services/GeometryNormalizer.cpp:191-195`
- 准确性：基本准确
- 建议动作：为 `step_mm` 增加最小步长下限（例如 0.01mm）
- 验收标准：极小步长配置不会造成过度离散化

### P2-006 Bulge 转弧数值稳定性保护不足
- 位置：`src/infrastructure/adapters/planning/dxf/internal/DXFParser.cpp:196-238`
- 准确性：部分准确
- 建议动作：根据数据范围决定上限/降级策略或日志提示
- 验收标准：极端 bulge 值不会导致异常或不可控结果

### P2-007 变间距模式未实现
- 位置：`src/domain/motion/CMPCoordinatedInterpolator.cpp:346-352`
- 准确性：准确
- 建议动作：实现变间距逻辑，或移除分支并返回明确错误
- 验收标准：行为明确且可测试

### P2-008 关键算法缺少复杂度说明
- 位置：`src/application/usecases/dispensing/dxf/ContourPathOptimizer.cpp`，`src/application/usecases/dispensing/dxf/DXFWebPlanningUseCase.cpp`，`src/infrastructure/adapters/planning/dxf/internal/DXFParser.cpp:739`
- 准确性：准确
- 建议动作：在关键入口函数增加时间/空间复杂度注释
- 验收标准：关键算法具备清晰复杂度说明
