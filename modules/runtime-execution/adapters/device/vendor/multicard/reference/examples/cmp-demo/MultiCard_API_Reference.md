# MC_Open教程

## 函数原型
```c
int MC_Open(short nCardNum, char* cPCEthernetIP, unsigned short nPCEthernetPort, char* cCardEthernetIP, unsigned short nCardEthernetPort);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCardNum` | 控制卡物理编号，从1开始。 |
| `cPCEthernetIP` | PC主机的IP地址字符串。 |
| `nPCEthernetPort` | PC主机的端口号。 |
| `cCardEthernetIP` | 控制卡的IP地址字符串。 |
| `nCardEthernetPort`| 控制卡的端口号。 |

## 返回值
- `0`: 成功。
- 非`0`: 失败，例如 `-6` 代表打开失败。

## 实际代码示例

### 1. 打开单个控制卡连接
```cpp
// 5个参数依次为板卡号、PC端IP地址、PC端端口号，板卡端IP地址，板卡端口号。
iRes = g_MultiCard.MC_Open(1,"192.168.0.200",60000,"192.168.0.1",60000);
if(iRes)
{
    MessageBox("Open Card Fail,Please turn off wifi ,check PC IP address or connection!");
}
```

### 2. 打开多个控制卡连接（示例）
```cpp
// iRes = g_MultiCard2.MC_Open(2,"192.168.0.200",60002,"192.168.0.2",60002);
// iRes = g_MultiCard3.MC_Open(3,"192.168.0.200",60002,"192.168.0.3",60002);
```

## 关键说明
- PC端端口号和板卡端口号必须保持一致。
- 连接多个板卡时，建议为每个板卡设置不同的IP地址和端口号。
- 连接失败时，请检查Wi-Fi是否关闭、PC与板卡的IP地址配置以及物理连接是否正常。

---

# MC_Close教程

## 函数原型
```c
int MC_Close(void);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| 无 | |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 关闭连接
```cpp
// 在退出程序或需要断开连接时调用
g_MultiCard.MC_Close();
```

## 关键说明
- 此函数用于断开与通过 `MC_Open` 建立的控制卡连接。
- 每个 `MultiCard` 对象实例对应一个连接，此函数只关闭调用它的对象的连接。

---

# MC_Reset教程

## 函数原型
```c
int MC_Reset();
```

## 参数说明
| 参数 | 说明 |
|------|------|
| 无 | |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 打开连接后复位控制卡
```cpp
iRes = g_MultiCard.MC_Open(1,"192.168.0.200",60000,"192.168.0.1",60000);
if(iRes == 0)
{
    g_MultiCard.MC_Reset();
    MessageBox("Open Card Successful!");
}
```

### 2. 通过UI按钮复位控制卡
```cpp
void CDemoVCDlg::OnBnClickedButton8()
{
	g_MultiCard.MC_Reset();
}
```

## 关键说明
- `MC_Reset` 通常在成功打开板卡连接后立即调用，以确保板卡处于已知的初始状态。
- 该函数会停止所有轴的运动并清除部分状态。

---

# MC_GetAllSysStatusSX教程

## 函数原型
```c
int MC_GetAllSysStatusSX(TAllSysStatusDataSX *pAllSysStatusData);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `pAllSysStatusData` | 指向 `TAllSysStatusDataSX` 结构体的指针，用于存储返回的系统状态数据。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 在定时器中循环读取状态
```cpp
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
	TAllSysStatusDataSX m_AllSysStatusDataTemp;
	//读取板卡状态数据。
	iRes = g_MultiCard.MC_GetAllSysStatusSX(&m_AllSysStatusDataTemp);

    // 更新规划位置
	strText.Format("%d",m_AllSysStatusDataTemp.lAxisPrfPos[iAxisNum-1]);
	GetDlgItem(IDC_STATIC_PRF_POS)->SetWindowText(strText);

    // 更新编码器位置
	strText.Format("%d",m_AllSysStatusDataTemp.lAxisEncPos[iAxisNum-1]);
	GetDlgItem(IDC_STATIC_ENC_POS)->SetWindowText(strText);
    
    // 更新轴状态
    if(m_AllSysStatusDataTemp.lAxisStatus[iAxisNum-1] & AXIS_STATUS_RUNNING)
	{
		((CButton*)GetDlgItem(IDC_RADIO_RUNNING))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_RUNNING))->SetCheck(false);
	}
}
```

## 关键说明
- 该函数用于一次性获取控制卡上所有轴、I/O、坐标系等的完整状态。
- 在UI程序中，通常在定时器中以固定频率（如100ms）调用此函数，以实时刷新界面显示。
- `TAllSysStatusDataSX` 结构体包含了丰富的信息，如轴规划位置、编码器位置、轴状态、通用输入/输出等。

---

# MC_ZeroPos教程

## 函数原型
```c
int MC_ZeroPos(short nAxisNum, short nCount=1);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 轴号，从1开始。 |
| `nCount` | 要操作的轴数量，默认为1。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 将指定轴的规划位置和编码器位置清零
```cpp
void CDemoVCDlg::OnBnClickedButton4()
{
	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;
	g_MultiCard.MC_ZeroPos(iAxisNum);
}
```

## 关键说明
- 此函数将指定轴的规划位置（PrfPos）和编码器位置（EncPos）同时设置为0。
- 常用于将当前物理位置定义为新的坐标原点。

---

# MC_ClrSts教程

## 函数原型
```c
int MC_ClrSts(short nAxisNum, short nCount=1);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 轴号，从1开始。 |
| `nCount` | 要操作的轴数量，默认为1。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 清除指定轴的报警状态
```cpp
void CDemoVCDlg::OnBnClickedButtonClrSts()
{
	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;
	iRes = g_MultiCard.MC_ClrSts(iAxisNum);
}
```

## 关键说明
- 此函数用于清除轴的报警状态和其他一些错误标志。
- 当驱动器发生报警（如 `AXIS_STATUS_SV_ALARM` 标志位置位）后，在排除物理故障后，需要调用此函数来清除软件中的报警状态，然后才能重新使能和运动。

---

# MC_AxisOn教程

## 函数原型
```c
int MC_AxisOn(short nAxisNum);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 轴号，从1开始。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 使能指定轴
```cpp
void CDemoVCDlg::OnBnClickedButtonAxisOn()
{
	CString strText;
	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

	GetDlgItemText(IDC_BUTTON_AXIS_ON,strText);
	if (strText == _T("伺服使能"))
	{
		iRes = g_MultiCard.MC_AxisOn(iAxisNum);
		GetDlgItem(IDC_BUTTON_AXIS_ON)->SetWindowText(_T("断开使能"));
	}
    // ...
}
```

## 关键说明
- 使能轴是进行任何运动操作的前提。
- 调用此函数后，伺服驱动器将上电并锁定轴的位置。

---

# MC_AxisOff教程

## 函数原型
```c
int MC_AxisOff(short nAxisNum);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 轴号，从1开始。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 断开指定轴的使能
```cpp
void CDemoVCDlg::OnBnClickedButtonAxisOn()
{
	CString strText;
	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

	GetDlgItemText(IDC_BUTTON_AXIS_ON,strText);
	if (strText == _T("伺服使能"))
	{
        // ...
	}
	else
	{
		iRes = g_MultiCard.MC_AxisOff(iAxisNum);
		GetDlgItem(IDC_BUTTON_AXIS_ON)->SetWindowText(_T("伺服使能"));
	}
}
```

## 关键说明
- 此函数会断开伺服驱动器的使能，轴将不再被锁定，可以被外力自由移动。

---

# MC_LmtsOn教程

## 函数原型
```c
int MC_LmtsOn(short nAxisNum, short limitType=-1);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 轴号，从1开始。 |
| `limitType`| 限位类型。`MC_LIMIT_POSITIVE` (正限位), `MC_LIMIT_NEGATIVE` (负限位), `-1` (正负限位同时)。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 开启正限位
```cpp
void CDemoVCDlg::OnBnClickedCheckPosLimEnable()
{
	if(((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->GetCheck())
	{
		iRes = g_MultiCard.MC_LmtsOn(iAxisNum,MC_LIMIT_POSITIVE);
	}
    // ...
}
```

### 2. 开启负限位
```cpp
void CDemoVCDlg::OnBnClickedCheckNegLimEnable()
{
	if(((CButton*)GetDlgItem(IDC_CHECK_NEG_LIM_ENABLE))->GetCheck())
	{
		iRes = g_MultiCard.MC_LmtsOn(iAxisNum,MC_LIMIT_NEGATIVE);
	}
    // ...
}
```

## 关键说明
- 开启硬件限位功能。当轴运动触发硬件限位开关时，会立即停止运动，防止设备损坏。

---

# MC_LmtsOff教程

## 函数原型
```c
int MC_LmtsOff(short nAxisNum, short limitType=-1);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 轴号，从1开始。 |
| `limitType`| 限位类型。`MC_LIMIT_POSITIVE` (正限位), `MC_LIMIT_NEGATIVE` (负限位), `-1` (正负限位同时)。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 关闭正限位
```cpp
void CDemoVCDlg::OnBnClickedCheckPosLimEnable()
{
    // ...
	else
	{
		iRes = g_MultiCard.MC_LmtsOff(iAxisNum,MC_LIMIT_POSITIVE);
	}
}
```

## 关键说明
- 关闭硬件限位功能。在某些特殊操作（如回零）中可能需要临时关闭限位。

---

# MC_PrfTrap教程

## 函数原型
```c
int MC_PrfTrap(short nAxisNum);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 轴号，从1开始。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 设置轴为点位运动模式
```cpp
void CDemoVCDlg::OnBnClickedButton6() // 定位运动
{
    // ...
	iRes = g_MultiCard.MC_PrfTrap(iAxisNum);
    // ...
}
```

## 关键说明
- 将指定轴的工作模式设置为梯形速度曲线的点位运动（Trap Profile）。
- 这是执行 `MC_SetPos` 定位运动的前提。

---

# MC_SetTrapPrm教程

## 函数原型
```c
int MC_SetTrapPrm(short nAxisNum, TTrapPrm *pPrm);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 轴号，从1开始。 |
| `pPrm` | 指向 `TTrapPrm` 结构体的指针。 |

## `TTrapPrm` 结构体
| 成员 | 说明 |
|------|------|
| `acc` | 加速度 |
| `dec` | 减速度 |
| `velStart` | 起始速度 |
| `smoothTime`| 平滑时间 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 设置点位运动参数
```cpp
void CDemoVCDlg::OnBnClickedButton6() // 定位运动
{
	TTrapPrm TrapPrm;

	TrapPrm.acc = 0.1;
	TrapPrm.dec = 0.1;
	TrapPrm.smoothTime = 0;
	TrapPrm.velStart = 0;

	iRes = g_MultiCard.MC_PrfTrap(iAxisNum);
	iRes += g_MultiCard.MC_SetTrapPrm(iAxisNum,&TrapPrm);
    // ...
}
```

## 关键说明
- 设置点位运动的速度曲线参数。必须在 `MC_PrfTrap` 之后，`MC_Update` 之前调用。

---

# MC_SetPos教程

## 函数原型
```c
int MC_SetPos(short nAxisNum, long pos);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 轴号，从1开始。 |
| `pos` | 目标位置，单位为脉冲。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 设置单轴目标位置
```cpp
void CDemoVCDlg::OnBnClickedButton6() // 定位运动
{
    UpdateData(true); // 获取UI上的 m_lTargetPos 值
    // ...
	iRes += g_MultiCard.MC_SetPos(iAxisNum,m_lTargetPos);
    // ...
	iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));
}
```

## 关键说明
- 设置点位运动的目标位置。
- 设置后需要调用 `MC_Update` 来启动运动。

---

# MC_SetVel教程

## 函数原型
```c
int MC_SetVel(short nAxisNum, double vel);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 轴号，从1开始。 |
| `vel` | 目标速度。单位为脉冲/ms。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 设置定位运动的速度
```cpp
void CDemoVCDlg::OnBnClickedButton6() // 定位运动
{
    // ...
	iRes = g_MultiCard.MC_SetVel(iAxisNum,5);
	iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));
}
```

### 2. 设置JOG运动的速度
```cpp
// in CDemoVCDlg::PreTranslateMessage
// ...
iRes += g_MultiCard.MC_SetVel(iAxisNum,-20); // 负向
iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));
```

## 关键说明
- 用于设置点位运动或JOG运动的速度。
- 速度值为正时正向运动，为负时反向运动。
- 设置后需要调用 `MC_Update` 来启动运动。

---

# MC_Update教程

## 函数原型
```c
int MC_Update(long mask);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `mask` | 轴掩码，指定哪些轴的运动指令需要被启动。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 启动单个轴的运动
```cpp
void CDemoVCDlg::OnBnClickedButton6() // 定位运动
{
    int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;
    // ... 设置模式、参数、速度、位置 ...
	iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));
}
```

## 参数映射表
| 用途 | `mask` |
|------|-------------|
| 启动轴1 | `0x0001` |
| 启动轴2 | `0x0002` |
| 启动轴3 | `0x0004` |
| 启动轴1和轴2 | `0x0003` |

## 关键说明
- `MC_Update` 是启动单轴运动（点位、JOG）的“点火”指令。
- 在调用 `MC_PrfTrap` / `MC_PrfJog`，并设置好速度/位置等参数后，调用此函数，轴才会开始运动。
- 参数 `mask` 是一个位掩码，每一位对应一个轴。例如 `(1 << (nAxisNum - 1))` 是一个常用的方法来生成单个轴的掩码。

---

# MC_PrfJog教程

## 函数原型
```c
int MC_PrfJog(short nAxisNum);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 轴号，从1开始。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 设置轴为JOG模式
```cpp
// in CDemoVCDlg::PreTranslateMessage
// ...
iRes = g_MultiCard.MC_PrfJog(iAxisNum);
// ...
```

## 关键说明
- 将指定轴的工作模式设置为JOG（点动）模式。
- 在JOG模式下，轴会以设定的速度持续运动，直到接收到停止指令。

---

# MC_SetJogPrm教程

## 函数原型
```c
int MC_SetJogPrm(short nAxisNum, TJogPrm *pPrm);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 轴号，从1开始。 |
| `pPrm` | 指向 `TJogPrm` 结构体的指针。 |

## `TJogPrm` 结构体
| 成员 | 说明 |
|------|------|
| `dAcc` | 加速度 |
| `dDec` | 减速度 |
| `dSmooth`| 平滑度 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 设置JOG运动参数
```cpp
// in CDemoVCDlg::PreTranslateMessage
// ...
TJogPrm m_JogPrm;
m_JogPrm.dAcc = 0.1;
m_JogPrm.dDec = 0.1;
m_JogPrm.dSmooth = 0;

iRes = g_MultiCard.MC_PrfJog(iAxisNum);
iRes += g_MultiCard.MC_SetJogPrm(iAxisNum,&m_JogPrm);
// ...
```

## 关键说明
- 设置JOG运动的速度曲线参数。必须在 `MC_PrfJog` 之后，`MC_Update` 之前调用。

---

# MC_Stop教程

## 函数原型
```c
int MC_Stop(long lMask, long lOption);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `lMask` | 轴掩码或坐标系掩码。 |
| `lOption`| 停止选项掩码（未使用，通常与lMask相同）。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 停止所有轴和坐标系的运动
```cpp
void CDemoVCDlg::OnBnClickedButton3() // 停止按钮
{
	g_MultiCard.MC_Stop(0XFFFFFFFF,0XFFFFFFFF);
}
```

### 2. 停止JOG运动
```cpp
// in CDemoVCDlg::PreTranslateMessage
// ...
case WM_LBUTTONUP:
    if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_NEG)->m_hWnd)
    {
        g_MultiCard.MC_Stop(0XFFFF,0XFFFF);
    }
// ...
```

## 关键说明
- 一个通用的停止指令，可以用来停止单轴运动和多轴插补运动。
- `lMask` 是一个位掩码，可以指定停止单个或多个轴/坐标系。使用 `0xFFFFFFFF` 或 `0xFFFF` 通常意味着停止所有活动。

---

# MC_SetCrdPrm教程

## 函数原型
```c
int MC_SetCrdPrm(short nCrdNum, TCrdPrm *pCrdPrm);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号，从1开始。 |
| `pCrdPrm` | 指向 `TCrdPrm` 结构体的指针。 |

## `TCrdPrm` 结构体
| 成员 | 说明 |
|------|------|
| `dimension` | 坐标系维数。 |
| `profile` | 关联的轴号数组。 |
| `synVelMax` | 最大合成速度。 |
| `synAccMax` | 最大合成加速度。 |
| `originPos` | 坐标系原点位置。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 建立一个二维坐标系
```cpp
void CDemoVCDlg::OnBnClickedButtonSetCrdPrm()
{
	TCrdPrm crdPrm;
	memset(&crdPrm,0,sizeof(crdPrm));

	crdPrm.dimension = 2;
	crdPrm.profile[0] = 1;
	crdPrm.profile[1] = 2;
	crdPrm.synAccMax = 0.2;
	crdPrm.synVelMax = 1000;
	crdPrm.setOriginFlag = 1;
	crdPrm.originPos[0] = m_lOrginX;
	crdPrm.originPos[1] = m_lOrginY;

	iRes = g_MultiCard.MC_SetCrdPrm(1,&crdPrm);
}
```

## 关键说明
- `MC_SetCrdPrm` 用于定义一个坐标系，包括其包含的轴、运动学属性和原点。
- 这是执行任何多轴插补运动（如直线、圆弧）的第一步。

---

# MC_CrdStart教程

## 函数原型
```c
int MC_CrdStart(short mask, short option);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `mask` | 坐标系掩码，指定要启动的坐标系。 |
| `option` | 启动选项，通常为0。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 启动坐标系1的运动
```cpp
void CDemoVCDlg::OnBnClickedButton11()
{
	int iRes = 0;
	iRes = g_MultiCard.MC_CrdStart(1,0);

	if(0 == iRes)
	{
		AfxMessageBox("启动坐标系成功！");
	}
}
```

## 参数映射表
| 用途 | `mask` |
|------|-------------|
| 启动坐标系1 | `1` (或 `0x01`) |
| 启动坐标系2 | `2` (或 `0x02`) |
| 启动坐标系1和2 | `3` (或 `0x03`) |

## 关键说明
- 在向缓冲区中添加了运动指令（如 `MC_LnXY`）后，调用此函数来启动插补运动。

---

# MC_LnXY教程

## 函数原型
```c
int MC_LnXY(short nCrdNum, long x, long y, double synVel, double synAcc, double velEnd=0, short FifoIndex=0, long segNum=0);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `x` | X轴目标位置。 |
| `y` | Y轴目标位置。 |
| `synVel` | 合成速度。 |
| `synAcc` | 合成加速度。 |
| `velEnd` | 段末速度，默认为0。 |
| `FifoIndex`| 缓冲区索引，默认为0。 |
| `segNum` | 用户自定义段号，默认为0。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 向缓冲区中添加一条二维直线插补指令
```cpp
void CDemoVCDlg::OnBnClickedButton10()
{
    // ...
	iRes = g_MultiCard.MC_LnXY(1,100000,100000,50,2,0,0,1);
	iRes = g_MultiCard.MC_LnXY(1,0,0,50,2,0,0,1);
    // ...
	g_MultiCard.MC_CrdStart(1,0);
}
```

## 关键说明
- 此函数向插补缓冲区中添加一条二维直线运动指令。
- 指令被添加到缓冲区后不会立即执行，需要调用 `MC_CrdStart` 来启动整个缓冲区任务。

---

# MC_ArcXYC教程

## 函数原型
```c
int MC_ArcXYC(short nCrdNum, long x, long y, double xCenter, double yCenter, short circleDir, double synVel, double synAcc, double velEnd=0, short FifoIndex=0, long segNum=0);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `x`, `y` | 圆弧终点坐标。 |
| `xCenter`, `yCenter` | 圆心相对于起点的坐标。 |
| `circleDir` | 圆弧方向（0: 顺时针, 1: 逆时针）。 |
| `synVel` | 合成速度。 |
| `synAcc` | 合成加速度。 |
| `velEnd` | 段末速度，默认为0。 |
| `FifoIndex`| 缓冲区索引，默认为0。 |
| `segNum` | 用户自定义段号，默认为0。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 添加一条XY平面顺时针圆弧
```cpp
void CDemoVCDlg::OnBnClickedButton10()
{
    // ...
	iRes = g_MultiCard.MC_ArcXYC(1,15000,12000,0,5000,0,50,0.2,0,0,3);
    // ...
}
```

## 关键说明
- 向插补缓冲区添加一条XY平面的圆弧指令。
- 圆心坐标是相对于当前点的相对值。

---

# MC_CrdSpace教程

## 函数原型
```c
int MC_CrdSpace(short nCrdNum, long *pSpace, short FifoIndex=0);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `pSpace` | 指向长整型变量的指针，用于接收缓冲区剩余空间大小。 |
| `FifoIndex`| 缓冲区索引，默认为0。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 在定时器中获取插补缓冲区剩余空间
```cpp
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    long lCrdSpace = 0;
    //获取坐标系剩余空间
	iRes = g_MultiCard.MC_CrdSpace(1,&lCrdSpace,0);

	strText.Format("板卡插补缓冲区剩余空间：%d",lCrdSpace);
	GetDlgItem(IDC_STATIC_CRD_SPACE)->SetWindowText(strText);
}
```

## 关键说明
- 在发送大量插补数据时，可以通过此函数判断缓冲区是否已满，以实现流量控制。

---

# MC_GetCrdPos教程

## 函数原型
```c
int MC_GetCrdPos(short nCrdNum, double *pPos);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `pPos` | 指向double数组的指针，用于接收坐标系各轴的当前位置。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 在定时器中获取坐标系XYZ的位置
```cpp
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    double dPos[8];
	iRes = g_MultiCard.MC_GetCrdPos(1,dPos);

	strText.Format("X:%.3f",dPos[0]);
	GetDlgItem(IDC_STATIC_POS_x)->SetWindowText(strText);

	strText.Format("Y:%.3f",dPos[1]);
	GetDlgItem(IDC_STATIC_POS_Y)->SetWindowText(strText);

	strText.Format("Z:%.3f",dPos[2]);
	GetDlgItem(IDC_STATIC_POS_Z)->SetWindowText(strText);
}
```

## 关键说明
- 用于实时监控插补运动过程中，坐标系在工件坐标系下的位置。

---

# MC_CrdStatus教程

## 函数原型
```c
int MC_CrdStatus(short nCrdNum, short *pCrdStatus, long *pSegment, short FifoIndex=0);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `pCrdStatus` | 指向short变量的指针，用于接收坐标系状态。 |
| `pSegment` | (可选) 指向长整型变量的指针，用于接收当前运行的段号。 |
| `FifoIndex`| 缓冲区索引，默认为0。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 在定时器中获取坐标系运行状态
```cpp
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    short nCrdStatus = 0;
	iRes = g_MultiCard.MC_CrdStatus(1,&nCrdStatus,NULL);

	//坐标系运行状态显示
	if(nCrdStatus & CRDSYS_STATUS_PROG_RUN)
	{
		((CButton*)GetDlgItem(IDC_RADIO_CRD_RUNNING))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_CRD_RUNNING))->SetCheck(false);
	}
}
```

## 关键说明
- 通过检查返回的 `pCrdStatus` 值的特定位（如 `CRDSYS_STATUS_PROG_RUN`）来判断坐标系的运行、停止、报警等状态。

---

# MC_SetOverride教程

## 函数原型
```c
int MC_SetOverride(short nCrdNum, double synVelRatio);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `synVelRatio`| 速度倍率，1.0代表100%原始速度。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 通过滑动条实时调整速度倍率
```cpp
void CDemoVCDlg::OnNMReleasedcaptureSliderOverRide(NMHDR *pNMHDR, LRESULT *pResult)
{
	double dOverRide = 0;
	dOverRide = m_SliderCrdVel.GetPos()/100.00;
	iRes = g_MultiCard.MC_SetOverride(1,dOverRide);

	*pResult = 0;
}
```

## 关键说明
- 用于在插补运动过程中实时调整合成速度。
- 倍率值可以大于1.0（加速）或小于1.0（减速）。

---

# MC_BufIO教程

## 函数原型
```c
int MC_BufIO(short nCrdNum, unsigned short nDoType, unsigned short nCardIndex, unsigned short doMask, unsigned short doValue, short FifoIndex=0, long segNum=0);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `nDoType` | 输出类型，如 `MC_GPO` (通用输出)。 |
| `nCardIndex`| 板卡索引，0为主卡。 |
| `doMask` | I/O掩码，指定要操作的位。 |
| `doValue` | I/O值，指定要设置的电平。 |
| `FifoIndex`| 缓冲区索引。 |
| `segNum` | 用户自定义段号。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 在运动指令之间插入I/O输出
```cpp
void CDemoVCDlg::OnBnClickedButton10()
{
    // ...
	iRes = g_MultiCard.MC_LnXYZ(1,50000,0,0,50,0.2,0,0,2);

	iRes = g_MultiCard.MC_BufIO(1,MC_GPO,1,0X0010,0X0010,0,0);

	iRes = g_MultiCard.MC_BufDelay(1,1000,0,8);
    // ...
}
```

## 关键说明
- `MC_BufIO` 是一条非运动指令，它被插入到插补运动的缓冲区中，与运动指令同步执行。
- `doMask` 和 `doValue` 配合使用，可以精确控制一个或多个I/O口的状态。

---

# MC_BufDelay教程

## 函数原型
```c
int MC_BufDelay(short nCrdNum, unsigned long ulDelayTime, short FifoIndex=0, long segNum=0);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `ulDelayTime`| 延迟时间，单位为毫秒。 |
| `FifoIndex`| 缓冲区索引。 |
| `segNum` | 用户自定义段号。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 在两条运动指令之间暂停1秒
```cpp
void CDemoVCDlg::OnBnClickedButton10()
{
    // ...
	iRes = g_MultiCard.MC_BufIO(1,MC_GPO,1,0X0010,0X0010,0,0);
	iRes = g_MultiCard.MC_BufDelay(1,1000,0,8);
	iRes = g_MultiCard.MC_BufIO(1,MC_GPO,1,0X0020,0X0020,0,0);
    // ...
}
```

## 关键说明
- 在插补运动序列中插入一个暂停。这会导致运动在执行到该指令时暂停指定的时间，然后再继续执行后续指令。

---

# MC_CrdData教程

## 函数原型
```c
int MC_CrdData(short nCrdNum, void *pCrdData, short FifoIndex=0);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `pCrdData` | 指向 `TCrdData` 结构体的指针，或为 `NULL`。 |
| `FifoIndex`| 缓冲区索引，默认为0。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 结束插补数据发送并刷新前瞻缓冲区
```cpp
void CDemoVCDlg::OnBnClickedButton10()
{
    // ... 添加了多条 MC_LnXY, MC_ArcXYC 等指令 ...

    // 插入最后一行标识符（系统会把前瞻缓冲区数据全部压入板卡）
	iRes += g_MultiCard.MC_CrdData(1,NULL,0);
}
```

## 关键说明
- 当 `pCrdData` 参数为 `NULL` 时，此函数用作插补数据流的结束标志。
- 调用 `MC_CrdData(..., NULL, ...)` 会强制将前瞻缓冲区中剩余的所有运动指令进行速度规划并发送到控制卡的FIFO中。
- 在发送完一批运动指令后，必须使用 `NULL` 调用一次此函数，以确保所有运动都被执行。

---

# MC_LnXYZ教程

## 函数原型
```c
int MC_LnXYZ(short nCrdNum, long x, long y, long z, double synVel, double synAcc, double velEnd=0, short FifoIndex=0, long segNum=0);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `x`, `y`, `z` | XYZ轴目标位置。 |
| `synVel` | 合成速度。 |
| `synAcc` | 合成加速度。 |
| `velEnd` | 段末速度，默认为0。 |
| `FifoIndex`| 缓冲区索引。 |
| `segNum` | 用户自定义段号。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 添加一条三维空间直线运动
```cpp
void CDemoVCDlg::OnBnClickedButton10()
{
    // ...
    // 插入三维直线数据，X=10000,Y=20000,Z=2000,速度=50脉冲/ms...
	iRes += g_MultiCard.MC_LnXYZ(1,10000,20000,2000,50,0.2,0,0,3);
    // ...
}
```

## 关键说明
- 向插补缓冲区中添加一条三维直线运动指令。
- 需要先通过 `MC_SetCrdPrm` 建立一个至少三维的坐标系。

---

# MC_BufWaitIO教程

## 函数原型
```c
int MC_BufWaitIO(short nCrdNum, unsigned short nCardIndex, unsigned short nIOPortIndex, unsigned short nLevel, unsigned long lWaitTimeMS, unsigned short nFilterTime, short FifoIndex=0, long segNum=-1);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `nCardIndex` | 板卡索引，0是主模块。 |
| `nIOPortIndex` | IO索引，0-15。 |
| `nLevel` | 等待的电平，1:等待有信号输入, 0:等待信号消失。 |
| `lWaitTimeMS`| 等待超时时间(ms)，为0则一直等待。 |
| `nFilterTime`| 滤波时间(ms)。 |
| `FifoIndex`| 缓冲区索引。 |
| `segNum` | 用户自定义段号。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 等待输入口0变为高电平
```cpp
void CDemoVCDlg::OnBnClickedButton10()
{
    // ...
    // 等待IO，nLevel=1等待有信号输入，lWaitTimeMS=0一直等待
	g_MultiCard.MC_BufWaitIO(1,0,0,1,0,100,0,3);

	iRes = g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,4);
    // ...
}
```

## 关键说明
- 此指令会暂停插补运动的执行，直到指定的I/O口满足等待条件或等待超时。
- `nFilterTime` 用于消除信号抖动，只有当信号稳定保持超过滤波时间才认为有效。

---

# MC_SetExtDoBit教程

## 函数原型
```c
int MC_SetExtDoBit(short nCardIndex, short nBitIndex, unsigned short nValue);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCardIndex` | 板卡索引，0是主模块，扩展模块从1开始。 |
| `nBitIndex` | I/O点位索引，0-15。 |
| `nValue` | 要设置的电平值，1为高电平，0为低电平。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 翻转输出口1的电平
```cpp
void CDemoVCDlg::OnBnClickedButtonOutPut1()
{
	CString strText;
	GetDlgItem(IDC_BUTTON_OUT_PUT_1)->GetWindowText(strText);

	if(strstr(strText,"ON"))
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,0,1);
		GetDlgItem(IDC_BUTTON_OUT_PUT_1)->SetWindowText("OutPut1 OFF");
	}
	else
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,0,0);
		GetDlgItem(IDC_BUTTON_OUT_PUT_1)->SetWindowText("OutPut1 ON");
	}
}
```

## 关键说明
- 此函数用于立即设置指定的单个通用数字输出口的电平状态。
- 区别于 `MC_BufIO`，此函数会立即执行，不进入插补缓冲区。

---

# MC_GetAdc教程

## 函数原型
```c
int MC_GetAdc(short nADCNum, short *pValue, short nCount=1, unsigned long *pClock=NULL);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nADCNum` | 模拟量输入通道号，从1开始。 |
| `pValue` | 指向short变量的指针，用于接收ADC值。 |
| `nCount` | 读取的通道数量，默认为1。 |
| `pClock` | (可选) 时间戳。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 在定时器中读取ADC通道1的值
```cpp
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    short nValue = 0;
	g_MultiCard.MC_GetAdc(1,&nValue,1);

	strText.Format("模拟量电压：%d",nValue);
	GetDlgItem(IDC_STATIC_ADC)->SetWindowText(strText);
}
```

## 关键说明
- 用于读取模拟量输入通道的电压值（经过数字转换后的ADC值）。

---

# MC_CmpPluse教程

## 函数原型
```c
int MC_CmpPluse(short nChannelMask, short nPluseType1, short nPluseType2, short nTime1, short nTime2, short nTimeFlag1, short nTimeFlag2);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nChannelMask` | 通道掩码，bit0为通道1，bit1为通道2。 |
| `nPluseType1`| 通道1输出类型 (0:低, 1:高, 2:脉冲)。 |
| `nTime1` | 通道1脉冲持续时间。 |
| `nTimeFlag1` | 通道1时间单位 (0:us, 1:ms)。 |
| `...2` | 通道2对应参数。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 强制比较器1输出高电平
```cpp
// 调试硬件用
g_MultiCard.MC_CmpPluse(1,1,1,0,0,0,0);
```

### 2. 强制比较器1输出一个1000us的脉冲
```cpp
// 调试硬件用
g_MultiCard.MC_CmpPluse(1,2,0,1000,0,m_ComboBoxCMPUnit.GetCurSel(),0);
```

## 关键说明
- 此函数用于强制控制比较器I/O口立即输出指定电平或脉冲，通常用于硬件调试。

---

# MC_CmpBufData教程

## 函数原型
```c
int MC_CmpBufData(short nCmpEncodeNum, short nPluseType, short nStartLevel, short nTime, long *pBuf1, short nBufLen1, long *pBuf2, short nBufLen2, short nAbsPosFlag=0, short nTimerFlag=0);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCmpEncodeNum` | 用于位置比较的编码器轴号。 |
| `nPluseType` | 到达位置后的输出类型 (如2:脉冲)。 |
| `nStartLevel` | 脉冲的起始电平。 |
| `nTime` | 脉冲持续时间。 |
| `pBuf1` | 指向待比较数据数组的指针。 |
| `nBufLen1` | 待比较数据的长度。 |
| `nAbsPosFlag` | 位置类型 (0:相对, 1:绝对)。 |
| `nTimerFlag` | 时间单位 (0:us, 1:ms)。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 写入10个位置比较数据
```cpp
void CDemoVCDlg::OnBnClickedButtonCmpData()
{
	long CMPData1[10];
	for(int i=0; i<10; i++)
	{
		CMPData1[i] = 10000*(i+1);
	}

	g_MultiCard.MC_CmpBufData(1,2,0,1000,CMPData1,10,CMPData2,0,0,0);
}
```

## 关键说明
- 将一批位置数据写入比较器缓冲区。当指定编码器的位置到达缓冲区中的设定值时，比较器会按设定输出脉冲或电平。
- 这是实现高速位置锁存、视觉飞拍等应用的核心功能。

---

# MC_CmpBufSts教程

## 函数原型
```c
int MC_CmpBufSts(unsigned short *pStatus, unsigned short *pRemainDaga1, unsigned short *pRemainDaga2, unsigned short *pRemainSpace1, unsigned short *pRemainSpace2);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `pStatus` | 指向ushort变量的指针，接收比较器状态。 |
| `pRemainData1`| 指向ushort变量的指针，接收通道1剩余数据个数。 |
| `pRemainSpace1`| 指向ushort变量的指针，接收通道1剩余空间。 |
| `...2` | 通道2对应参数。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 在定时器中查询比较器状态
```cpp
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    unsigned short CmpStatus, CmpRemainData1, CmpRemainData2, CmpRemainSpace1, CmpRemainSpace2;

	g_MultiCard.MC_CmpBufSts(&CmpStatus,&CmpRemainData1,&CmpRemainData2,&CmpRemainSpace1,&CmpRemainSpace2);

	strText.Format("剩余待比较数据个数：%d",CmpRemainData1);
	GetDlgItem(IDC_STATIC_CMP_COUNT)->SetWindowText(strText);
}
```

## 关键说明
- 用于查询比较器缓冲区的状态，如是否为空、剩余数据量和剩余空间。

---

# MC_HomeStart教程

## 函数原型
```c
int MC_HomeStart(short nAxisNum);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 轴号，从1开始。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 启动指定轴的回零运动
```cpp
void CDemoVCDlg::OnBnClickedButtonStartHome()
{
    int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;
    g_MultiCard.MC_HomeStart(iAxisNum);
}
```

## 关键说明
- 启动一个自动化的回零流程。回零的具体方式和速度等参数需要先通过 `MC_HomeSetPrm` 进行配置。

---

# MC_HomeStop教程

## 函数原型
```c
int MC_HomeStop(short nAxisNum);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 轴号，从1开始。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 停止指定轴的回零运动
```cpp
void CDemoVCDlg::OnBnClickedButtonStopHome()
{
    int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;
    g_MultiCard.MC_HomeStop(iAxisNum);
}
```

## 关键说明
- 用于手动停止正在进行中的回零过程。

---

# MC_StartHandwheel教程

## 函数原型
```c
int MC_StartHandwheel(short nAxisNum, short nMasterAxisNum, long lMasterEven, long lSlaveEven, ...);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 要进入手轮模式的从动轴号。 |
| `nMasterAxisNum` | 手轮脉冲输入的主轴通道号。 |
| `lMasterEven` | 主轴脉冲数（比例分子）。 |
| `lSlaveEven` | 从动轴脉冲数（比例分母）。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 将轴1设置为手轮模式
```cpp
void CDemoVCDlg::OnBnClickedButtonEnterHandwhell()
{
    // ...
    // 只需要关心第1个参数（轴号），第3个参数（比例分母），第4个参数（比例分子）
	iRes = g_MultiCard.MC_StartHandwheel(1,4,m_lMasterEven,m_lSlaveEven,0,1,1,50,0);
}
```

## 关键说明
- 将指定轴切换为手轮跟随模式。
- 轴的运动将由手轮的脉冲输入驱动，运动距离由主从比例（`lMasterEven` / `lSlaveEven`）决定。
- 在切换轴或倍率时，需要先调用 `MC_EndHandwheel` 退出当前模式。

---

# MC_EndHandwheel教程

## 函数原型
```c
int MC_EndHandwheel(short nAxisNum);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 要退出手轮模式的轴号。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 退出轴1的手轮模式
```cpp
void CDemoVCDlg::OnBnClickedButton14()
{
	g_MultiCard.MC_EndHandwheel(1);
}
```

### 2. 切换手轮轴前先退出旧轴
```cpp
void CDemoVCDlg::OnBnClickedButtonEnterHandwhell()
{
	//之前为手轮模式的轴先退出手轮模式
	g_MultiCard.MC_EndHandwheel(1);

	iRes = g_MultiCard.MC_StartHandwheel( ... );
}
```

## 关键说明
- 使指定轴退出JOG模式，恢复为普通的指令控制模式。

---

# MC_GetCrdVel教程

## 函数原型
```c
int MC_GetCrdVel(short nCrdNum, double *pSynVel);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `pSynVel` | 指向double变量的指针，用于接收当前合成速度。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 在定时器中获取坐标系1的当前速度
```cpp
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    double dCrdVel;
    g_MultiCard.MC_GetCrdVel(1,&dCrdVel);
    // ...
}
```

## 关键说明
- 用于实时监控插补运动过程中的合成速度。

---

# MC_GetLookAheadSpace教程

## 函数原型
```c
int MC_GetLookAheadSpace(short nCrdNum, long *pSpace, short FifoIndex=0);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `pSpace` | 指向长整型变量的指针，用于接收前瞻缓冲区剩余空间。 |
| `FifoIndex`| 缓冲区索引，默认为0。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 在定时器中获取前瞻缓冲区剩余空间
```cpp
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    long lCrdSpace = 0;
    iRes = g_MultiCard.MC_GetLookAheadSpace(1,&lCrdSpace,0);

	strText.Format("板卡前瞻缓冲区剩余空间：%d",lCrdSpace);
	GetDlgItem(IDC_STATIC_LOOK_AHEAD_SPACE)->SetWindowText(strText);
}
```

## 关键说明
- 用于判断是否可以向控制器继续发送新的运动指令。

---

# MC_GetUserSegNum教程

## 函数原型
```c
int MC_GetUserSegNum(short nCrdNum, long *pSegment, short FifoIndex=0);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `pSegment` | 指向长整型变量的指针，用于接收当前执行到的用户自定义段号。 |
| `FifoIndex`| 缓冲区索引，默认为0。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 在定时器中获取当前运行的段号
```cpp
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    long lSegNum = 0;
	g_MultiCard.MC_GetUserSegNum(1,&lSegNum);

	strText.Format("运行行号：%d",lSegNum);
	GetDlgItem(IDC_STATIC_CUR_SEG_NUM)->SetWindowText(strText);
}
```

## 关键说明
- 在发送运动指令（如 `MC_LnXY`）时可以附加一个自定义的 `segNum`，通过此函数可以追踪运动程序执行到了哪一步。

---

# MC_StartDebugLog教程

## 函数原型
```c
int MC_StartDebugLog(short nFlag);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nFlag` | 调试日志标志，0为开启。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 在打开板卡前开启日志
```cpp
void CDemoVCDlg::OnBnClickedButton7()
{
	g_MultiCard.MC_StartDebugLog(0);
	iRes = g_MultiCard.MC_Open(1,"192.168.0.200",60000,"192.168.0.1",60000);
    // ...
}
```

## 关键说明
- 用于开启SDK的日志功能，方便调试和问题定位。

---

# MC_EncOn教程

## 函数原型
```c
int MC_EncOn(short nEncoderNum);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nEncoderNum` | 编码器号，通常与轴号一致。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 开启编码器
```cpp
void CDemoVCDlg::OnBnClickedCheckNoEncode()
{
    // ...
	// 示例代码中的UI逻辑是反的，GetCheck()为true时关闭，为false时开启
	iRes = g_MultiCard.MC_EncOn(iAxisNum);
    // ...
}
```

## 关键说明
- 开启编码器计数功能。对于闭环控制系统是必须的。

---

# MC_EncOff教程

## 函数原型
```c
int MC_EncOff(short nEncoderNum);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nEncoderNum` | 编码器号，通常与轴号一致。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 关闭编码器
```cpp
void CDemoVCDlg::OnBnClickedCheckNoEncode()
{
    // ...
	// 示例代码中的UI逻辑是反的，GetCheck()为true时关闭，为false时开启
	iRes = g_MultiCard.MC_EncOff(iAxisNum);
    // ...
}
```

## 关键说明
- 关闭编码器计数。在某些开环应用或调试场景下使用。

---

# MC_GetEncOnOff教程

## 函数原型
```c
int MC_GetEncOnOff(short nAxisNum, short *pEncOnOff);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 轴号。 |
| `pEncOnOff`| 指向short变量的指针，用于接收编码器开关状态。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 获取编码器状态并更新UI
```cpp
void CDemoVCDlg::OnCbnSelchangeCombo1()
{
    short nEncOnOff = 0;
	iRes = g_MultiCard.MC_GetEncOnOff(iAxisNum,&nEncOnOff);

	if(nEncOnOff)
	{
		((CButton*)GetDlgItem(IDC_CHECK_NO_ENCODE))->SetCheck(false);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_CHECK_NO_ENCODE))->SetCheck(true);
	}
}
```

## 关键说明
- 用于查询指定轴的编码器是开启还是关闭状态。

---

# MC_GetLmtsOnOff教程

## 函数原型
```c
int MC_GetLmtsOnOff(short nAxisNum, short *pPosLmtsOnOff, short *pNegLmtsOnOff);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nAxisNum` | 轴号。 |
| `pPosLmtsOnOff`| 指向short变量的指针，接收正限位开关状态。 |
| `pNegLmtsOnOff`| 指向short变量的指针，接收负限位开关状态。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 获取限位状态并更新UI
```cpp
void CDemoVCDlg::OnCbnSelchangeCombo1()
{
    // ...
	iRes = g_MultiCard.MC_GetLmtsOnOff(iAxisNum,&nPosLimOnOff,&nNegLimOfOff);

	if(nPosLimOnOff)
	{
		((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->SetCheck(true);
	}
    // ...
}
```

## 关键说明
- 用于查询指定轴的正负硬件限位功能是开启还是关闭状态。

---

# MC_BufMoveAccEX教程

## 函数原型
```c
int MC_BufMoveAccEX(short nCrdNum, short nAxisMask, float* pAcc, short nFifoIndex=0, long lSegNum=-1);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `nAxisMask`| 轴掩码，指定要设置加速度的轴。 |
| `pAcc` | 指向浮点型数组的指针，包含各轴的加速度值。 |
| `nFifoIndex`| 缓冲区索引。 |
| `lSegNum` | 用户自定义段号。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 在缓冲区中设置多轴加速度
```cpp
void CDemoVCDlg::OnBnClickedButton10()
{
    float dAcc[16];
    memset(dAcc,0,sizeof(dAcc));
    for(int i=0; i<8; i++)
    {
        dAcc[i] = 1;
    }
	g_MultiCard.MC_BufMoveAccEX(1,0XFFF0,&dAcc[0],0,0);
    // ...
}
```

## 关键说明
- 此函数用于在插补缓冲区中为多个轴设置加速度。它允许在运动序列中动态调整轴的加速度。

---

# MC_BufMoveVelEX教程

## 函数原型
```c
int MC_BufMoveVelEX(short nCrdNum, short nAxisMask, float* pVel, short nFifoIndex=0, long lSegNum=-1);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `nAxisMask`| 轴掩码，指定要设置速度的轴。 |
| `pVel` | 指向浮点型数组的指针，包含各轴的速度值。 |
| `nFifoIndex`| 缓冲区索引。 |
| `lSegNum` | 用户自定义段号。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 在缓冲区中设置多轴速度
```cpp
void CDemoVCDlg::OnBnClickedButton10()
{
    float dVel[16];
    memset(dVel,0,sizeof(dVel));
    for(int i=0; i<8; i++)
    {
        dVel[i] = 100;
    }
	g_MultiCard.MC_BufMoveVelEX(1,0XFFF0,&dVel[0],0,0);
    // ...
}
```

## 关键说明
- 此函数用于在插补缓冲区中为多个轴设置速度。它允许在运动序列中动态调整轴的速度。

---

# MC_BufMoveEX教程

## 函数原型
```c
int MC_BufMoveEX(short nCrdNum, short nAxisMask, long* pPos, short nModalMask, short nFifoIndex, long lSegNum);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `nAxisMask`| 轴掩码，指定参与运动的轴。 |
| `pPos` | 指向目标位置数组的指针。 |
| `nModalMask`| 阻塞掩码，指定哪些轴需要运动到位后才执行下一条指令。 |
| `nFifoIndex`| 缓冲区索引。 |
| `lSegNum` | 用户自定义段号。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 在缓冲区中执行多轴点位运动
```cpp
void CDemoVCDlg::OnBnClickedButton10()
{
    long lPos[16];
    memset(lPos,0,sizeof(lPos));

	lPos[15] = 100000;
	g_MultiCard.MC_BufMoveEX(1,0XFFF0,&lPos[0],0XFFF0,0,0);

	lPos[15] = 0;
	g_MultiCard.MC_BufMoveEX(1,0XFFF0,&lPos[0],0XFFF0,0,0);
}
```

## 关键说明
- `MC_BufMoveEX` 类似于 `MC_SetPos` + `MC_Update` 的组合，但它是一条缓冲区指令，用于在插补运动序列中插入多轴的独立点位运动。
- `nModalMask` 非常关键，用于实现轴之间的同步等待。

---

# MC_BufIOReverse教程

## 函数原型
```c
int MC_BufIOReverse(short nCrdNum, unsigned short nDoType, unsigned short nCardIndex, unsigned short doMask, unsigned short doValue, unsigned short nReverseTime, short FifoIndex=0, long segNum=0);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `nDoType` | 输出类型，如 `MC_GPO` (通用输出)。 |
| `nCardIndex`| 板卡索引。 |
| `doMask` | I/O掩码。 |
| `doValue` | I/O值。 |
| `nReverseTime`| 翻转时间，单位ms。 |
| `FifoIndex`| 缓冲区索引。 |
| `segNum` | 用户自定义段号。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 在缓冲区中插入I/O翻转指令
```cpp
void CDemoVCDlg::OnBnClickedButton10()
{
    // ...
	g_MultiCard.MC_BufIOReverse(1,12,0,0X80,0X80,5000,0,0);
    // ...
}
```

## 关键说明
- 此函数用于在插补运动序列中插入一条I/O翻转指令。在指定时间后，I/O口会自动翻转回初始状态。

---

# MC_CrdClear教程

## 函数原型
```c
int MC_CrdClear(short nCrdNum, short FifoIndex);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nCrdNum` | 坐标系号。 |
| `FifoIndex`| 缓冲区索引。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 清空插补缓冲区
```cpp
void CDemoVCDlg::OnBnClickedButton10()
{
    // ...
    //清空插补缓冲区原有数据
	g_MultiCard.MC_CrdClear(1,0);
    // ...
}
```

## 关键说明
- 在开始一段新的连续插补任务前，调用此函数可以清空缓冲区中可能存在的旧指令。

---

# MC_UartConfig教程

## 函数原型
```c
int MC_UartConfig(unsigned short nUartNum, unsigned long uLBaudRate, unsigned short nDataLength, unsigned short nVerifyType, unsigned short nStopBitLen);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nUartNum` | 串口号，从1开始。 |
| `uLBaudRate` | 波特率。 |
| `nDataLength`| 数据位长度。 |
| `nVerifyType`| 校验类型。 |
| `nStopBitLen`| 停止位长度。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 配置串口参数
```cpp
void CDemoVCDlg::OnBnClickedButtonUartSet()
{
    UpdateData(true);
    g_MultiCard.MC_UartConfig(m_ComboBoxUartNum.GetCurSel() + 1, m_iBaudRate, m_nDataLen, m_iVerifyType, m_iStopBit);
}
```

## 关键说明
- 用于配置控制卡上的指定串口的通信参数。

---

# MC_SendEthToUartString教程

## 函数原型
```c
int MC_SendEthToUartString(short nUartNum, unsigned char* pSendBuf, short nLength);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nUartNum` | 串口号，从1开始。 |
| `pSendBuf` | 指向要发送数据的缓冲区。 |
| `nLength` | 要发送数据的长度。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 通过串口发送数据
```cpp
void CDemoVCDlg::OnBnClickedButtonUartSend()
{
    char cSendBuf[128];
    // ... 填充 cSendBuf ...
    g_MultiCard.MC_SendEthToUartString(m_ComboBoxUartNum.GetCurSel() + 1, (unsigned char*)cSendBuf, strlen(cSendBuf));
}
```

## 关键说明
- 通过以太网将数据发送到控制卡上的指定串口。

---

# MC_ReadUartToEthString教程

## 函数原型
```c
int MC_ReadUartToEthString(short nUartNum, unsigned char* pRecvBuf, short* pLength);
```

## 参数说明
| 参数 | 说明 |
|------|------|
| `nUartNum` | 串口号，从1开始。 |
| `pRecvBuf` | 指向接收数据缓冲区的指针。 |
| `pLength` | 指向short变量的指针，用于接收实际读取到的数据长度。 |

## 返回值
- `0`: 成功。
- 其他: 失败。

## 实际代码示例

### 1. 从串口读取数据
```cpp
void CDemoVCDlg::OnBnClickedButtonUartRec()
{
    unsigned char byRec[128];
    short nLen = 0;
    g_MultiCard.MC_ReadUartToEthString(m_ComboBoxUartNum.GetCurSel() + 1, byRec, &nLen);
    // ... 处理接收到的数据 ...
}
```

## 关键说明
- 通过以太网从控制卡上的指定串口读取数据。
