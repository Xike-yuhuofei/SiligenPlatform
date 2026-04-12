# domain

workflow 的 owner 事实、值对象、不变量与少量 orchestration boundary types 应收敛到此目录。

## 当前真值

- `domain/include/` 是 workflow 当前 domain public root，但其中仍有 compat header residue，不能视为已完全 canonical。
- workflow domain 当前只保留 canonical `siligen_workflow_domain_headers` / `siligen_workflow_domain_public`；`siligen_domain` 已于 round `18` 删除。
- recipe family 已从 workflow domain 退出；其 canonical owner 位于 `modules/recipe-lifecycle`。
- 事件发布契约不再落在 workflow domain；统一从 `shared/contracts/runtime` 暴露的 `runtime/contracts/system/IEventPublisherPort.h` 引入。
- `domain_machine` compat target 已于 round `23` 删除；workflow `domain/include/domain/machine/**` tracked payload 已退出，machine 相关剩余命中只保留在 contract guard 与历史文档中。

## 当前 residue

- `domain/domain/**` 当前只剩 dormant physical residue，不再定义 live target；bridge-root 下的 `siligen_trajectory` / `siligen_configuration` dead shell 已于 round `23` 删除。
- `domain/domain/dispensing/**` 仍保留一组 dormant foreign dispensing 文件 / compat 痕迹，但 workflow live build graph 已不再编译 `siligen_triggering` 或 `PositionTriggerController.cpp`。
- `domain/include/domain/{motion,safety}/**` 仍保留一组 compat header residue，需要继续向真正 owner surface 收口。

## 禁止事项

- 不允许继续把 machine、recipe、safety、motion execution concrete 回流到本目录。
- 不允许新增 sibling source bridge 或新的 fake domain/compat target。
