# 5. 插补运动指令API

插补运动API提供多轴协调运动功能，支持复杂轨迹控制和精密路径规划。

## 5.1 插补运动基础

### MC_MoveCrdStart

**函数原型**:
```c
int MC_MoveCrdStart(short nInterpGroup);
```

**参数说明**:
- `nInterpGroup` [输入]: 插补组号
  - 类型: short
  - 范围: [1, 8]
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 启动指定插补组的运动规划，开始多轴协调运动。

**典型应用场景**:
- 多轴协调运动启动
- 插补轨迹执行
- 复杂路径控制

**代码示例**:
```c
int result = 0;

// 插补运动启动演示
printf("插补运动启动演示...\n");

// 配置插补参数
short group = 1;
long mask = 0x03; // 轴1和轴2
double synVel = 10.0;
double synAcc = 1.0;
double startPosX = 0, startPosY = 0;
double endPosX = 10000, endPosY = 5000;

// 设置插补参数
result = MC_MoveCrdSetPrm(group, mask, synVel, synAcc, startPosX, startPosY);
result += MC_MoveCrdAddLine(group, endPosX, endPosY, synVel);

if (result == 0) {
    printf("插补参数配置完成\n");
    printf("插补组: %d\n", group);
    printf("参与轴: 轴1、轴2\n");
    printf("同步速度: %.1f 脉冲/ms\n", synVel);
    printf("起点: (%.0f, %.0f)\n", startPosX, startPosY);
    printf("终点: (%.0f, %.0f)\n", endPosX, endPosY);

    // 启动插补运动
    printf("启动插补运动...\n");
    result = MC_MoveCrdStart(group);

    if (result == 0) {
        printf("✓ 插补运动已启动\n");

        // 等待运动完成
        WaitInterpolationComplete(group);
        printf("✓ 插补运动完成\n");
    } else {
        printf("✗ 插补运动启动失败，错误码: %d\n", result);
    }
} else {
    printf("插补参数配置失败，错误码: %d\n", result);
}

printf("插补运动启动演示完成\n");
```

**预期输出**:
```
插补运动启动演示...
插补参数配置完成
插补组: 1
参与轴: 轴1、轴2
同步速度: 10.0 脉冲/ms
起点: (0, 0)
终点: (10000, 5000)
启动插补运动...
✓ 插补运动已启动
✓ 插补运动完成
插补运动启动演示完成
```

**异常处理机制**:
- 检查插补组号范围
- 验证插补参数配置
- 确认轴状态可用

### MC_MoveCrdStop

**函数原型**:
```c
int MC_MoveCrdStop(short nInterpGroup);
```

**参数说明**:
- `nInterpGroup` [输入]: 插补组号
  - 类型: short
  - 插补范围: [1, 8]
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 停止指定插补组的运动。

**典型应用场景**:
- 插补运动紧急停止
- 轨迹修改时停止
- 安全保护触发

**代码示例**:
```c
int result = 0;

// 插补运动停止演示
printf("插补运动停止演示...\n");

// 启动插补运动
MC_MoveCrdSetPrm(1, 0x03, 10.0, 1.0, 0, 0);
MC_MoveCrdAddLine(1, 8000, 6000, 8.0);
MC_MoveCrdAddLine(1, 16000, 8000, 12.0);
MC_MoveCrdStart(1);

printf("插补运动已启动，运行3秒后停止...\n");
Sleep(3000);

// 停止插补运动
printf("停止插补运动...\n");
result = MC_MoveCrdStop(1);

if (result == 0) {
    printf("✓ 插补运动已停止\n");

    // 检查插补状态
    short status = 0;
    MC_MoveCrdGetSts(1, &status);
    printf("插补状态: %d\n", status);
} else {
    printf("✗ 插补运动停止失败，错误码: %d\n", result);
}

printf("插补运动停止演示完成\n");
```

**预期输出**:
```
插补运动停止演示...
插补运动已启动，运行3秒后停止...
停止插补运动...
✓ 插补运动已停止
插补状态: 0
插补运动停止演示完成
```

**异常处理机制**:
- 检查插补组号范围
- 确认插补运动状态
- 验证停止权限

### MC_MoveCrdGetSts

**函数原型**:
```c
int MC_MoveCrdGetSts(short nInterpGroup, short *pStatus);
```

**参数说明**:
- `nInterpGroup` [输入]: 插补组号
  - 类型: short
  - 范围: [1, 8]
  - 单位: 无
- `pStatus` [输出]: 插补状态存放指针
  - 类型: short*
  - 范围: [0, 3]
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 读取指定插补组的运动状态。

**状态值说明**:
- 0: 插补停止
- 1: 插补运行中
- 2: 插补暂停
- 3: 插补缓冲区空

**典型应用场景**:
- 插补运动状态监控
- 运动完成检测
- 缓冲区状态检查

**代码示例**:
```c
int result = 0;

// 插补状态监控演示
printf("插补状态监控演示...\n");

// 启动插补运动
MC_MoveCrdSetPrm(1, 0x03, 8.0, 1.0, 0, 0);
MC_MoveCrdAddLine(1, 5000, 3000, 6.0);
MC_MoveCrdAddLine(1, 10000, 6000, 8.0);
MC_MoveCrdAddArc(1, 15000, 4000, 5000, 8.0, 0);
MC_MoveCrdStart(1);

// 实时监控插补状态
for (int i = 0; i < 30; i++) {
    short status = 0;
    result = MC_MoveCrdGetSts(1, &status);

    if (result == 0) {
        const char* statusStr[] = {"停止", "运行中", "暂停", "缓冲区空"};
        printf("插补组1状态: %s\r", statusStr[status]);
    } else {
        printf("读取插补状态失败，错误码: %d\n", result);
        break;
    }

    Sleep(100);
}

printf("\n插补状态监控完成\n");

// 检查最终状态
short finalStatus = 0;
MC_MoveCrdGetSts(1, &finalStatus);
printf("最终状态: %d\n", finalStatus);
```

**预期输出**:
```
插补状态监控演示...
插补组1状态: 运行中
插补组1状态: 运行中
插补组1状态: 运行中
插补组1状态: 运行中
...
插补组1状态: 停止
插补状态监控完成
最终状态: 0
```

**异常处理机制**:
- 检查插补组号范围
- 验证状态指针有效性
- 确认返回值正确性

---

# 6. 插补运动开发指南

本章提供插补运动的实际开发指南，包括示例代码、调试技巧和最佳实践。

## 6.1 插补运动基础示例

### 完整的直线插补运动

```c
#include <stdio.h>
#include <math.h>
#include "BopaiMotionController.h"

// 等待插补运动完成
void WaitInterpolationComplete(short group) {
    short status = 0;

    while (status != 0) {
        MC_MoveCrdGetSts(group, &status);
        if (status == 0) {
            break; // 插补停止
        }
        Sleep(10);
    }
}

// 直线插补运动示例
void LinearInterpolationExample() {
    int result = 0;

    printf("=== 直线插补运动示例 ===\n");

    // 配置插补参数
    short group = 1;
    long mask = 0x03; // 轴1和轴2
    double synVel = 15.0;
    double synAcc = 2.0;

    // 设置插补参数
    result = MC_MoveCrdSetPrm(group, mask, synVel, synAcc, 0.0, 0.0);
    if (result != 0) {
        printf("插补参数配置失败: %d\n", result);
        return;
    }

    // 定义运动轨迹点
    struct Point {
        double x, y;
        const char* description;
    } trajectory[] = {
        {0, 0, "起点"},
        {2000, 0, "水平移动2000脉冲"},
        {2000, 1500, "垂直移动1500脉冲"},
        {0, 1500, "斜向移动回到x=0"},
        {0, 0, "返回起点"}
    };

    // 添加直线段
    for (int i = 1; i < 5; i++) {
        result = MC_MoveCrdAddLine(group,
                                trajectory[i].x, trajectory[i].y,
                                trajectory[i].x - trajectory[i-1].x,
                                trajectory[i].y - trajectory[i-1].y,
                                synVel);

        printf("添加直线段%d: %s\n", i, trajectory[i].description);
        printf("  从(%.0f, %.0f)到(%.0f, %.0f)\n",
               trajectory[i-1].x, trajectory[i-1].y,
               trajectory[i].x, trajectory[i].y);
    }

    // 启动插补运动
    printf("\n启动插补运动...\n");
    result = MC_MoveCrdStart(group);

    if (result == 0) {
        printf("✓ 插补运动已启动\n");

        // 监控运动进度
        WaitInterpolationComplete(group);
        printf("✓ 插补运动完成\n");
    } else {
        printf("✗ 插补运动启动失败，错误码: %d\n", result);
    }

    printf("=== 直线插补运动示例完成 ===\n\n");
}
```

### 圆弧插补运动

```c
// 圆弧插补运动示例
void ArcInterpolationExample() {
    int result = 0;

    printf("=== 圆弧插补运动示例 ===\n");

    // 配置插补参数
    short group = 2;
    long mask = 0x0C; // 轴3和轴4
    double synVel = 12.0;
    double synAcc = 1.5;

    // 设置插补参数
    result = MC_MoveCrdSetPrm(group, mask, synVel, synAcc, 0.0, 0.0);
    if (result != 0) {
        printf("插补参数配置失败: %d\n", result);
        return;
    }

    // 定义圆弧参数
    double centerX = 5000, centerY = 3000;
    double radius = 2000;
    double startAngle = 0, endAngle = 270; // 270度圆弧

    // 添加圆弧段
    result = MC_MoveCrdAddArc(group,
                            centerX + radius * cos(startAngle * M_PI / 180.0),
                            centerY + radius * sin(startAngle * M_PI / 180.0),
                            centerX, centerY,
                            radius,
                            endAngle - startAngle,
                            synVel);

    printf("添加圆弧段:\n");
    printf("   圆心: (%.1f, %.1f)\n", centerX, centerY);
    printf("  半径: %.1f 脉冲\n", radius);
    printf("  起始角度: %.1f°\n", startAngle);
    printf  结束角度: %.1f°\n", endAngle);
    printf("  角度范围: %.1f°\n", endAngle - startAngle);

    // 启动插补运动
    printf("\n启动圆弧插补运动...\n");
    result = MC_MoveCrdStart(group);

    if (result == 0) {
        printf("✓ 圆弧插补运动已启动\n");
        WaitInterpolationComplete(group);
        printf("✓ 圆弧插补运动完成\n");
    } else {
        printf("✗ 圆弧插补运动启动失败，错误码: %d\n", result);
    }

    printf("=== 圆弧插补运动示例完成 ===\n\n");
}
```

## 6.2 复杂轨迹规划

### 二维图形轨迹

```c
// 绘制正方形轨迹
void SquareTrajectory() {
    int result = 0;

    printf("=== 正方形轨迹示例 ===\n");

    // 配置插补参数
    short group = 3;
    long mask = 0x03; // 轴1和轴2
    double synVel = 8.0;
    double synAcc = 1.0;

    MC_MoveCrdSetPrm(group, mask, synVel, synAcc, 0, 0);

    // 正方形四个顶点
    double points[][2] = {
        {0, 0},      // 左下角
        {3000, 0},    // 右下角
        {3000, 2000},  // 右上角
        {0, 2000}     // 左上角
    };

    // 添加四条边
    for (int i = 1; i <= 4; i++) {
        int next = (i % 4) + 1;
        MC_MoveCrdAddLine(group,
                                points[next-1][0], points[next-1][1],
                                points[i-1][0], points[i-1][1],
                                synVel);
    }

    // 闭合正方形
    MC_MoveCrdAddLine(group, points[3][0], points[3][1], points[0][0], points[0][1], synVel);

    // 启动运动
    printf("启动正方形轨迹运动...\n");
    MC_MoveCrdStart(group);
    WaitInterpolationComplete(group);
    printf("正方形轨迹完成\n");

    printf("=== 正方形轨迹示例完成 ===\n\n");
}

// 绘制星形轨迹
void StarTrajectory() {
    int result = 0;

    printf("=== 星形轨迹示例 ===\n");

    // 配置插补参数
    short group = 4;
    long mask = 0x03; // 轴1和轴2
    double synVel = 6.0;
    double synAcc = 0.8;

    MC_MoveCrdSetPrm(group, mask, synVel, synAcc, 0, 0);

    // 五角星参数
    double outerRadius = 3000;
    double innerRadius = 1000;
    double center = 0;

    // 五个外顶点
    for (int i = 0; i < 5; i++) {
        double angle1 = (i * 72 - 90) * M_PI / 180.0;
        double angle2 = ((i + 1) * 72 - 90) * M_PI / 180.0;
        double x1 = center + outerRadius * cos(angle1);
        double y1 = center + outerRadius * sin(angle1);
        double x2 = center + innerRadius * cos(angle1);
        double y2 = center + innerRadius * sin(angle1);

        // 外顶点到内顶点
        MC_MoveCrdAddLine(group, x1, y1, x2, y2, synVel);

        // 内顶点到下一个外顶点
        angle2 = ((i + 1) * 72 - 90) * M_PI / 180.0;
        x2 = center + outerRadius * cos(angle2);
        y2 = center + outerRadius * sin(angle2);
        MC_MoveCrdAddLine(group, x2, y2, x2, y2, synVel);
    }

    // 闭合星形
    // 最后一个外顶点到第一个外顶点
    double firstAngle = -90 * M_PI / 180.0;
    double lastAngle = (4 * 72 - 90) * M_PI / 180.0;
    double x1 = center + outerRadius * cos(firstAngle);
    double y1 = center + outerRadius * sin(firstAngle);
    double x2 = center + outerRadius * cos(lastAngle);
    double y2 = center + outerRadius * sin(lastAngle);
    MC_MoveCrdAddLine(group, x2, y2, x1, y1, synVel);

    // 启动运动
    printf("启动星形轨迹运动...\n");
    MC_MoveCrdStart(group);
    WaitInterpolationComplete(group);
    printf("星形轨迹完成\n");

    printf("=== 星形轨迹示例完成 ===\n\n");
}
```

## 6.3 多轴协调控制

### 三维空间轨迹

```c
// 三维螺旋线轨迹
void Spiral3DTrajectory() {
    int result = 0;

    printf("=== 三维螺旋线轨迹示例 ===\n");

    // 配置三维插补参数
    short group = 5;
    long mask = 0x07; // 轴1、轴2、轴3
    double synVel = 5.0;
    double synAcc = 0.5;

    MC_MoveCrdSetPrm(group, mask, synVel, synAcc, 0, 0, 0);

    // 三维螺旋线参数
    double radius = 2000;
    double height = 5000;
    int turns = 3; // 3圈
    int pointsPerTurn = 36; // 每圈36个点
    double angleStep = 360.0 / pointsPerTurn;

    // 生成螺旋线点
    for (int turn = 0; turn < turns; turn++) {
        double baseAngle = turn * 360.0;

        for (int i = 0; i < pointsPerTurn; i++) {
            double angle = baseAngle + i * angleStep;
            double radiusCurrent = radius + (double)turn * 500.0; // 螺旋扩大
            double z = (double)(turn * pointsPerTurn + i) * height / (turns * pointsPerTurn);

            double x = radiusCurrent * cos(angle * M_PI / 180.0);
            double y = radiusCurrent * sin(angle * M_PI / 180.0);

            // 添加螺旋线段
            if (turn == 0 && i == 0) {
                // 第一个点，不需要线段
            } else {
                double prevAngle = baseAngle + (i - 1) * angleStep;
                double prevRadius = radius + (double)turn * 500.0 + (i - 1) * height / (turns * pointsPerTurn);
                double prevZ = (double)(turn * pointsPerTurn + i - 1) * height / (turns * pointsPerTurn);
                double prevX = prevRadius * cos(prevAngle * M_PI / 180.0);
                double prevY = prevRadius * sin(prevAngle * M_PI / 180.0);

                MC_MoveCrdAddLine(group, prevX, prevY, prevZ, x, y, z, synVel);
            }
        }
    }

    // 启动三维运动
    printf("启动三维螺旋线运动...\n");
    MC_MoveCrdStart(group);
    WaitInterpolationComplete(group);
    printf("三维螺旋线轨迹完成\n");

    printf("=== 三维螺旋线轨迹示例完成 ===\n\n");
}
```

### 多轴同步控制

```c
// 多轴同步运动示例
void MultiAxisSync() {
    int result = 0;

    printf("=== 多轴同步运动示例 ===\n");

    // 配置三个插补组，控制不同轴组合
    // 组1: 轴1、轴2 (XY平面)
    // 组2: 轴2、轴3 (YZ平面)
    // 组3: 轴1、轴3 (XZ平面)

    // 组1: XY平面圆形运动
    MC_MoveCrdSetPrm(1, 0x03, 10.0, 1.0, 0, 0);
    MC_MoveCrdAddCircle(1, 2000.0, 2000.0, 1000.0, 0, 360.0, 10.0);

    // 组2: YZ平面圆形运动
    MC_MoveCrdSetPrm(2, 0x06, 8.0, 0.8, 0, 2000.0);
    MC_MoveCrdAddCircle(2, 3000.0, 4000.0, 800.0, 0, 180.0, 8.0);

    // 组3: XZ平面圆形运动
    MC_MoveCrdSetPrm(3, 0x05, 6.0, 0.6, 0, 3000.0);
    MC_MoveCrdAddCircle(3, 1000.0, 5000.0, 1500.0, 0, 270.0, 6.0);

    // 同时启动三个插补组
    printf("启动多轴同步运动...\n");

    for (int group = 1; group <= 3; group++) {
        result = MC_MoveCrdStart(group);
        if (result != 0) {
            printf("组%d启动失败: %d\n", group, result);
        }
    }

    // 等待所有插补组完成
    printf("等待所有插补组完成...\n");
    for (int group = 1; group <= 3; group++) {
        WaitInterpolationComplete(group);
        printf("插补组%d完成\n", group);
    }

    printf("=== 多轴同步运动示例完成 ===\n\n");
}
```

## 6.4 插补运动调试技巧

### 实时轨迹显示

```c
// 实时轨迹显示
void RealTimeTrajectoryDisplay() {
    int result = 0;

    printf("=== 实时轨迹显示 ===\n");

    // 配置插补参数
    short group = 1;
    long mask = 0x03;
    double synVel = 20.0;
    double synAcc = 2.0;

    MC_MoveCrdSetPrm(group, mask, synVel, synAcc, 0, 0);

    // 添加复杂的螺旋轨迹
    for (int i = 0; i < 100; i++) {
        double angle = i * 3.6 * M_PI / 180.0;
        double radius = 1000 + i * 10;
        double x = radius * cos(angle);
        double y = radius * sin(angle);

        MC_MoveCrdAddLine(group, x, y, synVel);
    }

    // 启动插补运动并实时显示位置
    MC_MoveCrdStart(group);

    printf("实时轨迹显示 (按任意键停止):\n");

    double positions[100][2];
    int pointIndex = 0;

    while (!_kbhit() && pointIndex < 100) {
        // 获取当前位置
        double x = 0, y = 0, z = 0;
        MC_GetPrfPos(1, &x);
        MC_GetPrfPos(2, &y);

        positions[pointIndex][0] = x;
        positions[pointIndex][1] = y;
        pointIndex++;

        // 绘制轨迹点
        printf("\r轨迹点%d: (%.1f, %.1f)", pointIndex, x, y);

        Sleep(50);
    }

    // 停止插补运动
    MC_MoveCrdStop(group);
    printf("\n轨迹显示完成\n");

    printf("=== 实时轨迹显示完成 ===\n\n");
}
```

### 插补运动分析工具

```c
// 插补运动数据分析
void AnalyzeInterpolation() {
    int result = 0;

    printf("=== 插补运动数据分析 ===\n");

    // 配置测试插补运动
    MC_MoveCrdSetPrm(1, 0x03, 10.0, 1.0, 0, 0);

    // 添加多种轨迹段进行测试
    struct TestSegment {
        const char* name;
        double length;
        double expectedTime;
    } segments[] = {
        {"短直线段", 1000.0, 100.0},
        {"中直线段", 5000.0, 500.0},
        {"长直线段", 10000.0, 1000.0},
        {"小圆弧", 3141.59, 314.16},
        {"大圆弧", 6283.19, 628.32}
    };

    for (int i = 0; i < 5; i++) {
        printf("测试段%d: %s\n", i + 1, segments[i].name);

        // 添加直线段
        if (strstr(segments[i].name, "直线段") != NULL) {
            MC_MoveCrdAddLine(1,
                segments[i].length, 0, segments[i].length, 0,
                segments[i].expectedTime);
        } else {
            // 添加圆弧段
            MC_MoveCrdAddArc(1,
                0, 0, segments[i].length, 0,
                segments[i].length / 2.0, 360.0,
                segments[i].expectedTime);
        }

        // 记录开始时间
        DWORD startTime = GetTickCount();
        MC_MoveCrdStart(1);

        // 等待完成
        WaitInterpolationComplete(1);

        DWORD actualTime = GetTickCount() - startTime;
        double expectedTimeMs = segments[i].expectedTime;

        printf("  预期时间: %.2f ms\n", expectedTimeMs);
        printf("  实际时间: %ld ms\n", actualTime);
        printf("  时间误差: %ld ms\n", actualTime - expectedTimeMs);
        printf("  精度: %.2f%%\n", (double)actualTime / expectedTimeMs * 100);

        printf("\n");
    }

    printf("=== 插补运动数据分析完成 ===\n\n");
}
```

---

