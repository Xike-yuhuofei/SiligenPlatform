# 配置布局

更新时间：`2026-03-18`

## 1. 目标

把机器配置收口到根级 `config/`，同时保留旧入口 `config/machine_config.ini` 和 `control-core` legacy 路径，避免现有运行入口在迁移期丢失配置。

## 2. 配置分类规则

| 分类 | 定义 | 允许落位 | 备注 |
|---|---|---|---|
| 配置源文件 | 手工维护、可审查、可发布的正式配置 | `config/` | 当前以机器配置为主 |
| 构建桥接文件 | 为兼容旧入口在 build/install 阶段复制出的同内容文件 | `build/config/` | 不得作为源文件反向编辑 |
| 运行期日志/快照 | 诊断输出、快照、缓存 | `logs/`、运行时目录 | 不进入 `config/` |

## 3. Canonical 布局

| 配置项 | owner | canonical 路径 | bridge |
|---|---|---|---|
| 机器配置 | `packages/runtime-host` + 运行入口消费者 | `config/machine/machine_config.ini` | `config/machine_config.ini` |
| 旧 control-core 机器配置 | legacy | `control-core/config/machine_config.ini` | 仅保留兼容 |
| 旧 src 资源配置 | legacy fallback | `control-core/src/infrastructure/resources/config/files/machine_config.ini` | 仅保留兼容 |

## 4. 迁移清单

| 来源 | 目标 | 动作 | 说明 |
|---|---|---|---|
| `control-core/config/machine_config.ini` | `config/machine/machine_config.ini` | 复制并修正路径 | 作为根级 canonical 配置 |
| `config/machine/machine_config.ini` | `config/machine_config.ini` | 复制 bridge | 兼容仍硬编码旧根级路径的调用方 |
| `config/machine/machine_config.ini` | `build/config/machine/machine_config.ini` | 构建复制 | 为 canonical build 输出保留同名文件 |
| `config/machine/machine_config.ini` | `build/config/machine_config.ini` | 构建复制 | 兼容旧 build 入口 |

## 5. 兼容策略

1. `packages/runtime-host` 的配置路径解析已改为优先寻找工作区根级 `WORKSPACE.md`，避免在 `control-core` 子项目根提前截断。
2. 默认配置入口已经切到 `config/machine/machine_config.ini`。
3. 对旧调用方继续保留 `config/machine_config.ini` bridge。
4. `ResolveConfigFilePath` 会按以下顺序尝试解析：
   `config/machine/machine_config.ini` 或用户显式传入路径
   `config/machine_config.ini`
   `control-core/config/machine_config.ini`
   `control-core/src/infrastructure/resources/config/files/machine_config.ini`
5. `control-core/CMakeLists.txt` 已改为从根级 canonical 复制配置到 build/install，同时产出 canonical 和 bridge 两套路径。

## 6. canonical 配置中的已修正项

| 配置键 | 旧值问题 | 新值 |
|---|---|---|
| `[DXF] dxf_file_path` | 指向 `Backend_CPP` 老仓上传目录 | 指向 `D:\Projects\SiligenSuite\examples\dxf\rect_diag.dxf` |
| `[DXFTrajectory] script` | 指向 legacy `tools/dxf_pipeline` 脚本 | 指向 `D:\Projects\SiligenSuite\packages\engineering-data\scripts\path_to_trajectory.py` |
| `[VelocityTrace] output_path` | 指向 `Backend_CPP` 日志目录 | 指向 `D:\Projects\SiligenSuite\logs\velocity_trace` |

## 7. 风险说明

1. 当前 machine config 仍包含工作站相关绝对路径，虽已从旧仓修正到当前工作区，但后续仍应模板化。
2. 旧文档、测试脚本和部分工具仍会展示 `config/machine_config.ini`；桥接期必须保留该路径。
3. 仍有 archive 文档引用 `src/infrastructure/resources/config/files/machine_config.ini`，这些引用不会影响运行，但可能误导维护者，应在后续文档清理阶段统一收口。
