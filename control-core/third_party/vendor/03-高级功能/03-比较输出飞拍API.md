## 第8章 比较输出(飞拍)类API

比较输出功能（飞拍）是运动控制中的高级功能，主要用于视觉定位、飞拍检测等应用场景。

### 8.1 比较输出控制API

#### 8.1.1 MC_CompareEnable - 启用比较输出

**函数原型**：
```c
int MC_CompareEnable(int axis, int channel);
```

**功能描述**：启用指定轴的指定通道比较输出功能。

**参数说明**：
- `axis`：轴号，范围[1, 1024]
- `channel`：比较输出通道，范围[1, 4]

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 轴或通道参数错误
- 轴未配置或未连接
- 比较输出参数未设置

**典型应用场景**：
```c
// 启用1轴通道1的比较输出功能
int result = MC_CompareEnable(1, 1);
if (result != 0) {
    printf("比较输出启用失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴通道1比较输出已启用\n");
}
```

#### 8.1.2 MC_CompareDisable - 禁用比较输出

**函数原型**：
```c
int MC_CompareDisable(int axis, int channel);
```

**功能描述**：禁用指定轴的指定通道比较输出功能。

**参数说明**：
- `axis`：轴号，范围[1, 1024]
- `channel`：比较输出通道，范围[1, 4]

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 轴或通道参数错误
- 比较输出未启用

**典型应用场景**：
```c
// 禁用1轴通道1的比较输出功能
int result = MC_CompareDisable(1, 1);
if (result != 0) {
    printf("比较输出禁用失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴通道1比较输出已禁用\n");
}
```

#### 8.1.3 MC_CompareOutput - 执行比较输出

**函数原型**：
```c
int MC_CompareOutput(int axis, int channel, double position, int output);
```

**功能描述**：在指定位置执行比较输出操作。

**参数说明**：
- `axis`：轴号，范围[1, 1024]
- `channel`：比较输出通道，范围[1, 4]
- `position`：比较位置（单位：pulse）
- `output`：输出值，0或1

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 轴或通道参数错误
- 比较输出未启用
- 位置参数超出范围

**典型应用场景**：
```c
// 在1轴位置1000处，通道1输出高电平
int result = MC_CompareOutput(1, 1, 1000.0, 1);
if (result != 0) {
    printf("比较输出执行失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("已设置在位置1000处输出高电平\n");
}
```

### 8.2 飞拍参数配置API

#### 8.2.1 MC_SetComparePrm - 设置比较输出参数

**函数原型**：
```c
int MC_SetComparePrm(int axis, int channel, double comparePos,
                    int outputMode, int triggerMode);
```

**功能描述**：设置比较输出的参数。

**参数说明**：
- `axis`：轴号，范围[1, 1024]
- `channel`：比较输出通道，范围[1, 4]
- `comparePos`：比较位置（单位：pulse）
- `outputMode`：输出模式，0=电平输出，1=脉冲输出
- `triggerMode`：触发模式，0=上升沿触发，1=下降沿触发

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 轴或通道参数错误
- 比较位置超出范围
- 输出模式或触发模式参数错误

**典型应用场景**：
```c
// 设置1轴通道1的比较参数
int result = MC_SetComparePrm(1, 1, 1000.0, 0, 0);
if (result != 0) {
    printf("比较参数设置失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("比较参数设置成功：位置1000，电平输出，上升沿触发\n");
}
```

#### 8.2.2 MC_SetCompareWindow - 设置比较窗口

**函数原型**：
```c
int MC_SetCompareWindow(int axis, int channel, double windowSize);
```

**功能描述**：设置比较输出的位置窗口。

**参数说明**：
- `axis`：轴号，范围[1, 1024]
- `channel`：比较输出通道，范围[1, 4]
- `windowSize`：窗口大小（单位：pulse）

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 轴或通道参数错误
- 窗口大小为负数

**典型应用场景**：
```c
// 设置1轴通道1的比较窗口为±10pulse
int result = MC_SetCompareWindow(1, 1, 10.0);
if (result != 0) {
    printf("比较窗口设置失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("比较窗口设置成功：±10pulse\n");
}
```

#### 8.2.3 MC_SetCompareDelay - 设置比较延迟

**函数原型**：
```c
int MC_SetCompareDelay(int axis, int channel, double delayTime);
```

**功能描述**：设置比较输出的延迟时间。

**参数说明**：
- `axis`：轴号，范围[1, 1024]
- `channel`：比较输出通道，范围[1, 4]
- `delayTime`：延迟时间（单位：ms）

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 轴或通道参数错误
- 延迟时间为负数

**典型应用场景**：
```c
// 设置1轴通道1的比较延迟为5ms
int result = MC_SetCompareDelay(1, 1, 5.0);
if (result != 0) {
    printf("比较延迟设置失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("比较延迟设置成功：5ms\n");
}
```

### 8.3 比较输出状态监控API

#### 8.3.1 MC_GetCompareStatus - 获取比较输出状态

**函数原型**：
```c
int MC_GetCompareStatus(int axis, int channel, int* isEnabled, int* isTriggered,
                       double* currentPos, int* outputState);
```

**功能描述**：获取指定轴指定通道的比较输出状态。

**参数说明**：
- `axis`：轴号，范围[1, 1024]
- `channel`：比较输出通道，范围[1, 4]
- `isEnabled`：输出参数，是否启用
- `isTriggered`：输出参数，是否已触发
- `currentPos`：输出参数，当前位置
- `outputState`：输出参数，当前输出状态

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 轴或通道参数错误
- 输出参数为空

**典型应用场景**：
```c
// 获取1轴通道1的比较输出状态
int isEnabled, isTriggered, outputState;
double currentPos;

int result = MC_GetCompareStatus(1, 1, &isEnabled, &isTriggered, &currentPos, &outputState);
if (result != 0) {
    printf("获取比较状态失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴通道1比较状态：\n");
    printf("  启用状态：%s\n", isEnabled ? "已启用" : "未启用");
    printf("  触发状态：%s\n", isTriggered ? "已触发" : "未触发");
    printf("  当前位置：%.2f\n", currentPos);
    printf("  输出状态：%s\n", outputState ? "高电平" : "低电平");
}
```

#### 8.3.2 MC_GetCompareCount - 获取比较触发次数

**函数原型**：
```c
int MC_GetCompareCount(int axis, int channel, int* triggerCount);
```

**功能描述**：获取比较输出的触发次数。

**参数说明**：
- `axis`：轴号，范围[1, 1024]
- `channel`：比较输出通道，范围[1, 4]
- `triggerCount`：输出参数，触发次数

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 轴或通道参数错误
- 输出参数为空

**典型应用场景**：
```c
// 获取1轴通道1的触发次数
int triggerCount;

int result = MC_GetCompareCount(1, 1, &triggerCount);
if (result != 0) {
    printf("获取触发次数失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴通道1已触发%d次\n", triggerCount);
}
```

### 8.4 开发实例

#### 8.4.1 飞拍定位系统

```c
/**
 * 飞拍定位系统示例
 * 实现基于运动控制的飞拍检测和定位功能
 */

#include <stdio.h>
#include <math.h>
#include "BP_MotionControl.h"

// 定义轴号和通道
#define X_AXIS 1          // X轴
#define Y_AXIS 2          // Y轴
#define Z_AXIS 3          // Z轴
#define TRIGGER_CHANNEL 1 // 触发输出通道
#define CAMERA_CHANNEL 2  // 相机触发通道
#define MARKING_CHANNEL 3 // 标记通道

typedef struct {
    int axis;
    int channel;
    double comparePosition;
    int outputMode;
    int triggerMode;
    double windowSize;
    double delayTime;
    const char* description;
} CompareOutputConfig;

// 比较输出配置
CompareOutputConfig compareConfigs[] = {
    {X_AXIS, TRIGGER_CHANNEL, 1000.0, 1, 0, 5.0, 0.0, "X轴触发位置"},
    {Y_AXIS, CAMERA_CHANNEL, 500.0, 1, 0, 3.0, 1.0, "Y轴相机触发"},
    {Z_AXIS, MARKING_CHANNEL, 200.0, 0, 0, 2.0, 2.0, "Z轴标记输出"}
};

#define COMPARE_CONFIG_COUNT (sizeof(compareConfigs) / sizeof(CompareOutputConfig))

typedef struct {
    int x, y, z;
    int quality;
    int detected;
} DetectionResult;

/**
 * 初始化比较输出系统
 */
int InitializeCompareSystem() {
    int result = 0;

    printf("正在初始化飞拍定位系统...\n");

    // 禁用所有比较输出
    for (int i = 0; i < COMPARE_CONFIG_COUNT; i++) {
        result += MC_CompareDisable(compareConfigs[i].axis, compareConfigs[i].channel);
    }

    // 配置各比较输出参数
    for (int i = 0; i < COMPARE_CONFIG_COUNT; i++) {
        CompareOutputConfig* config = &compareConfigs[i];

        printf("配置%s（轴%d通道%d）...\n", config->description,
               config->axis, config->channel);

        // 设置比较参数
        result += MC_SetComparePrm(config->axis, config->channel,
                                  config->comparePosition,
                                  config->outputMode, config->triggerMode);

        // 设置比较窗口
        result += MC_SetCompareWindow(config->axis, config->channel, config->windowSize);

        // 设置延迟时间
        result += MC_SetCompareDelay(config->axis, config->channel, config->delayTime);

        if (result != 0) {
            printf("%s配置失败，错误码：%d\n", config->description, result);
            return result;
        }

        // 启用比较输出
        result += MC_CompareEnable(config->axis, config->channel);
        if (result != 0) {
            printf("%s启用失败，错误码：%d\n", config->description, result);
            return result;
        }

        printf("  %s配置完成\n", config->description);
        printf("    位置：%.1f pulse\n", config->comparePosition);
        printf("    模式：%s，触发：%s\n",
               config->outputMode ? "脉冲输出" : "电平输出",
               config->triggerMode ? "下降沿" : "上升沿");
        printf("    窗口：±%.1f pulse，延迟：%.1f ms\n",
               config->windowSize, config->delayTime);
    }

    printf("比较输出系统初始化完成！\n");
    return 0;
}

/**
 * 运动到检测位置
 */
int MoveToDetectionPosition(double x, double y, double z, double vel) {
    printf("运动到检测位置：(%.1f, %.1f, %.1f)，速度：%.1f\n", x, y, z, vel);

    int result = 0;

    // 同步运动到目标位置
    MC_MoveCrdAddLine(0, 0, 0, 0, x, y, z, vel);
    Sleep(100);
    MC_MoveCrdStart(0);

    // 等待运动完成
    int done = 0;
    while (!done) {
        MC_GetCrdDone(0, &done);
        Sleep(10);
    }

    printf("已到达检测位置\n");
    return 0;
}

/**
 * 执行飞拍检测
 */
DetectionResult ExecuteFlyingDetection(double scanStart, double scanEnd, double scanStep) {
    printf("执行飞拍检测：扫描范围[%.1f, %.1f]，步长：%.1f\n",
           scanStart, scanEnd, scanStep);

    DetectionResult result = {0, 0, 0, 0, 0};

    int scanPoints = (int)((scanEnd - scanStart) / scanStep) + 1;

    printf("开始扫描，共%d个检测点...\n", scanPoints);

    for (int i = 0; i < scanPoints; i++) {
        double currentPos = scanStart + i * scanStep;

        // 运动到检测位置
        MoveToDetectionPosition(currentPos, 500.0, 200.0, 1000.0);

        // 等待比较输出触发
        Sleep(10); // 等待触发延迟

        // 检查比较输出状态
        for (int j = 0; j < COMPARE_CONFIG_COUNT; j++) {
            int isEnabled, isTriggered, outputState;
            double currentAxisPos;

            MC_GetCompareStatus(compareConfigs[j].axis, compareConfigs[j].channel,
                              &isEnabled, &isTriggered, &currentAxisPos, &outputState);

            if (isTriggered) {
                printf("位置%.1f处%s已触发\n", currentPos, compareConfigs[j].description);

                // 模拟检测结果
                if (j == 1) { // 相机触发
                    result.detected = 1;
                    result.x = (int)currentPos;
                    result.y = 500 + (rand() % 20 - 10); // 模拟Y轴误差
                    result.z = 200 + (rand() % 10 - 5);  // 模拟Z轴误差
                    result.quality = 85 + rand() % 15;   // 模拟检测质量

                    printf("检测到目标：(%d, %d, %d)，质量：%d%%\n",
                           result.x, result.y, result.z, result.quality);
                    break;
                }
            }
        }

        if (result.detected) {
            break;
        }
    }

    if (!result.detected) {
        printf("未检测到目标\n");
    }

    return result;
}

/**
 * 补偿定位误差
 */
int CompensatePositionError(DetectionResult* detected, double* targetX, double* targetY, double* targetZ) {
    printf("补偿定位误差...\n");

    // 计算误差补偿
    double errorX = detected->x - *targetX;
    double errorY = detected->y - *targetY;
    double errorZ = detected->z - *targetZ;

    printf("检测误差：X=%.1f, Y=%.1f, Z=%.1f\n", errorX, errorY, errorZ);

    // 应用补偿
    *targetX += errorX;
    *targetY += errorY;
    *targetZ += errorZ;

    printf("补偿后目标位置：(%.1f, %.1f, %.1f)\n", *targetX, *targetY, *targetZ);

    return 0;
}

/**
 * 监控比较输出状态
 */
void MonitorCompareOutputs(int duration) {
    printf("监控比较输出状态（%d秒）...\n", duration);

    int monitorCount = duration * 10; // 每100ms监控一次
    int count = 0;

    while (count < monitorCount) {
        printf("\n=== 第%d次监控 ===\n", count + 1);

        for (int i = 0; i < COMPARE_CONFIG_COUNT; i++) {
            CompareOutputConfig* config = &compareConfigs[i];

            int isEnabled, isTriggered, outputState;
            double currentPos;

            MC_GetCompareStatus(config->axis, config->channel,
                              &isEnabled, &isTriggered, &currentPos, &outputState);

            int triggerCount;
            MC_GetCompareCount(config->axis, config->channel, &triggerCount);

            printf("%s：", config->description);
            printf("%s", isEnabled ? "启用" : "禁用");
            if (isEnabled) {
                printf("，位置：%.1f", currentPos);
                printf("，%s", isTriggered ? "已触发" : "未触发");
                printf("，输出：%s", outputState ? "高" : "低");
                printf("，触发次数：%d", triggerCount);
            }
            printf("\n");
        }

        count++;
        Sleep(100);
    }
}

/**
 * 主函数
 */
int main() {
    printf("=== 飞拍定位系统演示程序 ===\n\n");

    // 初始化系统
    if (InitializeCompareSystem() != 0) {
        printf("系统初始化失败！\n");
        return -1;
    }

    printf("\n系统初始化成功，按Enter键开始飞拍检测...");
    getchar();

    // 执行飞拍检测
    DetectionResult result = ExecuteFlyingDetection(0, 2000, 200);

    if (result.detected) {
        printf("\n检测成功！按Enter键进行误差补偿...");
        getchar();

        double targetX = 1000.0, targetY = 500.0, targetZ = 200.0;
        CompensatePositionError(&result, &targetX, &targetY, &targetZ);

        printf("\n按Enter键开始状态监控...");
        getchar();

        // 监控比较输出状态
        MonitorCompareOutputs(10);
    }

    printf("\n程序执行完成！\n");
    return 0;
}
```

**预期输出**：
```
=== 飞拍定位系统演示程序 ===

正在初始化飞拍定位系统...
配置X轴触发位置（轴1通道1）...
  X轴触发位置配置完成
    位置：1000.0 pulse
    模式：脉冲输出，触发：上升沿
    窗口：±5.0 pulse，延迟：0.0 ms
配置Y轴相机触发（轴2通道2）...
  Y轴相机触发配置完成
    位置：500.0 pulse
    模式：脉冲输出，触发：上升沿
    窗口：±3.0 pulse，延迟：1.0 ms
配置Z轴标记输出（轴3通道3）...
  Z轴标记输出配置完成
    位置：200.0 pulse
    模式：电平输出，触发：上升沿
    窗口：±2.0 pulse，延迟：2.0 ms
比较输出系统初始化完成！

系统初始化成功，按Enter键开始飞拍检测...

执行飞拍检测：扫描范围[0.0, 2000.0]，步长：200.0
开始扫描，共11个检测点...
运动到检测位置：(0.0, 500.0, 200.0)，速度：1000.0
已到达检测位置
运动到检测位置：(200.0, 500.0, 200.0)，速度：1000.0
已到达检测位置
...
运动到检测位置：(1000.0, 500.0, 200.0)，速度：1000.0
已到达检测位置
位置1000.0处X轴触发位置已触发
位置1000.0处Y轴相机触发已触发
检测到目标：(1000, 505, 203)，质量：92%

检测成功！按Enter键进行误差补偿...

补偿定位误差...
检测误差：X=0.0, Y=5.0, Z=3.0
补偿后目标位置：(1000.0, 505.0, 203.0)
```

---

*(由于上下文长度限制，第8章仅展示了部分内容。完整的高级功能文档将继续在第9-14章中详细阐述自动回零、PT模式、手轮控制、串口通信、坐标系跟随和机械臂操作等高级API功能。)*

---

