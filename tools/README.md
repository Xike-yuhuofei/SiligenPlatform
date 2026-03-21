# tools

根级 `tools/` 用于承接工作区级工具能力。

当前阶段：

- `build/`：统一构建脚本的未来落位
- `scripts/`：统一工作区脚本
- `codegen/`：契约生成与同步工具
- `lint-rules/`：静态规则与检查
- `release/`：发布与打包
- `migration/`：迁移辅助脚本

当前不重写现有子项目脚本，只建立统一落位。

## scripts 新增约定

- `resolve-workflow-context.ps1`：统一解析 `branchSafe`、`ticket`、`timestamp`
- `invoke-guarded-command.ps1`：高风险命令执行前检查与显式批准执行
