# 配置布局

更新时间：`2026-03-19`

## 1. 目标

把机器配置收口到唯一正式路径 `config/machine/machine_config.ini`，不再保留根级 alias、build alias 或遗留 `CMP` 配置段。

## 2. 配置分类规则

| 分类 | 定义 | 允许落位 | 备注 |
|---|---|---|---|
| 配置源文件 | 手工维护、可审查、可发布的正式配置 | `config/` | 当前以机器配置为主 |
| 运行期日志/快照 | 诊断输出、快照、缓存 | `logs/`、运行时目录 | 不进入 `config/` |

## 3. Canonical 布局

| 配置项 | owner | canonical 路径 |
|---|---|---|
| 机器配置 | `packages/runtime-host` + 运行入口消费者 | `config/machine/machine_config.ini` |

## 4. 默认解析顺序

`packages/runtime-host` 当前只按以下顺序解析：

1. 用户显式传入路径
2. `config/machine/machine_config.ini`

唯一正式机器配置路径是 `config/machine/machine_config.ini`。

## 5. 构建期行为

- 根级验证与运行入口只认 `config/machine/machine_config.ini`。
- 任何需要发布或打包的步骤都必须直接消费 canonical 路径，不再生成 alias 文件。
- `[ValveDispenser]` 是点胶阀正式配置段；历史 `[CMP]` 段已退出。

## 6. 风险说明

1. 当前 machine config 仍包含工作站相关绝对路径，后续仍应模板化。
2. archive 文档中的历史配置路径不会影响运行，但会误导维护者，后续仍需继续清理。
