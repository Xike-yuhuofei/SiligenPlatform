# shared

`shared/` 是模板化公共层目标根，只承载稳定契约和低业务含义基础设施。

## 目标承载面

- `shared/contracts/`
- `shared/kernel/`
- `shared/testing/`
- `shared/ids/`
- `shared/artifacts/`
- `shared/commands/`
- `shared/events/`
- `shared/failures/`
- `shared/messaging/`
- `shared/config/`

## 约束

- 不承载一级业务 owner。
- 不把迁移来源根中的实现源码直接搬成新的“共享大杂烩”。
- 任何共享层扩张都必须与 `S05/S06` 和依赖规则同步更新。
