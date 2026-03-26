# shared/artifacts

`shared/artifacts/` 是共享结构化产物模型的 canonical 子根，用于承接跨模块复用的稳定 artifact 语义。

## 当前状态

- 本目录当前保留职责说明与目录占位，用于约束后续共享 artifact 抽象的落点。
- 已稳定的通用基础类型统一收敛在 `shared/kernel/`，业务专属产物继续留在各自 owner 模块。
- 新增共享 artifact 语义必须直接进入 canonical 根，不得在仓库内建立第二套平行事实源。

## 禁止承载

- 具体模块私有产物生成逻辑和场景专属序列化实现
- 绑定单一业务上下文的回放、查询、审批数据模型
