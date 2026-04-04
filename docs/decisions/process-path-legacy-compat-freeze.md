# Process-Path Legacy Compat Freeze

更新时间：`2026-04-04`

## 1. 决策结论

- `IDXFPathSourcePort` 继续保留为 `process-path` owner 提供的 legacy DXF contract，但它不是当前 live DXF 规划主链的必需入口。
- live DXF 规划主链固定为：`.pb -> IPathSourcePort::LoadFromFile -> ProcessPathFacade`。
- DXF legacy compat 实现的临时 owner 固定为 `modules/workflow/adapters/infrastructure/adapters/planning/dxf/`。
- `modules/dxf-geometry/adapters/dxf/` 这套同构实现判定为重复 legacy residue，不再保留 owner 身份。
- `modules/workflow/domain/domain/trajectory/` 判定为 orphan residue，不允许重新接入默认构建。

## 2. 当前允许保留的兼容面

- 契约层：
  - `modules/process-path/contracts/include/process_path/contracts/IDXFPathSourcePort.h`
- owner 内兼容桥：
  - `modules/process-path/domain/trajectory/ports/IDXFPathSourcePort.h`
  - `modules/workflow/domain/include/domain/trajectory/ports/IDXFPathSourcePort.h`
- 临时 compat 实现：
  - `modules/workflow/adapters/infrastructure/adapters/planning/dxf/`
- 直接验证：
  - `modules/workflow/tests/process-runtime-core/unit/infrastructure/adapters/planning/dxf/DXFAdapterFactoryTest.cpp`
  - 相关仓库级 contract test

## 3. 明确禁止项

- 禁止在默认 runtime 装配链中注册或消费 `IDXFPathSourcePort`。
- 禁止 live workflow 主链绕过 `.pb + IPathSourcePort + ProcessPathFacade` 重新接入 DXF 自动预处理 compat 入口。
- 禁止新增任何模块直接复用 `AutoPathSourceAdapter`、`DXFAdapterFactory`、`DXFMigrationValidator`、`DXFMigrationManager`。
- 禁止把 `modules/workflow/domain/domain/trajectory/` 或 `modules/dxf-geometry/adapters/dxf/` 重新接回默认构建。

## 4. 存续窗口与退出条件

兼容面当前只允许用于以下场景：

- legacy 测试
- 迁移校验
- 显式设置 `SILIGEN_DXF_AUTOPATH_LEGACY=1` 的临时回退

兼容面后续退出必须同时满足：

- 默认部署链零引用 `IDXFPathSourcePort`
- 默认构建图只保留 canonical PB 读取 target，不再默认编译 DXF legacy compat 实现
- repo 内不存在第二份 DXF compat 实现
- repo 内不存在 `workflow/domain/domain/trajectory` 这类 orphan trajectory residue

## 5. 实施约束

- canonical PB 读取 target 与 legacy compat target 必须拆分。
- default consumer 只能链接 canonical PB target。
- legacy compat target 只能被显式 legacy tests 或迁移校验链接。
