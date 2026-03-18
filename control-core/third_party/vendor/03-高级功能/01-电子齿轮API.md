# 博派运动控制卡技术文档 - 高级功能篇目录

- [博派运动控制卡技术文档 - 高级功能篇](#博派运动控制卡技术文档---高级功能篇)
  - [第6章 电子齿轮类API](#第6章-电子齿轮类api)
    - [6.1 电子齿轮控制API](#61-电子齿轮控制api)
    - [6.2 齿轮参数配置API](#62-齿轮参数配置api)
    - [6.3 齿轮状态监控API](#63-齿轮状态监控api)
    - [6.4 开发实例](#64-开发实例)
  - [第7章 电子凸轮类API](#第7章-电子凸轮类api)
    - [7.1 凸轮控制API](#71-凸轮控制api)
    - [7.2 凸轮曲线配置API](#72-凸轮曲线配置api)
    - [7.3 凸轮状态监控API](#73-凸轮状态监控api)
    - [7.4 开发实例](#74-开发实例)
  - [第8章 比较输出(飞拍)类API](#第8章-比较输出飞拍类api)
    - [8.1 比较输出控制API](#81-比较输出控制api)
    - [8.2 飞拍参数配置API](#82-飞拍参数配置api)
    - [8.3 比较输出状态监控API](#83-比较输出状态监控api)
    - [8.4 开发实例](#84-开发实例)
  - [第9章 自动回零相关API](#第9章-自动回零相关api)
    - [9.1 回零控制API](#91-回零控制api)
    - [9.2 回零参数配置API](#92-回零参数配置api)
    - [9.3 回零状态监控API](#93-回零状态监控api)
    - [9.4 开发实例](#94-开发实例)
  - [第10章 PT模式相关API](#第10章-pt模式相关api)
    - [10.1 PT控制API](#101-pt控制api)
    - [10.2 PT参数配置API](#102-pt参数配置api)
    - [10.3 PT状态监控API](#103-pt状态监控api)
    - [10.4 开发实例](#104-开发实例)
  - [第11章 手轮相关API](#第11章-手轮相关api)
    - [11.1 手轮控制API](#111-手轮控制api)
    - [11.2 手轮参数配置API](#112-手轮参数配置api)
    - [11.3 手轮状态监控API](#113-手轮状态监控api)
    - [11.4 开发实例](#114-开发实例)
  - [第12章 串口/485相关API](#第12章-串口485相关api)
    - [12.1 串口控制API](#121-串口控制api)
    - [12.2 串口参数配置API](#122-串口参数配置api)
    - [12.3 串口通信API](#123-串口通信api)
    - [12.4 开发实例](#124-开发实例)
  - [第13章 坐标系跟随相关API](#第13章-坐标系跟随相关api)
    - [13.1 跟随控制API](#131-跟随控制api)
    - [13.2 跟随参数配置API](#132-跟随参数配置api)
    - [13.3 跟随状态监控API](#133-跟随状态监控api)
    - [13.4 开发实例](#134-开发实例)
  - [第14章 机械臂操作类API](#第14章-机械臂操作类api)
    - [14.1 机械臂控制API](#141-机械臂控制api)
    - [14.2 机械臂参数配置API](#142-机械臂参数配置api)
    - [14.3 机械臂状态监控API](#143-机械臂状态监控api)
    - [14.4 开发实例](#144-开发实例)

---

## 第6章 电子齿轮类API

电子齿轮功能是实现主从轴同步运动的重要功能，广泛应用于传送带同步、印刷机械、包装机械等领域。

### 6.1 电子齿轮控制API

#### 6.1.1 MC_GearEnable - 启用电子齿轮

**函数原型**：
```c
int MC_GearEnable(int slaveAxis, int masterAxis);
```

**功能描述**：启用指定轴的电子齿轮功能，建立从轴与主轴的齿轮关系。

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

**典型应用场景**：
```c
// 启用1轴跟随2轴的电子齿轮
int result = MC_GearEnable(1, 2);
if (result != 0) {
    printf("电子齿轮启用失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴已设置为从轴，跟随2轴运动\n");
}
```

#### 6.1.2 MC_GearDisable - 禁用电子齿轮

**函数原型**：
```c
int MC_GearDisable(int slaveAxis);
```

**功能描述**：禁用指定轴的电子齿轮功能，断开从轴与主轴的齿轮关系。

**参数说明**：
- `slaveAxis`：从轴轴号，范围[1, 1024]

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 从轴参数错误
- 轴未启用电子齿轮

**典型应用场景**：
```c
// 禁用1轴的电子齿轮功能
int result = MC_GearDisable(1);
if (result != 0) {
    printf("电子齿轮禁用失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴电子齿轮已禁用\n");
}
```

#### 6.1.3 MC_GearOn - 启动齿轮运动

**函数原型**：
```c
int MC_GearOn(int slaveAxis);
```

**功能描述**：启动从轴的齿轮运动，从轴开始跟随主轴运动。

**参数说明**：
- `slaveAxis`：从轴轴号，范围[1, 1024]

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 从轴参数错误
- 电子齿轮未启用
- 主轴未运动

**典型应用场景**：
```c
// 启动1轴的齿轮运动
int result = MC_GearOn(1);
if (result != 0) {
    printf("齿轮运动启动失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴开始跟随主轴运动\n");
}
```

#### 6.1.4 MC_GearOff - 停止齿轮运动

**函数原型**：
```c
int MC_GearOff(int slaveAxis);
```

**功能描述**：停止从轴的齿轮运动，从轴停止跟随主轴。

**参数说明**：
- `slaveAxis`：从轴轴号，范围[1, 1024]

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 从轴参数错误
- 从轴未进行齿轮运动

**典型应用场景**：
```c
// 停止1轴的齿轮运动
int result = MC_GearOff(1);
if (result != 0) {
    printf("齿轮运动停止失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴停止跟随主轴运动\n");
}
```

### 6.2 齿轮参数配置API

#### 6.2.1 MC_SetGearPrm - 设置齿轮参数

**函数原型**：
```c
int MC_SetGearPrm(int slaveAxis, int masterAxis, double gearRatio, int direction);
```

**功能描述**：设置电子齿轮的传动比和方向参数。

**参数说明**：
- `slaveAxis`：从轴轴号，范围[1, 1024]
- `masterAxis`：主轴轴号，范围[1, 1024]
- `gearRatio`：传动比，范围[0.001, 1000.0]
- `direction`：方向，0=同向，1=反向

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 轴参数错误
- 传动比超出范围
- 方向参数错误

**典型应用场景**：
```c
// 设置1轴跟随2轴，传动比为2:1，同向运动
int result = MC_SetGearPrm(1, 2, 2.0, 0);
if (result != 0) {
    printf("齿轮参数设置失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("齿轮参数设置成功：传动比2:1，同向\n");
}

// 设置3轴跟随2轴，传动比为1:3，反向运动
result = MC_SetGearPrm(3, 2, 0.333, 1);
if (result != 0) {
    printf("齿轮参数设置失败，错误码：%d\n", result);
} else {
    printf("齿轮参数设置成功：传动比1:3，反向\n");
}
```

#### 6.2.2 MC_SetGearMaster - 设置齿轮主轴

**函数原型**：
```c
int MC_SetGearMaster(int slaveAxis, int masterAxis);
```

**功能描述**：设置或更改电子齿轮的主轴。

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
int result = MC_SetGearMaster(1, 3);
if (result != 0) {
    printf("主轴设置失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴主轴已更改为3轴\n");
}
```

#### 6.2.3 MC_SetGearRatio - 设置齿轮传动比

**函数原型**：
```c
int MC_SetGearRatio(int slaveAxis, double gearRatio);
```

**功能描述**：动态调整电子齿轮的传动比。

**参数说明**：
- `slaveAxis`：从轴轴号，范围[1, 1024]
- `gearRatio`：新的传动比，范围[0.001, 1000.0]

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 从轴参数错误
- 传动比超出范围
- 从轴正在进行齿轮运动

**典型应用场景**：
```c
// 动态调整1轴的传动比为3:1
int result = MC_SetGearRatio(1, 3.0);
if (result != 0) {
    printf("传动比调整失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴传动比已调整为3:1\n");
}
```

### 6.3 齿轮状态监控API

#### 6.3.1 MC_GetGearStatus - 获取齿轮状态

**函数原型**：
```c
int MC_GetGearStatus(int slaveAxis, int* isEnabled, int* isRunning,
                    int* masterAxis, double* gearRatio);
```

**功能描述**：获取指定轴的电子齿轮状态信息。

**参数说明**：
- `slaveAxis`：从轴轴号，范围[1, 1024]
- `isEnabled`：输出参数，是否启用电子齿轮
- `isRunning`：输出参数，是否正在运行
- `masterAxis`：输出参数，主轴轴号
- `gearRatio`：输出参数，当前传动比

**返回值**：
- 成功：返回0
- 失败：返回错误码

**异常处理**：
- 从轴参数错误
- 输出参数为空

**典型应用场景**：
```c
// 获取1轴的齿轮状态
int isEnabled, isRunning, masterAxis;
double gearRatio;

int result = MC_GetGearStatus(1, &isEnabled, &isRunning, &masterAxis, &gearRatio);
if (result != 0) {
    printf("获取齿轮状态失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("1轴齿轮状态：\n");
    printf("  启用状态：%s\n", isEnabled ? "已启用" : "未启用");
    printf("  运行状态：%s\n", isRunning ? "运行中" : "已停止");
    printf("  主轴：%d\n", masterAxis);
    printf("  传动比：%.3f\n", gearRatio);
}
```

#### 6.3.2 MC_GetGearPos - 获取齿轮位置

**函数原型**：
```c
int MC_GetGearPos(int slaveAxis, double* slavePos, double* masterPos);
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

int result = MC_GetGearPos(1, &slavePos, &masterPos);
if (result != 0) {
    printf("获取齿轮位置失败，错误码：%d\n", result);
    // 处理错误
} else {
    printf("齿轮位置信息：\n");
    printf("  从轴位置：%.2f pulse\n", slavePos);
    printf("  主轴位置：%.2f pulse\n", masterPos);
    printf("  实际传动比：%.6f\n", slavePos / masterPos);
}
```

### 6.4 开发实例

#### 6.4.1 多轴齿轮同步系统

```c
/**
 * 多轴齿轮同步系统示例
 * 实现4轴同步运动，适用于传送带、印刷机械等应用
 */

#include <stdio.h>
#include <math.h>
#include "BP_MotionControl.h"

// 定义轴号
#define MASTER_AXIS 2
#define SLAVE_AXIS_1 1
#define SLAVE_AXIS_3 3
#define SLAVE_AXIS_4 4

typedef struct {
    int axis;
    int masterAxis;
    double gearRatio;
    int direction;
    const char* name;
} GearConfig;

// 齿轮配置结构体
GearConfig gearConfigs[] = {
    {SLAVE_AXIS_1, MASTER_AXIS, 1.0, 0, "左传送带"},
    {SLAVE_AXIS_3, MASTER_AXIS, 2.0, 0, "右传送带"},
    {SLAVE_AXIS_4, MASTER_AXIS, 0.5, 1, "旋转机构"}
};

#define GEAR_CONFIG_COUNT (sizeof(gearConfigs) / sizeof(GearConfig))

/**
 * 初始化齿轮系统
 */
int InitializeGearSystem() {
    int result = 0;

    printf("正在初始化多轴齿轮同步系统...\n");

    // 禁用所有轴的齿轮功能
    for (int i = 0; i < GEAR_CONFIG_COUNT; i++) {
        result += MC_GearDisable(gearConfigs[i].axis);
    }

    // 配置各轴齿轮参数
    for (int i = 0; i < GEAR_CONFIG_COUNT; i++) {
        GearConfig* config = &gearConfigs[i];

        printf("配置%s（轴%d）...\n", config->name, config->axis);

        // 设置齿轮参数
        result += MC_SetGearPrm(config->axis, config->masterAxis,
                               config->gearRatio, config->direction);

        if (result != 0) {
            printf("%s齿轮参数配置失败，错误码：%d\n", config->name, result);
            return result;
        }

        // 启用齿轮功能
        result += MC_GearEnable(config->axis, config->masterAxis);
        if (result != 0) {
            printf("%s齿轮启用失败，错误码：%d\n", config->name, result);
            return result;
        }

        printf("  %s配置完成 - 主轴：%d，传动比：%.3f，方向：%s\n",
               config->name, config->masterAxis, config->gearRatio,
               config->direction ? "反向" : "同向");
    }

    printf("齿轮系统初始化完成！\n");
    return 0;
}

/**
 * 启动齿轮同步运动
 */
int StartGearSyncMotion() {
    int result = 0;

    printf("启动齿轮同步运动...\n");

    // 启动所有从轴的齿轮运动
    for (int i = 0; i < GEAR_CONFIG_COUNT; i++) {
        result += MC_GearOn(gearConfigs[i].axis);
        if (result != 0) {
            printf("%s齿轮运动启动失败，错误码：%d\n",
                   gearConfigs[i].name, result);
            return result;
        }
        printf("%s已开始跟随主轴运动\n", gearConfigs[i].name);
    }

    printf("所有从轴已启动同步运动！\n");
    return 0;
}

/**
 * 停止齿轮同步运动
 */
int StopGearSyncMotion() {
    int result = 0;

    printf("停止齿轮同步运动...\n");

    // 停止所有从轴的齿轮运动
    for (int i = 0; i < GEAR_CONFIG_COUNT; i++) {
        result += MC_GearOff(gearConfigs[i].axis);
        printf("%s齿轮运动已停止\n", gearConfigs[i].name);
    }

    printf("所有从轴已停止同步运动！\n");
    return 0;
}

/**
 * 监控齿轮系统状态
 */
void MonitorGearSystem(int duration) {
    printf("开始监控齿轮系统状态（%d秒）...\n", duration);

    int monitorCount = duration * 10; // 每100ms监控一次
    int count = 0;

    while (count < monitorCount) {
        printf("\n=== 第%d次监控 ===\n", count + 1);

        // 监控主轴状态
        int masterRun, masterDone;
        MC_GetPrfRun(2, &masterRun);
        MC_GetPrfDone(2, &masterDone);
        double masterPos = MC_GetAxisPos(2);
        double masterVel = MC_GetAxisVel(2);

        printf("主轴（轴2）：%s，位置：%.2f，速度：%.2f\n",
               masterRun ? "运行中" : "已停止", masterPos, masterVel);

        // 监控各从轴状态
        for (int i = 0; i < GEAR_CONFIG_COUNT; i++) {
            GearConfig* config = &gearConfigs[i];

            int isEnabled, isRunning, masterAxis;
            double gearRatio;
            MC_GetGearStatus(config->axis, &isEnabled, &isRunning,
                           &masterAxis, &gearRatio);

            double slavePos, masterGearPos;
            MC_GetGearPos(config->axis, &slavePos, &masterGearPos);
            double slaveVel = MC_GetAxisVel(config->axis);

            printf("%s（轴%d）：%s，位置：%.2f，速度：%.2f，传动比：%.3f\n",
                   config->name, config->axis,
                   isRunning ? "跟随中" : "已停止",
                   slavePos, slaveVel, gearRatio);

            // 计算实际传动比误差
            if (masterGearPos != 0) {
                double actualRatio = slavePos / masterGearPos;
                double error = fabs(actualRatio - gearRatio) / gearRatio * 100;
                printf("  传动比误差：%.2f%%\n", error);

                if (error > 1.0) {
                    printf("  警告：传动比误差过大！\n");
                }
            }
        }

        count++;
        Sleep(100); // 等待100ms
    }
}

/**
 * 动态调整传动比
 */
int AdjustGearRatio(int slaveAxis, double newRatio) {
    printf("动态调整轴%d的传动比为：%.3f\n", slaveAxis, newRatio);

    int result = MC_SetGearRatio(slaveAxis, newRatio);
    if (result != 0) {
        printf("传动比调整失败，错误码：%d\n", result);
        return result;
    }

    printf("传动比调整成功！\n");

    // 验证调整结果
    int isEnabled, isRunning, masterAxis;
    double currentRatio;
    MC_GetGearStatus(slaveAxis, &isEnabled, &isRunning, &masterAxis, &currentRatio);

    printf("当前传动比：%.6f\n", currentRatio);
    return 0;
}

/**
 * 主函数
 */
int main() {
    printf("=== 多轴齿轮同步系统演示程序 ===\n\n");

    // 初始化系统
    if (InitializeGearSystem() != 0) {
        printf("系统初始化失败！\n");
        return -1;
    }

    printf("\n系统初始化成功，按Enter键启动齿轮同步运动...");
    getchar();

    // 启动同步运动
    if (StartGearSyncMotion() != 0) {
        printf("启动同步运动失败！\n");
        return -1;
    }

    printf("\n齿轮同步运动已启动，按Enter键开始状态监控...");
    getchar();

    // 监控系统状态
    MonitorGearSystem(10);

    printf("\n状态监控完成，按Enter键进行传动比动态调整...");
    getchar();

    // 动态调整传动比
    AdjustGearRatio(SLAVE_AXIS_1, 1.5);
    Sleep(2000);
    AdjustGearRatio(SLAVE_AXIS_3, 2.5);
    Sleep(2000);

    printf("\n动态调整完成，按Enter键停止同步运动...");
    getchar();

    // 停止同步运动
    StopGearSyncMotion();

    printf("\n程序执行完成！\n");
    return 0;
}
```

#### 6.4.2 齿轮系统故障诊断

```c
/**
 * 齿轮系统故障诊断示例
 * 实现齿轮系统的自动故障检测和诊断功能
 */

#include <stdio.h>
#include <math.h>
#include "BP_MotionControl.h"

typedef struct {
    int axis;
    int masterAxis;
    double expectedRatio;
    double tolerance;
    double maxTrackingError;
} GearDiagnostic;

// 齿轮诊断参数
GearDiagnostic gearDiagnostics[] = {
    {1, 2, 1.0, 0.01, 100.0},   // 轴1跟随轴2，传动比1:1，容差1%，最大跟踪误差100
    {3, 2, 2.0, 0.015, 150.0},  // 轴3跟随轴2，传动比2:1，容差1.5%，最大跟踪误差150
    {4, 2, 0.5, 0.02, 80.0}     // 轴4跟随轴2，传动比0.5:1，容差2%，最大跟踪误差80
};

#define DIAG_COUNT (sizeof(gearDiagnostics) / sizeof(GearDiagnostic))

/**
 * 诊断单个齿轮轴
 */
int DiagnoseGearAxis(int axis) {
    int isEnabled, isRunning, masterAxis;
    double gearRatio;

    // 获取齿轮状态
    int result = MC_GetGearStatus(axis, &isEnabled, &isRunning, &masterAxis, &gearRatio);
    if (result != 0) {
        printf("轴%d齿轮状态获取失败，错误码：%d\n", axis, result);
        return -1;
    }

    printf("\n--- 轴%d齿轮诊断 ---\n", axis);
    printf("启用状态：%s\n", isEnabled ? "已启用" : "未启用");
    printf("运行状态：%s\n", isRunning ? "运行中" : "已停止");
    printf("主轴：%d\n", masterAxis);
    printf("传动比：%.6f\n", gearRatio);

    if (!isEnabled) {
        printf("诊断结果：齿轮未启用\n");
        return 1;
    }

    if (!isRunning) {
        printf("诊断结果：齿轮未运行\n");
        return 2;
    }

    // 检查位置同步
    double slavePos, masterPos;
    result = MC_GetGearPos(axis, &slavePos, &masterPos);
    if (result != 0) {
        printf("位置获取失败，错误码：%d\n", result);
        return -2;
    }

    printf("从轴位置：%.2f\n", slavePos);
    printf("主轴位置：%.2f\n", masterPos);

    if (masterPos != 0) {
        double actualRatio = slavePos / masterPos;
        double ratioError = fabs(actualRatio - gearRatio) / gearRatio * 100;
        printf("实际传动比：%.6f\n", actualRatio);
        printf("传动比误差：%.2f%%\n", ratioError);

        // 查找对应的诊断参数
        GearDiagnostic* diag = NULL;
        for (int i = 0; i < DIAG_COUNT; i++) {
            if (gearDiagnostics[i].axis == axis) {
                diag = &gearDiagnostics[i];
                break;
            }
        }

        if (diag) {
            // 检查传动比误差
            if (ratioError > diag->tolerance * 100) {
                printf("警告：传动比误差超过容限！\n");
                printf("  容限：%.1f%%，实际：%.2f%%\n",
                       diag->tolerance * 100, ratioError);
            }

            // 检查跟踪误差
            double expectedPos = masterPos * diag->expectedRatio;
            double trackingError = fabs(slavePos - expectedPos);
            printf("跟踪误差：%.2f\n", trackingError);

            if (trackingError > diag->maxTrackingError) {
                printf("警告：跟踪误差超过容限！\n");
                printf("  最大容限：%.1f，实际：%.2f\n",
                       diag->maxTrackingError, trackingError);
            }

            // 综合诊断结果
            if (ratioError <= diag->tolerance * 100 &&
                trackingError <= diag->maxTrackingError) {
                printf("诊断结果：正常\n");
                return 0;
            } else {
                printf("诊断结果：异常\n");
                return 3;
            }
        }
    }

    printf("诊断结果：无法确定\n");
    return -3;
}

/**
 * 批量诊断所有齿轮轴
 */
void DiagnoseAllGearAxes() {
    printf("=== 齿轮系统批量诊断 ===\n");

    int totalAxes = 0;
    int normalCount = 0;
    int warningCount = 0;
    int errorCount = 0;

    for (int i = 0; i < DIAG_COUNT; i++) {
        int axis = gearDiagnostics[i].axis;
        int result = DiagnoseGearAxis(axis);

        totalAxes++;

        if (result == 0) {
            normalCount++;
        } else if (result > 0 && result < 4) {
            warningCount++;
        } else {
            errorCount++;
        }
    }

    printf("\n=== 诊断汇总 ===\n");
    printf("总轴数：%d\n", totalAxes);
    printf("正常轴数：%d\n", normalCount);
    printf("警告轴数：%d\n", warningCount);
    printf("错误轴数：%d\n", errorCount);

    if (errorCount == 0 && warningCount == 0) {
        printf("系统状态：优秀\n");
    } else if (errorCount == 0) {
        printf("系统状态：良好（有警告）\n");
    } else {
        printf("系统状态：需要维护\n");
    }
}

/**
 * 实时监控齿轮系统
 */
void RealTimeGearMonitor(int duration) {
    printf("开始实时监控齿轮系统（%d秒）...\n", duration);

    int monitorCount = duration * 2; // 每500ms监控一次
    int count = 0;

    while (count < monitorCount) {
        printf("\n=== 实时监控 - 第%d次 ===\n", count + 1);

        for (int i = 0; i < DIAG_COUNT; i++) {
            int axis = gearDiagnostics[i].axis;

            int isEnabled, isRunning, masterAxis;
            double gearRatio;
            MC_GetGearStatus(axis, &isEnabled, &isRunning, &masterAxis, &gearRatio);

            if (isRunning) {
                double slavePos, masterPos;
                MC_GetGearPos(axis, &slavePos, &masterPos);

                if (masterPos != 0) {
                    double actualRatio = slavePos / masterPos;
                    double ratioError = fabs(actualRatio - gearRatio) / gearRatio * 100;

                    double expectedPos = masterPos * gearDiagnostics[i].expectedRatio;
                    double trackingError = fabs(slavePos - expectedPos);

                    printf("轴%d：传动比误差%.2f%%，跟踪误差%.1f\n",
                           axis, ratioError, trackingError);

                    // 异常报警
                    if (ratioError > gearDiagnostics[i].tolerance * 100 ||
                        trackingError > gearDiagnostics[i].maxTrackingError) {
                        printf("  ⚠️ 轴%d异常！\n", axis);
                    }
                }
            }
        }

        count++;
        Sleep(500); // 等待500ms
    }
}

/**
 * 主函数
 */
int main() {
    printf("=== 齿轮系统故障诊断程序 ===\n\n");

    // 批量诊断
    DiagnoseAllGearAxes();

    printf("\n按Enter键开始实时监控...");
    getchar();

    // 实时监控
    RealTimeGearMonitor(30);

    printf("\n程序执行完成！\n");
    return 0;
}
```

**预期输出**：
```
=== 齿轮系统故障诊断程序 ===

=== 齿轮系统批量诊断 ===

--- 轴1齿轮诊断 ---
启用状态：已启用
运行状态：运行中
主轴：2
传动比：1.000000
从轴位置：1000.00
主轴位置：1000.00
实际传动比：1.000000
传动比误差：0.00%
跟踪误差：0.0
诊断结果：正常

--- 轴3齿轮诊断 ---
启用状态：已启用
运行状态：运行中
主轴：2
传动比：2.000000
从轴位置：2000.00
主轴位置：1000.00
实际传动比：2.000000
传动比误差：0.00%
跟踪误差：0.0
诊断结果：正常

--- 轴4齿轮诊断 ---
启用状态：已启用
运行状态：运行中
主轴：2
传动比：0.500000
从轴位置：500.00
主轴位置：1000.00
实际传动比：0.500000
传动比误差：0.00%
跟踪误差：0.0
诊断结果：正常

=== 诊断汇总 ===
总轴数：3
正常轴数：3
警告轴数：0
错误轴数：0
系统状态：优秀
```

---

