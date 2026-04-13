# domain

coordinate-alignment 的领域事实应服务于 `CoordinateTransformSet` 及其对齐语义。

当前状态：

- `M5` root surface 已收缩到 `contracts/`。
- `domain/machine/` 已退出 live build，不在当前 `SILIGEN_TARGET_IMPLEMENTATION_ROOTS` 中。
- `domain/machine/` 本轮后仅保留退场说明文档，不再保留 dead forwarder header。
- 本目录禁止重新引入与 machine/runtime/hardware-test 绑定的新 public surface。
