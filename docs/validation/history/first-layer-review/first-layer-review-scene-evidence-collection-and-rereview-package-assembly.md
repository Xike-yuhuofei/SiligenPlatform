# 逐场景证据采集与复审输入包组装稿

## 文档定位

本稿用于把“证据补齐与首轮复审准备”进一步压成逐场景可执行动作，直接回答三个问题：

- 每个 `S1-S9` 具体应采哪些证据
- 这些证据当前在仓库里可依附哪些真实业务语义锚点
- 每个场景的复审输入包应由哪些固定材料组成

本稿不重开定义，不修改场景，不重写验收口径，也不替代单场景评审记录页。

## 使用原则

- 证据采集必须围绕当前阻断项 `B1-B3` 展开
- 证据采集必须围绕单场景证明责任展开，而不是围绕“多收集一点总没错”展开
- 复审输入包必须做到“拿到包即可重新判定”，而不是还要临场补解释
- 仓库内现有代码与命令语义只作为“证据锚点”和“采集线索”，不自动等于场景已通过

## 一、统一包结构

每个场景的复审输入包建议固定为以下 `6` 项：

1. 场景复审首页  
   场景编号、复审目标、对应阻断项、当前待解除缺口。

2. 输入条件页  
   本次复审使用的业务前提、验证上下文、关键输入条件。

3. 运行或异常记录页  
   本次复审新增的运行记录、状态记录、异常记录或对照结果。

4. 证据摘要页  
   明确列出本次新增证据分别解除哪一条缺口。

5. 判定候选页  
   只允许写 `通过` / `不通过` / `有条件通过` / `待补证据` 四类候选。

6. 复审回填页  
   明确是否解除阻断项、是否还需再次复审、需要回填到哪些总表。

## 二、统一证据类型

本阶段建议只采以下高价值证据：

- 启动请求与输入参数记录
- 状态流转记录
- 结果输出记录
- 交互请求/响应记录
- 异常触发与报警记录
- 恢复、复位、终止记录
- 两组或多组输入下的对照输出
- 重复验证记录

不建议优先采以下低价值材料：

- 只有界面截图但无判定上下文的材料
- 只有口头描述没有落盘记录的材料
- 只证明“代码里有这个命令”但不证明“场景已被真实承接”的材料

## 三、仓库内统一业务锚点

当前仓库里可作为复审包“业务语义锚点”的统一来源包括：

- [control-cli README](D:/Projects/SiligenSuite/apps/control-cli/README.md)
- [CommandHandlers.Dxf.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Dxf.cpp)
- [CommandHandlers.Dispensing.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Dispensing.cpp)
- [CommandHandlers.Motion.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Motion.cpp)
- [CommandHandlers.Connection.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Connection.cpp)
- [CommandHandlers.Recipe.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Recipe.cpp)
- [CommandLineParser.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandLineParser.cpp)
- [main.cpp](D:/Projects/SiligenSuite/apps/control-cli/main.cpp)

这些文件的作用是：

- 证明相关业务命令语义在当前仓库中存在
- 为运行证据命名、归档和解释提供统一锚点
- 为后续复审时的证据回溯提供路径入口

## 四、逐场景证据采集与复审包组装

### S1 主干成功闭环

- 当前待解除缺口：`B1`、`B3`
- 证据采集重点：
  - 主链启动记录
  - `dxf-plan` 与 `dxf-dispense` 相关结果记录
  - 主链结果输出
  - 至少一次重复验证记录
- 业务锚点：
  - [control-cli README](D:/Projects/SiligenSuite/apps/control-cli/README.md)
  - [CommandHandlers.Dxf.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Dxf.cpp)
  - [CommandHandlers.Dispensing.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Dispensing.cpp)
- 复审包最低组成：
  - 有效 DXF 与配方前提说明
  - 主链执行结果记录
  - 结果输出摘要
  - 重复验证对照页

### S2 配方/版本前置阻断

- 当前待解除缺口：前置阻断证据缺口、`B3`
- 证据采集重点：
  - 启动请求记录
  - 配方未激活或版本无效的阻断记录
  - 明确反馈内容
  - 至少一次重复验证记录
- 业务锚点：
  - [control-cli README](D:/Projects/SiligenSuite/apps/control-cli/README.md)
  - [CommandHandlers.Recipe.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Recipe.cpp)
  - [CommandHandlers.Dxf.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Dxf.cpp)
- 复审包最低组成：
  - 缺失前提说明
  - 阻断反馈记录
  - 阻断原因摘要
  - 重复验证页

### S3 设备就绪前置判断

- 当前待解除缺口：前置阻断证据缺口、`B3`
- 证据采集重点：
  - 未回零、供胶未开或未就绪状态记录
  - 启动请求记录
  - 阻断反馈记录
  - 至少一次重复验证记录
- 业务锚点：
  - [control-cli README](D:/Projects/SiligenSuite/apps/control-cli/README.md)
  - [CommandHandlers.Motion.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Motion.cpp)
  - [CommandHandlers.Dispensing.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Dispensing.cpp)
- 复审包最低组成：
  - 设备前提缺失说明
  - 就绪状态记录
  - 阻断反馈页
  - 重复验证页

### S4 输入前置校验阻断

- 当前待解除缺口：输入阻断证据缺口、`B3`
- 证据采集重点：
  - 无效 DXF/空轨迹/参数异常样本
  - 规划前或执行前阻断记录
  - 原因反馈记录
  - 至少一次重复验证记录
- 业务锚点：
  - [control-cli README](D:/Projects/SiligenSuite/apps/control-cli/README.md)
  - [CommandHandlers.Dxf.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Dxf.cpp)
- 复审包最低组成：
  - 无效输入样本页
  - 阻断记录页
  - 原因反馈页
  - 重复验证页

### S5 关键操作链路切换

- 当前待解除缺口：`B1`、`B3`
- 证据采集重点：
  - `pause`、`resume`、`stop`、复位、再进入相关状态流转记录
  - 再进入后的结果记录
  - 至少一次重复验证记录
- 业务锚点：
  - [control-cli README](D:/Projects/SiligenSuite/apps/control-cli/README.md)
  - [CommandHandlers.Dispensing.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Dispensing.cpp)
  - [CommandHandlers.Motion.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Motion.cpp)
  - [CommandLineParser.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandLineParser.cpp)
- 复审包最低组成：
  - 操作顺序说明
  - 状态流转记录
  - 再进入结果页
  - 重复验证页

### S6 关键交互时序异常

- 当前待解除缺口：`B2`、`B3`
- 证据采集重点：
  - 超时、错序或缺响应触发记录
  - 请求/响应或交互日志
  - 异常后的状态结果
  - 至少一次重复验证记录
- 业务锚点：
  - [control-cli README](D:/Projects/SiligenSuite/apps/control-cli/README.md)
  - [CommandHandlers.Connection.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Connection.cpp)
  - [CommandLineParser.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandLineParser.cpp)
- 复审包最低组成：
  - 异常注入点说明
  - 交互日志页
  - 状态结果页
  - 重复验证页

### S7 工艺异常与恢复

- 当前待解除缺口：`B2`、`B3`
- 证据采集重点：
  - 异常触发记录
  - 报警记录
  - 处理动作记录
  - 恢复或安全停止结果
  - 至少一次重复验证记录
- 业务锚点：
  - [control-cli README](D:/Projects/SiligenSuite/apps/control-cli/README.md)
  - [CommandHandlers.Dispensing.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Dispensing.cpp)
  - [CommandHandlers.Connection.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Connection.cpp)
- 复审包最低组成：
  - 异常类型说明
  - 报警与处理记录
  - 恢复/安全停止结果页
  - 重复验证页

### S8 急停处置链

- 当前待解除缺口：`B2`、`B3`
- 证据采集重点：
  - `estop` 触发记录
  - 锁定状态记录
  - 复位、重新入场或终止记录
  - 至少一次重复验证记录
- 业务锚点：
  - [control-cli README](D:/Projects/SiligenSuite/apps/control-cli/README.md)
  - [CommandHandlers.Motion.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Motion.cpp)
  - [main.cpp](D:/Projects/SiligenSuite/apps/control-cli/main.cpp)
- 复审包最低组成：
  - 急停触发页
  - 锁定与复位链记录
  - 后续状态结果页
  - 重复验证页

### S9 关键数据组合对照

- 当前待解除缺口：`B1`、`B3`
- 证据采集重点：
  - 两组关键输入
  - 两组结果输出
  - 差异说明
  - 至少一次重复验证记录
- 业务锚点：
  - [control-cli README](D:/Projects/SiligenSuite/apps/control-cli/README.md)
  - [CommandHandlers.Recipe.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Recipe.cpp)
  - [CommandHandlers.Dxf.cpp](D:/Projects/SiligenSuite/apps/control-cli/CommandHandlers.Dxf.cpp)
- 复审包最低组成：
  - 两组输入页
  - 两组输出页
  - 差异对照页
  - 重复验证页

## 五、建议的落盘组织方式

为保证复审包可回溯，建议按以下方式组织场景证据：

- `S1/`
  - `inputs.md`
  - `run-record.md`
  - `outputs.md`
  - `repeat-check.md`
- `S2/` 到 `S9/`
  - 采用同样结构

若暂时不新建目录，也至少应保证每个场景的证据包内容在单场景记录页中可逐条挂接。

## 六、从“待补证据”切到“可复审”的最小门槛

每个场景从当前状态进入可复审，至少应满足：

- 证据准备表中的“证据来源”不再写“待补”
- 至少有一条正式运行或异常记录可被直接引用
- 至少有一条结果页或差异页可被直接引用
- 已能明确写出本次复审对应解除的是哪一条阻断项或待补缺口

若以上任一条不满足，则该场景仍应维持 `待补证据`，不进入复审判定。

## 七、本稿结论

当前可冻结的结论是：

- 逐场景证据采集已经可以围绕 `S1-S9` 分别展开，不再停留在总表层
- 每个场景的复审输入包最低构成已经明确，可直接组织补证据工作
- 后续若继续推进，应以场景证据落盘和复审输入包组装为主，而不是继续新增上位文稿
