# application-contracts

`application-contracts` 固化当前 HMI / CLI / TCP / Core 之间已经存在的应用层协议事实，优先记录真实字段、真实方法名、真实兼容别名，不对现有语义做静默重写。

## 当前范围

- TCP 请求/响应 envelope：`id`、`method`、`params`、`version`、`success`、`result`、`error`
- 应用命令目录：`commands/*.json`
- 应用查询目录：`queries/*.json`
- 状态、错误、事件模型：`models/*.json`
- 协议版本与兼容策略：`docs/protocol-versioning.md`
- 事实样例：`fixtures/`
- 兼容性测试：`tests/test_protocol_compatibility.py`
- 现状映射与兼容差异：`mappings/`

## 设计原则

1. 先对齐现有协议事实，再讨论抽象。
2. 契约以 JSON 文档表达，便于 C++ / Python / 后续脚本统一消费。
3. 对已存在的兼容别名显式保留，例如 `recipeId` / `recipe_id`。
4. 对 HMI 与 TCP 当前未对齐的字段，不在这里偷偷改语义，只记录兼容层要求和未决项。

## 快速验证

```powershell
python D:\Projects\SiligenSuite\packages\application-contracts\tests\test_protocol_compatibility.py
```
