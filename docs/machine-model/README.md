# Machine Model

本目录承接机型级事实文档，不讨论抽象架构，只记录会直接影响现场联机、接线判定、IO 映射、回零/限位假设和机型约束的资料。

当前 authority 顺序：

1. [multicard-io-and-wiring-reference.md](./multicard-io-and-wiring-reference.md)：当前点胶机型的正式事实源。
2. [backend-cpp-library/README.md](./backend-cpp-library/README.md)：迁入的当前补充参考，用于现场排障和参数查表；若与根层权威页冲突，以根层权威页为准。
3. [history/README.md](./history/README.md)：历史诊断说明与不再承担当前真值职责的迁入材料。

当前已确认的机型事实：

- 仅 X/Y 两轴在用。
- `X7` 为急停输入。
- `HOME1` / `HOME2` 为回零输入。
- `LIM1+/2+`、`LIM1-/2-` 未接线。
- 当前机型未定义“安全门/门禁”输入；若软件出现 `door` / `safety_door_input`，应优先视为映射错误，而不是现场真的存在门禁。
- 当前机型 DXF 真机点胶主链以 `[ValveDispenser]` 为权威来源；`[CMP]` 段中的 `cmp_channel`、`signal_type`、`trigger_mode`、`pulse_width_us`、`delay_time_us`、`encoder_num`、`abs_position_flag` 及软件触发相关字段仅作兼容/历史保留。
- 当前机型已显式使用 `[ValveDispenser].cmp_axis_mask=3`；`[CMP].cmp_axis_mask` 不再进入 DXF 真机点胶主链。
