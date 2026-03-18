# MC_BufMoveVelEX

## 功能

在缓冲区中插入一个速度运动。

## 函数原型

```cpp
short __stdcall MC_BufMoveVelEX(short ConnectNo, short Crd, double Vel1, double Vel2, double Vel3, double Vel4, double Vel5, double Vel6, double Dist, short VelEndZero);
```

## 参数

- `ConnectNo`：连接号。
- `Crd`：坐标系号。
- `Vel1`~`Vel6`：各轴速度。
- `Dist`：运动距离。
- `VelEndZero`：终点速度是否为0，0-否，1-是。

## 返回值

- `0`：成功。
- `非0`：失败，返回错误码。
