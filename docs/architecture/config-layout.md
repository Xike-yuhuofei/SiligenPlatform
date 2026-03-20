# 配置布局

更新时间：`2026-03-19`

## 1. 目标

把机器配置收口到根级 `config/`，同时只保留根级 bridge，不再让 `control-core` 路径承担默认 fallback。

## 2. 配置分类规则

| 分类 | 定义 | 允许落位 | 备注 |
|---|---|---|---|
| 配置源文件 | 手工维护、可审查、可发布的正式配置 | `config/` | 当前以机器配置为主 |
| 构建桥接文件 | 为兼容旧根级入口在 build/install 阶段复制出的同内容文件 | `build/config/` | 不得作为源文件反向编辑 |
| 运行期日志/快照 | 诊断输出、快照、缓存 | `logs/`、运行时目录 | 不进入 `config/` |

## 3. Canonical 布局

| 配置项 | owner | canonical 路径 | bridge |
|---|---|---|---|
| 机器配置 | `packages/runtime-host` + 运行入口消费者 | `config/machine/machine_config.ini` | `config/machine_config.ini` |

## 4. 默认解析顺序

`packages/runtime-host` 当前只按以下顺序解析：

1. 用户显式传入路径
2. `config/machine/machine_config.ini`
3. `config/machine_config.ini`

`control-core/config/machine_config.ini` 与 `control-core/src/infrastructure/resources/config/files/machine_config.ini` 已退出默认解析链路。

## 5. 构建期行为

- `control-core/CMakeLists.txt` 现在要求 canonical `config/machine/machine_config.ini` 必须存在。
- 构建期继续复制到：
  - `build/config/machine/machine_config.ini`
  - `build/config/machine_config.ini`
- install 期继续导出：
  - `config/machine/`
  - `config/`

## 6. 风险说明

1. 当前 machine config 仍包含工作站相关绝对路径，后续仍应模板化。
2. 旧文档、测试脚本和部分工具仍会展示 `config/machine_config.ini`；bridge 仍需保留一个兼容周期。
3. archive 文档中的 `control-core` 配置路径不会影响运行，但会误导维护者，后续仍需继续清理。
