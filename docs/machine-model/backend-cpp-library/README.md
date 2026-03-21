# Backend_CPP Library 精选

本目录收录从外部仓库 `D:\Projects\Backend_CPP\docs\library` 精选迁入的资料，只保留对当前 MultiCard 真机联机、IO 判定、回零/限位诊断和现场排障直接有帮助的文档。

使用边界：

- 本目录文档是迁入副本，不是当前工作区的 canonical 配置事实源。
- 若本文档与当前仓库实现、机型事实或配置入口冲突，以 `docs/machine-model/multicard-io-and-wiring-reference.md`、`docs/troubleshooting/post-refactor-runbook.md` 和当前代码实现为准。
- 对 DXF 真机点胶主链，CMP 运行时配置入口以 `[ValveDispenser]` 为准；迁入文档中仍出现的 `[CMP]` 表述按历史背景理解，不再作为当前主链配置口径。

当前收录：

- [06_reference.md](./06_reference.md)：控制卡 IO、配置、阈值、版本矩阵与现场绑定信息。
- [05_runbook.md](./05_runbook.md)：控制卡连接、电机无响应、点胶阀/供胶阀故障等现场排障步骤。
- [hardware-limit-configuration.md](./hardware-limit-configuration.md)：HOME/LIM 当前硬件接线、极性与使用边界。
- [test_limit_diagnosis_guide.md](./test_limit_diagnosis_guide.md)：HOME/LIM/GPI 诊断历史说明，便于理解原始输入读取口径。

迁入原则：

- 不整库镜像，只保留与当前点胶机型和 MultiCard 诊断直接相关的内容。
- 原始来源仍是 `D:\Projects\Backend_CPP\docs\library`，本目录作为工作区内稳定副本使用。
- 若与本工作区现行机型事实冲突，以 [multicard-io-and-wiring-reference.md](../multicard-io-and-wiring-reference.md) 和当前代码实现为准。
