# 轨迹预览异常 Incident 报告

- Ticket: NOISSUE
- Branch: feat/hmi/NOISSUE-dispense-trajectory-preview-v2
- Timestamp: 20260322-040538
- 结论状态: Root Cause Confirmed

## 1. 事件描述
rect_diag.dxf 的轮廓是矩形（含一条对角线），但 HMI 预览中显示为波浪线。用户预期是与几何轮廓一致的点状轨迹。

## 2. 复现步骤
1. 启动 HMI 在线模式，并连接当前会话中的 mock 后端（mock_server.py，端口 9527）。
2. 上传 examples/dxf/rect_diag.dxf，执行计划准备与预览快照获取。
3. 抓取 dxf.preview.snapshot 返回的 trajectory_polyline。

复现证据（本次实测）：
- poly_count=240
- 前 10 个点呈现 x 线性递增、y 正弦变化。
- y_range 约等于 [-6.0, 6.0]

## 3. 根因分析
根因是后端数据源为 mock，且 mock 轨迹生成逻辑固定为正弦曲线，不读取 DXF 几何轮廓。

证据链：
1. 当前连接进程确认为 mock：
   - python ... apps/hmi-app/src/hmi_client/tools/mock_server.py --host 127.0.0.1 --port 9527
2. mock_server.py 在 dxf.preview.snapshot 中直接按公式生成轨迹：
   - x = t * total_length
   - y = 6.0 * math.sin(t * 4.0 * math.pi)
   - 位置：apps/hmi-app/src/hmi_client/tools/mock_server.py 第 539-543 行
3. mock_server.py 在 dxf.load 与 dxf.artifact.create 中未解析 DXF 实体，使用固定统计值：
   - segment_count = 120
   - total_length = 256.0
   - 位置：apps/hmi-app/src/hmi_client/tools/mock_server.py 第 471-473 行、第 499-500 行
4. HMI 渲染层仅按返回点集绘制点云，不会主动把矩形变成波浪线：
   - 位置：apps/hmi-app/src/hmi_client/ui/main_window.py 第 2386-2397 行

因此，矩形显示成波浪线不是渲染误差，而是 mock 返回的数据本身就是波浪线。

## 4. 修复说明
本次未提交代码修复，先给出可执行修复路径：

- 立即修复（运行层）：
  - 人工验收真实轨迹时，不使用 mock_server.py，改连真实 transport-gateway + runtime 链路。
- 工程修复（防误用）：
  - 方案 A：mock 响应增加 preview_source=mock_synthetic，HMI 明示“模拟轨迹，非真实几何”。
  - 方案 B：mock 侧接入 DXF 几何解析，按实体生成近似 polyline，而非正弦占位曲线。

## 5. 回归验证
已完成的验证：
1. 使用当前 mock 链路对 rect_diag.dxf 拉取快照，轨迹为正弦，复现成功。
2. 对照源码确认正弦生成公式与返回点集一致，根因闭环。

待执行验证（切换真实链路后）：
1. 预览点集应包含矩形边界的轴向段与对角线段，不应出现周期性正弦起伏。
2. 随机抽样点检查：边段应满足 x 或 y 近似常量。

## 6. 防复发措施
1. 在 HMI 状态栏或预览面板增加数据源标识（mock 或 runtime）。
2. 为 dxf.preview.snapshot 增加契约测试：mock 模式必须显式标记 synthetic。
3. 验收流程中新增门禁：涉及真实轨迹外观验收时，禁止仅凭 mock 结果签收。
