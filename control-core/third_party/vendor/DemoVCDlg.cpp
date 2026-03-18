
// DemoVCDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "DemoVC.h"
#include "DemoVCDlg.h"
#include "afxdialogex.h"

#include "MultiCardCPP.h"

#ifdef NDEBUG
#pragma comment(lib,"..\\Release\\MultiCard.lib")
#else
#pragma comment(lib,"..\\Debug\\MultiCard.lib")
#endif

//一个板卡就声明1个对象，多个板卡就声明多个对象，各个对象之间互不干涉
MultiCard g_MultiCard;
MultiCard g_MultiCard2;
MultiCard g_MultiCard3;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

TCrdData LookAheadBuf[FROCAST_LEN];                 //前瞻缓冲区

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CDemoVCDlg 对话框

CDemoVCDlg::CDemoVCDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CDemoVCDlg::IDD, pParent)
	, m_lTargetPos(0)
	, m_lOrginX(0)
	, m_lOrginY(0)
	, m_lOrginZ(0)
	, m_lSlaveEven(1)
	, m_lMasterEven(1)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CDemoVCDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO1, m_ComboBoxAxis);
	DDX_Text(pDX, IDC_EDIT1, m_lTargetPos);
	DDX_Control(pDX, IDC_COMBO_CARD_SEL, m_ComboBoxCardSel);
	DDX_Control(pDX, IDC_SLIDER_OVER_RIDE, m_SliderCrdVel);
	DDX_Text(pDX, IDC_EDIT_ORGIN_X, m_lOrginX);
	DDX_Text(pDX, IDC_EDIT_ORGIN_Y, m_lOrginY);
	DDX_Text(pDX, IDC_EDIT_ORGIN_Z, m_lOrginZ);
	DDX_Text(pDX, IDC_EDIT2, m_lSlaveEven);
	DDX_Text(pDX, IDC_EDIT3, m_lMasterEven);
	DDX_Control(pDX, IDC_COMBO_CMP_UNIT, m_ComboBoxCMPUnit);
	DDX_Control(pDX, IDC_COMBO_UART_NUM, m_ComboBoxUartNum);
	DDX_Text(pDX, IDC_EDIT_BAUD_RATE, m_iBaudRate);
	DDX_Text(pDX, IDC_EDIT_DATA_LEN, m_nDataLen);
	DDX_Text(pDX, IDC_EDIT_VERIFY, m_iVerifyType);
	DDX_Text(pDX, IDC_EDIT_STOP_BIT, m_iStopBit);
}

BEGIN_MESSAGE_MAP(CDemoVCDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON4, &CDemoVCDlg::OnBnClickedButton4)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_BUTTON7, &CDemoVCDlg::OnBnClickedButton7)
	ON_BN_CLICKED(IDC_BUTTON2, &CDemoVCDlg::OnBnClickedButton2)
	ON_BN_CLICKED(IDC_BUTTON3, &CDemoVCDlg::OnBnClickedButton3)
	ON_BN_CLICKED(IDC_BUTTON1, &CDemoVCDlg::OnBnClickedButton1)
	ON_BN_CLICKED(IDC_CHECK_NO_ENCODE, &CDemoVCDlg::OnBnClickedCheckNoEncode)
	ON_BN_CLICKED(IDC_BUTTON_CLR_STS, &CDemoVCDlg::OnBnClickedButtonClrSts)
	ON_BN_CLICKED(IDC_BUTTON_AXIS_ON, &CDemoVCDlg::OnBnClickedButtonAxisOn)
	ON_BN_CLICKED(IDC_BUTTON6, &CDemoVCDlg::OnBnClickedButton6)
	ON_CBN_SELCHANGE(IDC_COMBO1, &CDemoVCDlg::OnCbnSelchangeCombo1)
	ON_BN_CLICKED(IDC_RADIO_INPUT_1, &CDemoVCDlg::OnBnClickedRadioInput1)
	ON_BN_CLICKED(IDC_BUTTON_OUT_PUT_1, &CDemoVCDlg::OnBnClickedButtonOutPut1)
	ON_BN_CLICKED(IDC_BUTTON_OUT_PUT_2, &CDemoVCDlg::OnBnClickedButtonOutPut2)
	ON_BN_CLICKED(IDC_BUTTON_OUT_PUT_3, &CDemoVCDlg::OnBnClickedButtonOutPut3)
	ON_BN_CLICKED(IDC_BUTTON_OUT_PUT_4, &CDemoVCDlg::OnBnClickedButtonOutPut4)
	ON_BN_CLICKED(IDC_BUTTON_OUT_PUT_5, &CDemoVCDlg::OnBnClickedButtonOutPut5)
	ON_BN_CLICKED(IDC_BUTTON_OUT_PUT_6, &CDemoVCDlg::OnBnClickedButtonOutPut6)
	ON_BN_CLICKED(IDC_BUTTON_OUT_PUT_7, &CDemoVCDlg::OnBnClickedButtonOutPut7)
	ON_BN_CLICKED(IDC_BUTTON_OUT_PUT_8, &CDemoVCDlg::OnBnClickedButtonOutPut8)
	ON_BN_CLICKED(IDC_BUTTON_OUT_PUT_9, &CDemoVCDlg::OnBnClickedButtonOutPut9)
	ON_BN_CLICKED(IDC_BUTTON_OUT_PUT_10, &CDemoVCDlg::OnBnClickedButtonOutPut10)
	ON_BN_CLICKED(IDC_BUTTON_OUT_PUT_11, &CDemoVCDlg::OnBnClickedButtonOutPut11)
	ON_BN_CLICKED(IDC_BUTTON_OUT_PUT_12, &CDemoVCDlg::OnBnClickedButtonOutPut12)
	ON_BN_CLICKED(IDC_BUTTON_OUT_PUT_13, &CDemoVCDlg::OnBnClickedButtonOutPut13)
	ON_BN_CLICKED(IDC_BUTTON_OUT_PUT_14, &CDemoVCDlg::OnBnClickedButtonOutPut14)
	ON_BN_CLICKED(IDC_BUTTON_OUT_PUT_15, &CDemoVCDlg::OnBnClickedButtonOutPut15)
	ON_BN_CLICKED(IDC_BUTTON_OUT_PUT_16, &CDemoVCDlg::OnBnClickedButtonOutPut16)
	ON_BN_CLICKED(IDC_CHECK_POS_LIM_ENABLE, &CDemoVCDlg::OnBnClickedCheckPosLimEnable)
	ON_BN_CLICKED(IDC_CHECK_NEG_LIM_ENABLE, &CDemoVCDlg::OnBnClickedCheckNegLimEnable)
	ON_BN_CLICKED(IDC_BUTTON9, &CDemoVCDlg::OnBnClickedButton9)
	ON_BN_CLICKED(IDC_BUTTON8, &CDemoVCDlg::OnBnClickedButton8)
	ON_BN_CLICKED(IDC_BUTTON_JOG_POS, &CDemoVCDlg::OnBnClickedButtonJogPos)
	ON_BN_CLICKED(IDC_BUTTON_JOG_NEG, &CDemoVCDlg::OnBnClickedButtonJogNeg)
	ON_BN_CLICKED(IDC_BUTTON_SET_CRD_PRM, &CDemoVCDlg::OnBnClickedButtonSetCrdPrm)
	ON_BN_CLICKED(IDC_BUTTON10, &CDemoVCDlg::OnBnClickedButton10)
	ON_BN_CLICKED(IDC_BUTTON11, &CDemoVCDlg::OnBnClickedButton11)
	ON_BN_CLICKED(IDC_BUTTON12, &CDemoVCDlg::OnBnClickedButton12)
	ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_SLIDER_OVER_RIDE, &CDemoVCDlg::OnNMReleasedcaptureSliderOverRide)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_SLIDER_OVER_RIDE, &CDemoVCDlg::OnNMCustomdrawSliderOverRide)
	ON_BN_CLICKED(IDC_BUTTON13, &CDemoVCDlg::OnBnClickedButton13)
	ON_BN_CLICKED(IDC_CHECK_PRINT_SPEED, &CDemoVCDlg::OnBnClickedCheckPrintSpeed)
	ON_BN_CLICKED(IDC_BUTTON14, &CDemoVCDlg::OnBnClickedButton14)
	ON_BN_CLICKED(IDC_BUTTON_ENTER_HANDWHELL, &CDemoVCDlg::OnBnClickedButtonEnterHandwhell)
	ON_BN_CLICKED(IDC_COMBO_CMP_H, &CDemoVCDlg::OnBnClickedComboCmpH)
	ON_BN_CLICKED(IDC_COMBO_CMP_L, &CDemoVCDlg::OnBnClickedComboCmpL)
	ON_BN_CLICKED(IDC_COMBO_CMP_P, &CDemoVCDlg::OnBnClickedComboCmpP)
	ON_BN_CLICKED(IDC_BUTTON_CMP_DATA, &CDemoVCDlg::OnBnClickedButtonCmpData)
	ON_BN_CLICKED(IDC_BUTTON_SET_CRD_PRM2, &CDemoVCDlg::OnBnClickedButtonSetCrdPrm2)
	ON_BN_CLICKED(IDC_BUTTON_UART_SET, &CDemoVCDlg::OnBnClickedButtonUartSet)
	ON_BN_CLICKED(IDC_BUTTON_UART_SEND, &CDemoVCDlg::OnBnClickedButtonUartSend)
	ON_BN_CLICKED(IDC_BUTTON_UART_REC, &CDemoVCDlg::OnBnClickedButtonUartRec)

	ON_BN_CLICKED(IDC_BUTTON_START_HOME, &CDemoVCDlg::OnBnClickedButtonStartHome)
	ON_BN_CLICKED(IDC_BUTTON_STOP_HOME, &CDemoVCDlg::OnBnClickedButtonStopHome)

	
	ON_BN_CLICKED(IDC_BUTTON15, &CDemoVCDlg::OnBnClickedButton15)
END_MESSAGE_MAP()


// CDemoVCDlg 消息处理程序

BOOL CDemoVCDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	m_ComboBoxAxis.AddString("轴1（Axis1）");
	m_ComboBoxAxis.AddString("轴2（Axis2）");
	m_ComboBoxAxis.AddString("轴3（Axis3）");
	m_ComboBoxAxis.AddString("轴4（Axis4）");
	m_ComboBoxAxis.AddString("轴5（Axis5）");
	m_ComboBoxAxis.AddString("轴6（Axis6）");
	m_ComboBoxAxis.AddString("轴7（Axis7）");
	m_ComboBoxAxis.AddString("轴8（Axis8）");

	m_ComboBoxAxis.AddString("轴9（Axis9）");
	m_ComboBoxAxis.AddString("轴10（Axis10）");
	m_ComboBoxAxis.AddString("轴11（Axis11）");
	m_ComboBoxAxis.AddString("轴12（Axis12）");
	m_ComboBoxAxis.AddString("轴13（Axis13）");
	m_ComboBoxAxis.AddString("轴14（Axis14）");
	m_ComboBoxAxis.AddString("轴15（Axis15）");
	m_ComboBoxAxis.AddString("轴16（Axis16）");

	m_ComboBoxAxis.SetCurSel(0);

	m_ComboBoxCMPUnit.AddString("微秒");
	m_ComboBoxCMPUnit.AddString("毫秒");
	m_ComboBoxCMPUnit.SetCurSel(0);

	m_ComboBoxCardSel.AddString("主卡");
	m_ComboBoxCardSel.AddString("扩展卡1");
	m_ComboBoxCardSel.AddString("扩展卡2");
	m_ComboBoxCardSel.AddString("扩展卡3");
	m_ComboBoxCardSel.AddString("扩展卡4");
	m_ComboBoxCardSel.AddString("扩展卡5");
	m_ComboBoxCardSel.AddString("扩展卡6");
	m_ComboBoxCardSel.AddString("扩展卡7");
	m_ComboBoxCardSel.AddString("扩展卡8");
	m_ComboBoxCardSel.SetCurSel(0);

	m_ComboBoxUartNum.AddString("1");
	m_ComboBoxUartNum.AddString("2");
	m_ComboBoxUartNum.AddString("3");
	m_ComboBoxUartNum.SetCurSel(0);

	m_iBaudRate = 115200;
	m_nDataLen = 8;
	m_iVerifyType = 0;
	m_iStopBit = 1;
	UpdateData(false);

	//设置初始位置
	m_SliderCrdVel.SetPos(50);  
	m_SliderCrdVel.SetRange(0,120);
	//设置在控件上单击时滑块移动步长
	m_SliderCrdVel.SetPageSize(5);

	SetTimer(1,100,NULL);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CDemoVCDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CDemoVCDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CDemoVCDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CDemoVCDlg::OnBnClickedButton4()
{
	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;
	g_MultiCard.MC_ZeroPos(iAxisNum);
}


void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	int iRes = 0;
	
	unsigned long lValue = 0;
	CString strText;

	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();

	TAllSysStatusDataSX m_AllSysStatusDataTemp;

	short nValue = 0;
	
	g_MultiCard.MC_GetAdc(1,&nValue,1);

	//int iValue = nValue;
	strText.Format("模拟量电压：%d",nValue);
	GetDlgItem(IDC_STATIC_ADC)->SetWindowText(strText);
	//读取板卡状态数据。
	iRes = g_MultiCard.MC_GetAllSysStatusSX(&m_AllSysStatusDataTemp);


	double dCrdVel;
	g_MultiCard.MC_GetCrdVel(1,&dCrdVel);

	strText.Format("%d",m_AllSysStatusDataTemp.lAxisPrfPos[iAxisNum-1]);
	GetDlgItem(IDC_STATIC_PRF_POS)->SetWindowText(strText);

	strText.Format("%d",m_AllSysStatusDataTemp.lAxisEncPos[iAxisNum-1]);
	GetDlgItem(IDC_STATIC_ENC_POS)->SetWindowText(strText);

	//轴状态显示
	if(m_AllSysStatusDataTemp.lAxisStatus[iAxisNum-1] & AXIS_STATUS_SV_ALARM)
	{
		((CButton*)GetDlgItem(IDC_RADIO_ALARM))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_ALARM))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lAxisStatus[iAxisNum-1] & AXIS_STATUS_RUNNING)
	{
		((CButton*)GetDlgItem(IDC_RADIO_RUNNING))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_RUNNING))->SetCheck(false);
	}

	//正极限位
	if(m_AllSysStatusDataTemp.lAxisStatus[iAxisNum-1] & AXIS_STATUS_POS_HARD_LIMIT)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_STATUS))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_STATUS))->SetCheck(false);
	}

	//负极限位
	if(m_AllSysStatusDataTemp.lAxisStatus[iAxisNum-1] & AXIS_STATUS_NEG_HARD_LIMIT)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_STATUS))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_STATUS))->SetCheck(false);
	}

	//回零中
	if(m_AllSysStatusDataTemp.lAxisStatus[iAxisNum-1] & AXIS_STATUS_HOME_RUNNING)
	{
		((CButton*)GetDlgItem(IDC_RADIO_HOMING_STATUS))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_HOMING_STATUS))->SetCheck(false);
	}

	
//通用输入IO信号显示
	//iRes = g_MultiCard.MC_GetExtDiValue(iCardIndex,&lValue,1);

	if(m_AllSysStatusDataTemp.lGpiRaw[0] & 0X0001)
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_1))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_1))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lGpiRaw[0] & 0X0002)
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_2))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_2))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lGpiRaw[0] & 0X0004)
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_3))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_3))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lGpiRaw[0] & 0X0008)
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_4))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_4))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lGpiRaw[0] & 0X0010)
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_5))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_5))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lGpiRaw[0] & 0X0020)
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_6))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_6))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lGpiRaw[0] & 0X0040)
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_7))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_7))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lGpiRaw[0] & 0X0080)
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_8))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_8))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lGpiRaw[0] & 0X0100)
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_9))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_9))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lGpiRaw[0] & 0X0200)
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_10))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_10))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lGpiRaw[0] & 0X0400)
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_11))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_11))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lGpiRaw[0] & 0X0800)
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_12))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_12))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lGpiRaw[0] & 0X1000)
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_13))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_13))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lGpiRaw[0] & 0X2000)
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_14))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_14))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lGpiRaw[0] & 0X4000)
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_15))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_15))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lGpiRaw[0] & 0X8000)
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_16))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_INPUT_16))->SetCheck(false);
	}


//负硬限位IO输入信号显示
	//iRes = g_MultiCard.MC_GetDiRaw(MC_LIMIT_NEGATIVE,(long*)&lValue);

	if(m_AllSysStatusDataTemp.lLimitNegRaw & 0X0001)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_1))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_1))->SetCheck(false);
	}
	if(m_AllSysStatusDataTemp.lLimitNegRaw & 0X0002)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_2))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_2))->SetCheck(false);
	}
	if(m_AllSysStatusDataTemp.lLimitNegRaw & 0X0004)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_3))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_3))->SetCheck(false);
	}
	if(m_AllSysStatusDataTemp.lLimitNegRaw & 0X0008)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_4))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_4))->SetCheck(false);
	}
	if(m_AllSysStatusDataTemp.lLimitNegRaw & 0X0010)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_5))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_5))->SetCheck(false);
	}
	if(m_AllSysStatusDataTemp.lLimitNegRaw & 0X0020)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_6))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_6))->SetCheck(false);
	}
	if(m_AllSysStatusDataTemp.lLimitNegRaw & 0X0040)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_7))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_7))->SetCheck(false);
	}
	if(m_AllSysStatusDataTemp.lLimitNegRaw & 0X0080)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_8))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMN_8))->SetCheck(false);
	}


//正限位IO输入信号显示
	//iRes = g_MultiCard.MC_GetDiRaw(MC_LIMIT_POSITIVE,(long*)&lValue);

	if(m_AllSysStatusDataTemp.lLimitPosRaw & 0X0001)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_1))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_1))->SetCheck(false);
	}

	if(m_AllSysStatusDataTemp.lLimitPosRaw & 0X0002)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_2))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_2))->SetCheck(false);
	}
	if(m_AllSysStatusDataTemp.lLimitPosRaw & 0X0004)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_3))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_3))->SetCheck(false);
	}
	if(m_AllSysStatusDataTemp.lLimitPosRaw & 0X0008)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_4))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_4))->SetCheck(false);
	}
	if(m_AllSysStatusDataTemp.lLimitPosRaw & 0X0010)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_5))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_5))->SetCheck(false);
	}
	if(m_AllSysStatusDataTemp.lLimitPosRaw & 0X0020)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_6))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_6))->SetCheck(false);
	}
	if(m_AllSysStatusDataTemp.lLimitPosRaw & 0X0040)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_7))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_7))->SetCheck(false);
	}
	if(m_AllSysStatusDataTemp.lLimitPosRaw & 0X0080)
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_8))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_LIMP_8))->SetCheck(false);
	}

//手轮IO输入状态显示
	//X
	if(m_AllSysStatusDataTemp.lMPG & 0X0001)
	{
		((CButton*)GetDlgItem(IDC_RADIO_X))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_X))->SetCheck(false);
	}

	//Y
	if(m_AllSysStatusDataTemp.lMPG & 0X0002)
	{
		((CButton*)GetDlgItem(IDC_RADIO_Y))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_Y))->SetCheck(false);
	}

	//Z
	if(m_AllSysStatusDataTemp.lMPG & 0X0004)
	{
		((CButton*)GetDlgItem(IDC_RADIO_Z))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_Z))->SetCheck(false);
	}

	//A
	if(m_AllSysStatusDataTemp.lMPG & 0X0008)
	{
		((CButton*)GetDlgItem(IDC_RADIO_A))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_A))->SetCheck(false);
	}

	//B
	if(m_AllSysStatusDataTemp.lMPG & 0X0010)
	{
		((CButton*)GetDlgItem(IDC_RADIO_B))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_B))->SetCheck(false);
	}

	//X1
	if(m_AllSysStatusDataTemp.lMPG & 0X0020)
	{
		((CButton*)GetDlgItem(IDC_RADIO_X1))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_X1))->SetCheck(false);
	}

	//X10
	if(m_AllSysStatusDataTemp.lMPG & 0X0040)
	{
		((CButton*)GetDlgItem(IDC_RADIO_X10))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_X10))->SetCheck(false);
	}

	//X100(如果X1和X10没有触发，就默认为X100),这样可以节约一个IO口，X100可以空着，不接
	if((0 == (m_AllSysStatusDataTemp.lMPG & 0X0020)) && (0 == (m_AllSysStatusDataTemp.lMPG & 0X0040)))
	{
		((CButton*)GetDlgItem(IDC_RADIO_X100))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_X100))->SetCheck(false);
	}


	unsigned short CmpStatus;
	unsigned short CmpRemainData1;
	unsigned short CmpRemainData2;

	unsigned short CmpRemainSpace1;
	unsigned short CmpRemainSpace2;

	g_MultiCard.MC_CmpBufSts(&CmpStatus,&CmpRemainData1,&CmpRemainData2,&CmpRemainSpace1,&CmpRemainSpace2);


	strText.Format("剩余待比较数据个数：%d",CmpRemainData1);
	GetDlgItem(IDC_STATIC_CMP_COUNT)->SetWindowText(strText);

	if(CmpStatus & 0X0001)
	{
		((CButton*)GetDlgItem(IDC_RADIO_EMPTY))->SetCheck(false);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_EMPTY))->SetCheck(true);
	}

	long lCrdSpace = 0;

//获取坐标系剩余空间
	iRes = g_MultiCard.MC_CrdSpace(1,&lCrdSpace,0);

	strText.Format("板卡插补缓冲区剩余空间：%d",lCrdSpace);
	GetDlgItem(IDC_STATIC_CRD_SPACE)->SetWindowText(strText);

//获取前瞻缓冲区剩余空间
	iRes = g_MultiCard.MC_GetLookAheadSpace(1,&lCrdSpace,0);

	strText.Format("板卡前瞻缓冲区剩余空间：%d",lCrdSpace);
	GetDlgItem(IDC_STATIC_LOOK_AHEAD_SPACE)->SetWindowText(strText);

//获取坐标系位置

	double dPos[8];
	iRes = g_MultiCard.MC_GetCrdPos(1,dPos);

	strText.Format("X:%.3f",dPos[0]);
	GetDlgItem(IDC_STATIC_POS_x)->SetWindowText(strText);

	strText.Format("Y:%.3f",dPos[1]);
	GetDlgItem(IDC_STATIC_POS_Y)->SetWindowText(strText);


	strText.Format("Z:%.3f",dPos[2]);
	GetDlgItem(IDC_STATIC_POS_Z)->SetWindowText(strText);

	short nCrdStatus = 0;
	iRes = g_MultiCard.MC_CrdStatus(1,&nCrdStatus,NULL);

	//坐标系报警状态显示
	if(nCrdStatus & CRDSYS_STATUS_ALARM)
	{
		((CButton*)GetDlgItem(IDC_RADIO_CRD_ALARM))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_CRD_ALARM))->SetCheck(false);
	}

	//坐标系运行状态显示
	if(nCrdStatus & CRDSYS_STATUS_PROG_RUN)
	{
		((CButton*)GetDlgItem(IDC_RADIO_CRD_RUNNING))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_CRD_RUNNING))->SetCheck(false);
	}

	//板卡缓冲区数据执行完毕显示
	if(nCrdStatus & CRDSYS_STATUS_FIFO_FINISH_0)
	{
		((CButton*)GetDlgItem(IDC_RADIO_CRD_BUFFER_FINISH))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_RADIO_CRD_BUFFER_FINISH))->SetCheck(false);
	}

	long lSegNum = 0;
	g_MultiCard.MC_GetUserSegNum(1,&lSegNum);

	strText.Format("运行行号：%d",lSegNum);
	GetDlgItem(IDC_STATIC_CUR_SEG_NUM)->SetWindowText(strText);

	CDialogEx::OnTimer(nIDEvent);
}


void CDemoVCDlg::OnBnClickedButton7()
{
	int iRes = 0;

	g_MultiCard.MC_StartDebugLog(0);
	//一个板卡就声明1个对象，多个板卡就声明多个对象，各个对象之间互不干涉
	//5个参数依次为板卡号、PC端IP地址、PC端端口号，板卡端IP地址，板卡端口号。
	//通常建议不同板卡IP地址要不同，端口号也要不同
	//PC端端口号和板卡端端口要保持一致，设为0时系统自动分配可用端口（推荐）
	//iRes = g_MultiCard.MC_Open(1,"10.129.41.200",60001,"10.129.41.112",60001);

	iRes = g_MultiCard.MC_Open(1,"192.168.0.200",0,"192.168.0.1",0);
	//iRes = g_MultiCard2.MC_Open(2,"192.168.0.200",60002,"192.168.0.2",60002);
	//iRes = g_MultiCard3.MC_Open(3,"192.168.0.200",60002,"192.168.0.3",60002);

	if(iRes)
	{
		MessageBox("Open Card Fail,Please turn off wifi ,check PC IP address or connection!");
	}
	else
	{
		g_MultiCard.MC_Reset();

		MessageBox("Open Card Successful!");

		OnCbnSelchangeCombo1();
	}
}


void CDemoVCDlg::OnBnClickedButton2()
{
	
}


void CDemoVCDlg::OnBnClickedButton3()
{
	int iRes = 0;
	short nCrdStatus = 1;
	long segment = 0;
	unsigned char cRevBuf[128];
	short nLength;

	g_MultiCard.MC_Stop(0XFFFFFFFF,0XFFFFFFFF);
}


void CDemoVCDlg::OnBnClickedButton1()
{
	
}


void CDemoVCDlg::OnBnClickedCheck1()
{
	// TODO: 在此添加控件通知处理程序代码
}


void CDemoVCDlg::OnBnClickedCheckNoEncode()
{
	int iRes = 0;
	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

	if(((CButton*)GetDlgItem(IDC_CHECK_NO_ENCODE))->GetCheck())
	{
		iRes = g_MultiCard.MC_EncOn(iAxisNum);
		((CButton*)GetDlgItem(IDC_CHECK_NO_ENCODE))->SetCheck(false);
	}
	else
	{
		iRes = g_MultiCard.MC_EncOff(iAxisNum);
		((CButton*)GetDlgItem(IDC_CHECK_NO_ENCODE))->SetCheck(true);
	}
}

void CDemoVCDlg::OnBnClickedCheckPosLimEnable()
{
	int iRes = 0;
	short nPosLimOnOff = 0;
	short nNegLimOnOff = 0;

	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

	//正限位
	if(((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->GetCheck())
	{
		iRes = g_MultiCard.MC_LmtsOn(iAxisNum,MC_LIMIT_POSITIVE);

		((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->SetCheck(true);
	}
	else
	{
		iRes = g_MultiCard.MC_LmtsOff(iAxisNum,MC_LIMIT_POSITIVE);

		((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->SetCheck(false);
	}
}


void CDemoVCDlg::OnBnClickedCheckNegLimEnable()
{
	int iRes = 0;
	short nPosLimOnOff = 0;
	short nNegLimOnOff = 0;

	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

	//正限位
	if(((CButton*)GetDlgItem(IDC_CHECK_NEG_LIM_ENABLE))->GetCheck())
	{
		iRes = g_MultiCard.MC_LmtsOn(iAxisNum,MC_LIMIT_NEGATIVE);

		((CButton*)GetDlgItem(IDC_CHECK_NEG_LIM_ENABLE))->SetCheck(true);
	}
	else
	{
		iRes = g_MultiCard.MC_LmtsOff(iAxisNum,MC_LIMIT_NEGATIVE);

		((CButton*)GetDlgItem(IDC_CHECK_NEG_LIM_ENABLE))->SetCheck(false);
	}
}


void CDemoVCDlg::OnBnClickedButtonClrSts()
{
	int iRes = 0;
	double dVel = 1;

	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

	iRes = g_MultiCard.MC_ClrSts(iAxisNum);
}


void CDemoVCDlg::OnBnClickedButtonAxisOn()
{
	int iRes = 0;
	CString strText;
	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

	GetDlgItemText(IDC_BUTTON_AXIS_ON,strText);
	if (strText == _T("伺服使能"))
	{
		iRes = g_MultiCard.MC_AxisOn(iAxisNum);
		GetDlgItem(IDC_BUTTON_AXIS_ON)->SetWindowText(_T("断开使能"));
	}
	else
	{
		iRes = g_MultiCard.MC_AxisOff(iAxisNum);
		GetDlgItem(IDC_BUTTON_AXIS_ON)->SetWindowText(_T("伺服使能"));
	}
}


void CDemoVCDlg::OnBnClickedButton6()
{
	int iRes = 0;
	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

	UpdateData(true);

	TTrapPrm TrapPrm;

	TrapPrm.acc = 0.1;
	TrapPrm.dec = 0.1;
	TrapPrm.smoothTime = 0;
	TrapPrm.velStart = 0;

	iRes = g_MultiCard.MC_PrfTrap(iAxisNum);
	iRes += g_MultiCard.MC_SetTrapPrm(iAxisNum,&TrapPrm);

	iRes += g_MultiCard.MC_SetPos(iAxisNum,m_lTargetPos);
	iRes = g_MultiCard.MC_SetVel(iAxisNum,5);

	iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));
}


void CDemoVCDlg::OnCbnSelchangeCombo1()
{
	int iRes = 0;
	short nEncOnOff = 0;
	short nPosLimOnOff = 0;
	short nNegLimOfOff = 0;
	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;


	iRes = g_MultiCard.MC_GetEncOnOff(iAxisNum,&nEncOnOff);

	if(nEncOnOff)
	{
		((CButton*)GetDlgItem(IDC_CHECK_NO_ENCODE))->SetCheck(false);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_CHECK_NO_ENCODE))->SetCheck(true);
	}

	iRes = g_MultiCard.MC_GetLmtsOnOff(iAxisNum,&nPosLimOnOff,&nNegLimOfOff);

	if(nPosLimOnOff)
	{
		((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->SetCheck(false);
	}

	if(nNegLimOfOff)
	{
		((CButton*)GetDlgItem(IDC_CHECK_NEG_LIM_ENABLE))->SetCheck(true);
	}
	else
	{
		((CButton*)GetDlgItem(IDC_CHECK_NEG_LIM_ENABLE))->SetCheck(false);
	}
}


void CDemoVCDlg::OnBnClickedRadioInput1()
{

}


void CDemoVCDlg::OnBnClickedButtonOutPut1()
{
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();
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


void CDemoVCDlg::OnBnClickedButtonOutPut2()
{
	// TODO: 在此添加控件通知处理程序代码
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();
	CString strText;

	GetDlgItem(IDC_BUTTON_OUT_PUT_2)->GetWindowText(strText);

	if(strstr(strText,"ON"))
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,1,1);
		GetDlgItem(IDC_BUTTON_OUT_PUT_2)->SetWindowText("OutPut2 OFF");
	}
	else
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,1,0);
		GetDlgItem(IDC_BUTTON_OUT_PUT_2)->SetWindowText("OutPut2 ON");
	}
}


void CDemoVCDlg::OnBnClickedButtonOutPut3()
{
	// TODO: 在此添加控件通知处理程序代码
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();
	CString strText;

	GetDlgItem(IDC_BUTTON_OUT_PUT_3)->GetWindowText(strText);

	if(strstr(strText,"ON"))
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,2,1);
		GetDlgItem(IDC_BUTTON_OUT_PUT_3)->SetWindowText("OutPut3 OFF");
	}
	else
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,2,0);
		GetDlgItem(IDC_BUTTON_OUT_PUT_3)->SetWindowText("OutPut3 ON");
	}
}


void CDemoVCDlg::OnBnClickedButtonOutPut4()
{
	// TODO: 在此添加控件通知处理程序代码
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();
	CString strText;

	GetDlgItem(IDC_BUTTON_OUT_PUT_4)->GetWindowText(strText);

	if(strstr(strText,"ON"))
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,3,1);
		GetDlgItem(IDC_BUTTON_OUT_PUT_4)->SetWindowText("OutPut4 OFF");
	}
	else
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,3,0);
		GetDlgItem(IDC_BUTTON_OUT_PUT_4)->SetWindowText("OutPut4 ON");
	}
}


void CDemoVCDlg::OnBnClickedButtonOutPut5()
{
	// TODO: 在此添加控件通知处理程序代码
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();
	CString strText;

	GetDlgItem(IDC_BUTTON_OUT_PUT_5)->GetWindowText(strText);

	if(strstr(strText,"ON"))
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,4,1);
		GetDlgItem(IDC_BUTTON_OUT_PUT_5)->SetWindowText("OutPut5 OFF");
	}
	else
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,4,0);
		GetDlgItem(IDC_BUTTON_OUT_PUT_5)->SetWindowText("OutPut5 ON");
	}
}


void CDemoVCDlg::OnBnClickedButtonOutPut6()
{
	// TODO: 在此添加控件通知处理程序代码
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();
	CString strText;

	GetDlgItem(IDC_BUTTON_OUT_PUT_6)->GetWindowText(strText);

	if(strstr(strText,"ON"))
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,5,1);
		GetDlgItem(IDC_BUTTON_OUT_PUT_6)->SetWindowText("OutPut6 OFF");
	}
	else
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,5,0);
		GetDlgItem(IDC_BUTTON_OUT_PUT_6)->SetWindowText("OutPut6 ON");
	}
}


void CDemoVCDlg::OnBnClickedButtonOutPut7()
{
	// TODO: 在此添加控件通知处理程序代码
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();
	CString strText;

	GetDlgItem(IDC_BUTTON_OUT_PUT_7)->GetWindowText(strText);

	if(strstr(strText,"ON"))
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,6,1);
		GetDlgItem(IDC_BUTTON_OUT_PUT_7)->SetWindowText("OutPut7 OFF");
	}
	else
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,6,0);
		GetDlgItem(IDC_BUTTON_OUT_PUT_7)->SetWindowText("OutPut7 ON");
	}
}


void CDemoVCDlg::OnBnClickedButtonOutPut8()
{
	// TODO: 在此添加控件通知处理程序代码
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();
	CString strText;

	GetDlgItem(IDC_BUTTON_OUT_PUT_8)->GetWindowText(strText);

	if(strstr(strText,"ON"))
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,7,1);
		GetDlgItem(IDC_BUTTON_OUT_PUT_8)->SetWindowText("OutPut8 OFF");
	}
	else
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,7,0);
		GetDlgItem(IDC_BUTTON_OUT_PUT_8)->SetWindowText("OutPut8 ON");
	}
}


void CDemoVCDlg::OnBnClickedButtonOutPut9()
{
	// TODO: 在此添加控件通知处理程序代码
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();
	CString strText;

	GetDlgItem(IDC_BUTTON_OUT_PUT_9)->GetWindowText(strText);

	if(strstr(strText,"ON"))
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,8,1);
		GetDlgItem(IDC_BUTTON_OUT_PUT_9)->SetWindowText("OutPut9 OFF");
	}
	else
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,8,0);
		GetDlgItem(IDC_BUTTON_OUT_PUT_9)->SetWindowText("OutPut9 ON");
	}
}


void CDemoVCDlg::OnBnClickedButtonOutPut10()
{
	// TODO: 在此添加控件通知处理程序代码
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();
	CString strText;

	GetDlgItem(IDC_BUTTON_OUT_PUT_10)->GetWindowText(strText);

	if(strstr(strText,"ON"))
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,9,1);
		GetDlgItem(IDC_BUTTON_OUT_PUT_10)->SetWindowText("OutPut10 OFF");
	}
	else
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,9,0);
		GetDlgItem(IDC_BUTTON_OUT_PUT_10)->SetWindowText("OutPut10 ON");
	}
}


void CDemoVCDlg::OnBnClickedButtonOutPut11()
{
	// TODO: 在此添加控件通知处理程序代码
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();
	CString strText;

	GetDlgItem(IDC_BUTTON_OUT_PUT_11)->GetWindowText(strText);

	if(strstr(strText,"ON"))
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,10,1);
		GetDlgItem(IDC_BUTTON_OUT_PUT_11)->SetWindowText("OutPut11 OFF");
	}
	else
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,10,0);
		GetDlgItem(IDC_BUTTON_OUT_PUT_11)->SetWindowText("OutPut11 ON");
	}
}


void CDemoVCDlg::OnBnClickedButtonOutPut12()
{
	// TODO: 在此添加控件通知处理程序代码
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();
	CString strText;

	GetDlgItem(IDC_BUTTON_OUT_PUT_12)->GetWindowText(strText);

	if(strstr(strText,"ON"))
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,11,1);
		GetDlgItem(IDC_BUTTON_OUT_PUT_12)->SetWindowText("OutPut12 OFF");
	}
	else
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,11,0);
		GetDlgItem(IDC_BUTTON_OUT_PUT_12)->SetWindowText("OutPut12 ON");
	}
}


void CDemoVCDlg::OnBnClickedButtonOutPut13()
{
	// TODO: 在此添加控件通知处理程序代码
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();
	CString strText;

	GetDlgItem(IDC_BUTTON_OUT_PUT_13)->GetWindowText(strText);

	if(strstr(strText,"ON"))
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,12,1);
		GetDlgItem(IDC_BUTTON_OUT_PUT_13)->SetWindowText("OutPut13 OFF");
	}
	else
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,12,0);
		GetDlgItem(IDC_BUTTON_OUT_PUT_13)->SetWindowText("OutPut13 ON");
	}
}


void CDemoVCDlg::OnBnClickedButtonOutPut14()
{
	// TODO: 在此添加控件通知处理程序代码
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();
	CString strText;

	GetDlgItem(IDC_BUTTON_OUT_PUT_14)->GetWindowText(strText);

	if(strstr(strText,"ON"))
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,13,1);
		GetDlgItem(IDC_BUTTON_OUT_PUT_14)->SetWindowText("OutPut14 OFF");
	}
	else
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,13,0);
		GetDlgItem(IDC_BUTTON_OUT_PUT_14)->SetWindowText("OutPut14 ON");
	}
}


void CDemoVCDlg::OnBnClickedButtonOutPut15()
{
	// TODO: 在此添加控件通知处理程序代码
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();
	CString strText;

	GetDlgItem(IDC_BUTTON_OUT_PUT_15)->GetWindowText(strText);

	if(strstr(strText,"ON"))
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,14,1);
		GetDlgItem(IDC_BUTTON_OUT_PUT_15)->SetWindowText("OutPut15 OFF");
	}
	else
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,14,0);
		GetDlgItem(IDC_BUTTON_OUT_PUT_15)->SetWindowText("OutPut15 ON");
	}
}


void CDemoVCDlg::OnBnClickedButtonOutPut16()
{
	// TODO: 在此添加控件通知处理程序代码
	int iCardIndex = m_ComboBoxCardSel.GetCurSel();
	CString strText;

	GetDlgItem(IDC_BUTTON_OUT_PUT_16)->GetWindowText(strText);

	if(strstr(strText,"ON"))
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,15,1);
		GetDlgItem(IDC_BUTTON_OUT_PUT_16)->SetWindowText("OutPut16 OFF");
	}
	else
	{
		g_MultiCard.MC_SetExtDoBit(iCardIndex,15,0);
		GetDlgItem(IDC_BUTTON_OUT_PUT_16)->SetWindowText("OutPut16 ON");
	}
}





void CDemoVCDlg::OnBnClickedButton9()
{
	g_MultiCard.MC_Close();
}


void CDemoVCDlg::OnBnClickedButton8()
{
	g_MultiCard.MC_Reset();
}


void CDemoVCDlg::OnBnClickedButton5()
{
	int iRes = 0;
	TJogPrm m_JogPrm;

	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

	m_JogPrm.dAcc = 0.1;
	m_JogPrm.dDec = 0.1;
	m_JogPrm.dSmooth = 0;

	iRes = g_MultiCard.MC_PrfJog(iAxisNum);

	iRes += g_MultiCard.MC_SetJogPrm(iAxisNum,&m_JogPrm);

	iRes += g_MultiCard.MC_SetVel(iAxisNum,50);

	iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));

	if(0 == iRes)
	{
		TRACE("正向连续移动2......\r\n");
	}
}


BOOL CDemoVCDlg::PreTranslateMessage(MSG* pMsg)
{
	// TODO: 在此添加专用代码和/或调用基类
	switch(pMsg->message)
	{
	case WM_LBUTTONDOWN:

		UpdateData(TRUE);
		//负向
		if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_NEG)->m_hWnd)
		{
			int iRes = 0;
			TJogPrm m_JogPrm;

			int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

			m_JogPrm.dAcc = 0.1;
			m_JogPrm.dDec = 0.1;
			m_JogPrm.dSmooth = 0;

			iRes = g_MultiCard.MC_PrfJog(iAxisNum);

			iRes += g_MultiCard.MC_SetJogPrm(iAxisNum,&m_JogPrm);

			iRes += g_MultiCard.MC_SetVel(iAxisNum,-20);

			iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));

			if(0 == iRes)
			{
				TRACE("负向连续移动......\r\n");
			}
		}
		//正向
		if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_POS)->m_hWnd)
		{
			int iRes = 0;
			TJogPrm m_JogPrm;

			int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

			m_JogPrm.dAcc = 0.1;
			m_JogPrm.dDec = 0.1;
			m_JogPrm.dSmooth = 0;

			iRes = g_MultiCard.MC_PrfJog(iAxisNum);

			iRes += g_MultiCard.MC_SetJogPrm(iAxisNum,&m_JogPrm);

			iRes += g_MultiCard.MC_SetVel(iAxisNum,20);

			iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));

			if(0 == iRes)
			{
				TRACE("正向连续移动......\r\n");
			}
		}
		break;
	case WM_LBUTTONUP:
		//负向
		if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_NEG)->m_hWnd)
		{
			g_MultiCard.MC_Stop(0XFFFF,0XFFFF);
		}
		//正向
		if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_POS)->m_hWnd)
		{
			g_MultiCard.MC_Stop(0XFFFF,0XFFFF);
		}
		break;
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}


void CDemoVCDlg::OnBnClickedButtonJogPos()
{
	//具体实现方法在 PreTranslateMessage 函数里面，在本文档搜索 PreTranslateMessage 即可

	//按钮按下，电机正转，按钮抬起，电机停止
}


void CDemoVCDlg::OnBnClickedButtonJogNeg()
{
	//具体实现方法在 PreTranslateMessage 函数里面，在本文档搜索 PreTranslateMessage 即可

	//按钮按下，电机反转，按钮抬起，电机停止
}


void CDemoVCDlg::OnBnClickedButtonSetCrdPrm()
{
	int iRes = 0;

	UpdateData(true);

//---------------------------------------建立坐标系------------------------------------------------
	//坐标系参数初始化
	TCrdPrm crdPrm;
	memset(&crdPrm,0,sizeof(crdPrm));

	//设置坐标系最小匀速时间为0
	crdPrm.evenTime = 0;

	//设置原点坐标值标志,0:默认当前规划位置为原点位置;1:用户指定原点位置
	crdPrm.setOriginFlag = 1;

	//设置坐标系原点
	
	crdPrm.originPos[0] = m_lOrginX;
	crdPrm.originPos[1] = m_lOrginY;
	crdPrm.originPos[2] = m_lOrginZ;
	crdPrm.originPos[3] = 0;
	crdPrm.originPos[4] = 0;
	crdPrm.originPos[5] = 0;

	
	//设置坐标系维数为3
	crdPrm.dimension = 2;

	//设置坐标系规划对应轴通道，通道1对应轴1，通道2对应轴3，通道3对应轴5
	crdPrm.profile[0] = 1;
	crdPrm.profile[1] = 2;
	crdPrm.profile[2] = 0;
	crdPrm.profile[3] = 0;
	crdPrm.profile[4] = 0;
	crdPrm.profile[5] = 0;


	//设置坐标系最大加速度为0.2脉冲/毫秒^2
	crdPrm.synAccMax = 0.2;
	//设置坐标系最大速度为100脉冲/毫秒
	crdPrm.synVelMax = 1000;

	//设置坐标系参数，建立坐标系
	iRes = g_MultiCard.MC_SetCrdPrm(1,&crdPrm);

	if(0 == iRes)
	{
		AfxMessageBox("建立坐标系成功！");
	}
	else
	{
		AfxMessageBox("建立坐标系失败！");
		return;
	}
//------------------------------------------------------------------------------------------------


//---------------------------------------初始化前瞻缓冲区-----------------------------------------
	//声明前瞻缓冲区参数
	TLookAheadPrm LookAheadPrm;

	memset(&LookAheadPrm,0,sizeof(LookAheadPrm));

	//各轴的最大加速度，单位脉冲/毫秒^2
	LookAheadPrm.dAccMax[0] = 2;
	LookAheadPrm.dAccMax[1] = 2;
	LookAheadPrm.dAccMax[2] = 2;

	//各轴的最大速度变化量（相当于启动速度）,单位脉冲/毫秒
	LookAheadPrm.dMaxStepSpeed[0] = 10;
	LookAheadPrm.dMaxStepSpeed[1] = 10;
	LookAheadPrm.dMaxStepSpeed[2] = 10;

	//各轴的脉冲当量(通常为1)
	LookAheadPrm.dScale[0] = 1;
	LookAheadPrm.dScale[1] = 1;
	LookAheadPrm.dScale[2] = 1;
	LookAheadPrm.dScale[3] = 1;
	LookAheadPrm.dScale[4] = 1;
	LookAheadPrm.dScale[5] = 1;

	//各轴的最大速度(脉冲/毫秒)
	LookAheadPrm.dSpeedMax[0] = 1000;
	LookAheadPrm.dSpeedMax[1] = 1000;
	LookAheadPrm.dSpeedMax[2] = 1000;
	LookAheadPrm.dSpeedMax[3] = 1000;
	LookAheadPrm.dSpeedMax[4] = 1000;

	//定义前瞻缓冲区长度
	LookAheadPrm.lookAheadNum = 200;
	//定义前瞻缓冲区指针
	LookAheadPrm.pLookAheadBuf = LookAheadBuf;

	//初始化前瞻缓冲区
	//iRes = g_MultiCard.MC_InitLookAhead(1,0,&LookAheadPrm);

	if(0 == iRes)
	{
		AfxMessageBox("初始化前瞻缓冲区成功！");
	}
	else
	{
		AfxMessageBox("初始化前瞻缓冲区失败！");
		return;
	}
}


void CDemoVCDlg::OnBnClickedButton10()
{
	int iRes = 0;

//----------------------------------------------------------------------------------
	//板卡为动态缓存，支持一边加工，一边下发数据，所以加工文件再大都不用担心
//----------------------------------------------------------------------------------
	long lPosTemp[16];
	float dAcc[16];
	float dVel[16];
	
	memset(&lPosTemp,0,sizeof(lPosTemp));
	lPosTemp[5] = 50000;
	
	memset(dVel,0,sizeof(dVel));

	for(int i=0; i<8; i++)
	{
		dVel[i] = 100;
	}

	memset(dAcc,0,sizeof(dAcc));
	for(int i=0; i<8; i++)
	{
		dAcc[i] = 1;
	}

	lPosTemp[3] = 50000;

	g_MultiCard.MC_EncOff(1);

	iRes = g_MultiCard.MC_LnXY(1,100000,100000,50,2,0,0,1);
	iRes = g_MultiCard.MC_LnXY(1,0,0,50,2,0,0,1);

	long lPos[16];

	memset(lPos,0,sizeof(lPos));

	dAcc[15] = 1;
	g_MultiCard.MC_BufMoveAccEX(1,0XFFF0,&dAcc[0],0,0);

	dVel[15] = 5;
	g_MultiCard.MC_BufMoveVelEX(1,0XFFF0,&dVel[0],0,0);

	lPos[15] = 100000;
	g_MultiCard.MC_BufMoveEX(1,0XFFF0,&lPos[0],0XFFF0,0,0);

	lPos[15] = 0;
	g_MultiCard.MC_BufMoveEX(1,0XFFF0,&lPos[0],0XFFF0,0,0);

	/*
	for(int i=0; i<3; i++)
	{
		iRes = g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,2);

		g_MultiCard.MC_BufIOReverse(1,12,0,0X80,0X80,5000,0,0);

		iRes = g_MultiCard.MC_LnXYZ(1,10000,20000,30000,50,0.2,0,0,2);
	}

	iRes = g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,2);
	*/

	/*
	long lData[8];

	lData[0] = 111;
	lData[1] = 222;
	lData[2] = 333;
	lData[3] = 444;
	lData[4] = 555;
	lData[5] = 666;
	lData[6] = 777;
	lData[7] = 888;

	g_MultiCard.MC_BufCmpData(1,1,2,3,4,&lData[0],8,1,0,0,0);
	*/
	/*
	iRes = g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,1);
	iRes = g_MultiCard.MC_LnXYZ(1,50000,0,0,50,0.2,0,0,2);

	iRes = g_MultiCard.MC_BufIO(1,MC_GPO,1,0X0010,0X0010,0,0);
	iRes = g_MultiCard.MC_BufDelay(1,1000,0,8);

	iRes = g_MultiCard.MC_BufIO(1,MC_GPO,1,0X0020,0X0020,0,0);
	iRes = g_MultiCard.MC_BufIO(1,MC_GPO,0,0X0100,0X0100,0,0);

	iRes = g_MultiCard.MC_BufDelay(1,1000,0,8);
	*/

	//int MultiCard::MC_BufWaitIO(short nCrdNum,unsigned short nCardIndex,unsigned short nIOPortIndex,unsigned short nLevel,unsigned long lWaitTimeMS,unsigned short nFilterTime,short FifoIndex,long segNum)
	//nCrdNum坐标系号，取值范围：[1,CRDSYS_MAX_COUNT]
			//nCardIndex:板卡索引(0~63),0是主模块，扩展模块从1开始
			//nIOPortIndex:    IO索引，0~15
			//nLevel:          1等待有信号输入，0等待信号消失
			//lWaitTimeMS：    等待超时时间,单位ms(超过该时间，会自动执行下一条命令，如果该时间为0，会一直等待)
			//nFilterTime：    滤波时间，单位ms。连续在这段时间检测到信号，才认为真正有信号
			//FifoIndex   插补缓存区号，取值范围：[0,1]，默认为：0
			//segNum：    用户自定义行号
	//g_MultiCard.MC_BufWaitIO(1,0,0,1,0,100,0,3);

	//iRes = g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,4);
	//iRes = g_MultiCard.MC_LnXYZ(1,50000,0,0,50,0.2,0,0,5);
	//清空插补缓冲区原有数据
	//g_MultiCard.MC_CrdClear(1,0);

	//插入二维直线数据，X=10000,Y=20000,速度=50脉冲/ms，加速度=0.2脉冲/毫秒^2,终点速度为0，Fifo为0，用户自定义行号为1


	//iRes = g_MultiCard.MC_LnXY(1,30000,30000,50,0.2,0,0,1);
	//iRes = g_MultiCard.MC_LnXY(1,0,0,50,0.2,0,0,1);

	//iRes = g_MultiCard.MC_ArcXYC(1,40000,0,20000,0,0,50,0.2,0,0,3);
	//iRes = g_MultiCard.MC_LnXY(1,0,0,50,0.2,0,0,1);

	//iRes = g_MultiCard.MC_LnXY(1,5000,2000,50,0.2,0,0,1);
	//iRes = g_MultiCard.MC_LnXY(1,15000,2000,50,0.2,0,0,2);
	//iRes = g_MultiCard.MC_ArcXYC(1,15000,12000,0,5000,0,50,0.2,0,0,3);
	//iRes = g_MultiCard.MC_LnXY(1,5000,12000,50,0.2,0,0,4);

	/*
	//插入三维直线数据，X=0,Y=0,Z=0速度=50脉冲/ms，加速度=0.2脉冲/毫秒^2,终点速度为0，Fifo为0，用户自定义行号为2
	iRes = g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,2);

	//插入三维直线数据，X=10000,Y=20000,Z=2000,速度=50脉冲/ms，加速度=0.2脉冲/毫秒^2,终点速度为0，Fifo为0，用户自定义行号为3
	iRes += g_MultiCard.MC_LnXYZ(1,10000,20000,2000,50,0.2,0,0,3);

	//插入三维直线数据，X=0,Y=0,Z=0,速度=50脉冲/ms，加速度=0.2脉冲/毫秒^2,终点速度为0，Fifo为0，用户自定义行号为4
	iRes += g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,4);

	//插入二维直线数据，X=10000,Y=20000,速度=50脉冲/ms，加速度=0.2脉冲/毫秒^2,终点速度为0，Fifo为0，用户自定义行号为5
	iRes = g_MultiCard.MC_LnXY(1,10000,20000,50,0.2,0,0,5);

	//插入IO输出指令（不会影响运动）
	iRes = g_MultiCard.MC_BufIO(1,MC_GPO,0,0X0001,0X0001,0,5);

	//插入二维直线数据，X=10000,Y=20000,速度=50脉冲/ms，加速度=0.2脉冲/毫秒^2,终点速度为0，Fifo为0，用户自定义行号为1
	iRes = g_MultiCard.MC_LnXY(1,50000,60000,50,0.2,0,0,6);

	//插入IO输出指令（不会影响运动）
	iRes = g_MultiCard.MC_BufIO(1,MC_GPO,0,0X0001,0X0000,0,6);

	//插入三维直线数据，X=0,Y=0,Z=0,速度=50脉冲/ms，加速度=0.2脉冲/毫秒^2,终点速度为0，Fifo为0，用户自定义行号为7
	iRes += g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,7);
	*/
	//iRes += g_MultiCard.MC_ArcXYC(1,50000,50000,25000,0,0,50,0.2,0,0,8);

	//iRes = g_MultiCard.MC_BufDelay(1,2000,0,8);

	//iRes += g_MultiCard.MC_ArcXYC(1,50000,0,0,-25000,1,50,0.2,0,0,3);

	//iRes = g_MultiCard.MC_BufDelay(1,2000,0,8);

	//iRes = g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,4);

	//插入最后一行标识符（系统会把前瞻缓冲区数据全部压入板卡）
	iRes += g_MultiCard.MC_CrdData(1,NULL,0);

	if(0 == iRes)
	{
		//AfxMessageBox("插入插补数据成功！");
	}
	else
	{
		//AfxMessageBox("插入插补数据失败！");
		return;
	}
}


void CDemoVCDlg::OnBnClickedButton11()
{
	int iRes = 0;
	iRes = g_MultiCard.MC_CrdStart(1,0);

	if(0 == iRes)
	{
		AfxMessageBox("启动坐标系成功！");
	}
	else
	{
		AfxMessageBox("启动坐标系失败！");
		return;
	}
}


void CDemoVCDlg::OnBnClickedButton12()
{
	g_MultiCard.MC_Stop(0XFFFFFF00,0XFFFFFF00);
}


void CDemoVCDlg::OnNMReleasedcaptureSliderOverRide(NMHDR *pNMHDR, LRESULT *pResult)
{
	int iRes = 0;
	// TODO: 在此添加控件通知处理程序代码
	double dOverRide = 0;

	double dParm[16];
	memset(dParm,0,sizeof(dParm));

	CString strText;
	//获取滑块位置
	strText.Format("%d%%",m_SliderCrdVel.GetPos());
	GetDlgItem(IDC_STATIC_CRD_VEL_OVER_RIDE)->SetWindowText(strText);

	dOverRide = m_SliderCrdVel.GetPos()/100.00;
	iRes = g_MultiCard.MC_SetOverride(1,dOverRide);

	*pResult = 0;
}


void CDemoVCDlg::OnNMCustomdrawSliderOverRide(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
}


void CDemoVCDlg::OnBnClickedButton13()
{
	int iRes = 0;

	iRes = g_MultiCard2.MC_Open(2,"192.168.0.200",0,"192.168.0.2",0);

	if(iRes)
	{
		MessageBox("Open Card Fail,Please turn off wifi ,check PC IP address or connection!");
	}
	else
	{
		MessageBox("Open Card Successful!");
		OnCbnSelchangeCombo1();
	}
}


void CDemoVCDlg::OnBnClickedCheckPrintSpeed()
{
	g_MultiCard.MC_StartDebugLog(0);
}


//X轴退出手轮模式
void CDemoVCDlg::OnBnClickedButton14()
{
	g_MultiCard.MC_EndHandwheel(1);
}

//X轴进入手轮模式
void CDemoVCDlg::OnBnClickedButtonEnterHandwhell()
{
	int iRes;

	//之前为手轮模式的轴先退出手轮模式
	g_MultiCard.MC_EndHandwheel(1);

	//说明：MC_StartHandwheel函数共9个参数，实际使用中，只需要关心第1个参数（轴号），第3个参数（比例分母），第4个参数（比例分子）
	//第一个参数轴号决定了哪个轴进入手轮模式，第3和第4个参数比例分子和比例分母则决定了手轮摇动过程中，该轴的移动速度。如果比例分子和比例分母都是1，则手轮摇动1格，电机走1个脉冲
	//其他6个参数固定，不用修改。
	iRes = g_MultiCard.MC_StartHandwheel(1,4,m_lMasterEven,m_lSlaveEven,0,1,1,50,0);


	//轴选和倍率部分参见定时器函数OnTimer
	//X/Y/Z/A/B/X1/X10/X100这些逻辑，是自己组合的。
	
	//比如，读取到X轴的轴选IO信号时，将X轴切换到手轮模式，读取到Y轴的轴选IO信号时，首先将X轴退出手轮模式后，将Y轴设置为手轮模式。

	//比如：读取到X1倍率信号时，将比例分子分母设置为1：1，当读取到X10信号时，将比例分子分母设置为1：10(根据自己机床的螺距、电机每圈脉冲个数自行设置，不一定是1：10)
	//注意要先退出手轮模式，再进入手轮模式，比例分子和比例分母才能生效
}

//强制比较器1输出一个高电平（调试硬件用）
void CDemoVCDlg::OnBnClickedComboCmpH()
{
/*
函数名：    int MC_CmpPluse(short nChannelMask, short nPluseType1, short nPluseType2, short nTime1,short nTime2, short nTimeFlag1, short nTimeFlag2)
函数说明：	设置比较器输出IO立即输出指定电平或者脉冲
参数说明：	nChannelMask,bit0表示通道1，bit1表示通道2
            nPluseType1：通道1输出类型，0低电平1高电平2脉冲
			nPluseType2：通道2输出类型，0低电平1高电平2脉冲
            nTime1：通道1脉冲持续时间，输出类型为脉冲时该参数有效
			nTime2：通道2脉冲持续时间，输出类型为脉冲时该参数有效
			nTimeFlag1：比较器1的脉冲时间单位，0us，1ms
			nTimeFlag2：比较器2的脉冲时间单位，0us，1ms
返回值：	0成功，其他：失败

注意事项：  无
*/
	g_MultiCard.MC_CmpPluse(1,1,1,0,0,0,0);
}

//强制比较器1输出一个低电平（调试硬件用）
void CDemoVCDlg::OnBnClickedComboCmpL()
{
	g_MultiCard.MC_CmpPluse(1,0,0,0,0,0,0);
}

//强制比较器1输出一个脉冲（调试硬件用）
void CDemoVCDlg::OnBnClickedComboCmpP()
{
	g_MultiCard.MC_CmpPluse(1,2,0,1000,0,m_ComboBoxCMPUnit.GetCurSel(),0);
}

//写入比较数据
void CDemoVCDlg::OnBnClickedButtonCmpData()
{
	long CMPData1[10];
	long CMPData2[10];

	//初始化待比较数据，这里一次下发10个待比较数据，CMPData2实际是没用的，这里也初始化一下
	for(int i=0; i<10; i++)
	{
		CMPData1[i] = 10000*(i+1);
	}

	//第一个参数代表轴1，说明是用轴1的编码器进行比较
	//第二个参数2的含义是脉冲，意思是到比较位置后，输出一个脉冲
	//第三个参数含义是初始电平，0代表初始化为低电平
	//第四个参数1000，代表脉冲持续时间
	//第五个参数是待比较数据指针
	//第六个参数是待比较数据长度
	//第七个参数是通道2带比较数据存放指针，一般用不到。数据较多，连续比较数据时候可以用。
	//第八个参数是待比较数据长度，一般用不到。数据较多，连续比较数据时候可以用。
	//第9个参数代表位置类型，0：相对当前位置1：绝对位置
	//最后一个参数是脉冲持续时间的单位，0us，1ms

	g_MultiCard.MC_CmpBufData(1,2,0,1000,CMPData1,10,CMPData2,0,0,0);

	//数据写完以后，当轴1运动到10000、20000、30000、位置的时候，比较器会输出一个脉冲
}


void CDemoVCDlg::OnBnClickedButtonSetCrdPrm2()
{
	int iRes = 0;

	UpdateData(true);

//---------------------------------------建立坐标系------------------------------------------------
	//坐标系参数初始化
	TCrdPrm crdPrm;
	memset(&crdPrm,0,sizeof(crdPrm));

	//设置坐标系最小匀速时间为0
	crdPrm.evenTime = 0;

	//设置原点坐标值标志,0:默认当前规划位置为原点位置;1:用户指定原点位置
	crdPrm.setOriginFlag = 1;

	//设置坐标系原点
	
	crdPrm.originPos[0] = m_lOrginX;
	crdPrm.originPos[1] = m_lOrginY;
	crdPrm.originPos[2] = m_lOrginZ;
	

	//设置坐标系维数为3
	crdPrm.dimension = 2;

	//设置坐标系规划对应轴通道，通道1对应轴1，通道2对应轴3，通道3对应轴5
	crdPrm.profile[0] = 2;
	crdPrm.profile[1] = 4;
	crdPrm.profile[2] = 0;


	//设置坐标系最大加速度为0.2脉冲/毫秒^2
	crdPrm.synAccMax = 0.2;
	//设置坐标系最大速度为100脉冲/毫秒
	crdPrm.synVelMax = 1000;

	//设置坐标系参数，建立坐标系
	iRes = g_MultiCard.MC_SetCrdPrm(2,&crdPrm);

	if(0 == iRes)
	{
		AfxMessageBox("建立坐标系2成功！");
	}
	else
	{
		AfxMessageBox("建立坐标系2失败！");
		return;
	}
//------------------------------------------------------------------------------------------------


//---------------------------------------初始化前瞻缓冲区-----------------------------------------
	//声明前瞻缓冲区参数
	TLookAheadPrm LookAheadPrm;

	//各轴的最大加速度，单位脉冲/毫秒^2
	LookAheadPrm.dAccMax[0] = 2;
	LookAheadPrm.dAccMax[1] = 2;
	LookAheadPrm.dAccMax[2] = 2;

	//各轴的最大速度变化量（相当于启动速度）,单位脉冲/毫秒
	LookAheadPrm.dMaxStepSpeed[0] = 10;
	LookAheadPrm.dMaxStepSpeed[1] = 10;
	LookAheadPrm.dMaxStepSpeed[2] = 10;

	//各轴的脉冲当量(通常为1)
	LookAheadPrm.dScale[0] = 1;
	LookAheadPrm.dScale[1] = 1;
	LookAheadPrm.dScale[2] = 1;

	//各轴的最大速度(脉冲/毫秒)
	LookAheadPrm.dSpeedMax[0] = 1000;
	LookAheadPrm.dSpeedMax[1] = 1000;
	LookAheadPrm.dSpeedMax[2] = 1000;
	LookAheadPrm.dSpeedMax[3] = 1000;
	LookAheadPrm.dSpeedMax[4] = 1000;

	//定义前瞻缓冲区长度
	LookAheadPrm.lookAheadNum = FROCAST_LEN;
	//定义前瞻缓冲区指针
	LookAheadPrm.pLookAheadBuf = LookAheadBuf;

	//初始化前瞻缓冲区
	iRes = g_MultiCard.MC_InitLookAhead(2,0,&LookAheadPrm);

	if(0 == iRes)
	{
		AfxMessageBox("初始化前瞻缓冲区2成功！");
	}
	else
	{
		AfxMessageBox("初始化前瞻缓冲区2失败！");
		return;
	}
}


void CDemoVCDlg::OnBnClickedButtonUartSet()
{
	int iRes;
	

	UpdateData(true);

	iRes = g_MultiCard.MC_UartConfig(m_ComboBoxUartNum.GetCurSel()+1,m_iBaudRate,m_nDataLen,m_iVerifyType,m_iStopBit);

	if(0 == iRes)
	{
		AfxMessageBox("配置串口成功！");
	}
	else
	{
		AfxMessageBox("配置串口失败！");
		return;
	}
}


void CDemoVCDlg::OnBnClickedButtonUartSend()
{
	g_MultiCard.MC_SendEthToUartString(m_ComboBoxUartNum.GetCurSel()+1,(unsigned char *)"1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890",100);
}


void CDemoVCDlg::OnBnClickedButtonUartRec()
{
	unsigned char cTemp[1024];
	short nLen;
	CString strText;

	g_MultiCard.MC_ReadUartToEthString(m_ComboBoxUartNum.GetCurSel()+1,cTemp,&nLen);

	if(nLen < 1024)
	{
		cTemp[nLen] = 0;
		strText.Format("%s",cTemp);
		GetDlgItem(IDC_STATIC_REV_UART)->SetWindowText(strText);
	}
}

//启动回零
void CDemoVCDlg::OnBnClickedButtonStartHome()
{
	TAxisHomePrm AxisHomePrm;

	AxisHomePrm.nHomeMode = 1;//回零方式：0--无 1--HOME回原点	2--HOME加Index回原点3----Z脉冲	
	AxisHomePrm.nHomeDir = 0;//回零方向，1-正向回零，0-负向回零
	AxisHomePrm.lOffset = 1;//回零偏移，回到零位后再走一个Offset作为零位
	AxisHomePrm.dHomeRapidVel = 1;//回零快移速度，单位：Pluse/ms
	AxisHomePrm.dHomeLocatVel = 1;//回零定位速度，单位：Pluse/ms
	AxisHomePrm.dHomeIndexVel = 1;//回零寻找INDEX速度，单位：Pluse/ms
	AxisHomePrm.dHomeAcc = 1;//回零使用的加速度

	AxisHomePrm.ulHomeIndexDis=0;           //找Index最大距离
	AxisHomePrm.ulHomeBackDis=0;            //回零时，第一次碰到零位后的回退距离
	AxisHomePrm.nDelayTimeBeforeZero=1000;    //位置清零前，延时时间，单位ms
	AxisHomePrm.ulHomeMaxDis=0;//回零最大寻找范围，单位脉冲

	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

	g_MultiCard.MC_HomeSetPrm(iAxisNum,&AxisHomePrm);
	g_MultiCard.MC_HomeStop(iAxisNum);
	g_MultiCard.MC_HomeStart(iAxisNum);
}

//停止回零
void CDemoVCDlg::OnBnClickedButtonStopHome()
{
	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

	g_MultiCard.MC_HomeStop(iAxisNum);
}


void CDemoVCDlg::OnBnClickedButton15()
{
	unsigned char cMessage[32];
	CString strText;

	g_MultiCard.MC_GetCardMessage(cMessage);

	strText.Format("%s",cMessage);
	AfxMessageBox(strText);
}
