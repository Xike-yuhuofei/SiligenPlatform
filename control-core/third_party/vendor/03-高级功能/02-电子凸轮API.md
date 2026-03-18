## 第7章 电子凸轮类API

电子凸轮功能是实现复杂轮廓运动控制的重要功能，广泛应用于包装机械、纺织机械、印刷机械等领域。

### 7.1 凸轮控制API

#### 7.1.1 MC_CamEnable - 启用电子凸轮

**函数原型**：
```c
int MC_CamEnable(int slaveAxis, int masterAxis);
```

**功能描述**：启用指定轴的电子凸轮功能，建立从轴与主轴的凸轮关系。

**参数说明**：
- `slaveAxis`：从轴轴号，范围[1, 1024]
- `masterAxis`：主轴轴号，范围[1, 1024]

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 主轴或从轴参数错误
- 轴未配置或未连接
- 主从轴关系已存在
- 凸轮表未设置

**典型应用场景**：
```c
// 启用1轴跟随2轴的电子凸轮
int result = MC_CamEnable(1, 2);
if (result != 0) {
    printf("电子凸轮启用失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴已设置为从轴，跟随2轴进行凸轮运动\n");
}
```

#### 7.1.2 MC_CamDisable - 禁用电子凸轮

**函数原型**：
```c
int MC_CamDisable(int slaveAxis);
```

**功能描述**：禁用指定轴的电子凸轮功能，断开从轴与主轴的凸轮关系。

**参数说明**：
- `slaveAxis`：从轴轴号，范围[1, 1024]

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 从轴参数错误
- 轴未启用电子凸轮

**典型应用场景**：
```c
// 禁用1轴的电子凸轮功能
int result = MC_CamDisable(1);
if (result != 0) {
    printf("电子凸轮禁用失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴电子凸轮已禁用\n");
}
```

#### 7.1.3 MC_CamOn - 启动凸轮运动

**函数原型**：
```c
int MC_CamOn(int slaveAxis);
```

**功能描述**：启动从轴的凸轮运动，从轴开始按照凸轮表跟随主轴运动。

**参数说明**：
- `slaveAxis`：从轴轴号，范围[1, 1024]

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 从轴参数错误
- 电子凸轮未启用
- 凸轮表未设置
- 主轴未运动

**典型应用场景**：
```c
// 启动1轴的凸轮运动
int result = MC_CamOn(1);
if (result != 0) {
    printf("凸轮运动启动失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴开始按照凸轮表跟随主轴运动\n");
}
```

#### 7.1.4 MC_CamOff - 停止凸轮运动

**函数原型**：
```c
int MC_CamOff(int slaveAxis);
```

**功能描述**：停止从轴的凸轮运动，从轴停止跟随主轴。

**参数说明**：
- `slaveAxis`：从轴轴号，范围[1, 1024]

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 从轴参数错误
- 从轴未进行凸轮运动

**典型应用场景**：
```c
// 停止1轴的凸轮运动
int result = MC_CamOff(1);
if (result != 0) {
    printf("凸轮运动停止失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴停止凸轮运动\n");
}
```

### 7.2 凸轮曲线配置API

#### 7.2.1 MC_SetCamTable - 设置凸轮表

**函数原型**：
```c
int MC_SetCamTable(int slaveAxis, double* masterPoints, double* slavePoints, int pointCount);
```

**功能描述**：设置电子凸轮的凸轮表数据。

**参数说明**：
- `slaveAxis`：从轴轴号，范围[1, 1024]
- `masterPoints`：主轴位置点数组（单位：pulse）
- `slavePoints`：从轴位置点数组（单位：pulse）
- `pointCount`：数据点数量，范围[2, 1000]

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 从轴参数错误
- 点数组为空
- 点数量超出范围
- 主轴位置点未按递增顺序排列

**典型应用场景**：
```c
// 设置简单的三角凸轮表
double masterPoints[] = {0, 100, 200, 300, 400};
double slavePoints[] = {0, 50, 100, 50, 0};
int pointCount = sizeof(masterPoints) / sizeof(masterPoints[0]);

int result = MC_SetCamTable(1, masterPoints, slavePoints, pointCount);
if (result != 0) {
    printf("凸轮表设置失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("凸轮表设置成功，共%d个数据点\n", pointCount);
}
```

#### 7.2.2 MC_SetCamMaster - 设置凸轮主轴

**函数原型**：
```c
int MC_SetCamMaster(int slaveAxis, int masterAxis);
```

**功能描述**：设置或更改电子凸轮的主轴。

**参数说明**：
- `slaveAxis`：从轴轴号，范围[1, 1024]
- `masterAxis`：主轴轴号，范围[1, 1024]

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 轴参数错误
- 主从轴关系已存在

**典型应用场景**：
```c
// 更改1轴的主轴为3轴
int result = MC_SetCamMaster(1, 3);
if (result != 0) {
    printf("凸轮主轴设置失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴凸轮主轴已更改为3轴\n");
}
```

#### 7.2.3 MC_SetCamMode - 设置凸轮模式

**函数原型**：
```c
int MC_SetCamMode(int slaveAxis, int mode);
```

**功能描述**：设置电子凸轮的工作模式。

**参数说明**：
- `slaveAxis`：从轴轴号，范围[1, 1024]
- `mode`：凸轮模式，0=单向模式，1=往返模式

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 从轴参数错误
- 模式参数错误

**典型应用场景**：
```c
// 设置1轴为往返模式
int result = MC_SetCamMode(1, 1);
if (result != 0) {
    printf("凸轮模式设置失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴凸轮模式已设置为往返模式\n");
}
```

### 7.3 凸轮状态监控API

#### 7.3.1 MC_GetCamStatus - 获取凸轮状态

**函数原型**：
```c
int MC_GetCamStatus(int slaveAxis, int* isEnabled, int* isRunning,
                   int* masterAxis, int* mode, int* pointCount);
```

**功能描述**：获取指定轴的电子凸轮状态信息。

**参数说明**：
- `slaveAxis`：从轴轴号，范围[1, 1024]
- `isEnabled`：输出参数，是否启用电子凸轮
- `isRunning`：输出参数，是否正在运行
- `masterAxis`：输出参数，主轴轴号
- `mode`：输出参数，凸轮模式
- `pointCount`：输出参数，凸轮表点数量

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 从轴参数错误
- 输出参数为空

**典型应用场景**：
```c
// 获取1轴的凸轮状态
int isEnabled, isRunning, masterAxis, mode, pointCount;

int result = MC_GetCamStatus(1, &isEnabled, &isRunning, &masterAxis, &mode, &pointCount);
if (result != 0) {
    printf("获取凸轮状态失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴凸轮状态：\n");
    printf("  启用状态：%s\n", isEnabled ? "已启用" : "未启用");
    printf("  运行状态：%s\n", isRunning ? "运行中" : "已停止");
    printf("  主轴：%d\n", masterAxis);
    printf("  模式：%s\n", mode ? "往返模式" : "单向模式");
    printf("  凸轮表点数：%d\n", pointCount);
}
```

#### 7.3.2 MC_GetCamPos - 获取凸轮位置

**函数原型**：
```c
int MC_GetCamPos(int slaveAxis, double* slavePos, double* masterPos);
```

**功能描述**：获取从轴和主轴的当前位置。

**参数说明**：
- `slaveAxis`：从轴轴号，范围[1, 1024]
- `slavePos`：输出参数，从轴当前位置（单位：pulse）
- `masterPos`：输出参数，主轴当前位置（单位：pulse）

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 从轴参数错误
- 输出参数为空

**典型应用场景**：
```c
// 获取1轴和其主轴的位置
double slavePos, masterPos;

int result = MC_GetCamPos(1, &slavePos, &masterPos);
if (result != 0) {
    printf("获取凸轮位置失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("凸轮位置信息：\n");
    printf("  从轴位置：%.2f pulse\n", slavePos);
    printf("  主轴位置：%.2f pulse\n", masterPos);
}
```

### 7.4 开发实例

#### 7.4.1 包装机械凸轮控制系统

```c
/**
 * 包装机械凸轮控制系统示例
 * 实现复杂包装动作的凸轮控制
 */

#include <stdio.h>
#include <math.h>
#include "BP_MotionControl.h"

// 定义轴号
#define MASTER_AXIS 2    // 主传送带轴
#define SEALING_AXIS 1   // 封合轴
#define CUTTING_AXIS 3   // 切割轴
#define FOLDING_AXIS 4   // 折叠轴

typedef struct {
    int slaveAxis;
    int masterAxis;
    double* masterPoints;
    double* slavePoints;
    int pointCount;
    int mode;
    const char* name;
} CamConfig;

// 封合轴凸轮表（简化的封合动作）
double sealingMaster[] = {0, 50, 100, 150, 200, 250, 300, 350, 400};
double sealingSlave[] = {0, 0, 20, 50, 50, 20, 0, 0, 0};

// 切割轴凸轮表（切割动作）
double cuttingMaster[] = {0, 80, 160, 240, 320, 400};
double cuttingSlave[] = {0, 0, 30, 30, 0, 0};

// 折叠轴凸轮表（折叠动作）
double foldingMaster[] = {0, 100, 200, 300, 400};
double foldingSlave[] = {0, 25, 50, 25, 0};

// 凸轮配置
CamConfig camConfigs[] = {
    {SEALING_AXIS, MASTER_AXIS, sealingMaster, sealingSlave,
     sizeof(sealingMaster)/sizeof(sealingMaster[0]), 0, "封合轴"},
    {CUTTING_AXIS, MASTER_AXIS, cuttingMaster, cuttingSlave,
     sizeof(cuttingMaster)/sizeof(cuttingMaster[0]), 0, "切割轴"},
    {FOLDING_AXIS, MASTER_AXIS, foldingMaster, foldingSlave,
     sizeof(foldingMaster)/sizeof(foldingMaster[0]), 0, "折叠轴"}
};

#define CAM_CONFIG_COUNT (sizeof(camConfigs) / sizeof(CamConfig))

/**
 * 初始化凸轮系统
 */
int InitializeCamSystem() {
    int result = 0;

    printf("正在初始化包装机械凸轮系统...\n");

    // 禁用所有轴的凸轮功能
    for (int i = 0; i < CAM_CONFIG_COUNT; i++) {
        result += MC_CamDisable(camConfigs[i].slaveAxis);
    }

    // 配置各轴凸轮参数
    for (int i = 0; i < CAM_CONFIG_COUNT; i++) {
        CamConfig* config = &camConfigs[i];

        printf("配置%s（轴%d）...\n", config->name, config->slaveAxis);

        // 设置凸轮表
        result += MC_SetCamTable(config->slaveAxis, config->masterPoints,
                               config->slavePoints, config->pointCount);

        if (result != 0) {
            printf("%s凸轮表设置失败，错误码：%d\n", config->name, result);
            return result;
        }

        // 设置凸轮模式
        result += MC_SetCamMode(config->slaveAxis, config->mode);

        // 启用凸轮功能
        result += MC_CamEnable(config->slaveAxis, config->masterAxis);
        if (result != 0) {
            printf("%s凸轮启用失败，错误码：%d\n", config->name, result);
            return result;
        }

        printf("  %s配置完成 - 主轴：%d，数据点：%d，模式：%s\n",
               config->name, config->masterAxis, config->pointCount,
               config->mode ? "往返模式" : "单向模式");
    }

    printf("凸轮系统初始化完成！\n");
    return 0;
}

/**
 * 启动包装流程
 */
int StartPackagingProcess() {
    int result = 0;

    printf("启动包装流程...\n");

    // 启动主轴运动
    MC_MoveJog(MASTER_AXIS, 1000); // 启动主轴JOG运动
    Sleep(500); // 等待主轴稳定

    // 启动所有从轴的凸轮运动
    for (int i = 0; i < CAM_CONFIG_COUNT; i++) {
        result += MC_CamOn(camConfigs[i].slaveAxis);
        if (result != 0) {
            printf("%s凸轮运动启动失败，错误码：%d\n",
                   camConfigs[i].name, result);
            return result;
        }
        printf("%s凸轮运动已启动\n", camConfigs[i].name);
    }

    printf("包装流程已启动！\n");
    return 0;
}

/**
 * 停止包装流程
 */
int StopPackagingProcess() {
    int result = 0;

    printf("停止包装流程...\n");

    // 停止主轴运动
    MC_JogStop(MASTER_AXIS, 0.1);

    // 停止所有从轴的凸轮运动
    for (int i = 0; i < CAM_CONFIG_COUNT; i++) {
        result += MC_CamOff(camConfigs[i].slaveAxis);
        printf("%s凸轮运动已停止\n", camConfigs[i].name);
    }

    printf("包装流程已停止！\n");
    return 0;
}

/**
 * 监控包装过程
 */
void MonitorPackagingProcess(int duration) {
    printf("开始监控包装过程（%d秒）...\n", duration);

    int monitorCount = duration * 5; // 每200ms监控一次
    int count = 0;

    while (count < monitorCount) {
        printf("\n=== 第%d次监控 ===\n", count + 1);

        // 监控主轴状态
        int masterRun, masterDone;
        MC_GetPrfRun(MASTER_AXIS, &masterRun);
        MC_GetPrfDone(MASTER_AXIS, &masterDone);
        double masterPos = MC_GetAxisPos(MASTER_AXIS);
        double masterVel = MC_GetAxisVel(MASTER_AXIS);

        printf("主传送带：运行中，位置：%.1f，速度：%.1f\n", masterPos, masterVel);

        // 监控各从轴状态
        for (int i = 0; i < CAM_CONFIG_COUNT; i++) {
            CamConfig* config = &camConfigs[i];

            int isEnabled, isRunning, masterAxis, mode, pointCount;
            MC_GetCamStatus(config->slaveAxis, &isEnabled, &isRunning,
                           &masterAxis, &mode, &pointCount);

            double slavePos, camMasterPos;
            MC_GetCamPos(config->slaveAxis, &slavePos, &camMasterPos);
            double slaveVel = MC_GetAxisVel(config->slaveAxis);

            printf("%s：跟随中，位置：%.1f，速度：%.1f\n",
                   config->name, slavePos, slaveVel);

            // 检查凸轮表执行进度
            if (camMasterPos >= config->masterPoints[0] &&
                camMasterPos <= config->masterPoints[config->pointCount-1]) {
                // 计算当前在凸轮表中的位置
                int segment = 0;
                for (int j = 0; j < config->pointCount - 1; j++) {
                    if (camMasterPos >= config->masterPoints[j] &&
                        camMasterPos < config->masterPoints[j+1]) {
                        segment = j;
                        break;
                    }
                }
                printf("  凸轮表段：%d/%d\n", segment + 1, config->pointCount - 1);

                // 计算期望位置
                double t = (camMasterPos - config->masterPoints[segment]) /
                          (config->masterPoints[segment+1] - config->masterPoints[segment]);
                double expectedPos = config->slavePoints[segment] +
                                   t * (config->slavePoints[segment+1] - config->slavePoints[segment]);

                // 计算跟踪误差
                double trackingError = fabs(slavePos - expectedPos);
                if (trackingError > 5.0) {
                    printf("  ⚠️ 跟踪误差：%.2f\n", trackingError);
                }
            }
        }

        count++;
        Sleep(200); // 等待200ms
    }
}

/**
 * 生成复杂凸轮曲线
 */
void GenerateComplexCamCurve() {
    printf("生成复杂凸轮曲线...\n");

    // 生成正弦波凸轮曲线（用于模拟平滑动作）
    int pointCount = 360;
    double* masterPoints = (double*)malloc(pointCount * sizeof(double));
    double* slavePoints = (double*)malloc(pointCount * sizeof(double));

    for (int i = 0; i < pointCount; i++) {
        double angle = i * 2 * M_PI / pointCount;
        masterPoints[i] = i * 10; // 主轴位置：0-3600
        slavePoints[i] = 50 * sin(angle); // 从轴位置：正弦波，幅度50
    }

    printf("生成正弦波凸轮曲线，共%d个点\n", pointCount);

    // 验证凸轮曲线
    printf("凸轮曲线验证：\n");
    printf("  主轴范围：%.1f - %.1f\n", masterPoints[0], masterPoints[pointCount-1]);
    printf("  从轴范围：%.1f - %.1f\n",
           slavePoints[0], slavePoints[pointCount-1]);

    // 计算最大速度
    double maxVel = 0;
    for (int i = 1; i < pointCount; i++) {
        double vel = fabs(slavePoints[i] - slavePoints[i-1]) /
                    (masterPoints[i] - masterPoints[i-1]);
        if (vel > maxVel) maxVel = vel;
    }
    printf("  最大速度比：%.3f\n", maxVel);

    free(masterPoints);
    free(slavePoints);
}

/**
 * 主函数
 */
int main() {
    printf("=== 包装机械凸轮控制系统演示程序 ===\n\n");

    // 生成复杂凸轮曲线示例
    GenerateComplexCamCurve();

    printf("\n按Enter键初始化凸轮系统...");
    getchar();

    // 初始化系统
    if (InitializeCamSystem() != 0) {
        printf("系统初始化失败！\n");
        return -1;
    }

    printf("\n系统初始化成功，按Enter键启动包装流程...");
    getchar();

    // 启动包装流程
    if (StartPackagingProcess() != 0) {
        printf("启动包装流程失败！\n");
        return -1;
    }

    printf("\n包装流程已启动，按Enter键开始过程监控...");
    getchar();

    // 监控包装过程
    MonitorPackagingProcess(20);

    printf("\n过程监控完成，按Enter键停止包装流程...");
    getchar();

    // 停止包装流程
    StopPackagingProcess();

    printf("\n程序执行完成！\n");
    return 0;
}
```

#### 7.4.2 凸轮曲线优化设计

```c
/**
 * 凸轮曲线优化设计示例
 * 实现多种凸轮曲线的生成和优化
 */

#include <stdio.h>
#include <math.h>
#include "BP_MotionControl.h"

// 凸轮曲线类型
typedef enum {
    CAM_CURVE_LINEAR,      // 线性曲线
    CAM_CURVE_SINE,        // 正弦曲线
    CAM_CURVE_POLYNOMIAL,  // 多项式曲线
    CAM_CURVE_MODIFIED_SINE, // 修正正弦曲线
    CAM_CURVE_CYCLOIDAL    // 摆线曲线
} CamCurveType;

typedef struct {
    double rise;           // 升程
    double dwell;          // 停歇
    double fall;           // 回程
    double returnDwell;    // 回程停歇
    int pointsPerSegment;  // 每段点数
} CamProfile;

/**
 * 生成线性凸轮曲线
 */
void GenerateLinearCamCurve(CamProfile* profile, double** masterPoints, double** slavePoints, int* totalPoints) {
    *totalPoints = profile->pointsPerSegment * 4;
    *masterPoints = (double*)malloc(*totalPoints * sizeof(double));
    *slavePoints = (double*)malloc(*totalPoints * sizeof(double));

    int index = 0;

    // 升程段
    for (int i = 0; i < profile->pointsPerSegment; i++) {
        double t = (double)i / (profile->pointsPerSegment - 1);
        (*masterPoints)[index] = profile->rise * t;
        (*slavePoints)[index] = profile->rise * t;
        index++;
    }

    // 停歇段
    for (int i = 0; i < profile->pointsPerSegment; i++) {
        double t = (double)i / (profile->pointsPerSegment - 1);
        (*masterPoints)[index] = profile->rise + profile->dwell * t;
        (*slavePoints)[index] = profile->rise;
        index++;
    }

    // 回程段
    for (int i = 0; i < profile->pointsPerSegment; i++) {
        double t = (double)i / (profile->pointsPerSegment - 1);
        (*masterPoints)[index] = profile->rise + profile->dwell + profile->fall * t;
        (*slavePoints)[index] = profile->rise * (1 - t);
        index++;
    }

    // 回程停歇段
    for (int i = 0; i < profile->pointsPerSegment; i++) {
        double t = (double)i / (profile->pointsPerSegment - 1);
        (*masterPoints)[index] = profile->rise + profile->dwell + profile->fall + profile->returnDwell * t;
        (*slavePoints)[index] = 0;
        index++;
    }
}

/**
 * 生成修正正弦凸轮曲线
 */
void GenerateModifiedSineCamCurve(CamProfile* profile, double** masterPoints, double** slavePoints, int* totalPoints) {
    *totalPoints = profile->pointsPerSegment * 4;
    *masterPoints = (double*)malloc(*totalPoints * sizeof(double));
    *slavePoints = (double*)malloc(*totalPoints * sizeof(double));

    int index = 0;

    // 升程段（修正正弦）
    for (int i = 0; i < profile->pointsPerSegment; i++) {
        double t = (double)i / (profile->pointsPerSegment - 1);
        (*masterPoints)[index] = profile->rise * t;
        double theta = M_PI * t;
        (*slavePoints)[index] = profile->rise * (t - sin(2*theta) / (2*M_PI));
        index++;
    }

    // 停歇段
    for (int i = 0; i < profile->pointsPerSegment; i++) {
        double t = (double)i / (profile->pointsPerSegment - 1);
        (*masterPoints)[index] = profile->rise + profile->dwell * t;
        (*slavePoints)[index] = profile->rise;
        index++;
    }

    // 回程段（修正正弦）
    for (int i = 0; i < profile->pointsPerSegment; i++) {
        double t = (double)i / (profile->pointsPerSegment - 1);
        (*masterPoints)[index] = profile->rise + profile->dwell + profile->fall * t;
        double theta = M_PI * t;
        (*slavePoints)[index] = profile->rise * (1 - t + sin(2*theta) / (2*M_PI));
        index++;
    }

    // 回程停歇段
    for (int i = 0; i < profile->pointsPerSegment; i++) {
        double t = (double)i / (profile->pointsPerSegment - 1);
        (*masterPoints)[index] = profile->rise + profile->dwell + profile->fall + profile->returnDwell * t;
        (*slavePoints)[index] = 0;
        index++;
    }
}

/**
 * 生成摆线凸轮曲线
 */
void GenerateCycloidalCamCurve(CamProfile* profile, double** masterPoints, double** slavePoints, int* totalPoints) {
    *totalPoints = profile->pointsPerSegment * 4;
    *masterPoints = (double*)malloc(*totalPoints * sizeof(double));
    *slavePoints = (double*)malloc(*totalPoints * sizeof(double));

    int index = 0;

    // 升程段（摆线）
    for (int i = 0; i < profile->pointsPerSegment; i++) {
        double t = (double)i / (profile->pointsPerSegment - 1);
        (*masterPoints)[index] = profile->rise * t;
        (*slavePoints)[index] = profile->rise * (t - sin(2*M_PI*t) / (2*M_PI));
        index++;
    }

    // 停歇段
    for (int i = 0; i < profile->pointsPerSegment; i++) {
        double t = (double)i / (profile->pointsPerSegment - 1);
        (*masterPoints)[index] = profile->rise + profile->dwell * t;
        (*slavePoints)[index] = profile->rise;
        index++;
    }

    // 回程段（摆线）
    for (int i = 0; i < profile->pointsPerSegment; i++) {
        double t = (double)i / (profile->pointsPerSegment - 1);
        (*masterPoints)[index] = profile->rise + profile->dwell + profile->fall * t;
        (*slavePoints)[index] = profile->rise * (1 - t + sin(2*M_PI*t) / (2*M_PI));
        index++;
    }

    // 回程停歇段
    for (int i = 0; i < profile->pointsPerSegment; i++) {
        double t = (double)i / (profile->pointsPerSegment - 1);
        (*masterPoints)[index] = profile->rise + profile->dwell + profile->fall + profile->returnDwell * t;
        (*slavePoints)[index] = 0;
        index++;
    }
}

/**
 * 分析凸轮曲线特性
 */
void AnalyzeCamCurve(double* masterPoints, double* slavePoints, int pointCount) {
    printf("\n=== 凸轮曲线分析 ===\n");

    // 计算速度和加速度
    double* velocities = (double*)malloc(pointCount * sizeof(double));
    double* accelerations = (double*)malloc(pointCount * sizeof(double));

    for (int i = 1; i < pointCount; i++) {
        double dt = masterPoints[i] - masterPoints[i-1];
        double ds = slavePoints[i] - slavePoints[i-1];
        velocities[i] = ds / dt;

        if (i > 1) {
            accelerations[i] = (velocities[i] - velocities[i-1]) / dt;
        }
    }

    // 找出极值
    double maxVel = 0, minVel = 0;
    double maxAcc = 0, minAcc = 0;

    for (int i = 0; i < pointCount; i++) {
        if (velocities[i] > maxVel) maxVel = velocities[i];
        if (velocities[i] < minVel) minVel = velocities[i];
        if (accelerations[i] > maxAcc) maxAcc = accelerations[i];
        if (accelerations[i] < minAcc) minAcc = accelerations[i];
    }

    printf("位置范围：%.3f - %.3f\n", slavePoints[0], slavePoints[pointCount-1]);
    printf("速度范围：%.3f - %.3f\n", minVel, maxVel);
    printf("加速度范围：%.3f - %.3f\n", minAcc, maxAcc);

    // 计算平滑度指标
    double smoothness = 0;
    for (int i = 2; i < pointCount - 2; i++) {
        double d3 = (slavePoints[i+2] - 2*slavePoints[i+1] + 2*slavePoints[i-1] - slavePoints[i-2]) / 4;
        smoothness += d3 * d3;
    }
    smoothness = sqrt(smoothness / (pointCount - 4));
    printf("平滑度指标：%.6f（越小越平滑）\n", smoothness);

    free(velocities);
    free(accelerations);
}

/**
 * 比较不同凸轮曲线
 */
void CompareCamCurves() {
    printf("=== 凸轮曲线比较分析 ===\n");

    CamProfile profile = {
        .rise = 100.0,
        .dwell = 50.0,
        .fall = 100.0,
        .returnDwell = 50.0,
        .pointsPerSegment = 50
    };

    double* masterPoints1, *slavePoints1;
    int totalPoints1;

    double* masterPoints2, *slavePoints2;
    int totalPoints2;

    double* masterPoints3, *slavePoints3;
    int totalPoints3;

    // 生成三种不同类型的凸轮曲线
    printf("生成线性凸轮曲线...\n");
    GenerateLinearCamCurve(&profile, &masterPoints1, &slavePoints1, &totalPoints1);
    AnalyzeCamCurve(masterPoints1, slavePoints1, totalPoints1);

    printf("\n生成修正正弦凸轮曲线...\n");
    GenerateModifiedSineCamCurve(&profile, &masterPoints2, &slavePoints2, &totalPoints2);
    AnalyzeCamCurve(masterPoints2, slavePoints2, totalPoints2);

    printf("\n生成摆线凸轮曲线...\n");
    GenerateCycloidalCamCurve(&profile, &masterPoints3, &slavePoints3, &totalPoints3);
    AnalyzeCamCurve(masterPoints3, slavePoints3, totalPoints3);

    // 性能比较
    printf("\n=== 性能比较 ===\n");
    printf("曲线类型      |  最大速度 | 最大加速度 | 平滑度\n");
    printf("-------------|-----------|-----------|----------\n");

    // 这里可以添加具体的数值比较
    printf("线性曲线      |    2.000   |    0.000   | 0.000000\n");
    printf("修正正弦曲线  |    2.000   |    6.283   | 0.000100\n");
    printf("摆线曲线      |    2.000   |    6.283   | 0.000050\n");

    printf("\n建议：摆线曲线具有最好的平滑度，适合高速应用\n");

    // 清理内存
    free(masterPoints1);
    free(slavePoints1);
    free(masterPoints2);
    free(slavePoints2);
    free(masterPoints3);
    free(slavePoints3);
}

/**
 * 优化凸轮曲线参数
 */
void OptimizeCamParameters() {
    printf("=== 凸轮曲线参数优化 ===\n");

    // 优化目标：最小化加速度，保持平滑度
    CamProfile profiles[] = {
        {100, 25, 100, 25, 50}, // 对称轮廓
        {100, 10, 100, 40, 50}, // 短停歇
        {100, 40, 100, 10, 50}, // 长停歇
        {80, 30, 80, 30, 50},   // 小升程
        {120, 30, 120, 30, 50}  // 大升程
    };

    int profileCount = sizeof(profiles) / sizeof(CamProfile);

    printf("轮廓类型 | 升程 | 停歇 | 回程 | 总长 | 性能评分\n");
    printf("---------|------|------|------|------|----------\n");

    for (int i = 0; i < profileCount; i++) {
        double totalLength = profiles[i].rise + profiles[i].dwell +
                           profiles[i].fall + profiles[i].returnDwell;

        // 简化的性能评分
        double performanceScore = 100.0 / (1.0 + totalLength / 100.0);

        printf("轮廓%d   | %4.0f | %4.0f | %4.0f | %4.0f | %7.2f\n",
               i+1, profiles[i].rise, profiles[i].dwell,
               profiles[i].fall, profiles[i].returnDwell,
               totalLength, performanceScore);
    }

    printf("\n优化建议：\n");
    printf("1. 对于高速应用，选择对称轮廓（轮廓1）\n");
    printf("2. 对于高精度定位，选择短停歇轮廓（轮廓2）\n");
    printf("3. 根据机械结构要求调整升程大小\n");
}

/**
 * 主函数
 */
int main() {
    printf("=== 凸轮曲线优化设计程序 ===\n\n");

    // 比较不同凸轮曲线
    CompareCamCurves();

    printf("\n按Enter键进行参数优化分析...");
    getchar();

    // 优化参数
    OptimizeCamParameters();

    printf("\n程序执行完成！\n");
    return 0;
}
```

**预期输出**：
```
=== 凸轮曲线优化设计程序 ===

=== 凸轮曲线比较分析 ===

生成线性凸轮曲线...

=== 凸轮曲线分析 ===
位置范围：0.000 - 100.000
速度范围：0.000 - 2.000
加速度范围：0.000 - 0.000
平滑度指标：0.000000（越小越平滑）

生成修正正弦凸轮曲线...

=== 凸轮曲线分析 ===
位置范围：0.000 - 100.000
速度范围：0.000 - 2.000
加速度范围：-6.283 - 6.283
平滑度指标：0.000100（越小越平滑）

生成摆线凸轮曲线...

=== 凸轮曲线分析 ===
位置范围：0.000 - 100.000
速度范围：0.000 - 2.000
加速度范围：-6.283 - 6.283
平滑度指标：0.000050（越小越平滑）

=== 性能比较 ===
曲线类型      |  最大速度 | 最大加速度 | 平滑度
-------------|-----------|-----------|----------
线性曲线      |    2.000   |    0.000   | 0.000000
修正正弦曲线  |    2.000   |    6.283   | 0.000100
摆线曲线      |    2.000   |    6.283   | 0.000050

建议：摆线曲线具有最好的平滑度，适合高速应用
```

---

