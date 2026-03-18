## 四、API返回值及其意义

| 返回值 | 意义 | 处理方法 |
|---|---|---|
| 0 | 执行成功 | |
| 1 | 执行失败 | 检测命令执行条件是否满足 |
| 2 | 版本不支持该API | 如有需要，联系厂家 |
| 7 | 参数错误 | 检测参数是否合理 |
| -1 | 通讯失败 | 接线是否牢靠，更换板卡 |
| -6 | 打开控制器失败 | 是否输入正确串口名，是否调用2次MC_Open |
| -7 | 运动控制器无响应 | 检测运动控制器是否连接，是否打开。更换板卡 |

# 五、API使用说明

## 5.1、板卡打开关闭API

| API | 说明 |
|---|---|
| MC_SetCardNo | 切换当前运动控制器卡号 |
| MC_GetCardNo | 读取当前运动控制器卡号 |
| MC_Open | 打开板卡 |
| MC_OpenByIP | 打开板卡 |
| MC_SetReset | 复位板卡 |
| MC_Close | 关闭板卡 |

### 参数详细说明：

| 函数声明 | 参数说明 |
|---|---|
| `int MC_SetCardNo(short iCardNum)` | **iCardNum**: 将被设置为当前运动控制器的卡号，取值范围：[1,255] |
| `int MC_GetCardNo(short *pCardNum)` | **pCardNum**: 读取的当前运动控制器的卡号 |
| `int MC_Open(short iType=0, char* cName="COM1")` | **iType**: 打开方式，0网口，1串口<br>**cName**: 当iType=0（网口方式打开）时，该参数代表PC端IP地址<br>当iType=1（串口方式打开）时，该参数代表默认串口号 |
| `int MC_Open(short nCardNum, char* cPCEthernetIP, unsigned short nPCEthernetPort, char* cCardEthernetIP, unsigned short nCardEthernetPort)` | **nCardNum**: 卡号，从1开始，第一个卡填1，第二个卡填2......<br>**cPCEthernetIP**: 电脑IP地址（需要和板卡在同一个网段）<br>**nPCEthernetPort**: 电脑端口号，通常第一个卡填60000，第二个卡填60001......<br>**cCardEthernetIP**: 板卡IP地址（出厂默认192.168.0.1）<br>**nCardEthernetPort**: 板卡端口号（和电脑端口号保持一致即可） |
| `int MC_OpenByIP(char* cPCIP, char* cCardIP, unsigned long ulID, unsigned short nRetryTime)` | **cPCIP**: 电脑IP<br>**cCardIP**: 板卡IP<br>**ulID**: 固定为0<br>**nRetryTime**: 固定为0 |
| `int MC_Reset()` | 无参数 |
| `int MC_Clear()` | 无参数 |

### 示例代码：

```c
int iRes = 0;

iRes += MC_SetCardNo(1); //切换到第1块板卡

iRes += MC_Open(0, "192.168.0.200"); //打开板卡(通过网口，PC端IP地址为192.168.0.200)

iRes += MC_Reset(); //复位板卡

iRes += MC_Close(); //关闭板卡
```

## 5.2、板卡配置类API

| API | 说明 |
|---|---|
| MC_AlarmOn | 设置轴驱动报警信号有效 |
| MC_AlarmOff | 设置轴驱动报警信号无效 |
| MC_AlarmSns | 设置运动控制器轴报警信号电平逻辑 |
| MC_LmtsOn | 设置轴限位信号有效 |
| MC_LmtsOff | 设置轴限位信号无效 |
| MC_LmtSns | 设置运动控制器各轴限位触发电平 |
| MC_EncOn | 设置为"外部编码器"计数方式 |
| MC_EncOff | 设置为"脉冲计数器"计数方式 |
| MC_EncSns | 设置编码器的计数方向 |
| MC_StepSns | 设置脉冲输出通道的方向 |
| MC_HomeSns | 设置运动控制器 HOME 输入的电平逻辑 |

### 参数详细说明：

**`int MC_AlarmOn(short nAxisNum)`**
*   **nAxisNum**: 控制轴号,取值范围: [1,AXIS_MAX_COUNT]

**`int MC_AlarmOff(short nAxisNum)`**
*   **nAxisNum**: 控制轴号,取值范围: [1,AXIS_MAX_COUNT]

**`int MC_AlarmSns(unsigned short nSense)`**
*   **nSense**: 按位表示各数量输入的电平逻辑,从bit0~bit7,分别对应-8轴的电平逻辑

**`int MC_LmtsOn(short nAxisNum,short limitType = -1)`**
*   **nAxisNum**: 控制轴号,取值范围: [1,AXIS_MAX_COUNT]
*   **limitType**: 需要有效的限位类型
    *   0:需要将该轴的正限位有效
    *   1:需要将该轴的负限位有效
    *   -1:需要将该轴的正限位和负限位都有效,默认为该值

**`int MC_LmtsOff(short nAxisNum,short limitType=-1)`**
*   **nAxisNum**: 控制轴号,取值-范围: [1,AXIS_MAX_COUNT]
*   **limitType**: 需要有效的限位类型
    *   0:需要将该轴的正限位无效
    *   1:需要将该轴的负限位无
    *   -1:需要将该轴的正限位和负限位都无效,默认为该值

**`int MC_LmtSns(unsigned short nSense)`**
*   **nSense**:
    1.  此函数用于按位设置轴的限位的触发电平状态。
    2.  运动控制器默认的限位开关是常闭开关,即各轴处于正常工作状态时,其限位信号输入为低电平,当限位信号为高电平时,限位触发。
    3.  如果使用的传感器是常开,则需要调用本函数,将限位的逻辑电平反一下。
    4.  参数nSense一共16位,分别代表8个轴的正限位和负限位。如下表所示

| Bit | Description |
|---|---|
| Bit0 | 轴1正限位逻辑电平(1低电平触发,0高电平触发) |
| Bit1 | 轴1负限位逻辑电平(1低电平触发,0高电平触发) |
| Bit2 | 轴2正限位逻辑电平(1低电平触发,0高电平触发) |
| Bit3 | 轴2负限位逻辑电平(1低电平触发,0高电平触发) |
| Bit4 | 轴3正限位逻辑电平(1低电平触发,0高电平触发) |
| Bit5 | 轴3负限位逻辑电平(1低电平触发,0高电平触发) |
| Bit6 | 轴4正限位逻辑电平(1低电平触发,0高电平触发) |
| Bit7 | 轴4负限位逻辑电平(1低电平触发,0高电平触发) |
| Bit8 | 轴5正限位逻辑电平(1低电平触发,0高电平触发) |
| Bit9 | 轴5负限位逻辑电平(1低电平触发,0高电平触发) |
| Bit10 | 轴6正限位逻辑电平(1低电平触发,0高电平触发) |
| Bit11 | 轴6负限位逻辑电平(1低电平触发,0高电平触发) |
| Bit12 | 轴7正限位逻辑电平(1低电平触发,0高电平触发) |
| Bit13 | 轴7负限位逻辑电平(1低电平触发,0高电平触发) |
| Bit14 | 轴8正限位逻辑电平(1低电平触发,0高电平触发) |
| Bit15 | 轴8负限位逻辑电平(1低电平触发,0高电平触发) |

```c
//例程
//将轴1的正负限位都设置为常开,低电平触发
MC_LmtSns(0X03);
//将轴2的正负限位都设置为常开,低电平触发
MC_LmtSns(0X0C);
//将轴1和2的正负限位都设置为常开,低电平触发
MC_LmtSns(0X0F);
//注意:这个函数是一次性设置8个轴的限位电平
```

**`int MC_EncOn(short nEncoderNum)`**
*   **nEncoderNum**: 编码器通道号

**`int MC_EncOff(short nEncoderNum)`**
*   **nEncoderNum**: 编码器通道号

**`int MC_EncSns(unsigned short nSense)`**
*   **nSense**: 按位标识编码器的计数方向,bit0~bit7依次对应编码器~8,bit8对应辅助编码器:该编码器计数方向不取反

**`int MC_StepSns(unsigned short sense)`**
*   **sense**: bit0对应脉冲输出通道1,bit1对应脉冲输出通道2,以此类推对应位为0表示不反向,对应位为1表示反向

**`int MC_HomeSns(unsigned short sense)`**
*   **sense**: 按位表示各数量输入的电平逻辑,从bit0~bit7,分别对应轴1-8
    *   0:HOME电平不取反
    *   1:HOME电平取反

### 示例代码：
```c
int iRes = 0;

iRes += MC_SetCardNo(1); //切换到第1块板卡

iRes += MC_Open(0, "192.168.0.200"); //打开板卡（通过网口，PC端IP地址为192.168.0.200）

iRes += MC_Set(); //复位板卡

iRes += MC_AlarmOn(1); //设置轴1驱动报警信号有效

iRes += MC_AlarmOff(1); //设置轴1驱动报警信号无效

iRes += MC_Close();//关闭板卡
```

## 5.3、IO/模拟量/PWM 常规操作 API

| API | 说明 |
|---|---|
| MC_GetDiRaw | 获取IO输入(包含主卡IO、限位、零位) |
| MC_GetDiReverseCount | 读取数字量输入信号的变化次数 |
| MC_SetDiReverseCount | 设置数字量输入信号的变化次数的初值 |
| MC_SetExtDoValue | 设置IO输出(包含主模块和扩展模块) |
| MC_GetExtDiValue | 获取IO输入(包含主模块和扩展模块) |
| MC_GetExtDoValue | 获取IO输出(包含主模块和扩展模块) |
| MC_SetExtDoBit | 设置指定IO模块的指定位输出(包含主模块和扩展模块) |
| MC_GetExtDiBit | 获取指定IO模块的指定位输入(包含主模块和扩展模块) |
| MC_GetExtDoBit | 获取指定IO模块的指定位输出(包含主模块和扩展模块) |
| MC_SetDac(仅限模拟量版本) | 设置DAC输出电压(仅主卡) |
| MC_GetAdc(仅限模拟量版本) | 读取ADC输入电压(仅主卡) |
| MC_SetExDac(仅限模拟量版本) | 设置DAC输出电压(包含主卡和扩展卡) |
| MC_GetExAdc(仅限模拟量版本) | 读取ADC输入电压(包含主卡和扩展卡) |
| MC_SetAdcFilter(仅限模拟量版本) | 设置模拟量输入滤波时间 |
| MC_SetAdcBias(仅限模拟量版本) | 设置模拟量输入通道的零漂电压补偿值 |
| MC_GetAdcBias(仅限模拟量版本) | 读取模拟量输入通道的零漂电压补偿值 |
| MC_SetPwm(仅限模拟量版本) | 设置PWM输出频率以及占空比 |
| MC_SetDoBitReverse | 设置数字IO输出指定时间的单个脉冲 |
| MC_GpioCmpBufData | 向GPIO比较缓冲区发送比较数据(筛选机专用) |
| MC_GpioCmpBufStop | 停止GPIO比较器,并清空缓冲区数据(筛选机专用) |

### 参数详细说明：

**`int MC_GetDiRaw(short nDiType,long *pValue)`**
*   **diType**: 指定数字IO类型
    *   MC_LIMIT_POSITIVE(0): 正限位
    *   MC_LIMIT_NEGATIVE(1): 负限位
    *   MC_ALARM(2): 驱动报警
    *   MC_HOME(3): 原点开关
    *   MC_GPI(4): 通用输入
    *   MC_MPG(7): 手轮IO输入
*   **pValue**: IO输入值存放指针

**`int MC_GetDiReverseCount(short nDiType,short diIndex,unsigned long*pReserveCount,short nCount=1)`**
*   **nDiType**: 指定数字IO类型
    *   MC_LIMIT_POSITIVE(0): 正限位
    *   MC_LIMIT_NEGATIVE(1): 负限位
    *   MC_ALARM(2): 驱动报警
    *   MC_HOME(3): 原点开关
    *   MC_GPI(4): 通用输入
    *   MC_ARIVE(5): 电机到位信号
*   **diIndex**: 数字量输入的索引,取值范围:
    *   nDiType=MC_LIMIT_POSITIVE时: [0,7]
    *   nDiType=MC_LIMIT_NEGATIVE时: [0,7]
    *   nDiType=MC_ALARM时: [0,0]
    *   nDiType=MC_HOME时: [0,7]
    *   nDiType=MC_GPI时: [0,15]
    *   nDiType=MC_ARIVE时: [0,7]
*   **pReserveCount**: 读取的数字量输入的变化次数
*   **nCount**: 读取变化次数的数字量输入的个数，默认为1

**`int MC_SetDiReverseCount(short nDiType,short diIndex,unsigned long ReserveCount,short nCount)`**
*   **nDiType**: 指定数字IO类型
    *   MC_LIMIT_POSITIVE(0): 正限位
    *   MC_LIMIT_NEGATIVE(1): 负限位
    *   MC_ALARM(2): 驱动报警
    *   MC_HOME(3): 原点开关
    *   MC_GPI(4): 通用输入
    *   MC_ARIVE(5): 电机到位信号
*   **diIndex**: 数字量输入的索引,取值范围:
    *   nDiType=MC_LIMIT_POSITIVE时: [0,7]
    *   nDiType=MC_LIMIT_NEGATIVE时: [0,7]
    *   nDiType=MC_ALARM时: [0,7]
    *   nDiType=MC_HOME时: [0,7]
    *   nDiType=MC_GPI时: [0,15]
    *   nDiType=MC_ARIVE时: [0,7]
*   **ReserveCount**: 设置的数字量输入的变化次数
*   **nCount**: 设置变化次数的数字量输入的个数，默认为1

**`int MC_SetExtDoValue(short nCardIndex,unsigned long *value,short nCount=1)`**
*   **nCardIndex**: 起始板卡索引(0~63),0是主模块,扩展模块从1开始
*   **value**: IO输出值存放指针
*   **nCount**: 本次设置的模块数量(1~64)

**`int MC_GetExtDiValue(short nCardIndex,unsigned long *pValue,short nCount=1)`**
*   **nCardIndex**: 起始板卡索引(0~63),0是主模块,扩展模块从1开始
*   **pValue**: IO输入值存放指针
*   **nCount**: 本次获取的模块数量(1~64)

**`int MC_GetExtDoValue(short nCardIndex,unsigned long *pValue,short nCount=1)`**
*   **nCardIndex**: 起始板卡索引(0~63),0是主模块,扩展模块从1开始
*   **pValue**: IO输出值存放指针
*   **nCount**: 本次获取的模块数量(1~64)

**`int MC_SetExtDoBit(short nCardIndex,short nBitIndex,unsigned short nValue)`**
*   **nCardIndex**: 起始板卡索引。0代表主卡,1代表扩展卡1......
*   **nBitIndex**: IO位索引号.0~31
*   **nValue**: IO输出值.1代表打开输出口,0代表关闭输出口

**`int MC_GetExtDiBit(short nCardIndex,short nBitIndex,unsigned short *pValue)`**
*   **nCardIndex**: 起始板卡索引。0代表主卡,1代表扩展卡1......
*   **nBitIndex**: IO位索引号.0~31
*   **pValue**: IO输入值存放指针

**`int MC_GetExtDoBit(short nCardIndex,short nBitIndex,unsigned short *pValue)`**
*   **nCardIndex**: 起始板卡索引。0代表主卡,1代表扩展卡1......
*   **nBitIndex**: IO位索引号.0~31
*   **pValue**: IO输出值存放指针

**`int MC_SetDac(short nDacNum,double dValue)`**
*   **nDacNum**: DAC通道号,取值范围: [1,4]
*   **dValue**: DAC输出电压值,取值范围: [-10,10]

**`int MC_GetAdc(short nAdcNum,double *dValue)`**
*   **nAdcNum**: ADC通道号,取值范围: [1,16]
*   **dValue**: ADC输入电压值存放指针

**`int MC_SetExDac(short nCardIndex,short nDacNum,double dValue)`**
*   **nCardIndex**: 起始板卡索引。0代表主卡,1代表扩展卡1......
*   **nDacNum**: DAC通道号,取值范围: [1,4]
*   **dValue**: DAC输出电压值,取值范围: [-10,10]

**`int MC_GetExAdc(short nCardIndex,short nAdcNum,double *dValue)`**
*   **nCardIndex**: 起始板卡索引。0代表主卡,1代表扩展卡1......
*   **nAdcNum**: ADC通道号,取值范围: [1,16]
*   **dValue**: ADC输入电压值存放指针

**`int MC_SetAdcFilter(short nAdcNum,short nFilterTime)`**
*   **nAdcNum**: ADC通道号,取值范围: [1,16]
*   **nFilterTime**: 滤波时间,单位ms,取值范围: [0,1000]

**`int MC_SetAdcBias(short nAdcNum,double dBias)`**
*   **nAdcNum**: ADC通道号,取值范围: [1,16]
*   **dBias**: 零漂电压补偿值

**`int MC_GetAdcBias(short nAdcNum,double *dBias)`**
*   **nAdcNum**: ADC通道号,取值范围: [1,16]
*   **dBias**: 零漂电压补偿值存放指针

**`int MC_SetPwm(short nPwmNum,double dFreq,double dDuty)`**
*   **nPwmNum**: PWM通道号,取值范围: [1,4]
*   **dFreq**: PWM输出频率,取值范围: [1,100000]
*   **dDuty**: PWM输出占空比,取值范围: [0,1]

**`int MC_SetDoBitReverse(short nCardNum,short nDoIndex,short nValue,short nReverseTime)`**
*   **nCardNum**: 卡号,主卡为0,扩展IO卡依次为1、2、3、4......
*   **nDoIndex**: IO索引,取值范围: [1,16]
*   **nValue**: 设置数字IO输出电平。1表示高电平，0表示低电平
*   **nReverseTime**: 维持value所设置电平的时间，取值范围: [0,32767]，单位:1ms

**`int MC_GpioCmpBufData(short nCmpGpoIndex,short nCmpSource,short nPluseType,short nStartLevel,short nTime,short nTimerFlag,short nAbsPosFlag,short nBufLen,long *pBuf)`**
*   **nCmpGpoIndex**: IO索引，取值范围0-15
*   **nCmpSource**: 比较数据来源
    *   1~16指定轴编码器小于等于指定值时，触发IO
    *   101~116指定轴编码器大于等于指定值时，触发IO
*   **nPUseType**: 固定为2
*   **nPStartLevel**: 固定为0
*   **nPTime**: 脉冲时间，单位ms
*   **nPTimerFlag**: 固定为1
*   **nPAbsPosFlag**: 0相对，1绝对
*   **nPBufLen**: 数据长度，通常为1
*   **pBuf**: 数据存放指针

**`int MC_GpioCmpBufStop(long *pGpioMask,short nCount)`**
*   **pGpioMask**: 掩码数据存放指针，指定位为1时，停止指定IO的比较
*   **nCount**: 卡数量

### 示例代码:

```c
int iRes = 0;

iRes += MC_SetCardNo(1); //切换到第1块板卡

iRes += MC_Open(0, "192.168.0.200"); //打开板卡（通过网口，PC端IP地址为192.168.0.200）

iRes += MC_Set(); //复位板卡

iRes += MC_SetExtDoBit(0, 4, 1); //设置板卡1端口3IO输出

iRes += MC_SetExtDoBit(0, 4, 0); //关闭板卡1端口3IO输出

//Y3 发出一个 100ms 时间宽度的负脉冲。

iRes += MC_SetDoBitReverse(12, 4, 0, 100);

iRes += MC_Close(); //关闭板卡
```

## 5.4、点位运动API

| API | 说明 |
|---|---|
| MC_PrfTrap | 设置指定轴为点位模式 |
| MC_SetTrapPrm | 设置点位模式运动参数 |
| MC_SetTrapPrmSingle | 设置点位模式运动参数（可替代MC_SetTrapPrm） |
| MC_GetTrapPrm | 读取点位模式运动参数 |
| MC_GetTrapPrmSingle | 读取点位模式运动参数（可替代MC_GetTrapPrm） |
| MC_SetPos | 设置目标位置 |
| MC_SetVel | 设置目标速度 |
| MC_Update | 启动点位运动 |
| MC_SetTrapPosAndUpdate | 设置指定轴进行点位运动，可代替以上所有函数，效率高 |

### 参数详细说明：

**`int MC_PrfTrap(short nAxisNum)`**
*   **iAxis**: 规划轴号

**`int MC_SetTrapPrm(short nAxisNum,TTrapPrm *pPrm)`**
*   **iAxis**: 规划轴号
*   **pPrm**: 设置点位模式运动参数
    ```c
    //点位模式参数结构体
    typedef struct TrapPrm
    {
        double acc; //加速度
        double dec; //减速度
        double velStart; //预留,固定为0
        short smoothTime; //预留,固定为0
    }TTrapPrm;
    ```
    Labview下可用MC_SetTrapPrmSingle函数替代本函数

**`int MC_SetTrapPrmSingle(short nAxisNum,double dAcc,double dDec,double dVelStart,short dSmoothTime)`**
*   **nAxisNum**: 规划轴号
*   **dAcc**: 加速度
*   **dDec**: 减速度
*   **dVelStart**: 预留,固定为0
*   **dSmoothTime**: 预留,固定为0

**`int MC_GetTrapPrm(short nAxisNum,TTrapPrm *pPrm)`**
*   **iAxis**: 规划轴号
*   **pPrm**: 读取点位模式运动参数
    ```c
    //点位模式参数结构体
    typedef struct TrapPrm
    {
        double acc; //加速度
        double dec; //减速度
        double velStart; //预留, 固定为0
        shortSmoothTime; //预留, 固定为0
    }TTrapPrm;
    ```
    Labview下可用MC_GetTrapPrmSingle函数替代本函数

**`int MC_GetTrapPrmSingle(short nAxisNum, double* dAcc, double* dDec, double* dVelStart, short* dSmoothTime)`**
*   **nAxisNum**: 规划轴号
*   **dAcc**: 加速度存放指针
*   **dDec**: 减速度存放指针
*   **dVelStart**: 数据存放指针, 无意义
*   **dSmoothTime**: 数据存放指针, 无意义

**`int MC_SetPos(short nAxisNum, long pos)`**
*   **iAxis**: 规划轴号
*   **pos**: 设置目标位置, 单位是脉冲

**`int MC_SetVel(short nAxisNum, double vel)`**
*   **iAxis**: 规划轴号
*   **vel**: 设置目标速度, 单位是“脉冲/毫秒”

**`int MC_Update(long mask)`**
*   **mask**: 按位指示需要启动点位运动的轴号
    *   bit0表示轴, bit1表示2轴, ......
    *   举例说明:
        *   MC_Update(1)//启动轴1 (Bit0为1)
        *   MC_Update(2)//启动轴2 (Bit1为1)
        *   MC_Update(7)//启动轴1、2、3 (Bit0、1、2为1)
        *   MC_Update(8)//启动轴4 (Bit3为1)

**`int MC_SetTrapPosAndUpdate(short nAxisNum, long long llPos, double dVel, double dAcc, double dDec, double dVelStart, short nSmoothTime, short nBlock)`**
*   **nAxisNum**: 轴号, 从1开始
*   **llPos**: 目标位置, 单位脉冲
*   **dVel**: 速度, 单位脉冲/毫米, 可以为小数
*   **dAcc**: 加速度, 单位脉冲/毫秒/毫秒, 可以为小数
*   **dDec**: 减速度, 单位脉冲/毫秒/毫秒, 可以为小数
*   **dVelStart**: 固定为0
*   **nSmoothTime**: 固定为0
*   **nBlock**: 固定为0

### 示例代码:

```c
int iRes = 0;

iRes += MC_SetCardNo(1); //切换到第1块板卡

iRes += MC_Open(0, "192.168.0.200"); //打开板卡（通过网口，PC端IP地址为192.168.0.200）

iRes += MC_Set(); //复位板卡

iRes += MC_AxisOn(1); //设置轴1使能

iRes += MC_PrfTrap(1); //设置板卡轴1为点位模式

TTrapPrm TrapPrm;

TrapPrm.acc = 0.5; //设置点位运动加速度为0.5脉冲/毫秒^2

TrapPrm.dec = 0.5; //设置点位运动减速度为0.5脉冲/毫秒^2

TrapPrm. velStart = 0; //固定为 0

TrapPrm.smoothTime = 0; //固定为0

iRes += MC_SetTrapPrm(m_iAxisNum, &TrapPrm);

iRes += MC_SetVel(1, 10); //设置轴1点位运动速度为10脉冲/毫秒

iRes += MC_SetPos(1, 10000); //设置轴1目标位置为10000

iRes += MC_Update(0XFF); //启动点位运动
```

## 5.5、JOG运动API

| API | 说明 |
|---|---|
| MC_PrfJog | 设置指定轴为 JOG 模式(速度模式) |
| MC_SetJogPrm | 设置 JOG 模式运动参数 |
| MC_SetJogPrmSingle | 设置 JOG 模式运动参数（可替代 MC_SetJogPrm） |
| MC_GetJogPrm | 读取 JOG 模式运动参数 |
| MC_GetJogPrmSingle | 读取 JOG 模式运动参数（可替代 MC_GetJogPrm） |
| MC_SetVel | 设置目标速度 |
| MC_Update | 启动 JOG 运动 |

### 参数详细说明：

**`int MC_PrfJog(short nAxisNum)`**
*   **iAxis**: 轴号，从1开始

**`int MC_SetJogPrm(short nAxisNum,TJogPrm *pPrm)`**
*   **iAxis**: 轴号，从1开始
*   **pPrm**: 设置 Jog 模式运动参数
    ```c
    //JOG 模式参数结构体
    typedef struct JogPrm
    {
        double dAcc; //加速度
        double dDec; //减速度
        double dSmooth; //预留，无意义
    }TJogPrm;
    ```
    Labview 下可用 MC_SetJogPrmSingle 函数替代本函数

**`int MC_SetJogPrmSingle(short nAxisNum,double dAcc,double dDec,double dSmooth)`**
*   **nAxisNum**: 规划轴号
*   **dAcc**: 加速度
*   **dDec**: 减速度
*   **dSmooth**: 平滑时间

**`int MC_GetJogPrm(short nAxisNum,TJogPrm *pPrm)`**
*   **iAxis**: 轴号,从1开始
*   **pPrm**: 获取 Jog 模式运动参数
    ```c
    //JOG 模式参数结构体
    typedef struct JogPrm
    {
        double dAcc; //加速度
        double dDec; //减速度
        double dSmooth; //预留,无意义
    }TJogPrm;
    ```
    Labview下可用MC_GetJogPrmSingle函数替代本函数

**`int MC_GetJogPrmSingle(short nAxisNum,double* dAcc,double* dDec,double* dSmooth)`**
*   **nAxisNum**: 规划轴号
*   **dAcc**: 加速度存放指针
*   **dDec**: 减速度存放指针
*   **dSmooth**: 数据指针,无意义

**`int MC_SetVel(short nAxisNum,double vel)`**
*   **iAxis**: 轴号,从1开始
*   **vel**: 设置目标速度,单位是“脉冲/毫秒”

**`int MC_Update(long mask)`**
*   **mask**: 按位指示需要启动JOG运动的轴号
    *   bit0表示轴,bit1表示2轴,...

### 示例代码:

```c
int iRes = 0;

iRes += MC_SetCardNo(1); //切换到第1块板卡

iRes += MC_Open(0, "192.168.0.200"); //打开板卡（通过网口，PC端IP地址为192.168.0.200）

iRes += MC_Set(); //复位板卡

iRes += MC_PrfJog(1); //设置轴1为Jog模式

iRes += MC_AxisOn(1); //设置轴1使能

TJogPrm JogPrm;

JogPrm.dSmooth = 0;

JogPrm.dAcc = 0.5; //设置 JOG 运动加速度为 0.5 脉冲/毫秒^2

JogPrm.dDec = 0.5; //设置 JOG 运动减速度为 0.5 脉冲/毫秒^2

//注意：如果轴当前模式不是 Jog 模式，设置 JOG 运动参数会失败，会返回 1

iRes = MC_SetJogPrm(1, &JogPrm); //设置 JOG 运动参数

iRes = MC_SetVel(1, -1*fabs(50)); //设置JOG运动速度为50脉冲/毫秒

iRes += MC_Update(0XFF); // 启动 JOG 运动
```

## 5.6、运动状态检测类 API

| API | 说明 |
|---|---|
| MC_AxisOn | 打开驱动器使能 |
| MC_AxisOff | 关闭驱动器使能 |
| MC_Stop | 停止一个或多个轴的规划运动,停止坐标系运动 |
| MC_StopEx | 停止一个或多个轴的规划运动,停止坐标系运动 |
| MC_GetSts | 读取轴状态 |
| MC_ClrSts | 清除驱动器报警标志、跟随误差越限标志、限位触发标志 |
| MC_GetPrfPos | 读取规划位置 |
| MC_GetPrfVel | 读取规划速度 |
| MC_GetAxisEncPos | 读取编码器位值 |
| MC_GetAllSysStatus | 获取所有板卡相关状态 |

### 参数详细说明：

**`int MC_AxisOn(short nAxisNum)`**
*   **nAxisNum**: 轴编号,取值范围: [1,AXIS_MAX_COUNT]

**`int MC_AxisOff(short nAxisNum)`**
*   **nAxisNum**: 轴编号,取值范围: [1,AXIS_MAX_COUNT]

**`int MC_Stop(long lMask, long lOption)`**
*   **lMask**: 按位指示需要停止运动的轴号或者坐标系号
    *   bit0表示轴1, bit1表示轴2, , , bit7表示轴8
    *   bit8表示坐标系1, bit9表示坐标系2
    *   当bit位为1时表示停止对应的轴或者坐标系
*   **lOption**: 按位指示停止方式
    *   bit0表示轴1, bit1表示轴2, , , bit7表示轴8
    *   bit8表示坐标系1, bit9表示坐标系2
    *   当bit位为0时表示平滑停止对应的轴或坐标系
    *   当bit位为1时表示紧急停止对应的轴或坐标系

**`int MC_StopEx(int lCrdMask, int lCrdOpion, int lAxisMask0, int lAxisOption0)`**
*   **lCrdMask**: 按位指示需要停止运动的坐标系号
    *   bit0表示坐标系1, bit1表示坐标系2
    *   当bit位为1时表示停止对应的坐标系
*   **lCrdOpion**: 按位指示停止方式
    *   bit0表示坐标系1, bit1表示坐标系2
    *   当bit位为0时表示平滑停止对应的坐标系
    *   当bit位为1时表示急停对应的坐标系
*   **lAxisMask0**: 按位指示需要停止运动的轴号或者坐标系号
    *   bit0表示1轴, bit1表示2轴, , , bit7表示8轴
    *   当bit位为1时表示停止对应的轴
*   **lAxisOption0**: 按位指示停止方式
    *   bit0表示1轴, bit1表示2轴, , , bit7表示8轴
    *   当bit位为0时表示平滑停止对应的轴或坐标系
    *   当bit位为1时表示急停对应的轴

**`int MC_GetSts(short nAxisNum, long *pSts, short nCount=1, unsigned long *pClock=NULL)`**
*   **nAxisNum**: 起始轴号
*   **pSts**: 32位轴状态字,详细定义参见光盘头文件GAS_N.h的轴状态位定义部分
    ```c
    //轴状态位定义
    #define AXIS_STATUS_ESTOP (0x00000001) //急停
    #define AXIS_STATUS_SV_ALARM (0x00000002) //驱动器报警标志
    #define AXIS_STATUS_POSsoft_LIMIT (0x00000004) //正软限位触发标志
    #define AXIS_STATUS_NEGsoft_LIMIT (0x00000008) //负软位触发标志
    #define AXIS_STATUS_FOLLOWERSERR (0x00000010) //规划位置和实际位置的误差过大时置1
    #define AXIS_STATUS_POSHard_LIMIT (0x00000020) //正硬限位触发标志
    #define AXIS_STATUS_NEGHard_LIMIT (0x00000040) //负硬限位触发标志
    #define AXIS_STATUS_IOSMS_STOP (0x00000080) //保留
    #define AXIS_STATUS_IO EMG_STOP (0x00000100) //保留
    #define AXIS_STATUS_ENABLE (0x00000200) //电机使能标志
    #define AXIS_STATUS_RUNNING (0x00000400) //规划运动标志,规划器运动时置1
    #define AXIS_STATUS_ARIVE (0x00000800) //电机到位
    #define AXIS_STATUS_HOME_RUNNING (0x00001000) //回零运动标志
    #define AXIS_STATUS_HOME_FINISH (0x00002000) //回零完成标志
    #define AXIS_STATUS_HOME_OK (0x00004000) //回零成功标志
    #define AXIS_STATUS_HOME_FAIL (0x00008000) //回零失败标志
    #define AXIS_STATUS_PROFILE_RUNNING (0x00010000) //Profile运动标志
    #define AXIS_STATUS_GEAR_RUNNING (0x00020000) //Gear运动标志
    #define AXIS_STATUS_CAM_RUNNING (0x00040000) //Cam运动标志
    #define AXIS_STATUS_PT_RUNNING (0x00080000) //PT运动标志
    #define AXIS_STATUS_INTERPOLATION_RUNNING (0x00100000) //插补运动标志
    ```
*   **nCount**: 读取的轴数,默认为1,1次最多可以读取8个轴
*   **pClock**: 读取控制器时钟

**`int MC_ClrSts(short nAxisNum, short nCount=1)`**
*   **nAxisNum**: 起始轴号
*   **nCount**: 清除状态的轴数,默认为1

**`int MC_GetPrfPos(short nAxisNum, double *dValue, short nCount=1, unsigned long *pClock=NULL)`**
*   **nAxisNum**: 起始轴号
*   **dValue**: 规划位置存放指针
*   **nCount**: 读取的轴数,默认为1,1次最多可以读取8个轴
*   **pClock**: 读取控制器时钟

**`int MC_GetPrfVel(short nAxisNum, double *dValue, short nCount=1, unsigned long *pClock=NULL)`**
*   **nAxisNum**: 起始轴号
*   **dValue**: 规划速度存放指针
*   **nCount**: 读取的轴数,默认为1,1次最多可以读取8个轴
*   **pClock**: 读取控制器时钟

**`int MC_GetAxisEncPos(short nAxisNum, double *dValue, short nCount=1, unsigned long *pClock=NULL)`**
*   **nAxisNum**: 起始轴号
*   **dValue**: 编码器位置存放指针
*   **nCount**: 读取的轴数,默认为1,1次最多可以读取8个轴
*   **pClock**: 读取控制器时钟

**`int MC_GetAllSysStatus(short nCardNum, unsigned long *pStatus, unsigned long *pDi, unsigned long *pDo, double *pAdc, double *pDac, double *pPwm, double *pPrfPos, double *pEncPos, double *pPrfVel, unsigned long *pClock)`**
*   **nCardNum**: 卡号
*   **pStatus**: 轴状态字存放指针
*   **pDi**: IO输入值存放指针
*   **pDo**: IO输出值存放指针
*   **pAdc**: ADC输入电压值存放指针
*   **pDac**: DAC输出电压值存放指针
*   **pPwm**: PWM输出频率及占空比存放指针
*   **pPrfPos**: 规划位置存放指针
*   **pEncPos**: 编码器位置存放指针
*   **pPrfVel**: 规划速度存放指针
*   **pClock**: 读取控制器时钟

### 示例代码:

```c
int iRes = 0;
long lSts = 0;
double dPrfPos = 0;
double dEncPos = 0;

iRes += MC_SetCardNo(1); //切换到第1块板卡
iRes += MC_Open(0, "192.168.0.200"); //打开板卡（通过网口，PC端IP地址为192.168.0.200）
iRes += MC_Set(); //复位板卡

iRes += MC_AxisOn(1); //设置轴1使能
iRes += MC_AxisOff(1); //设置轴1断开使能

iRes += MC_Stop((0X0001 << (5-1)), 0); //设置轴5停止运动
iRes += MC_GetSts(1, &lSts); //获取轴1状态

iRes += MC_ClrSts(1); //清除轴1运动状态

iRes += MC_GetPrfPos(1, &dPrfPos, 1, NULL);

iRes += MC_GetAxisEncPos(1, &dEncPos, 1, NULL);
```

## 5.7、安全机制API

**重点说明：** 默认各轴软限位不使能。如果要启用硬限位，还要调用MC_LmtsOn函数。

| API | 说明 |
|---|---|
| MC_SetSoftLimit | 设置软限位 |
| MC_GetSoftLimit | 获取软限位 |
| MC_LmtsOn | 设置轴限位信号有效 |
| MC_LmtsOff | 设置轴限位信号无效 |
| MC_LmtSns | 设置运动控制器各轴限位触发电平 |
| MC_SetLmtSnsSingle | 设置运动控制器指定轴限位触发电平 |
| MC_EStopConfig | 配置急停触发时,模拟量值,IO状态等 |
| MC_EStopSetIO | 设置系统紧急停止IO |
| MC_EStopOnOff | 开启/关闭紧急停止功能 |
| MC_EStopGetSts | 获取紧急停止触发状态 |
| MC_EStopCrlSts | 清除紧急停止触发状态 |
| MC_SetHardLimP(10轴以上用) | 设置正硬限位映射IO |
| MC_SetHardLimN(10轴以上用) | 设置负硬限位映射IO |
| MC_CrdHlimEnable | 坐标系硬限位使能开启/关闭 |

### 参数详细说明：

**`int MC_SetSoftLimit(short nAxisNum, long lPositive, long lNegative)`**
*   **axis**: 轴编号
*   **positive**: 正限位，单位脉冲
*   **negative**: 负限位，单位脉冲

**`int MC_GetSoftLimit(short nAxisNum, long *pPositive, long *pNegative)`**
*   **axis**: 轴编号
*   **pPositive**: 正限位存放指针，单位脉冲
*   **pNegative**: 负限位存放指针，单位脉冲

**`int MC_LmtsOn(short nAxisNum, short limitType = -1)`**
*   **nAxisNum**: 控制轴号，取值范围：[1,AXIS_MAX_COUNT]
*   **limitType**: 需要有效的限位类型
    *   MC_LIMIT_POSITIVE(0):需要将该轴的正限位有效
    *   MC_LIMIT_NEGATIVE(1):需要将该轴的负限位有效
    *   -1:需要将该轴的正限位和负限位都有效，默认为该值

**`int MC_LmtsOff(short nAxisNum, short limitType=-1)`**
*   **nAxisNum**: 控制轴号，取值范围：[1,AXIS_MAX_COUNT]
*   **limitType**: 需要有效的限位类型
    *   MC_LIMIT_POSITIVE(0):需要将该轴的正限位无效
    *   MC_LIMIT_NEGATIVE(1):需要将该轴的负限位无效
    *   -1:需要将该轴的正限位和负限位都无效，默认为该值

**`int MC_LmtSns(unsigned short nSense)`**
*   **nSense**:
    1.  此函数用于按位设置轴的限位的触发电平状态。
    2.  运动控制器默认的限位开关是常闭开关，即各轴处于正常工作状态时，其限位信号输入为低电平，当限位信号为高电平时，限位触发。
    3.  如果使用的传感器是常开，则需要调用本函数，将限位的逻辑电平反一下。
    4.  参数 nSense 一共 16 位，分别代表 8 个轴的正限位和负限位。

    如下表所示

| Bit | Description |
|---|---|
| Bit0 | 轴 1 正限位逻辑电平 (1 低电平触发, 0 高电平触发) |
| Bit1 | 轴 1 负限位逻辑电平 (1 低电平触发, 0 高电平触发) |
| Bit2 | 轴 2 正限位逻辑电平 (1 低电平触发, 0 高电平触发) |
| Bit3 | 轴 2 负限位逻辑电平 (1 低电平触发, 0 高电平触发) |
| Bit4 | 轴 3 正限位逻辑电平 (1 低电平触发, 0 高电平触发) |
| Bit5 | 轴 3 负限位逻辑电平 (1 低电平触发, 0 高电平触发) |
| Bit6 | 轴 4 正限位逻辑电平 (1 低电平触发, 0 高电平触发) |
| Bit7 | 轴 4 负限位逻辑电平 (1 低电平触发, 0 高电平触发) |
| Bit8 | 轴 5 正限位逻辑电平 (1 低电平触发, 0 高电平触发) |
| Bit9 | 轴 5 负限位逻辑电平 (1 低电平触发, 0 高电平触发) |
| Bit10 | 轴 6 正限位逻辑电平 (1 低电平触发, 0 高电平触发) |
| Bit11 | 轴 6 负限位逻辑电平 (1 低电平触发, 0 高电平触发) |
| Bit12 | 轴 7 正限位逻辑电平 (1 低电平触发, 0 高电平触发) |
| Bit13 | 轴 7 负限位逻辑电平 (1 低电平触发, 0 高电平触发) |
| Bit14 | 轴 8 正限位逻辑电平 (1 低电平触发, 0 高电平触发) |
| Bit15 | 轴 8 负限位逻辑电平 (1 低电平触发, 0 高电平触发) |

```c
//例程
//将轴1的正负限位都设置为常开，低电平触发
MC_LmtSns(0X03)；
//将轴2的正负限位都设置为常开，低电平触发
MC_LmtSns(0XOC)；
//将轴1和2的正负限位都设置为常开，低电平触发
MC_LmtSns(0XOF)；
//注意：这个函数是一次性设置8个轴的限位电平
//如果控制卡是8轴~16轴，请使用函数MC_LmtSnsEX，用法与本函数一样
```

**`int MC_SetLmtSnsSingle(short nAxisNum,short nPosSns,short nNegSns)`**
*   **nAxisNum**: 轴号
*   **nPosSns**: 电平逻辑,0不反转,常开,1反转,常闭
*   **nNegSns**: 电平逻辑,0不反转,常开,1反转,常闭

**`int MC_EStopConfig(unsigned long ulEnableMask, unsigned long ulEnableValue, short nAdcMask, short nAdcValue, unsigned long ulIOMask, unsigned long ulIOValue)`**
*   **ulEnableMask**: 急停触发时要设置使能状态的轴掩码, Bit0代表轴1, Bit1代表轴2, Bit2代表轴3......
*   **ulEnableValue**: 急停触发时,要设置的使能状态,Bit0代表轴1,Bit1代表轴2,Bit2代表轴3......
*   **nAdcMask**: 急停触发时,要设置的模拟量通道掩码,Bit0代表通道1,Bit1代表通道2......
*   **nAdcValue**: 急停触发时,要设置的模拟量值
*   **ulIOMask**: 急停触发时要设置使能状态的IO掩码, Bit0代表Y0, Bit1代表Y1, Bit2代表Y2......
*   **ulIOValue**: 急停触发时要设置IO值, Bit0代表Y0, Bit1代表Y1, Bit2代表Y2......

**`int MC_EStopSetIO(short nCardIndex, short nIOIndex, short nEStopSns, unsigned long lFilterTime)`**
*   **nCardIndex**: 卡号,主卡为0,扩展IO卡依次为1、2、3、4......
*   **nIOIndex**: I0索引,0~15
*   **nEStopSns**: 电平逻辑,0不反转,1反转
*   **lFilterTime**: 滤波时间,单位ms

**`int MC_EStopOnOff(short nEStopOnOff)`**
*   **nEStopOnOff**: 0紧急停止功能关闭,1紧急停止功能打开

**`int MC_EStopGetSts(short *nStatus)`**
*   **nStatus**: 0紧急停止未触发,1紧急停止触发

**`int MC_EStopCls()`**

**`int MC_SetHardLimP(short nAxisNum, short nType, short nCardIndex, short nIOIndex)`**
*   **nAxisNum**: 轴编号
*   **nType**: -1:硬限位无效,0:用零位信号当限位,1:用通用输入IO当限位
*   **nCardIndex**: 卡号,主卡为0,扩展IO卡依次为1、2、3、4......
*   **nIOIndex**: I0索引,0~31
    *   例如将Home1作为轴1正限位:MC_SetHardLimP(1,0,0,0)//第一个1代表轴号,第二个0代表用零位当限位,第三个0代表主卡,第四个0代表IO索引
    *   例如将Home8作为轴1正限位:MC_SetHardLimP(1,0,0,7)

**`int MC_SetHardLimN(short nAxisNum, short nType, short nCardIndex, short nIOIndex)`**
*   **nAxisNum**: 轴编号
*   **nType**: -1:硬限位无效,0:用零位信号当限位,1:用通用输入IO当限位
*   **nCardIndex**: 卡号,主卡为0,扩展IO卡依次为1、2、3、4......
*   **nIOIndex**: I0索引,0~31
    *   例如将X0作为轴3负限位：MC_SetHardLimN(3,1,0,0) //第一个3代表轴号，第二个1代表用通用输入当限位，第三个0代表主卡，第四个0代表I0索引
    *   例如将X8作为轴3负限位：MC_SetHardLimN(3,1,0,8) //第一个3代表轴号，第二个1代表用通用输入当限位，第三个0代表主卡，第四个8代表I0索引

**`int MC_CrdHlimEnable(short nCrdNum,short nEnableFlag)`**
*   **nCrdNum**: 坐标系号
*   **nEnableFlag**: 0坐标系硬限位关闭, 1坐标系硬限位打开

### 示例代码:

```c
int iRes = 0;
long lSts = 0;
//long lSoftLimPos= 0X7FFFFFF;
long lSoftLimPos= 2147483647;
//long lSoftLimNeg=0X80000000;
long lSoftLimNeg=-2147483648;

iRes += MC_SetCardNo(1); //切换到第1块板卡
iRes += MC_Open(0, "192.168.0.200"); //打开板卡（通过网口，PC端IP地址为192.168.0.200）
iRes += MC_Set(); //复位板卡

//iRes = MC_SetSoftLimit(1, lSoftLimPos, lSoftLimNeg);

//注意，板卡默认上电是没有软限位的

//设置轴1正软限位为10000(单位脉冲)，负软限位为10000(单位脉冲)
iRes = MC_SetSoftLimit(1, 10000, -10000);

//这样就相当于关闭软限位
iRes = MC_SetSoftLimit(1, 2147483647, -2147483648);

iRes = MC_LmtsOn(1, -1); //将第一个轴的正硬限位和负硬限位都有效
iRes = MC_EStopSetIO(0, 0, 0, 10); //设置主卡通用输入X0为紧急停止IO，滤波时间10ms
iRes = MC_EStopOnOff(1); //紧急停止功能打开
iRes = MC_EStopGetSts(&nStatus); //获取紧急停止触发状态
iRes = MC_EStopClsSts(); //清除紧急停止状态
```

## 5.8、其他指令 API

| API | 说明 |
|---|---|
| MC_ZeroPos | 清零轴的规划和编码器位置 |
| MC_GetID | 获取板卡唯一标识ID |
| MC_SetAxisBand | 设置轴到位误差带 |
| MC_SetBacklash | 设置反向间隙补偿的相关参数 |
| MC_GetBacklash | 读取反向间隙补偿的相关参数 |
| MC_SetStopDec | 设置平滑停止减速度和急停减速度 |
| MC_WriteInterFlash | 写板卡内部Flash |
| MC_ReadInterFlash | 读板卡内部Flash |
| MC_DownPitchErrorTable | 下载螺距误差补偿表 |
| MC_ReadPitchErrorTable | 读取螺距误差补偿表 |
| MC_AxisErrPitchOn | 打开指定螺距误差补偿 |
| MC_AxisErrPitchOff | 关闭指定轴螺距误差补偿 |
| MC_SetPrfPos | 修改规划(脉冲)位置,修改时,轴不能处于运动状态 |
| MC_SetEncPos | 修改编码器位置,修改时,轴不能处于运动状态 |

### 参数详细说明：

**`int MC_ZeroPos(short nAxisNum,short nCount=1)`**
*   **nAxisNum**: 需要位置清零的起始轴号,取值范围: [1,AXIS_MAX_COUNT]
*   **nCount**: 需要位置清零的轴数

**`int MC_SetAxisBand(short nAxisNum,long lBand,long lTime)`**
*   **nAxisNum**: 轴号
*   **band**: 误差带大小,单位:脉冲(默认值:3)
*   **time**: 误差带保持时间,单位:ms(默认值:3)

**`int MC_SetBacklash(short nAxisNum,long lCompValue,double dCompChangeValue,long lCompDir)`**
*   **nAxisNum**: 需要进行反向间隙补偿的轴的编号,取值范围: [1,8]
*   **lCompValue**: 反向间隙补偿值,当为0时表示没有使能反向间隙补偿功能,取值只能为正,范围: [0,1073741824],单位:脉冲
*   **dCompChangeValue**: 反向间隙补偿的变化量,取值只能为正,范围: [0,1073741824],单位:脉冲/毫秒当该参数的值为0或者大于等于lCompValue时,则反向间隙的补偿量将瞬间叠加在规划位置上,没有渐变的过程
*   **lCompDir**: 反向间隙补偿方向
    *   0:只补偿负方向,当电机向负方向运动时,将施加补偿量,电机向正方向运动时,不施加补偿量
    *   1:只补偿正方向,当电机向正方向运动时,将施加补偿量,电机向负方向运动时,不施加补偿量

**`int MC_GetBacklash(short nAxisNum,long *pCompValue,double *pCompChangeValue,long *pCompDir)`**
*   **nAxisNum**: nAxisNum 查询的轴号,取值范围: [1,AXIS_MAX_COUNT]
*   **pCompValue**: pCompValue 读取的反向间隙补偿值
*   **pCompChangeValue**: pCompChangeValue 读取的反向间隙补偿值的变化量
*   **pCompDir**: pCompDir 读取的反向间隙补偿的补偿方向

**`int MC_SetStopDec(short nAxisNum, double decSmoothStop, double decAbruptStop)`**
*   **nAxisNum**: 轴编号
*   **DecSmoothStop**: 平滑停止减速度,单位:脉冲/毫秒/毫秒,建议范围0.1~2,默认0.5
*   **DecAbruptStop**: 急停减速度,单位:脉冲/毫秒/毫秒,建议范围1~20,默认1

**`int MC_WriteInterFlash(unsigned char* pData, short nLength)`**
*   **pData**: 数据指针
*   **nLength**: 数据长度,不能大于500

**`int MC_ReadInterFlash(unsigned char*pData, short nLength)`**
*   **pData**: 数据指针
*   **nLength**: 数据长度,不能大于500

**`int MC_DownPitchErrorTable(short nAxisNum, short nPointCount, long lStartPos, long lEndPos, short *pErrValue1, short *pErrValue2)`**
*   **nAxisNum**: 轴号
*   **nPointCount**: 点数量
*   **lStartPos**: 起始位置
*   **lEndPos**: 终止位置
*   **pErrValue1**: 误差表1指针
*   **pErrValue2**: 误差表2指针

**`int MC_ReadPitchErrorTable(short nAxisNum, short *pPointCount, long *pStartPos, long *pEndPos, short *pErrValue1, short *pErrValue2)`**
*   **nAxisNum**: 轴号
*   **pPointCount**: 点数量指针
*   **pStartPos**: 起始位置指针
*   **pEndPos**: 终止位置指针
*   **pErrValue1**: 误差表1指针
*   **pErrValue2**: 误差表2指针

**`int MC_AxisErrPitchOn(short nAxisNum)`**
*   **nAxisNum**: 轴号

**`int MC_AxisErrPitchOff(short nAxisNum)`**
*   **nAxisNum**: 轴号

**`int MC_SetPrfPos(short nAxisNum, long lPos)`**
*   **nAxisNum**: 轴号
*   **lPos**: 规划位置

**`int MC_SetEncPos(short nAxisNum, long lPos)`**
*   **nAxisNum**: 轴号
*   **lPos**: 编码器位置

### 示例代码:

```c
int iRes = 0;
unsigned long u1ID = 0;
long lSoftLimPos = 0X7FFFFFF;
long lSoftLimNeg = 0X8000000;

iRes += MC_SetCardNo(1); //切换到第1块板卡

iRes += MC_Open(0, "192.168.0.200"); //打开板卡（通过网口，PC端IP地址为192.168.0.200）

iRes += MC_Set(); //复位板卡

iRes += MC_ZeroPos(1, 8); //清零所有8个轴的位置

iRes += MC_SetAxisBand(1,3,5)//设置轴1到位误差带为3个脉冲，保持时间为5ms

//设置轴 1 反向间隙为 3 个脉冲, 补偿速率 2 脉冲/ms，反向运动时补偿

iRes = MC_SetBacklash(1,3,2,0);

iRes = MC_GetID(&ulID);

short ErrValue1[2000];

short ErrValue2[2000];

//任意初始化了一个螺距误差表(这里仅仅用于测试，实际应由激光干涉仪生成)

for(int i=0; i<=1000; i++)
{
    ErrValue1[i] = i;
    ErrValue2[i] = i;
}

//下载轴2的螺距误差表，点数量1001，起始位置10000，终止位置20000

iRes = MC_DownPitchErrorTable(2, 1001, 10000, 20000, & ErrValue1[0], & ErrValue2[0]);

//启用轴2螺距误差表

iRes = MC_AxisErrPitchOn(2);
```

## 5.9、插补运动指令API

| API | 说明 |
|---|---|
| MC_SetCrdPrm | 设置坐标系参数,确立坐标系映射,建立坐标系 |
| MC_SetCrdPrmSingleEX | 用于代替MC_SetCrdPrm函数,方便不擅长结构体的客户使用。 |
| MC_GetCrdPrm | 查询坐标系参数 |
| MC_InitLookAhead | 配置指定坐标系指定FifoIndex前瞻缓冲区的拐弯速率,最大加速度,缓冲区深度,缓冲区指针等参数 |
| MC_InitLookAheadSingle | 用于替代MC_InitLookAhead函数,方便不擅长结构体的客户使用。 |
| MC_InitLookAheadSingleEX | 用于替代MC_InitLookAhead函数,方便不擅长结构体的客户使用。 |
| MC_CrdClear | 清除插补缓存区内的插补数据 |
| MC_LnXY | 缓存区指令,两维直线插补 |
| MC_LnXYZ | 缓存区指令,三维直线插补 |
| MC_LnXYZA | 缓存区指令,四维直线插补 |
| MC_LnXYZAB | 缓存区指令,五维直线插补 |
| MC_LnXYZABC | 缓存区指令,六维直线插补 |
| MC_LnAll | 缓存区指令,7维~14维直线插补 |
| MC_ArcXYC | 缓存区指令,XY平面圆弧插补(以终点坐标和圆心位置为输入参数) |
| MC_ArcXZC | 缓存区指令,XZ平面圆弧插补(以终点坐标和圆心位置为输入参数) |
| MC_ArcYZC | 缓存区指令,YZ平面圆弧插补(以终点坐标和圆心位置为输入参数) |
| MC_HelixXYCZ | 缓存区指令,XY平面螺旋线插补(以终点坐标和圆心位置为输入参数) |
| MC_HelixXZCY | 缓存区指令,XZ平面螺旋线插补(以终点坐标和圆心位置为输入参数) |
| MC_HelixYZCX | 缓存区指令,YZ平面螺旋线插补(以终点坐标和圆心位置为输入参数) |
| MC_HelixXYCCount | 缓存区指令,XY平面螺旋线插补(以终点坐标和圆心位置为输入参数) |
| MC_HelixXZCCount | 缓存区指令,XZ平面螺旋线插补(以终点坐标和圆心位置为输入参数) |
| MC_HelixYZCCount | 缓存区指令,YZ平面螺旋线插补(以终点坐标和圆心位置为输入参数) |
| MC_BufPWM | 缓存区指令,设置PWM频率及占空比 |
| MC_BufIO | 缓存区指令,设置IO输出 |
| MC_BufIOReverse | 缓存区指令,设置IO输出一个指定时间的脉冲 |
| MC_BufWaitIO | 缓存区指令,等待IO输入 |
| MC_BufCmpPluse | 缓存区指令,在插补运动中,插入立即比较输出 |
| MC_BufDelay | 缓存区指令,延时一段时间 |
| MC_BufSetM | 缓冲区指令,设置M变量(PMC系列支持) |
| MC_BufWaitM | 缓冲区指令,等待M变量(PMC系列支持) |
| MC_BufMoveVel | 在插补运动的过程中插入BufferMove轴的速度设定 |
| MC_BufMoveAcc | 在插补运动的过程中插入BufferMove轴的加速度设定 |
| MC_BufMove | 在插补运动的过程中插入阻塞和非阻塞的点位运动 |
| MC_BufGear | 设定了脉冲输出的个数。它会保证与其后紧挨的指令同时启动,同时停止 |
| MC_CrdData | 向插补缓存区增加插补数据 |
| MC_CrdStart | 启动插补运动 |
| MC_SetOverride | 在运动中修改坐标系运动的合成速度的倍率 |
| MC_ResetOverride | 复位坐标系运动的合成速度的倍率 |
| MC_GetCrdSts | 读取坐标系状态 |
| MC_GetCrdPos | 读取坐标系当前位置 |
| MC_GetCrdVel | 读取坐标系当前速度 |
| MC_GetFifoSpace | 读取坐标系FIFO的剩余空间 |
| MC_GetSegNum | 读取当前坐标系正在执行的程序段号 |

### 参数详细说明：

**`int MC_SetCrdPrm(short nCrdNum, TCrdPrm *pCrdPrm)`**
*   **nCrdNum**: 坐标系号，取值范围：[1, CRDSYS_MAX_COUNT]
*   **pCrdPrm**:
    ```c
    typedef struct CrdPrm{
        short dimension;
        short profile[8];
        double synVelMax;
        double synAccMax;
        short evenTime;
        short setOriginFlag;
        long originPos[8];
    }TCrdPrm;
    ```
    *   **dimension**: 坐标系的维数，取值范围：[1, 14]。
    *   **Profile[8]**: 坐标系与规划器的映射关系，每个元素的取值范围：[0, 16]
    *   **synVelMax**: 最大合成速度。取值范围：(0, 4000)，单位：pulse/ms
    *   **synAccMax**: 最大合成加速度。取值范围：(0, 4000)，单位：pulse/(ms*ms)
    *   **evenTime**: 最小匀速段时间。取值范围：[0, 32767)，单位：ms
    *   **setOriginFlag**: 表示是否需要指定坐标系的原点坐标的规划位置
        *   0: 不需要指定原点坐标值，则坐标系的原点在当前规划位置上；
        *   1: 需要指定原点坐标值，坐标系的原点在originPos指定的规划位置上
    *   **originPos[8]**: 指定的坐标系原点的规划位置值

**`int MC_SetCrdPrmSingleEX(short nCrdNum, short dimension, short profile0, short profile1, short profile2, short profile3, short profile4, short profile5, short profile6, short profile7, double synVelMax, double synAccMax, short evenTime, short setOriginFlag, long originPos0, long originPos1, long originPos2, long originPos3, long originPos4, long originPos5, long originPos6, long originPos7)`**
*   **nCrdNum**:坐标系号，取值范围：[1, CRDSYS_MAX_COUNT]
*   **dimension**: 坐标系的维数，取值范围：[1, 14]。
*   **profile0**: 坐标系X轴号，取值范围：[1, 16]
*   **Profile1**: 坐标系Y轴号，取值范围：[1, 16]，用不到可以设置为0
*   **Profile2**: 坐标系Z轴号，取值范围：[1, 16]，用不到可以设置为0
*   **Profile3**: 坐标系A轴号，取值范围：[1, 16]，用不到可以设置为0
*   **Profile4**: 坐标系B轴号，取值范围：[1, 16]，用不到可以设置为0
*   **Profile5**: 保留，固定为0
*   **Profile6**: 保留,固定为0
*   **Profile7**: 保留,固定为0
*   **synVelMax**: 该坐标系的最大合成速度。取值范围:(0,4000),单位:pulse/ms
*   **synAccMax**: 该坐标系的最大合成加速度。取值范围:(0,4000),单位:pulse/(ms*ms)
*   **evenTime**: 每个插补段的最小匀速段时间。取值范围:[0,32767),单位:ms
*   **setOriginFlag**: 表示是否需要指定坐标系的原点坐标的规划位置。
    *   0:不需要指定原点坐标值,则坐标系的原点在当前规划位置上
    *   1:需要指定原点坐标值,按照后面的originPos设定原点坐标
*   **originPos0**: 坐标系原点X轴坐标
*   **originPos1**: 坐标系原点Y轴坐标
*   **originPos2**: 坐标系原点Z轴坐标
*   **originPos3**: 坐标系原点A轴坐标
*   **originPos4**: 坐标系原点B轴坐标
*   **originPos5**: 保留
*   **originPos6**: 保留
*   **originPos7**: 保留

**`int MC_GetCrdPrm(short nCrdNum, TCrdPrm *pCrdPrm)`**
*   **nCrdNum**: 坐标系号，取值范围：[1, CRDSYS_MAX_COUNT]
*   **pCrdPrm**:
    ```c
    typedef struct CrdPrm{
        short dimension;
        short profile[8];
        double synVelMax;
        double synAccMax;
        short evenTime;
        short setOriginFlag;
        long originPos[8];
    }TCrdPrm;
    ```

**`int MC_InitLookAhead(short nCrdNum, short FifoIndex, TLookAheadPrm *pLookAhead)`**
*   **nCrdNum**: 坐标系编号,通常为1
*   **FifoIndex**: FifoIndex 索引,通常为0
*   **pLookAhead**:
    ```c
    typedef struct LookAheadPrm{
        int lookAheadNum; //前瞻段数
        TCrData *pLookAheadBuf; //前瞻缓冲区指针
        double dSpeedMax[INTERPOLATION_AXIS_MAX]; //各轴的最大速度(p/ms)
        double dAccMax[INTERPOLATION_AXIS_MAX]; //各轴的最大加速度
        double dMaxStepSpeed[INTERPOLATION_AXIS_MAX]; //各轴最大速度变化量(相当于启动速度)
        double dScale[INTERPOLATION_AXIS_MAX]; //各轴的脉冲当量
    }TLookAheadPrm;
    ```

**`int MC_InitLookAheadSingle(short nCrdNum, short FifoIndex, int lookAheadNum, double* dSpeedMax, double* dAccMax, double *dMaxStepSpeed, double *dScale)`**
*   **nCrdNum**: 坐标系编号,通常为1
*   **FifoIndex**: FifoIndex 索引,通常为0
*   **lookAheadNum**: 前瞻段数,通常为50~200
*   **dSpeedMax**: 数组指针,数组长度为6,存放各轴的最大速度(p/ms)
*   **dAccMax**: 数组指针,数组长度为6,存放各轴的最大加速度
*   **dMaxStepSpeed**: 数组指针,数组长度为6,各轴的最大速度变化量(相当于启动速度)
*   **dScale**: 数组指针,数组长度为6,存放各轴的脉冲当量(固定为1)

**`int MC_InitLookAheadSingleEx(short nCrdNum, short FifoIndex, int lookAheadNum, double dSpeedMax0, double dSpeedMax1, double dSpeedMax2, double dSpeedMax3, double dSpeedMax4, double dAccMax0, double dAccMax1, double dAccMax2, double dAccMax3, double dAccMax4, double dMaxStepSpeed0, double dMaxStepSpeed1, double dMaxStepSpeed2, double dMaxStepSpeed3, double dMaxStepSpeed4, double dScale0, double dScale1, double dScale2, double dScale3, double dScale4)`**
*   **nCrdNum**: 坐标系号,通常为1
*   **FifoIndex**: 缓冲区索引,通常为0
*   **lookAheadNum**: 前瞻段数,通常为200
*   **dSpeedMax0**: X轴最大速度,单位脉冲/毫秒,通常为1000
*   **dSpeedMax1**: Y轴最大速度,单位脉冲/毫秒,通常为1000
*   **dSpeedMax2**: Z轴最大速度,单位脉冲/毫秒,通常为1000
*   **dSpeedMax3**: A轴最大速度,单位脉冲/毫秒,通常为1000
*   **dSpeedMax4**: B轴最大速度,单位脉冲/毫秒,通常为1000
*   **dAccMax0**: X轴最大加速度,单位脉冲/毫秒/毫秒,通常为0.1~10,建议1
*   **dAccMax1**: Y轴最大加速度,单位脉冲/毫秒/毫秒,通常为0.1~10,建议1
*   **dAccMax2**: Z轴最大加速度,单位脉冲/毫秒/毫秒,通常为0.1~10,建议1
*   **dAccMax3**: A轴最大加速度,单位脉冲/毫秒/毫秒,通常为0.1~10,建议1
*   **dAccMax4**: B轴最大加速度,单位脉冲/毫秒/毫秒,通常为0.1~10,建议1
*   **dMaxStepSpeed0**: X轴最大速度变化量,单位脉冲/毫秒,通常为0.1~20,建议2
*   **dMaxStepSpeed1**: Y轴最大速度变化量,单位脉冲/毫秒,通常为0.1~20,建议2
*   **dMaxStepSpeed2**: Z轴最大速度变化量,单位脉冲/毫秒,通常为0.1~20,建议2
*   **dMaxStepSpeed3**: A轴最大速度变化量,单位脉冲/毫秒,通常为0.1~20,建议2
*   **dMaxStepSpeed4**: B轴最大速度变化量,单位脉冲/毫秒,通常为0.1~20,建议2
*   **dScale0**: X轴脉冲当量,固定为1
*   **dScale1**: Y轴脉冲当量,固定为1
*   **dScale2**: Z轴脉冲当量,固定为1
*   **dScale3**: A轴脉冲当量,固定为1
*   **dScale4**: B轴脉冲当量,固定为1

**`int MC_CrdClear(short nCrdNum, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **FifoIndex**: 缓冲区索引

**`int MC_LnXY(short nCrdNum, double x, double y, double synVel, double synAcc, double velEnd, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **x**: X轴终点位置
*   **y**: Y轴终点位置
*   **synVel**: 合成速度
*   **synAcc**: 合成加速度
*   **velEnd**: 终点速度
*   **FifoIndex**: 缓冲区索引

**`int MC_LnXYZ(short nCrdNum, double x, double y, double z, double synVel, double synAcc, double velEnd, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **x**: X轴终点位置
*   **y**: Y轴终点位置
*   **z**: Z轴终点位置
*   **synVel**: 合成速度
*   **synAcc**: 合成加速度
*   **velEnd**: 终点速度
*   **FifoIndex**: 缓冲区索引

**`int MC_LnXYZA(short nCrdNum, double x, double y, double z, double a, double synVel, double synAcc, double velEnd, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **x**: X轴终点位置
*   **y**: Y轴终点位置
*   **z**: Z轴终点位置
*   **a**: A轴终点位置
*   **synVel**: 合成速度
*   **synAcc**: 合成加速度
*   **velEnd**: 终点速度
*   **FifoIndex**: 缓冲区索引

**`int MC_LnXYZAB(short nCrdNum, double x, double y, double z, double a, double b, double synVel, double synAcc, double velEnd, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **x**: X轴终点位置
*   **y**: Y轴终点位置
*   **z**: Z轴终点位置
*   **a**: A轴终点位置
*   **b**: B轴终点位置
*   **synVel**: 合成速度
*   **synAcc**: 合成加速度
*   **velEnd**: 终点速度
*   **FifoIndex**: 缓冲区索引

**`int MC_LnXYZABC(short nCrdNum, double x, double y, double z, double a, double b, double c, double synVel, double synAcc, double velEnd, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **x**: X轴终点位置
*   **y**: Y轴终点位置
*   **z**: Z轴终点位置
*   **a**: A轴终点位置
*   **b**: B轴终点位置
*   **c**: C轴终点位置
*   **synVel**: 合成速度
*   **synAcc**: 合成加速度
*   **velEnd**: 终点速度
*   **FifoIndex**: 缓冲区索引

**`int MC_LnAll(short nCrdNum, double *pPos, double synVel, double synAcc, double velEnd, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **pPos**: 终点位置数组指针
*   **synVel**: 合成速度
*   **synAcc**: 合成加速度
*   **velEnd**: 终点速度
*   **FifoIndex**: 缓冲区索引

**`int MC_ArcXYC(short nCrdNum, double x, double y, double R, short circleDir, double synVel, double synAcc, double velEnd, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **x**: X轴终点位置
*   **y**: Y轴终点位置
*   **R**: 半径
*   **circleDir**: 0:顺时针, 1:逆时针
*   **synVel**: 合成速度
*   **synAcc**: 合成加速度
*   **velEnd**: 终点速度
*   **FifoIndex**: 缓冲区索引

**`int MC_ArcXZC(short nCrdNum, double x, double z, double R, short circleDir, double synVel, double synAcc, double velEnd, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **x**: X轴终点位置
*   **z**: Z轴终点位置
*   **R**: 半径
*   **circleDir**: 0:顺时针, 1:逆时针
*   **synVel**: 合成速度
*   **synAcc**: 合成加速度
*   **velEnd**: 终点速度
*   **FifoIndex**: 缓冲区索引

**`int MC_ArcYZC(short nCrdNum, double y, double z, double R, short circleDir, double synVel, double synAcc, double velEnd, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **y**: Y轴终点位置
*   **z**: Z轴终点位置
*   **R**: 半径
*   **circleDir**: 0:顺时针, 1:逆时针
*   **synVel**: 合成速度
*   **synAcc**: 合成加速度
*   **velEnd**: 终点速度
*   **FifoIndex**: 缓冲区索引

**`int MC_HelixXYCZ(short nCrdNum, double x, double y, double z, double R, short circleDir, double synVel, double synAcc, double velEnd, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **x**: X轴终点位置
*   **y**: Y轴终点位置
*   **z**: Z轴终点位置
*   **R**: 半径
*   **circleDir**: 0:顺时针, 1:逆时针
*   **synVel**: 合成速度
*   **synAcc**: 合成加速度
*   **velEnd**: 终点速度
*   **FifoIndex**: 缓冲区索引

**`int MC_HelixXZCY(short nCrdNum, double x, double z, double y, double R, short circleDir, double synVel, double synAcc, double velEnd, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **x**: X轴终点位置
*   **z**: Z轴终点位置
*   **y**: Y轴终点位置
*   **R**: 半径
*   **circleDir**: 0:顺时针, 1:逆时针
*   **synVel**: 合成速度
*   **synAcc**: 合成加速度
*   **velEnd**: 终点速度
*   **FifoIndex**: 缓冲区索引

**`int MC_HelixYZCX(short nCrdNum, double y, double z, double x, double R, short circleDir, double synVel, double synAcc, double velEnd, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **y**: Y轴终点位置
*   **z**: Z轴终点位置
*   **x**: X轴终点位置
*   **R**: 半径
*   **circleDir**: 0:顺时针, 1:逆时针
*   **synVel**: 合成速度
*   **synAcc**: 合成加速度
*   **velEnd**: 终点速度
*   **FifoIndex**: 缓冲区索引

**`int MC_HelixXYCCount(short nCrdNum, double x, double y, double z, int count, double synVel, double synAcc, double velEnd, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **x**: X轴终点位置
*   **y**: Y轴终点位置
*   **z**: Z轴终点位置
*   **count**: 圈数
*   **synVel**: 合成速度
*   **synAcc**: 合成加速度
*   **velEnd**: 终点速度
*   **FifoIndex**: 缓冲区索引

**`int MC_HelixXZCCount(short nCrdNum, double x, double z, double y, int count, double synVel, double synAcc, double velEnd, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **x**: X轴终点位置
*   **z**: Z轴终点位置
*   **y**: Y轴终点位置
*   **count**: 圈数
*   **synVel**: 合成速度
*   **synAcc**: 合成加速度
*   **velEnd**: 终点速度
*   **FifoIndex**: 缓冲区索引

**`int MC_HelixYZCCount(short nCrdNum, double y, double z, double x, int count, double synVel, double synAcc, double velEnd, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **y**: Y轴终点位置
*   **z**: Z轴终点位置
*   **x**: X轴终点位置
*   **count**: 圈数
*   **synVel**: 合成速度
*   **synAcc**: 合成加速度
*   **velEnd**: 终点速度
*   **FifoIndex**: 缓冲区索引

**`int MC_BufPWM(short nCrdNum, short nPwmNum, double dFreq, double dDuty, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **nPwmNum**: PWM通道号
*   **dFreq**: 频率
*   **dDuty**: 占空比
*   **FifoIndex**: 缓冲区索引

**`int MC_BufIO(short nCrdNum, unsigned long ulMask, unsigned long ulValue, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **ulMask**: IO掩码
*   **ulValue**: IO值
*   **FifoIndex**: 缓冲区索引

**`int MC_BufIOReverse(short nCrdNum, short nCardNum, short nIOIndex, short nValue, short nReverseTime, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **nCardNum**: 卡号
*   **nIOIndex**: IO索引
*   **nValue**: 电平
*   **nReverseTime**: 翻转时间
*   **FifoIndex**: 缓冲区索引

**`int MC_BufWaitIO(short nCrdNum, unsigned long ulMask, unsigned long ulValue, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **ulMask**: IO掩码
*   **ulValue**: IO值
*   **FifoIndex**: 缓冲区索引

**`int MC_BufCmpPluse(short nCrdNum, short nChannel, short nPluseType, short nTime, short nTimeFlag, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **nChannel**: 通道号
*   **nPluseType**: 脉冲类型
*   **nTime**: 时间
*   **nTimeFlag**: 时间单位
*   **FifoIndex**: 缓冲区索引

**`int MC_BufDelay(short nCrdNum, unsigned long ulDelay, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **ulDelay**: 延时时间
*   **FifoIndex**: 缓冲区索引

**`int MC_BufSetM(short nCrdNum, short nMIndex, long lMValue, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **nMIndex**: M变量索引
*   **lMValue**: M变量值
*   **FifoIndex**: 缓冲区索引

**`int MC_BufWaitM(short nCrdNum, short nMIndex, long lMValue, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **nMIndex**: M变量索引
*   **lMValue**: M变量值
*   **FifoIndex**: 缓冲区索引

**`int MC_BufMoveVel(short nCrdNum, short nAxis, double dVel, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **nAxis**: 轴号
*   **dVel**: 速度
*   **FifoIndex**: 缓冲区索引

**`int MC_BufMoveAcc(short nCrdNum, short nAxis, double dAcc, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **nAxis**: 轴号
*   **dAcc**: 加速度
*   **FifoIndex**: 缓冲区索引

**`int MC_BufMove(short nCrdNum, short nAxis, long lPos, double dVel, double dAcc, short nBlock, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **nAxis**: 轴号
*   **lPos**: 位置
*   **dVel**: 速度
*   **dAcc**: 加速度
*   **nBlock**: 阻塞标志
*   **FifoIndex**: 缓冲区索引

**`int MC_BufGear(short nCrdNum, short nAxis, long lPluseCount, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **nAxis**: 轴号
*   **lPluseCount**: 脉冲数
*   **FifoIndex**: 缓冲区索引

**`int MC_CrdData(short nCrdNum, TCrData *pCrdData, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **pCrdData**: 插补数据指针
*   **FifoIndex**: 缓冲区索引

**`int MC_CrdStart(short nCrdNum, short FifoIndex)`**
*   **nCrdNum**: 坐标系号
*   **FifoIndex**: 缓冲区索引

**`int MC_SetOverride(short nCrdNum, double dOverride)`**
*   **nCrdNum**: 坐标系号
*   **dOverride**: 倍率

**`int MC_ResetOverride(short nCrdNum)`**
*   **nCrdNum**: 坐标系号

**`int MC_GetCrdSts(short nCrdNum, long *pSts, short *pRunSeg, long *pRemainSeg, unsigned long *pClock)`**
*   **nCrdNum**: 坐标系号
*   **pSts**: 状态字指针
*   **pRunSeg**: 正在执行的段号指针
*   **pRemainSeg**: 剩余段数指针
*   **pClock**: 时钟指针

**`int MC_GetCrdPos(short nCrdNum, double *pPos)`**
*   **nCrdNum**: 坐标系号
*   **pPos**: 位置指针

**`int MC_GetCrdVel(short nCrdNum, double *pVel)`**
*   **nCrdNum**: 坐标系号
*   **pVel**: 速度指针

**`int MC_GetFifoSpace(short nCrdNum, short FifoIndex, long *pSpace)`**
*   **nCrdNum**: 坐标系号
*   **FifoIndex**: 缓冲区索引
*   **pSpace**: 剩余空间指针

**`int MC_GetSegNum(short nCrdNum, short FifoIndex, long *pSegNum)`**
*   **nCrdNum**: 坐标系号
*   **FifoIndex**: 缓冲区索引
*   **pSegNum**: 段号指针

## 5.10、硬件捕获类API

### 参数详细说明：

| API | 说明 |
|---|---|
| MC_SetCaptureMode | 设置编码器捕获方式，并启动捕获 |
| MC_GetCaptureMode | 读取编码器捕获方式 |
| MC_GetCaptureStatus | 读取编码器捕获状态 |
| MC_SetCaptureSense | 设置捕获电平 |
| MC_GetCaptureSense | 获取捕获电平 |
| MC_ClearCaptureStatus | 清除捕获状态 |

**`int MC_SetCaptureMode(short nEncodeNum, short nMode)`**
*   **nEncodeNum**: 编码器轴号
*   **nMode**: 编码器捕获模式
    *   1: Home 捕获
    *   2: Index 捕获
    *   3: 探针捕获

**`int MC_GetCaptureMode(short nEncodeNum, short *pMode, short nCount=1)`**
*   **nEncodeNum**: 编码器起始轴号
*   **pMode**: 编码器捕获模式
*   **nCount**: 读取的轴数,默认为1,1次最多可以读取8个编码器轴

**`int MC_GetCaptureStatus(short nEncodeNum, short *pStatus, long *pValue, short nCount=1, unsigned long *pClock=NULL)`**
*   **nEncodeNum**: 编码器起始轴号
*   **pStatus**: 读取编码器捕获状态为1时表示对应轴捕获触发
*   **pValue**: 读取编码器捕获值,当捕获触发时,编码器捕获值会自动更新
*   **nCount**: 读取的轴数,默认为1,1次最多可以读取8个编码器轴
*   **pClock**: 读取控制器时钟

**`int MC_SetCaptureSense(short nEncodeNum, short nMode, short nSence)`**
*   **nEncodeNum**: 编码器起始轴号
*   **nMode**: 捕获模式
    *   1: Home 捕获
    *   2: Index 捕获
    *   3: 探针捕获
*   **nSence**: 捕获电平
    *   0: 下降沿触发
    *   1: 上升沿触发

**`int MC_GetCaptureSense(short nEncodeNum, short nMode, short *pSence)`**
*   **nEncodeNum**: 编码器号
*   **nMode**: 捕获模式
    *   1: Home 捕获
    *   2: Index 捕获
    *   3: 探针捕获
*   **pSence**: 捕获电平,0或者1
    *   0: 下降沿触发
    *   1: 上升沿触发

**`int MC_ClearCaptureStatus(short nEncodeNum)`**
*   **nEncodeNum**: 需要被清除捕获状态的编码器轴号

## 5.11、Gear/电子齿轮类API

| API | 说明 |
|---|---|
| MC_PrfGear | 设置指定轴进入电子齿轮模式 |
| MC_SetGearMaster | 设置电子齿轮运动跟随主轴 |
| MC_GetGearMaster | 读取电子齿轮运动跟随主轴 |
| MC_SetGearRatio | 设置电子齿轮比 |
| MC_GetGearRatio | 获取电子齿轮比 |
| MC_GearStart | 启动电子齿轮运动 |
| MC_GearStop | 停止电子齿轮运动 |
| MC_SetGearEvent | 设置电子齿轮触发事件 |
| MC_GetGearEvent | 获取电子齿轮触发事件 |
| MC_SetGearIntervalTime | 设置电子齿轮平滑系数 |
| MC_GetGearIntervalTime | 获取电子齿轮平滑系数 |

### 参数详细说明：

**`int MC_PrfGear(short nAxisNum,short nDir=0)`**
*   **nAxisNum**: 轴号
*   **nDir**: 无意义,固定为0

**`int MC_SetGearMaster(short nAxisNum,short nMasterAxisNum,short nMasterType=2)`**
*   **nAxisNum**: 轴号
*   **nMasterAxisNum**: 主轴号
*   **n/masterType**: 主轴类型,2,跟随规划,1,跟随编码器

**`int MC_GetGearMaster(short nAxisNum,short *pnMasterAxisNum,short *pMasterType=NULL)`**
*   **nAxisNum**: 轴号
*   **pnMasterAxisNum**: 主轴号
*   **p/masterType**: 主轴类型,2,跟随规划,1,跟随编码器

**`int MC_SetGearRatio(short nAxisNum,long lMasterEven,long lSlaveEven,long lMasterSlope=0,long lStopSmoothTime=200)`**
*   **nAxisNum**: 轴号
*   **lMasterEven**: 传动比系数,主轴位移,单位:pulse
*   **lSlaveEven**: 传动比系数,从轴位移,单位:pulse
*   **lMasterSlope**: 主轴离合区位移,单位:pulse 取值范围:不能小于0或者等于1
*   **lStopSmoothTime**: 脱离离合区缓冲时间,单位:ms

**`int MC_GetGearRatio(short nAxisNum,long *pMasterEven,long *pSlaveEven,long *pMasterSlope,long *pStopSmoothTime)`**
*   **nAxisNum**: 轴号
*   **lMasterEven**: 传动比系数,主轴位移,单位:pulse
*   **lSlaveEven**: 传动比系数,从轴位移,单位:pulse
*   **lMasterSlope**: 主轴离合区位移,主轴离合区位移,主轴离合区位移,主轴离合区位移,单位:pulse
*   **1StopSmoothTime**: 脱离离合区缓冲时间,单位:ms

**`int MC_GearStart(long lMask)`**
*   **lMask**: 按位指示需要启动Gear运动的轴号。当bit位为1时表示启动对应的轴Bit7、6、5、4、3、2、1、0对应8轴、7轴、6轴、5轴、4轴、3轴、2轴、1轴

**`int MC_GearStop(long lAxisMask,long lEMGMask)`**
*   **lMask**: 按位指示需要停止Gear运动的轴号。当bit位为1时表示停止对应的轴Bit7~0对应8~1轴
*   **lEMGMask**: 按位指示需要立即停止Gear运动的轴号。当bit位为1时表示立即停止对应的轴,忽略平滑停止时间当bit位为0时表示平滑停止对应的轴,StopSmoothTime时间内平滑停止Bit7~0对应8~1轴

**`int MC_SetGearEvent(short nAxisNum,short nEvent,double startPara0,double startParal)`**
*   **nAxisNum**: 轴号
*   **nEvent**:
    *   1:立即启动电子齿轮
    *   2:主轴规划或者编码器位置大于等于指定数值时启动电子齿轮
    *   3:主轴规划或者编码器位置小于等于指定数值时启动电子齿轮
    *   4:指定IO为ON时启动电子齿轮
    *   5:指定IO为OFF时启动电子齿轮
*   **startPara0**: 事件参数0
    *   nEvent=2或者3时代表编码器或者规划值
    *   nEvent=4或者5时代表IO端口号
*   **startParal**: 保留

**`int MC_GetGearEvent(short nAxisNum,short *pEvent,double *pStartPara0,double *pStartParal)`**
*   **nAxisNum**: 轴号
*   **pEvent**:
    *   1:立即启动电子齿轮
    *   2:主轴规划或者编码器位置大于等于指定数值时启动电子齿轮
    *   3:主轴规划或者编码器位置小于等于指定数值时启动电子齿轮
    *   4:指定IO为ON时启动电子齿轮
    *   5:指定IO为OFF时启动电子齿轮
*   **pStartPara0**: 事件参数0
    *   nEvent=2或者3时代表编码器或者规划值
    *   nEvent=4或者5时代表IO端口号
*   **pStartParal**: 保留

**`int MC_SetGearIntervalTime(short nAxisNum,short nIntervalTime)`**
*   **nAxisNum**: 轴号
*   **nIntervalTime**: 平滑时间，单位ms

**`int MC_GetGearIntervalTime(short nAxisNum,short *nIntervalTime)`**
*   **nAxisNum**: 轴号
*   **nIntervalTime**: 平滑时间，单位ms

### 例程代码：

```c
//设置轴3轴4进入电子齿轮模式，双向跟随
MC_PrfGear(3,0);
MC_PrfGear(4,0);

//设置轴3轴4跟随轴1的编码器
MC_SetGearMaster(3,1,1);
MC_SetGearMaster(4,1,1);

//设置轴3跟随比例为1比1
MC_SetGearRatio(3,1,1,0,0);
//设置轴4跟随比例为1比1
MC_SetGearRatio(4,1,1,0,0);

//设置轴3跟随触发方式为立即开始
MC_SetGearEvent(3,1,0,0)
//设置轴4跟随触发方式为立即开始
MC_SetGearEvent(4,1,0,0)

//启动轴3和轴4的跟随
MC_GearStart(0XOC);
```

## 5.12、电子凸轮类API

| API | 说明 |
|---|---|
| MC_PrfCam | 设置指定轴进入电子凸轮模式 |
| MC_SetCamMaster | 设置电子凸轮运动跟随主轴 |
| MC_GetCamMaster | 读取电子凸轮运动跟随主轴 |
| MC_SetCamEvent | 设置电子凸轮触发事件 |
| MC_GetCamEvent | 获取电子凸轮触发事件 |
| MC_SetCamIntervalTime | 设置电子凸轮均值滤波时间 |
| MC_GetCamIntervalTime | 获取电子凸轮均值滤波时间 |
| MC_SetUpCamTable | 建立凸轮表 |
| MC_DownCamTable | 下载电子凸轮表到控制器 |
| MC_CamStart | 启动电子凸轮运动 |
| MC_CamStop | 停止电子凸轮运动 |

### 参数详细说明：

**`int MC_PrfCam(short nAxisNum,short nTableNum)`**
*   **nAxisNum**: 轴号
*   **nTableNum**: 凸轮表编号,1~16

**`int MC_SetCamMaster(short nAxisNum,short nMasterAxisNum,short nMasterType)`**
*   **nAxisNum**: 轴号
*   **nMasterAxisNum**: 主轴号
*   **nMasterType**: 主轴类型,1:跟随编码器2:跟随规划

**`int MC_GetCamMaster(short nAxisNum,short *pnMasterAxisNum,short *pMasterType=NULL)`**
*   **nAxisNum**: 轴号
*   **pnMasterAxisNum**: 主轴号
*   **pMasterType**: 主轴类型,1:跟随编码器2:跟随规划

**`int MC_SetCamEvent(short nAxisNum,short nEvent,double startPara0,double startParal)`**
*   **nAxisNum**: 轴号
*   **nEvent**:
    *   1:立即启动电子齿轮
    *   2:主轴规划或者编码器位置大于等于指定数值时启动凸轮
    *   3:主轴规划或者编码器位置小于等于指定数值时启动凸轮
    *   4:指定IO为ON时启动电子齿轮
    *   5:指定IO为OFF时启动电子齿轮
*   **startPara0**: 事件参数0
    *   nEvent=2或者3时代表编码器或者规划值
    *   nEvent=4或者5时代表IO端口号
*   **startParal**: 保留

**`int MC_GetCamEvent(short nAxisNum,short *pEvent,double *pStartPara0,double *pStartParal)`**
*   **nAxisNum**: 轴号
*   **pEvent**:
    *   1:立即启动电子齿轮
    *   2:主轴规划或者编码器位置大于等于指定数值时启动凸轮
    *   3:主轴规划或者编码器位置小于等于指定数值时启动凸轮
    *   4:指定IO为ON时启动电子齿轮
    *   5:指定IO为OFF时启动电子齿轮
*   **pStartPara0**: 事件参数0
    *   nEvent=2或者3时代表编码器或者规划值
    *   nEvent=4或者5时代表IO端口号
*   **pStartParal**: 保留

**`int MC_SetCamIntervalTime(short nAxisNum,short nIntervalTime)`**
*   **nAxisNum**: 轴号
*   **nIntervalTime**: 平滑时间，单位ms

**`int MC_GetCamIntervalTime(short nAxisNum,short *nIntervalTime)`**
*   **nAxisNum**: 轴号
*   **nIntervalTime**: 平滑时间，单位ms

**`int MC_SetUpCamTable(short nCamTableNum,long lMasterValueMax,long *plSlaveCamData,long lCamTableLen)`**
*   **nCamTableNum**: 表号，1~16
*   **lMasterValueMax**: 一个周期主轴最大值，单位脉冲
*   **plSlaveCamData**: 从轴数据存放指针，数据单位：脉冲
*   **lCamTableLen**: 表长度

**`int MC_DownCamTable(int *pProgress)`**
*   **pProgress**: 下载进度

**`int MC_CamStart(long lMask)`**
*   **LMask**: lMask按位指示需要启动Cam运动的轴号。当bit位为1时表示启动对应的轴Bit7、6、5、4、3、2、1、0对应8轴、7轴、6轴、5轴、4轴、3轴、2轴、1轴

**`int MC_CamStop(long lAxisMask,long lEMGMask)`**
*   **lAxisMask**: lMask按位指示需要停止Cam运动的轴号。当bit位为1时表示启动对应的轴Bit7、6、5、4、3、2、1、0对应8轴、7轴、6轴、5轴、4轴、3轴、2轴、1轴
*   **lEMGMask**: lEMGMask按位指示需要停止Cam运动的轴号。当bit位为1时表示立即停止对应的轴Bit7、6、5、4、3、2、1、0对应8轴、7轴、6轴、5轴、4轴、3轴、2轴、1轴

## 5.13、比较输出(飞拍)类API

### 参数详细说明：

| API | 说明 |
|---|---|
| MC_CmpPluse | 设置比较器输出IO立即输出指定电平或者脉冲 |
| MC_CmpBufSetChannel | 设置比较缓冲区对应输出通道 |
| MC_CmpBufData | 向比较器缓冲区发送比较数据 |
| MC_CmpBufSts | 获取比较器缓冲区状态 |
| MC_CmpBufStop | 停止比较器缓冲区 |
| MC_CmpRpt | 设置比较器缓冲区等比输出 |
| MC_CmpSetTriggerCount | 设置比较器缓冲区触发计数初值 |
| MC_CmpGetTriggerCount | 获取比较器缓冲区触发计数初值 |

**`int MC_CmpPluse(short nChannelMask, short nPluseType1, short nPluseType2, short nTime1,short nTime2, short nTimeFlag1, short nTimeFlag2)`**
*   **nChannel**: bit0 表示通道 1, bit1 表示通道 2
*   **nPluseType1**: 通道 1 输出类型, 0 低电平 1 高电平 2 脉冲
*   **nPluseType2**: 通道 2 输出类型, 0 低电平 1 高电平 2 脉冲
*   **nTime1**: 通道 1 脉冲持续时间
*   **nTime2**: 通道 2 脉冲持续时间
*   **nTimeFlag1**: 比较器 1 的脉冲时间单位, 0:us, 1:ms
*   **nTimeFlag2**: 比较器 2 的脉冲时间单位, 0:us, 1:ms

**`int MC_CmpBufSetChannel(short nBuf1ChannelNum, short nBuf2ChannelNum)`**
*   **nBuf1ChannelNum**: 比较缓冲区 1 对应输出通道号, 默认为通道 1, 可设置为 1 或者 2
*   **nBuf2ChannelNum**: 比较缓冲区 2 对应输出通道号, 默认为通道 2, 可设置为 1 或者 2

**`int MC_CmpBufData(short nCmpEncodeNum, short nPluseType, short nStartLevel, short nTime, long *pBuf1, short nBufLen1, long *pBuf2, short nBufLen2, short nAbsPosFlag=0)`**
*   **nCmpEncodeNum**: 轴号
*   **nPluseType**: 2 表示输出脉冲, 1 表示反转电平。
*   **nStartLevel**: 按位设置比较器输出的初始电平, bit0 比较器 11, bit1 比较器 2; 0: 表示初始为低电平; 1: 表示初始为高电平
*   **nTime**: 输出脉冲时, 该参数用来设定脉冲输出宽度, 取值范围: [1, 65535], 单位: us, 输出电平时, 该参数无效
*   **pBuf1**: 比较器 1 数据缓冲区, 位置值为相对当前位置的距离
*   **nBufLen1**: 比较器 1 数据缓冲区长度, 0~128
*   **pBuf2**: 比较器 2 数据缓冲区, 位置值为相对当前位置的距离
*   **nBufLen2**: 比较器 2 数据缓冲区长度, 0~128
*   **nAbsPosFlag**: 0: 相对当前位置 1: 绝对位置

**`int MC_CmpBufSts(short *pStatus, unsigned short *pCount1, unsigned short *pCount2)`**
*   **pStatus**: 按位指示比较器状态 bit0 代表比较器 1, bit1 代表比较器 2
    *   0 代表板卡比较缓冲区数据已空, 1 代表板卡比较缓冲区数据未完成
*   **pCount1**: 比较器 1 板卡缓冲区剩余待比较数据
*   **pCount2**: 比较器 2 板卡缓冲区剩余待比较数据

**`int MC_CmpBufStop(short nChannel)`**
*   **nChannel**: bit0 表示通道 1, bit1 表示通道 2

**`int MC_CmpRpt(short nChannel, short nRptCount, short nStartLevel, short nTime, long lInterval, short nAbsPosFlag, short nTimeFlag)`**
*   **nChannel**: bit0 表示通道 1, bit1 表示通道 2
*   **nRptCount**: 重复次数
*   **nStartLevel**: 初始电平
*   **nTime**: 脉冲宽度
*   **lInterval**: 间隔
*   **nAbsPosFlag**: 绝对位置标志
*   **nTimeFlag**: 时间单位

**`int MC_CmpSetTriggerCount(short nChannel, unsigned short nTriggerCount)`**
*   **nChannel**: bit0 表示通道 1, bit1 表示通道 2
*   **nTriggerCount**: 触发计数

**`int MC_CmpGetTriggerCount(short nChannel, unsigned short *pTriggerCount)`**
*   **nChannel**: bit0 表示通道 1, bit1 表示通道 2
*   **pTriggerCount**: 触发计数指针

### 示例代码:

```c
int iRes = 0;

//下面代码控制比较输出端口输出高电平（手动测试用）。

//第1个参数为1代表通道1
//第2个参数为1，代表立即输出高电平
//第3个参数预留，固定为1，无意义
//第4个参数代表脉冲时间，这里因为是输出高电平，并非脉冲，所以无意义
//第5个参数为预留，跟第4个参数相同即可
//第6个参数为时间单位，0代表微秒，1代表毫秒（这里因为是输出高电平，所以参数6无意义）
//第7个参数为预留，与第6个参数相同即可
iRes = MC_CmpPluse(1, 1, 1, 100, 100, 0, 0);

//下面代码控制比较输出端口输出低电平（手动测试用）。

//第1个参数为1代表通道1
//第2个参数为0，代表立即输出低电平
//第3个参数预留，固定为1，无意义
//第4个参数代表脉冲时间，这里因为是输出高电平，并非脉冲，所以无意义
//第5个参数为预留，跟第4个参数相同即可
//第6个参数为时间单位，0代表微秒，1代表毫秒（这里因为是输出高电平，所以参数6无意义）
//第7个参数为预留，与第6个参数相同即可
iRes = MC_CmpPluse(1,0,1,100,100,0,0);

//下面代码控制比较输出端口输出一个 200 ms 的脉冲（手动测试用）。

//第1个参数为1代表通道1
//第2个参数为0，代表立即输出低电平
//第3个参数预留，固定为1，无意义
//第 4 个参数代表脉冲时间，这里因为是输出高电平，并非脉冲，所以无意义
//第5个参数为预留，跟第4个参数相同即可
//第 6 个参数为时间单位, 0 代表微秒, 1 代表毫秒
//第7个参数为预留，与第6个参数相同即可
iRes = MC_CmpPluse(1, 2, 1, 200, 200, 1, 1);

//下面代码控制比较输出端口1在相对轴3当前位置为10000、20000、30000、40000、50000时分表输出一个持续时间为10ms的脉冲（自动流程用）。

int iRes = 0;

//定义一个长度为 5 的数组，存储待比较数据点，如果只有一个位置要比较，就把数组长度设为 1
long lBufData[5] = {10000, 20000, 30000, 40000, 50000};

//第1个参数为3代表使用轴3的编码器位置进行比较
//第2个参数为2代表输出类型为脉冲(1代表反转电平，2代表脉冲)
//第3个参数为0代表初始电平为低电平（1代表初始电平为高电平）
//第4个参数为10代表脉冲持续时间为10微秒或者10毫秒（取决于最后一个参数）
//第5个参数代表待比较数据点存放指针
//第6个参数为5代表待比较数据长度为5个数据点
//第7个参数固定为NULL
//第8个参数固定为0
//第9个参数为0代表坐标点为相对坐标（1代表绝对坐标）
//第10个参数为1代表时间单位为毫秒（0代表时间单位为微秒）
iRes = MC_CmpBufData(3,2,0,10,&lBufData[0],5,NULL,0,0,1);
```

## 5.14、自动回零相关 API

| API | 说明 |
|---|---|
| MC_HomeStart | 启动轴回零 |
| MC_HomeStop | 停止轴回零注意,如果回零没有正常完成,必须调用该函数结束回零,否则轴不能运动 |
| MC_HomeSetPrm | 设置回零参数 |
| MC_HomeSetPrmSingle | 设置回零参数(非结构体方式,可以代替MC_HomeSetPrm) |
| MC_HomeGetPrm | 获取回零参数 |
| MC_HomeGetPrmSingle | 获取回零参数(非结构体方式,可以代替MC_HomeGetPrm) |
| MC_HomeGetSts | 获取回零状态 |

### 参数详细说明：

**`int MC_HomeStart(short iAxisNum)`**
*   **nAxisNum**: 需要回零的轴号,取值范围: [1,AXIS_MAX_COUNT]

**`int MC_HomeStop(short iAxisNum)`**
*   **nAxisNum**: 需要停止回零的轴号,取值范围: [1,AXIS_MAX_COUNT]

**`int MC_HomeSetPrrn(short iAxisNum,TAxisHomePrrn *pAxisHomePrrn)`**
*   **nAxisNum**: 需要设置回零参数的轴号,取值范围: [1,AXIS_MAX_COUNT]
*   **pAxisHomePrrn**:
    ```c
    //轴回零参数
    typedef struct _AxisHomeParm{
        short nHomeMode; //1:HOME回原点
        //2:HOME加Index回原点
        //3:Index回零
        //11:正限位回零
        //21:负限位回零
        //31:先找正限位,再负向回零
        //41:先找负限位,再正向回零
        short nHomeDir; //回零方向,1:正向回零,0:负向回零
        long lOffset; //回零偏移,回到零位后再走一个Offset作为零位
        double dHomeRapidVel; //回零快移速度,单位:Pluse/ms
        double dHomeLocatVel; //回零定位速度,单位:Pluse/ms
        double dHomeIndexVel; //回零寻找INDEX速度,单位:Pluse/ms
        double dHomeAcc; //回零使用的加速度,单位Pluse/ms/ms
        long ulHomeIndexDis; //找Index时最大距离,通常为0即可
        long ulHomeBackDis; //回零时,首次碰到零位后回退距离,单位脉冲,通常为0
        unsigned short nDelayTimeBeforeZero; //清零前延时时间,单位ms,通常为0
        unsigned long ulHomeMaxDis; //回零最大寻找范围,单位脉冲,通常给0即可
    }TAxisHomePrrn;
    ```
    *   **回零方式1步骤**:
        1.  快速找零位
        2.  反转离开零位
        3.  再次找零
        4.  移动一个偏移量
        5.  位置清零,回零结束
    *   **回零方式2步骤**:
        1.  快速找零位
        2.  反转离开零位
        3.  再次找零
        4.  寻找Index信号
        5.  移动一个偏移量
        6.  位置清零,回零结束
    *   **回零方式11步骤**:
        1.  快速找正限位
        2.  反转离开正限位
        3.  再次找正限位
        4.  移动一个偏移量
        5.  位置清零,回零结束
    *   **回零方式21步骤**:
        1.  快速找负限位
        2.  反转离开负限位
        3.  再次找负限位
        4.  移动一个偏移量
        5.  位置清零,回零结束
    *   **回零方式31步骤**:
        1.  快速正向找正限位
        2.  快速负向找零位
        3.  反转离开零位
        4.  再次找零位
        5.  移动一个偏移量
        6.  位置清零,回零结束
    *   **回零方式41步骤**:
        1.  快速负向找负限位
        2.  快速正向找零位
        3.  反转离开零位
        4.  再次找零位
        5.  移动一个偏移量
        6.  位置清零,回零结束

**`int MC_homeSetPrmSingle(short iAxisNum,short nHomeMode,short nHomeDir,long lOffset,double dHomeRapidVel,doubledHomeLocatVel,doubledHomeIndexVel,doubledHomeAcc,long lHomeIndexDis,long lHomeBackDis,shortnDelayTimeBeforeZero)`**
*   **nAxisNum**: 需要设置回零参数的轴号,取值范围: [1,AXIS_MAX_COUNT]
*   **nHomeMode**:
    *   1:HOME回原点,此回零方式最常用
    *   2:HOME加Index回原点(仅支持带Index的驱动)
    *   3:Index回原点(仅支持带Index的驱动)
*   **nHomeDir**: 回零方向,1:正向回零,0:负向回零
*   **lOffset**: 回零偏移,回到零位后再走一个Offset作为零位,通常该参数为0
*   **dHomeRapidVel**: 回零快移速度,单位:Pluse/ms
*   **dHomeLocatVel**: 回零定位速度,单位:Pluse/ms
*   **dHomeIndexVel**: 回零寻找INDEX速度,单位:Pluse/ms
*   **dHomeAcc**: 回零使用的加速度,单位Pluse/ms/ms
*   **lHomeIndexDis**: 找Index时最大距离,通常为0即可
*   **lHomeBackDis**: 回零时,首次碰到零位后回退距离,单位脉冲,通常为0
*   **nDelayTimeBeforeZero**: 清零前延时时间,单位ms,通常为0

**`int MC_HomeGetPrm(short iAxisNum,TAxisHomePrm *pAxisHomePrm)`**
*   **nAxisNum**: 需要获取回零参数的轴号,取值范围: [1,AXIS_MAX_COUNT]
*   **pAxisHomePrm**:
    ```c
    //轴回零参数
    typedef struct _AxisHomeParm{
        short nHomeMode; //1--HOME回原点 2--HOME加Index回原点
        short nHomeDir; //回零方向,1-正向回零,0-负向回零
        long lOffset; //回零偏移,回到零位后再走一个Offset作为零位
        double dHomeRapidVel; //回零快移速度,单位:Pluse/ms
        double dHomeLocatVel; //回零定位速度,单位:Pluse/ms
        double dHomeIndexVel; //回零寻找INDEX速度,单位:Pluse/ms
        double dHomeAcc; //回零使用的加速度
    }TAxisHomePrm;
    ```

**`int MC_HomeGetPrmSingle(short iAxisNum,short *nHomeMode,short *nHomeDir,long *lOffset,double *dHomeRapidVel,double *dHomeLocatVel,double *dHomeIndexVel,double *dHomeAcc,long *lHomeIndexDis,long *lHomeBackDis,short *nDelayTimeBeforeZero)`**
*   **nAxisNum**: 需要获取回零参数的轴号,取值范围: [1,AXIS_MAX_COUNT]
*   **nHomeMode**: 回零模式指针
*   **nHomeDir**: 回零方向指针
*   **lOffset**: 回零偏移指针
*   **dHomeRapidVel**: 回零快移速度指针
*   **dHomeLocatVel**: 回零定位速度指针
*   **dHomeIndexVel**: 回零寻找INDEX速度指针
*   **dHomeAcc**: 回零加速度指针
*   **lHomeIndexDis**: 找Index时最大距离指针
*   **lHomeBackDis**: 回零时,首次碰到零位后回退距离指针
*   **nDelayTimeBeforeZero**: 清零前延时时间指针

**`int MC_HomeGetSts(short iAxisNum,short *nStatus)`**
*   **nAxisNum**: 需要获取回零状态的轴号,取值范围: [1,AXIS_MAX_COUNT]
*   **nStatus**: 回零状态指针
    *   0:回零未完成
    *   1:回零成功
    *   2:回零失败

### 示例代码:

```c
int iRes = 0;
short nStatus = 0;
TAXisHomePrm AxisHomePrm;
memset(&AxisHomePrm, 0, sizeof(AxisHomePrm));

AxisHomePrm.nHomeMode = 1; //回零模式为 HOME 回原点
AxisHomePrm.nHomeDir = 0; //回零方向为负向回零
AxisHomePrm.dHomeRapidVel = 5; //回零快移速度，5 脉冲/毫秒
AxisHomePrm.dHomeLocatVel = 1; //回零定位速度，1 脉冲/毫秒
AxisHomePrm.dHomeAcc = 0.5; //回零使用的加速度，0.5 脉冲/毫秒/毫秒
AxisHomePrm.lOffset = 0; //回零偏移

//设置轴1回零参数
iRes = MC_HomeSetPrm(1, &AxisHomePrm);

//启动轴1回零
iRes = MC_HomeStart(1);

//获取轴1回零状态
iRes = MC_HomeGetSts(1, &nStatus);
```

## 5.15、PT模式相关API

| API | 说明 |
|---|---|
| MC_PrfPt | 设置指定轴为PT模式 |
| MC_PtSpace | 读取指定轴的PT缓冲区空闲存储空间 |
| MC_PtRemain | 读取指定轴的PT存储空间尚未执行的数据长度 |
| MC_PtData | 向指定轴的PT缓冲区发送数据 |
| MC_PtClear | 清除指定轴PT缓冲区的数据 |
| MC_PtStart | 启动PT模式运动 |

### 参数详细说明：

**`int MC_PrfPt(short nAxisNum,short mode=PT_MODESTATIC)`**
*   **nAxisNum**: 轴编号,1、2、3、4......
*   **mode**:
    *   PT_MODESTATIC: 静态
    *   PT_MODE_DYNAMIC: 动态

**`int MC_PtSpace(short nAxisNum,long *pSpace,short nCount)`**
*   **nAxisNum**: 轴编号,1、2、3、4......
*   **pSpace**: 空闲存储空间的数据长度存放指针
*   **nCount**: 一次获取的PT空间个数(1~2)

**`int MC_PtRemain(short nAxisNum,long *pRemainSpace,short nCount)`**
*   **nAxisNum**: 轴编号,1、2、3、4......
*   **pRemainSpace**: 尚未执行的数据长度存放指针
*   **nCount**: 一次获取的PT空间个数(1~2)

**`int MC_PtData(short nAxisNum,short* pData,long lLength,double dDataID)`**
*   **nAxisNum**: 轴编号,1、2、3、4......
*   **pData**: 数据存放指针
*   **lLength**: 数据长度
*   **nDataID**: 数据标识,每一包数据都应该和上一包不同,否则会被丢弃1~4循环。

**`int MC_PtClear(long lAxisMask)`**
*   **nAxisNum**: 轴掩码一个位代表一个轴,0X0001代表轴1,0X0007表轴1轴2和轴3

**`int MC_PtStart(long lAxisMask)`**
*   **nAxisNum**: 轴掩码一个位代表一个轴,0X0001代表轴1,0X0007表轴1轴2和轴3

### 示例代码：

```c
int iRes = 0;
int iAxisNum = 1;
int g_dDataID = 1;

//声明一个长度为100的数组
short nData[100];

//设置轴1为PT模式
iRes = MC_PrpfPt(iAxisNum);

//压入PT数据
iRes = MC_PtData(iAxisNum, nData, 100, g_dDataID);
g_dDataID++;

//启动PT运动
MC_PtStart(0X0001 << (iAxesNum-1));
```

## 5.16、手轮相关 API(支持手轮接口的型号可用)

### 参数详细说明：

| API | 说明 |
|---|---|
| MC_GetDiRaw | 获取手轮轴选和倍率IO |
| MC_StartHandwheel | 指定轴开始手轮模式 |
| MC_EndHandwheel | 指定轴结束手轮模式 |

**`int MC_GetDiRaw(short nDiType,long *pValue)`**
*   **nDiType读取手轮IO时,该参数固定为7**: 指定数字IO类型
    *   MC_LIMIT_POSITIVE(0)正限位
    *   MC_LIMIT_NEGATIVE(1)负限位
    *   MC_ALARM(2)驱动报警
    *   MC_HOME(3)原点开关
    *   MC_GPI(4)通用输入
    *   MC_MPG(7)手轮IO输入
*   **pValue**: IO输入值存放指针

**`int MC_StartHandwheel(short nAxisNum,short nMasterAxisNum,long lMasterEven,long lSlaveEven,short nIntervalTime,double dAcc,double dDec,double dVel,short nStopWaitTime)`**
*   **nAxisNum**: 轴编号
*   **nMasterAxisNum**: 跟随轴号,设置为10000
*   **lMasterEven**: 跟随比例系数,可以先设定为1,然后逐渐调整到合适比例。
*   **lSlaveEven**: 跟随比例系数,可以先设定为1,然后逐渐调整到合适比例。
*   **nIntervalTime**: 固定为0
*   **dAcc**: 固定为0.1
*   **dDec**: 固定为0.1
*   **dVel**: 固定为50
*   **nStopWaitTime**: 固定为0

**`int MC_EndHandwheel(short nAxisNum)`**
*   **nAxisNum**: 轴编号

### 重点说明：

1.  手轮接线图参见章节3.2硬件接口说明
2.  手轮X/Y/Z/A/B/X1/X10/X100的IO读取参见“5.3、IO操作API”章节。使用MC_GetDiRaw函数读取。
3.  X100 通常不用接。默认 X1 和 X10 读不到的情况下就是 X100,这样可以节约 IO 端口。
4.  X/Y/Z/A/B/X1/X10/X100 这些 IO 信号读取后，需要自行在代码里面处理哪些轴进入手轮模式以及比例。
5.  假设 X 轴当前在手轮模式, 程序检测到用户切换到 Y 轴了, 那么 Y 轴进入手轮模式时, X 一定要退出手轮模式。
6.  手轮倍率切换时，该轴必须先退出手轮模式才能修改倍率

## 5.17、串口/485相关API(可选项)

| API | 说明 |
|---|---|
| MC_UartConfig | 设置串口波特率、数据长度、校验和、停止位长度等通讯相关参数 |
| MC_SendEthToUartString | 通过以太网转串口发送数据 |
| MC_ReadUartToEthString | 读取串口转以太网数据 |

### 参数详细说明：

**`int MC_UartConfig(unsigned short nUartNum, unsigned long uLBaudRate, unsigned short nDataLength, unsigned short nVerifyType, unsigned short nStopBitLen)`**
*   **nUartNum**: 串口号(485的串口号为3)
*   **uLBaudRate**: 波特率
*   **nDataLength**: 数据长度,7或者8
*   **nVerifyType**: 校验类型,0无校验,1奇校验,2偶校验
*   **nStopBitLen**: 停止位(该参数固定为0,0代表一个停止位)

**`int MC_SendEthToUartString(short nUartNum, unsigned char*pSendBuf, short nLength)`**
*   **nUartNum**: 串口号(485的串口号为3)
*   **pSendBuf**: 待发送数据存储指针
*   **nLength**: 收取到的数据长度,不会大于256,0代表未收到任何数据

**`int MC_ReadUartToEthString(short nUartNum, unsigned char* pRecvBuf, short* pLength)`**
*   **nUartNum**: 串口号(485的串口号为3)
*   **pRecvBuf**: 待收取数据存储指针
*   **pLength**: 收取到的数据长度,不会大于256,0代表未收到任何数据

## 5.18、坐标系跟随相关API(仅高端款支持)

| API | 说明 |
|---|---|
| MC_SetAddAxis | 设置指定轴的累加运动轴。 |

### 参数详细说明：

**`int MC_SetAddAxis(short nAxisNum, short nAddAxisNum)`**
*   **nAxisNum**: 轴号，1~AXIS_MAX_COUNT
*   **nAddAxisNum**: 累加运动轴轴号，1~(AXIS_MAX_COUNT)

坐标系跟随主要用于对运动中产品进行加工。

举例说明：

某产品在拉带上运动，需要对该产品进行加工，但在加工过程中，拉带不能停止。此时就需要用到坐标系跟随功能，需要将拉带的运动补偿到坐标系相关轴当中。

```c
MC_SetAddAxis(2,3); //把轴3运动叠加到轴2上面

MC_SetAddAxis(2,0); //取消轴2的运动叠加
```

# 七、PC端IP配置及多轴板卡并联实现方法

通过多个板卡并联的方案，一台电脑可以控制16轴、32轴、48轴.....1024轴

电脑IP地址需设置为192.168.0.200

板卡出厂默认IP地址为192.168.0.1，通常情况下无需做修改。

打开板卡代码为：`MC_Open(0, "192.168.0.200");`

用户如果需要多个轴板卡同时使用，可以用串口方式打开板卡，修改IP地址。如下图所示：

![](images/5e374f39e5adb6f4aa9303dc540f584e8ce87cd66f9102dbcb85b75a24b0db27.jpg)

例如同时使用3个轴板卡，IP地址分别为192.168.0.1、192.168.0.2、192.168.0.3，则打开板卡代码为：

```c
int iRes = 0;
iRes += MC_SetCardNo(1);
iRes += MC_Open(0, "192.168.0.200");
iRes += MC_SetCardNo(2);
iRes += MC_Open(0, "192.168.0.200");
iRes += MC_SetCardNo(3);
iRes += MC_Open(0, "192.168.0.200");
```

设置板卡1第1个IO输出，然后板卡2第1个IO输出，最后板卡3第1个IO输出代码为：

```c
iRes += MC_SetCardNo(1);
iRes += MC_SetExtDoBit(0, 0, 1);
iRes += MC_SetCardNo(2);
iRes += MC_SetExtDoBit(0, 0, 1);
iRes += MC_SetCardNo(3);
iRes += MC_SetExtDoBit(0, 0, 1);
```

**重点说明：** 如果使用交换机，通常建议板卡IP从192.168.0.2开始。

# 十、附录API一览

| API | 说明 |
|---|---|
| **板卡打开关闭API** | |
| MC_SetCardNo | 切换当前运动控制器卡号 |
| MC_GetCardNo | 读取当前运动控制器卡号 |
| MC_Open | 打开板卡 |
| MC_Reset | 复位板卡 |
| MC_Close | 关闭板卡 |
| **板卡配置类API** | |
| MC_AlarmOn | 设置轴驱动报警信号有效 |
| MC_AlarmOff | 设置轴驱动报警信号无效 |
| MC_AlarmSns | 设置运动控制器轴报警信号电平逻辑 |
| MC_LmtsOn | 设置轴限位信号有效 |
| MC_LmtsOff | 设置轴限位信号无效 |
| MC_LmtSns | 设置运动控制器各轴限位触发电平 |
| MC_EncOn | 设置为“外部编码器”计数方式 |
| MC_EncOff | 设置为“脉冲计数器”计数方式 |
| MC_EncSns | 设置编码器的计数方向 |
| MC_StepSns | 设置脉冲输出通道的方向 |
| **IO操作API** | |
| MC_GetDiRaw | 读取数字IO输入状态的原始值 |
| MC_GetDiReverseCount | 读取数字量输入信号的变化次数 |
| MC_SetDiReverseCount | 设置数字量输入信号的变化次数的初值 |
| MC_SetExtDoValue | 设置IO输出(包含主模块和扩展模块) |
| MC_GetExtDiValue | 获取IO输入(包含主模块和扩展模块) |
| MC_GetExtDoValue | 获取IO输出(包含主模块和扩展模块) |
| MC_SetExtDoBit | 设置指定IO模块的指定位输出(包含主模块和扩展模块) |
| MC_GetExtDiBit | 获取指定IO模块的指定位输入(包含主模块和扩展模块) |
| MC_GetExtDoBit | 获取指定IO模块的指定位输出(包含主模块和扩展模块) |
| MC_SetDac | 设置DAC输出电压 |
| MC_GetAdc | 读取ADC输入电压 |
| MC_SetAdcFilter | 设置模拟量输入滤波时间 |
| MC_SetAdcBias | 设置模拟量输入通道的零漂电压补偿值 |
| MC_GetAdcBias | 读取模拟量输入通道的零漂电压补偿值 |
| MC_SetPwm | 设置PWM输出频率以及占空比 |
| MC_SetDoBitReverse | 设置数字IO输出指定时间的单个脉冲 |
| **轴点位运动API** | |
| MC_PrpfTrap | 设置指定轴为点位模式 |
| MC_SetTrapPrm | 设置点位模式运动参数 |
| MC_SetTrapPrmSingle | 设置点位模式运动参数(可替代MC_SetTrapPrm) |
| MC_GetTrapPrm | 读取点位模式运动参数 |
| MC_GetTrapPrmSingle | 读取点位模式运动参数(可替代MC_GetTrapPrm) |
| MC_SetPos | 设置目标位置 |
| MC_SetVel | 设置目标速度 |
| MC_Update | 启动点位运动 |
| MC_SetTrapPosAndUpdate | 设置指定轴进行点位运动，可代替以上所有函数，效率高 |
| **JOG运动API** | |
| MC_PrfJog | 设置指定轴为JOG模式(速度模式) |
| MC_SetJogPrm | 设置JOG模式运动参数 |
| MC_SetJogPrmSingle | 设置JOG模式运动参数(可替代MC_SetJogPrm) |
| MC_GetJogPrm | 读取JOG模式运动参数 |
| MC_GetJogPrmSingle | 读取JOG模式运动参数(可替代MC_GetJogPrm) |
| MC_SetVel | 设置目标速度 |
| MC_Update | 启动JOG运动 |
| **运动状态检测类API** | |
| MC_AxisOn | 打开驱动器使能 |
| MC_AxisOff | 关闭驱动器使能 |
| MC_Stop | 停止一个或多个轴的规划运动,停止坐标系运动 |
| MC_StopEx | 停止一个或多个轴的规划运动,停止坐标系运动 |
| MC_GetSts | 读取轴状态 |
| MC_ClrSts | 清除驱动器报警标志、跟随误差越限标志、限位触发标志 |
| MC_GetPrfPos | 读取规划位置 |
| MC_GetPrfVel | 读取规划速度 |
| MC_GetAxisEncPos | 读取编码器位值 |
| MC_GetAllSysStatus | 获取所有板卡相关状态 |
| **安全机制API** | |
| MC_SetSoftLimit | 设置软限位 |
| MC_GetSoftLimit | 获取软限位 |
| MC_LmtsOn | 设置轴限位信号有效 |
| MC_LmtsOff | 设置轴限位信号无效 |
| MC_LmtSns | 设置运动控制器各轴限位触发电平 |
| MC_SetLmtSnsSingle | 设置运动控制器指定轴限位触发电平 |
| MC_EStopConfig | 配置急停触发时,模拟量值,IO状态等 |
| MC_EStopSetIO | 设置系统紧急停止IO |
| MC_EStopOnOff | 开启/关闭紧急停止功能 |
| MC_EStopGetSts | 获取紧急停止触发状态 |
| MC_EStopCrlSts | 清除紧急停止触发状态 |
| MC_SetHardLimP(10轴以上用) | 设置正硬限位映射IO |
| MC_SetHardLimN(10轴以上用) | 设置负硬限位映射IO |
| MC_CrdHlimEnable | 坐标系硬限位使能开启/关闭 |
| **其他指令API** | |
| MC_ZeroPos | 清零轴的规划和编码器位置 |
| MC_GetID | 获取板卡唯一标识ID |
| MC_SetAxisBand | 设置轴到位误差带 |
| MC_SetBacklash | 设置反向间隙补偿的相关参数 |
| MC_GetBacklash | 读取反向间隙补偿的相关参数 |
| MC_SetStopDec | 设置平滑停止减速度和急停减速度 |
| MC_WriteInterFlash | 写板卡内部Flash |
| MC_ReadInterFlash | 读板卡内部Flash |
| MC_DownPitchErrorTable | 下载螺距误差补偿表 |
| MC_ReadPitchErrorTable | 读取螺距误差补偿表 |
| MC_AxisErrPitchOn | 打开指定螺距误差补偿 |
| MC_AxisErrPitchOff | 关闭指定轴螺距误差补偿 |
| **插补运动指令API** | |
| MC_SetCrdPrm | 设置坐标系参数,确立坐标系映射,建立坐标系 |
| MC_GetCrdPrm | 查询坐标系参数 |
| MC_InitLookAhead | 配置指定坐标系指定FifoIndex 前瞻缓冲区的拐弯速率,最大加速度,缓冲区深度,缓冲区指针等参数 |
| MC_InitLookAheadSingle | 用于替代MC_InitLookAhead函数,方便不擅长结构体的客户使用。 |
| MC_CrdClear | 清除插补缓存区内的插补数据 |
| MC_LnXY | 缓存区指令,两维直线插补 |
| MC_LnXYZ | 缓存区指令,三维直线插补 |
| MC_LnXYZA | 缓存区指令,四维直线插补 |
| MC_LnXYZAB | 缓存区指令,五维直线插补 |
| MC_LnXYZABC | 缓存区指令,六维直线插补 |
| MC_LnAll | 缓存区指令,7~14维直线插补 |
| MC_ArcXYC | 缓存区指令,XY平面圆弧插补(以终点坐标和圆心位置为输入参数) |
| MC_ArcXZC | 缓存区指令,XZ平面圆弧插补(以终点坐标和圆心位置为输入参数) |
| MC_ArcYZC | 缓存区指令,YZ平面圆弧插补(以终点坐标和圆心位置为输入参数) |
| MC_HelixXYCZ | 缓存区指令,XY平面螺旋线插补(以终点坐标和圆心位置为输入参数) |
| MC_HelixXZCY | 缓存区指令,XZ平面螺旋线插补(以终点坐标和圆心位置为输入参数) |
| MC_HelixYZCX | 缓存区指令,YZ平面螺旋线插补(以终点坐标和圆心位置为输入参数) |
| MC_HelixXYCount | 缓存区指令,XY平面螺旋线插补(以终点坐标和圆心位置为输入参数) |
| MC_HelixXZCount | 缓存区指令,XZ平面螺旋线插补(以终点坐标和圆心位置为输入参数) |
| MC_HelixYZCount | 缓存区指令,YZ平面螺旋线插补(以终点坐标和圆心位置为输入参数) |
| MC_BufPWM | 缓存区指令,设置PWM频率及占空比 |
| MC_BufIO | 缓存区指令,设置IO输出 |
| MC_BufIOReverse | 缓存区指令,设置IO输出一个指定时间的脉冲 |
| MC_BufDelay | 缓存区指令,延时一段时间 |
| MC_BufMoveVel | 在插补运动的过程中插入BufferMove轴的速度设定 |
| MC_BufMoveAcc | 在插补运动的过程中插入BufferMove轴的加速度设定 |
| MC_BufMove | 在插补运动的过程中插入阻塞和非阻塞的点位运动 |
| MC_BufGear | 设定了脉冲输出的个数。它会保证与其后紧挨的指令同时启动,同时停止 |
| MC_CrdData | 向插补缓存区增加插补数据 |
| MC_CrdStart | 启动插补运动 |
| MC_SetOverride | 在运动中修改坐标系运动的合成速度的倍率 |
| MC_ResetOverride | 复位坐标系运动的合成速度的倍率 |
| MC_GetCrdSts | 读取坐标系状态 |
| MC_GetCrdPos | 读取坐标系当前位置 |
| MC_GetCrdVel | 读取坐标系当前速度 |
| MC_GetFifoSpace | 读取坐标系FIFO的剩余空间 |
| MC_GetSegNum | 读取当前坐标系正在执行的程序段号 |
| **硬件捕获类API** | |
| MC_SetCaptureMode | 设置编码器捕获方式，并启动捕获 |
| MC_GetCaptureMode | 读取编码器捕获方式 |
| MC_GetCaptureStatus | 读取编码器捕获状态 |
| MC_SetCaptureSense | 设置捕获电平 |
| MC_GetCaptureSense | 获取捕获电平 |
| MC_ClearCaptureStatus | 清除捕获状态 |
| **Gear/电子齿轮类API** | |
| MC_PrfGear | 设置指定轴进入电子齿轮模式 |
| MC_SetGearMaster | 设置电子齿轮运动跟随主轴 |
| MC_GetGearMaster | 读取电子齿轮运动跟随主轴 |
| MC_SetGearRatio | 设置电子齿轮比 |
| MC_GetGearRatio | 获取电子齿轮比 |
| MC_GearStart | 启动电子齿轮运动 |
| MC_GearStop | 停止电子齿轮运动 |
| MC_SetGearEvent | 设置电子齿轮触发事件 |
| MC_GetGearEvent | 获取电子齿轮触发事件 |
| MC_SetGearIntervalTime | 设置电子齿轮平滑系数 |
| MC_GetGearIntervalTime | 获取电子齿轮平滑系数 |
| **电子凸轮类API** | |
| MC_PrfCam | 设置指定轴进入电子凸轮模式 |
| MC_SetCamMaster | 设置电子凸轮运动跟随主轴 |
| MC_GetCamMaster | 读取电子凸轮运动跟随主轴 |
| MC_SetCamEvent | 设置电子凸轮触发事件 |
| MC_GetCamEvent | 获取电子凸轮触发事件 |
| MC_SetCamIntervalTime | 设置电子凸轮均值滤波时间 |
| MC_GetCamIntervalTime | 获取电子凸轮均值滤波时间 |
| MC_SetUpCamTable | 建立凸轮表 |
| MC_DownCamTable | 下载电子凸轮表到控制器 |
| MC_CamStart | 启动电子凸轮运动 |
| MC_CamStop | 停止电子凸轮运动 |
| **比较输出(飞拍)类API** | |
| MC_CmpPluse | 设置比较器输出IO立即输出指定电平或者脉冲 |
| MC_CmpBufSetChannel | 设置比较缓冲区对应输出通道 |
| MC_CmpBufData | 向比较器缓冲区发送比较数据 |
| MC_CmpBufSts | 获取比较器缓冲区状态 |
| MC_CmpBufStop | 停止比较器缓冲区 |
| MC_CmpRpt | 设置比较器缓冲区等比输出 |
| MC_CmpSetTriggerCount | 设置比较器缓冲区触发计数初值 |
| MC_CmpGetTriggerCount | 获取比较器缓冲区触发计数初值 |
| **自动回零相关 API** | |
| MC_HomeStart | 启动轴回零 |
| MC_HomeStop | 停止轴回零 |
| MC_HomeSetPrm | 设置回零参数 |
| MC_HomeSetPrmSingle | 设置回零参数(非结构体方式,可以代替MC_HomeSetPrm) |
| MC_HomeGetPrm | 获取回零参数 |
| MC_HomeGetPrmSingle | 获取回零参数(非结构体方式,可以代替MC_HomeGetPrm) |
| MC_HomeGetSts | 获取回零状态 |
| **PT模式相关API** | |
| MC_PrfPt | 设置指定轴为PT模式 |
| MC_PtSpace | 读取指定轴的PT缓冲区空闲存储空间 |
| MC_PtRemain | 读取指定轴的PT存储空间尚未执行的数据长度 |
| MC_PtData | 向指定轴的PT缓冲区发送数据 |
| MC_PtClear | 清除指定轴PT缓冲区的数据 |
| MC_PtStart | 启动PT模式运动 |
| **手轮相关 API** | |
| MC_GetDiRaw | 获取手轮轴选和倍率IO |
| MC_StartHandwheel | 指定轴开始手轮模式 |
| MC_EndHandwheel | 指定轴结束手轮模式 |
| **串口/485相关API** | |
| MC_UartConfig | 设置串口波特率、数据长度、校验和、停止位长度等通讯相关参数 |
| MC_SendEthToUartString | 通过以太网转串口发送数据 |
| MC_ReadUartToEthString | 读取串口转以太网数据 |
| **坐标系跟随相关API** | |
| MC_SetAddAxis | 设置指定轴的累加运动轴。 |
| **机械臂操作类API** | |
| MC_RobotSetPrm | 设置机械手参数 |
| MC_RobotSetPrmDelta40001 | 设置 Delta 机械手 40001 类型参数专用函数 |
| MC_RobotSetPrmScara40101 | 设置 Scara 机械手 40101 类型参数专用函数 |
| MC_RobotSetPrmPolar50101 | 设置极坐标机械手 50101 类型参数专用函数 |
| MC_RobotSetPrmSixArm60001 | 设置标准 6 轴机械臂参数专用函数 |
| MC_RobotSetForward | 设置机械手进入关节运动 |
| MC_RobotSetInverse | 设置机械手进入坐标系运动 |
