# 点胶机轨迹算法行业最佳实践与方案对比（归档）

日期：2026-01-28

## 说明
- 目标：总结点胶机轨迹算法行业最佳实践与可用第三方工具，形成可选方案清单。
- 方法：Web Search 资料汇总与工程经验整理。
- Context7：当前环境未发现可读资源（list 为空），故本报告暂未引用 context7。若后续提供资源，可补充更新。

## 方案总览（未做最终选型）

### 方案 A：离线轨迹规划 + 工艺参数前馈（基准工业做法）
**核心要点**
- 基于 CAD/工艺几何生成无碰撞路点与连续轨迹，连接工具并验证可达性。
- 通过样条/圆弧/渐变曲率进行拐角平滑，降低过度停顿与重叠。
- 对复杂表面可采用点云切片/栅格法规划路径，并优化速度与切片厚度以提升均匀性。
- 引入加速度/加加速度约束，防止材料拉断或堆胶。
- 关键工艺参数：喷嘴高度、行进速度、压力/预压、胶材温度与粘度。

**优点**
- 实现成熟、成本可控、易于集成；对固定治具和高重复性工件效果好。

**缺点**
- 对工件位置/形变变化适应差；参数标定周期较长。

**风险**
- 夹具误差或热变形导致轨迹偏移；速度/加速度设置不当引发断胶或堆胶。

**适用场景**
- 工件稳定、路径规则、批量一致的 2D/2.5D 轨迹任务。

---

### 方案 B：材料行为建模驱动的轨迹生成
**核心要点**
- 建立胶材行为模型（解析模型或学习模型）预测胶线形态，并据此调整路径、速度与高度。
- 解析模型数据需求少但适用范围有限；学习模型泛化强但数据要求高。

**优点**
- 精度高，显著减少人工试调；对高粘度/易拉丝材料更友好。

**缺点**
- 数据采集与模型维护成本高；材料或工况变化时需再标定/再训练。

**风险**
- 模型漂移或材料批次变化导致补偿错误。

**适用场景**
- 对胶线几何/位置精度要求高、材料行为复杂、产品迭代频繁的场景。

---

### 方案 C：视觉/传感器闭环实时修正
**核心要点**
- 采用 3D 视觉/边缘跟踪实时纠偏，动态修正喷嘴高度与横向位置。
- 可实现缺胶检测与局部返工。

**优点**
- 对工件偏差/变形鲁棒；降低停线与重教导成本。

**缺点**
- 硬件与集成成本高；系统调试复杂度上升。

**风险**
- 遮挡/反光/污染导致误校正；系统延迟影响节拍。

**适用场景**
- 高变差工件、热胀冷缩明显、对节拍与良率要求高的产线。

---

### 方案 D：分层混合（离线基准轨迹 + 模型前馈 + 在线微调）
**核心要点**
- 方案 A 生成基准轨迹；方案 B 做材料补偿；方案 C 在线微调/返修。

**优点**
- 兼顾稳定性与自适应，综合性能最好。

**缺点**
- 系统复杂度最高，需要跨部门协作。

**风险**
- 多算法协同不当可能相互干扰，需要严格优先级与安全边界。

**适用场景**
- 关键质量/高产能/高变差并存的旗舰产线。

---

## 第三方库/工具清单（可选）

### 机器人运动规划/时间参数化
- MoveIt 2 + OMPL：多规划器框架；支持多种采样规划器。
- Pilz 工业规划器：PTP/LIN/CIRC 工业语义轨迹。
- CHOMP / STOMP：优化式平滑轨迹。
- Tesseract 规划栈：ROS 无关、适合工业过程规划，支持 OMPL/TrajOpt/Descartes。
- Ruckig：实时轨迹生成（含加加速度约束）。

### 几何/刀路/点云
- OpenCASCADE：CAD/几何内核。
- OpenCAMLib：刀路生成。
- CGAL：几何算法库（双许可证）。
- PCL：点云处理。

### 商业离线编程/仿真
- RoboDK、ABB RobotStudio、Siemens Process Simulate、Yaskawa MotoSim EG‑VRC、FANUC ROBOGUIDE、KUKA.Sim。

> 注：具体许可条款与兼容性需结合项目与供应商评估。

## 选型建议（用于后续确认）
- 若系统稳定、治具固定：优先 A，成本最低。
- 若精度要求高或材料复杂：优先 B 或 D。
- 若工件偏差大且需在线纠偏：优先 C 或 D。
- 若需快速离线验证：引入商业仿真工具做验证与后处理。

## 待确认事项
- 设备类型（龙门/六轴/SCARA）、控制器、现场总线。
- 胶材类型与粘度、阀类型（针阀/喷射阀）。
- 轨迹形态（点/线/面/3D）与精度要求。
- 是否允许 ROS 2/MoveIt 生态；是否具备视觉/传感器与预算范围。

## 参考链接（归档）
- https://moveit.picknik.ai/main/doc/concepts/motion_planning.html
- https://ompl.kavrakilab.org/core/index.html
- https://github.com/tesseract-robotics/tesseract
- https://github.com/tesseract-robotics/tesseract_planning
- https://tesseract-robotics.github.io/tesseract_python/readme.html
- https://www.opencascade.com/occt3d-technology/
- https://www.cgal.org/
- https://robodk.com/offline-programming
- https://www.abb.com/global/en/areas/robotics/products/software/robotstudio-suite/robotstudio-desktop
- https://www.mathworks.com/help/robotics/ug/design-manipulator-path-for-dispensing-task-ikd.html
- https://www.mdpi.com/1424-8220/22/23/9269
- https://www.cambridge.org/core/journals/robotica/article/point-cloud-modeling-and-slicing-algorithm-for-trajectory-planning-of-spray-painting-robot/8E2665DCBDCA68E200FFEE4CF2E86FAF
- https://link.springer.com/article/10.1007/s40313-024-01093-x
- https://link.springer.com/article/10.1007/s00170-025-15121-w
- https://www.nordson.com/en/products/efd-products/optimum-precision-nozzles
- https://coherix.com/3d-vision-will-revolutionize-dispensing/
- https://www.inbolt.com/use-cases/dispensing/
