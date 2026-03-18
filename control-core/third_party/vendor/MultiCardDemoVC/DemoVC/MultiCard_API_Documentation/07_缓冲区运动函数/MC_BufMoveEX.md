# MC_BufMoveEX

## 功能

在缓冲区中插入一个直线插补运动。

## 函数原型

```cpp
short __stdcall MC_BufMoveEX(short ConnectNo, short Crd, double Pos1, double Pos2, double Pos3, double Pos4, double Pos5, double Pos6, double Vel, short VelEndZero);
```

## 参数

- `ConnectNo`：连接号。
- `Crd`：坐标系号。
- `Pos1`~`Pos6`：各轴终点位置。
- `Vel`：合成速度。
- `VelEndZero`：终点速度是否为0，0-否，1-是。

## 返回值

- `0`：成功。
- `非0`：失败，返回错误码。
