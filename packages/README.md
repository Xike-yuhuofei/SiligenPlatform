# packages

根级 `packages/` 是可复用能力、契约和宿主支撑的 canonical 落位。

使用原则：

- 运行入口放 `apps/`
- 复用逻辑放 `packages/`
- 契约优先独立成 package，不夹带在 app 或实现包里
