# Proto 归属说明

`control-core` 不再维护本地 `proto/` 真源文件。

## 当前策略

- `dxf_primitives.proto` 的真源归属 `dxf-pipeline/proto/`。
- `control-core` 在构建时优先从兄弟项目 `../dxf-pipeline/proto/` 读取 proto 定义。
- 仅当共享 proto 不存在时，`control-core` 才会回退到本地副本；第二轮清理后，本地副本已移除。

## 后续约定

- proto 结构变更先在 `dxf-pipeline` 中完成。
- `control-core` 只消费生成结果和共享协议，不再复制维护一份独立 proto 源。
