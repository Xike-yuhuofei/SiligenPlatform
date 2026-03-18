# MC_CmpPluse函数教程

## 函数原型
```c
int MC_CmpPluse(short nChannelMask, short nPluseType1, short nPluseType2,
                short nTime1, short nTime2, short nTimeFlag1, short nTimeFlag2)
```

## 参数说明
| 参数 | 说明 |
|------|------|
| nChannelMask | 通道掩码(1=通道1, 2=通道2, 3=双通道) |
| nPluseType1 | 通道1类型(0=低电平, 1=高电平, 2=脉冲) |
| nPluseType2 | 通道2类型(0=低电平, 1=高电平, 2=脉冲) |
| nTime1 | 通道1脉冲宽度(脉冲时有效) |
| nTime2 | 通道2脉冲宽度(脉冲时有效) |
| nTimeFlag1 | 通道1时间单位(0=μs, 1=ms) |
| nTimeFlag2 | 通道2时间单位(0=μs, 1=ms) |

## 返回值
- 0: 成功
- 非0: 失败

## 实际代码示例

### 1. 输出高电平
```cpp
g_MultiCard.MC_CmpPluse(1,1,1,0,0,0,0);
```

### 2. 输出低电平
```cpp
g_MultiCard.MC_CmpPluse(1,0,0,0,0,0,0);
```

### 3. 输出脉冲
```cpp
g_MultiCard.MC_CmpPluse(1,2,0,1000,0,comboBoxUnit,0);
```

## 参数映射表
| 用途 | nChannelMask | nPluseType1 | nTime1 | nTimeFlag1 |
|------|-------------|-------------|--------|------------|
| 高电平 | 1 | 1 | 0 | 0 |
| 低电平 | 1 | 0 | 0 | 0 |
| 脉冲 | 1 | 2 | 宽度值 | 0(μs)/1(ms) |

## 关键说明
- 电平输出时，时间参数无意义
- 脉冲输出时，必须设置有效时间参数
- nPluseType2,nTime2,nTimeFlag2为预留参数
- 短脉冲建议用μs单位获得更高精度

## 函数区别
- MC_CmpPluse: 立即输出
- MC_CmpBufData: 位置比较输出
- MC_BufCmpPluse: 插补运动输出