# domain/domain

本目录是 workflow 迁移期保留下来的 bridge-domain residue，不是 `M0 workflow` 的终态 canonical domain root。

## 当前物理内容

- `_shared/`
- `dispensing/`
- `geometry/`
- `motion/`

## 当前边界真值

- recipe、machine、system、diagnostics、planning、recovery、supervision 等旧物理壳层已不再由本目录承载。
- recipe family 的 canonical owner 已迁到 `modules/recipe-lifecycle`，不再属于 workflow domain。
- 事件发布契约的 canonical owner 已迁到 `shared/contracts/runtime`，代码统一从 `runtime/contracts/system/IEventPublisherPort.h` 引入。
- round `23` 起，本目录的根 CMake 只保留 dormant placeholder；`siligen_trajectory` / `siligen_configuration` dead shell 已删除，本目录不再定义 live target。

## 当前 live residue

- workflow bridge-domain 已不再编译 dispensing concrete；`siligen_triggering` / `PositionTriggerController.cpp` 已于 round `21` 退出 live build graph，`dispensing/**` 当前只剩 dormant residue / compat 痕迹。
- `motion/`、`geometry/` 中仍存在一批 bridge header/backref residue，需要继续向真正 owner surface 收口。
- `domain/include/domain/safety/**` 已降为 compat public surface；canonical implementation 已切到 `modules/runtime-execution/application`。

## 禁止事项

- 不允许把 recipe、machine、system、diagnostics concrete 重新塞回本目录。
- 不允许把本目录继续当作长期 bridge-domain 装配根。
- 不允许为了兼容旧 include/path 再新增 fake layer、wrapper 或 facade shell。
