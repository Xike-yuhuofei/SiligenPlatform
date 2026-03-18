# shared-kernel

legacy 兼容目录，默认 target owner 已切到 `packages/shared-kernel`。

当前职责：

- 保留迁移期说明与待删除目录占位
- 不再作为 `siligen_shared_kernel` 的默认注册入口

删除前仍需确认：

- `control-core` 其余构建脚本与文档不再把本目录当作默认 include 来源
- 文档中的 legacy 路径说明同步收口到 canonical package
