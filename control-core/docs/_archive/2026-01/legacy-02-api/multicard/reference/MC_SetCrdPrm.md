# MC_SetCrdPrm 函数教程

## 函数原型

```c
int MC_SetCrdPrm(short nCrdNum, TCrdPrm* pCrdPrm)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，取值范围：1-16 |
| pCrdPrm | TCrdPrm* | 坐标系参数结构体指针 |

### TCrdPrm 结构体主要成员
```c
typedef struct {
    double evenTime;           // 最小匀速时间 (ms)
    short setOriginFlag;        // 原点坐标值标志
    double originPos[8];        // 原点坐标值
    short dimension;           // 坐标系维数
    short profile[8];          // 坐标系规划对应轴通道
    double synAccMax;          // 坐标系最大加速度
    double synVelMax;          // 坐标系最大速度
} TCrdPrm;
```

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本坐标系参数设置
```cpp
// 建立二维坐标系用于插补运动
void CDemoVCDlg::OnBnClickedButtonSetCrdPrm()
{
    int iRes = 0;
    UpdateData(true);

    // 坐标系参数初始化
    TCrdPrm crdPrm;
    memset(&crdPrm, 0, sizeof(crdPrm));

    // 设置坐标系最小匀速时间为0
    crdPrm.evenTime = 0;

    // 设置原点坐标值标志，0:默认当前位置为原点;1:用户指定原点位置
    crdPrm.setOriginFlag = 1;

    // 设置坐标系原点坐标
    crdPrm.originPos[0] = m_lOrginX;  // X轴原点
    crdPrm.originPos[1] = m_lOrginY;  // Y轴原点
    crdPrm.originPos[2] = m_lOrginZ;  // Z轴原点
    crdPrm.originPos[3] = 0;         // 其他轴原点
    crdPrm.originPos[4] = 0;
    crdPrm.originPos[5] = 0;

    // 设置坐标系维数为2（二维坐标系）
    crdPrm.dimension = 2;

    // 设置坐标系规划对应轴通道，通道1对应轴1，通道2对应轴3
    crdPrm.profile[0] = 1;  // X轴映射到轴1
    crdPrm.profile[1] = 2;  // Y轴映射到轴2
    crdPrm.profile[2] = 0;  // 未使用
    crdPrm.profile[3] = 0;  // 未使用
    crdPrm.profile[4] = 0;  // 未使用
    crdPrm.profile[5] = 0;  // 未使用

    // 设置坐标系最大加速度为0.2脉冲/毫秒²
    crdPrm.synAccMax = 0.2;
    // 设置坐标系最大速度为1000脉冲/毫秒
    crdPrm.synVelMax = 1000;

    // 设置坐标系参数，建立坐标系
    iRes = g_MultiCard.MC_SetCrdPrm(1, &crdPrm);

    if(0 == iRes)
    {
        AfxMessageBox("建立坐标系成功！");
    }
    else
    {
        AfxMessageBox("建立坐标系失败！");
        return;
    }
}
```

### 示例2：三维坐标系参数设置
```cpp
// 建立三维坐标系用于3D插补运动
bool Create3DCoordinateSystem(int crdNum)
{
    int iRes = 0;
    TCrdPrm crdPrm;

    // 初始化参数结构
    memset(&crdPrm, 0, sizeof(crdPrm));

    // 基本参数设置
    crdPrm.evenTime = 0;           // 无最小匀速时间
    crdPrm.setOriginFlag = 1;       // 用户指定原点

    // 设置原点坐标
    crdPrm.originPos[0] = 0;        // X原点
    crdPrm.originPos[1] = 0;        // Y原点
    crdPrm.originPos[2] = 0;        // Z原点

    // 设置为三维坐标系
    crdPrm.dimension = 3;

    // 设置轴映射：X->轴1, Y->轴2, Z->轴3
    crdPrm.profile[0] = 1;  // X轴 → 轴1
    crdPrm.profile[1] = 2;  // Y轴 → 轴2
    crdPrm.profile[2] = 3;  // Z轴 → 轴3

    // 运动参数设置
    crdPrm.synAccMax = 1.0;      // 较大加速度
    crdPrm.synVelMax = 500.0;     // 适中的最大速度

    // 创建坐标系
    iRes = g_MultiCard.MC_SetCrdPrm(crdNum, &crdPrm);

    if(iRes == 0)
    {
        TRACE("坐标系%d(3D)创建成功\n", crdNum);
        return true;
    }
    else
    {
        TRACE("坐标系%d创建失败，错误代码: %d\n", crdNum, iRes);
        return false;
    }
}
```

### 示例3：高性能坐标系配置
```cpp
// 为高速运动配置高性能坐标系参数
bool CreateHighPerformanceCrd(int crdNum)
{
    TCrdPrm crdPrm;
    memset(&crdPrm, 0, sizeof(crdPrm));

    // 高性能配置
    crdPrm.evenTime = 0;        // 无匀速时间限制
    crdPrm.setOriginFlag = 1;     // 用户指定原点

    // 设置工作原点
    crdPrm.originPos[0] = 100.0;  // X工作原点偏移
    crdPrm.originPos[1] = 50.0;   // Y工作原点偏移
    crdPrm.originPos[2] = 0.0;    // Z工作原点偏移

    // 二维坐标系配置
    crdPrm.dimension = 2;
    crdPrm.profile[0] = 1;  // 轴1
    crdPrm.profile[1] = 3;  // 轴3（XZ平面）

    // 高性能运动参数
    crdPrm.synAccMax = 5.0;      // 高加速度
    crdPrm.synVelMax = 2000.0;    // 高速度

    int iRes = g_MultiCard.MC_SetCrdPrm(crdNum, &crdPrm);
    return (iRes == 0);
}
```

### 示例4：精密运动坐标系配置
```cpp
// 为精密应用配置高精度坐标系参数
bool CreatePrecisionCrd(int crdNum)
{
    TCrdPrm crdPrm;
    memset(&crdPrm, 0, sizeof(crdPrm));

    // 精密配置
    crdPrm.evenTime = 10;       // 短暂匀速时间，提高精度
    crdPrm.setOriginFlag = 1;     // 用户指定原点

    // 设置精密原点
    crdPrm.originPos[0] = 0.0;    // 精确X原点
    crdPrm.originPos[1] = 0.0;    // 精确Y原点
    crdPrm.originPos[2] = 0.0;    // 精确Z原点

    // 二维坐标系
    crdPrm.dimension = 2;
    crdPrm.profile[0] = 1;  // 轴1
    crdPrm.profile[1] = 2;  // 轴2

    // 精密运动参数
    crdPrm.synAccMax = 0.1;      // 低加速度，减少振动
    crdPrm.synVelMax = 100.0;    // 低速度，保证精度

    int iRes = g_MultiCard.MC_SetCrdPrm(crdNum, &crdPrm);
    return (iRes == 0);
}
```

### 示例5：多坐标系批量配置
```cpp
// 批量配置多个坐标系
bool ConfigureMultipleCoordinateSystems()
{
    struct CrdConfig {
        int crdNum;
        int dimension;
        int axis1, axis2, axis3;
        double maxAcc, maxVel;
        double originX, originY, originZ;
    };

    // 定义多个坐标系配置
    CrdConfig configs[] = {
        {1, 2, 1, 2, 0,  0.5, 500.0, 0.0, 0.0, 0.0},   // XY平面坐标系
        {2, 2, 1, 3, 0,  0.3, 300.0, 100.0, 0.0, 0.0}, // XZ平面坐标系
        {3, 2, 2, 4, 0,  0.8, 800.0, 0.0, 0.0, 0.0}   // YZ平面坐标系
    };

    int configCount = sizeof(configs) / sizeof(configs[0]);

    for(int i = 0; i < configCount; i++)
    {
        TCrdPrm crdPrm;
        memset(&crdPrm, 0, sizeof(crdPrm));

        // 配置基本参数
        crdPrm.evenTime = 0;
        crdPrm.setOriginFlag = 1;
        crdPrm.dimension = configs[i].dimension;

        // 设置原点坐标
        crdPrm.originPos[0] = configs[i].originX;
        crdPrm.originPos[1] = configs[i].originY;
        crdPrm.originPos[2] = configs[i].originZ;

        // 设置轴映射
        crdPrm.profile[0] = configs[i].axis1;
        crdPrm.profile[1] = configs[i].axis2;
        crdPrm.profile[2] = configs[i].axis3;

        // 设置运动参数
        crdPrm.synAccMax = configs[i].maxAcc;
        crdPrm.synVelMax = configs[i].maxVel;

        // 创建坐标系
        int iRes = g_MultiCard.MC_SetCrdPrm(configs[i].crdNum, &crdPrm);
        if(iRes == 0)
        {
            TRACE("坐标系%d配置成功\n", configs[i].crdNum);
        }
        else
        {
            TRACE("坐标系%d配置失败\n", configs[i].crdNum);
            return false;
        }
    }

    return true;
}
```

## 参数映射表

| 应用类型 | dimension | profile | synAccMax | synVelMax | 说明 |
|----------|-----------|---------|------------|-----------|------|
| XY平面 | 2 | 轴1,轴2 | 0.2 | 1000 | 标准XY平面插补 |
| XZ平面 | 2 | 轴1,轴3 | 0.2 | 1000 | XZ平面插补 |
| YZ平面 | 2 | 轴2,轴3 | 0.2 | 1000 | YZ平面插补 |
| 高性能 | 2 | 自定义 | 1.0-5.0 | 1500-2000 | 高速运动 |
| 精密 | 2 | 自定义 | 0.05-0.2 | 50-200 | 高精度运动 |
| 3D空间 | 3 | 轴1,轴2,轴3 | 0.5 | 500 | 三维空间插补 |

## 关键说明

1. **坐标系维度 (dimension)**：
   - **1**: 一维运动（单轴）
   - **2**: 二维平面运动（XY、XZ、YZ平面）
   - **3**: 三维空间运动
   - **4-8**: 高维运动（特殊应用）

2. **轴映射 (profile)**：
   - 定义坐标系各维度对应的物理轴
   - 0表示该维度未使用
   - 1-16对应轴1-16
   - 合理的轴映射对插补精度很重要

3. **原点设置**：
   - **setOriginFlag=0**: 使用当前位置作为原点
   - **setOriginFlag=1**: 使用originPos[]指定的坐标作为原点
   - 原点设置影响后续的绝对定位

4. **运动参数**：
   - **synAccMax**: 协调加速度，影响加减速性能
   - **synVelMax**: 协调速度，影响运动效率
   - 需要根据机械特性合理设置

5. **evenTime参数**：
   - 最小匀速时间，用于路径平滑
   - 0表示无匀速时间限制
   - 适中的值可以改善运动质量

6. **注意事项**：
   - 坐标系创建后需要初始化前瞻缓冲区
   - 轴映射不能冲突，每个轴只能被一个坐标系使用
   - 运动参数需要与机械特性匹配
   - 坐标系数量受系统限制

## 函数区别

- **MC_SetCrdPrm() vs MC_InitLookAhead()**: 坐标系参数 vs 前瞻缓冲区
- **MC_SetCrdPrm() vs MC_CrdStart()**: 参数设置 vs 启动运动
- **2D vs 3D坐标系**: 维度不同，轴映射数量不同

---

**使用建议**: 在创建坐标系前，建议先规划好轴的使用分配，避免轴映射冲突。运动参数应该根据机械系统的实际特性进行优化，过高的参数可能导致振动或失步。对于精密应用，建议使用较小的加速度和速度。