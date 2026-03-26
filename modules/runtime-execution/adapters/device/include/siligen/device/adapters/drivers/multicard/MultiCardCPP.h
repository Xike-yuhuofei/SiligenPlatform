#pragma once
#include "string.h"

#include <stddef.h>

#pragma pack(push)
#pragma pack(2)

#define MC_API extern "C" __declspec(dllimport)

#define MAX_MACRO_CHAR_LENGTH (128)

typedef void (*GAS_IOCallBackFun)(unsigned long, unsigned long);

// ����ִ�з���ֵ
#define MC_COM_SUCCESS (0)              // ִ�гɹ�
#define MC_COM_ERR_EXEC_FAIL (1)        // ִ��ʧ��
#define MC_COM_ERR_LICENSE_WRONG (2)    // license��֧��
#define MC_COM_ERR_DATA_WORRY (7)       // ��������
#define MC_COM_ERR_SEND (-1)            // ����ʧ��
#define MC_COM_ERR_CARD_OPEN_FAIL (-6)  // ��ʧ��
#define MC_COM_ERR_TIME_OUT (-7)        // ����Ӧ
#define MC_COM_ERR_COM_OPEN_FAIL (-8)   // �򿪴���ʧ��

// ��״̬λ����
#define AXIS_STATUS_ESTOP (0x00000001)  // ��ͣ
#define AXIS_STATUS_SV_ALARM (0x00000002)  // ������������־��1-�ŷ��б�����0-�ŷ��ޱ�����
#define AXIS_STATUS_POS_SOFT_LIMIT (0x00000004)  // ������λ������־���滮λ�ô�����������λʱ��1��
#define AXIS_STATUS_NEG_SOFT_LIMIT (0x00000008)  // ����λ������־���滮λ��С�ڸ�������λʱ��1��
#define AXIS_STATUS_FOLLOW_ERR (0x00000010)  // ��滮λ�ú�ʵ��λ�õ��������趨����ʱ��1��

// DEPRECATED: 以下硬限位状态位定义与其他位冲突，不应使用
// 使用 MultiCardMotionAdapter::IsHardLimitTriggered() 方法通过 MC_GetDi() 直接读取IO状态
// 问题: AXIS_STATUS_POS_HARD_LIMIT (0x00000008) 与 AXIS_STATUS_NEG_SOFT_LIMIT 冲突
//       AXIS_STATUS_NEG_HARD_LIMIT (0x00000010) 与 AXIS_STATUS_FOLLOW_ERR 冲突
// #define AXIS_STATUS_POS_HARD_LIMIT (0x00000008)  // ��Ӳ��λ������־������λ���ص�ƽ״̬Ϊ��λ������ƽʱ��1��
// #define AXIS_STATUS_NEG_HARD_LIMIT (0x00000010)  // ��Ӳ��λ������־������λ���ص�ƽ״̬Ϊ��λ������ƽʱ��1��
#define AXIS_STATUS_IO_SMS_STOP (0x00000080)  // IOƽ��ֹͣ������־������λ���ص�ƽ״̬Ϊ��λ������ƽʱ��1���滮λ�ô�����������λʱ��1��
#define AXIS_STATUS_IO_EMG_STOP (0x00000100)  // IO����ֹͣ������־������λ���ص�ƽ״̬Ϊ��λ������ƽʱ��1���滮λ��С�ڸ�������λʱ��1��
#define AXIS_STATUS_ENABLE (0x00000200)       // ���ʹ�ܱ�־
#define AXIS_STATUS_RUNNING (0x00000400)      // �滮�˶���־���滮���˶�ʱ��1
#define AXIS_STATUS_ARRIVE (0x00000800)  // �����λ���滮����ֹ���滮λ�ú�ʵ��λ�õ����С���趨���������������ڱ����趨ʱ�������λ��־��
#define AXIS_STATUS_HOME_RUNNING (0x00001000)  // ���ڻ���
#define AXIS_STATUS_HOME_SUCESS (0x00002000)   // ����ɹ�
#define AXIS_STATUS_HOME_SWITCH (0x00004000)   // ��λ�ź�
#define AXIS_STATUS_INDEX (0x00008000)         // �����ź�
#define AXIS_STATUS_GEAR_START (0x00010000)    // ���ӳ��ֿ�ʼ����
#define AXIS_STATUS_GEAR_FINISH (0x00020000)   // ���ӳ����������
#define AXIS_STATUS_HOME_FAIL (0x00400000)     // ����ʧ��
#define AXIS_STATUS_ECAT_HOME (0x00800000)     // �ŷ���λ
#define AXIS_STATUS_ECAT_PROBE (0x01000000)    // �ŷ�̽��

// ����ϵ״̬λ����
#define CRDSYS_STATUS_PROG_RUN (0x00000001)    // ������
#define CRDSYS_STATUS_PROG_STOP (0x00000002)   // ƽ��ֹͣ��
#define CRDSYS_STATUS_PROG_ESTOP (0x00000004)  // ����ֹͣ��

#define CRDSYS_STATUS_FIFO_FINISH_0 (0x00000010)  // �忨FIFO-0������ִ����ϵ�״̬λ
#define CRDSYS_STATUS_FIFO_FINISH_1 (0x00000020)  // �忨FIFO-1������ִ����ϵ�״̬λ
#define CRDSYS_STATUS_ALARM (0x00000040)          // ����ϵ�б���

// ����IO���ͺ궨��
#define MC_LIMIT_POSITIVE 0
#define MC_LIMIT_NEGATIVE 1
#define MC_ALARM 2
#define MC_HOME 3
#define MC_GPI 4
#define MC_ARRIVE 5
#define MC_IP_SWITCH 6
#define MC_MPG 7

// ���IO���ͺ궨��
#define MC_ENABLE 10
#define MC_CLEAR 11
#define MC_GPO 12


// ���ٲ����������ͺ궨��
#define CAPTURE_HOME 1    // HOME����
#define CAPTURE_INDEX 2   // INDEX����
#define CAPTURE_PROBE1 3  // ̽�벶��
#define CAPTURE_PROBE2 4
#define CAPTURE_X0 100
#define CAPTURE_X1 101
#define CAPTURE_X2 102
#define CAPTURE_X3 103
#define CAPTURE_X4 104
#define CAPTURE_X5 105
#define CAPTURE_X6 106
#define CAPTURE_X7 107
#define CAPTURE_X8 108
#define CAPTURE_X9 109
#define CAPTURE_X10 110
#define CAPTURE_X11 111
#define CAPTURE_X12 112
#define CAPTURE_X13 113
#define CAPTURE_X14 114
#define CAPTURE_X15 115

#define CAPTURE_HOME_STOP 11    // HOME���񣬲��Զ�ֹͣ
#define CAPTURE_INDEX_STOP 12   // INDEX���񣬲��Զ�ֹͣ
#define CAPTURE_PROBE1_STOP 13  // ̽�벶�񣬲��Զ�ֹͣ
#define CAPTURE_PROBE2_STOP 14
#define CAPTURE_X0_STOP 200
#define CAPTURE_X1_STOP 201
#define CAPTURE_X2_STOP 202
#define CAPTURE_X3_STOP 203
#define CAPTURE_X4_STOP 204
#define CAPTURE_X5_STOP 205
#define CAPTURE_X6_STOP 206
#define CAPTURE_X7_STOP 207
#define CAPTURE_X8_STOP 208
#define CAPTURE_X9_STOP 209
#define CAPTURE_X10_STOP 210
#define CAPTURE_X11_STOP 211
#define CAPTURE_X12_STOP 212
#define CAPTURE_X13_STOP 213
#define CAPTURE_X14_STOP 214
#define CAPTURE_X15_STOP 215

// PTģʽ�궨��
#define PT_MODE_STATIC 0
#define PT_MODE_DYNAMIC 1

#define PT_SEGMENT_NORMAL 0
#define PT_SEGMENT_EVEN 1
#define PT_SEGMENT_STOP 2

#define GEAR_MASTER_ENCODER 1  // ���ӳ��֣����������
#define GEAR_MASTER_PROFILE 2  // ���ӳ��֣�����滮ֵ(�������)
#define GEAR_MASTER_AXIS 3     // ����


// ���ӳ��������¼�����
#define GEAR_EVENT_IMMED 1  // �����������ӳ���
#define GEAR_EVENT_BIG_EQU 2  // ����滮���߱�����λ�ô��ڵ���ָ����ֵʱ�������ӳ���
#define GEAR_EVENT_SMALL_EQU 3  // ����滮���߱�����λ��С�ڵ���ָ����ֵʱ�������ӳ���
#define GEAR_EVENT_IO_ON 4   // ָ��IOΪONʱ�������ӳ���
#define GEAR_EVENT_IO_OFF 5  // ָ��IOΪOFFʱ�������ӳ���

#define CAM_EVENT_IMMED 1
#define CAM_EVENT_BIG_EQU 2
#define CAM_EVENT_SMALL_EQU 3
#define CAM_EVENT_IO_ON 4
#define CAM_EVENT_IO_OFF 5

#define FROCAST_LEN (200)  // ǰհ���������

#define INTERPOLATION_AXIS_MAX 32
#define CRD_FIFO_MAX 4096
#define CRD_MAX 16


// ��λģʽ�����ṹ��
typedef struct TrapPrm {
    double acc;        // ���ٶ�
    double dec;        // ���ٶ�
    double velStart;   // ��ʼ�ٶ�
    short smoothTime;  // ƽ��ʱ��
} TTrapPrm;

// JOGģʽ�����ṹ��
typedef struct JogPrm {
    double dAcc;
    double dDec;
    double dSmooth;
} TJogPrm;

// �岹����״̬�ṹ��
typedef struct _CrdDataState {
    double dLength[32];  // ������������ĳ���
    double dSynLength;   // ���β岹���ݺϳɳ���
    double dEndSpeed;    // ���β岹�յ��ٶ�
} TCrdDataState;

// ����ϵ�����ṹ��
typedef struct _CrdPrm {
    short dimension;      // ����ϵά��
    short profile[8];     // ����profile��������(��1��ʼ)
    double synVelMax;     // ���ϳ��ٶ�
    double synAccMax;     // ���ϳɼ��ٶ�
    short evenTime;       // ��С����ʱ��
    short setOriginFlag;  // ����ԭ������ֵ��־,0:Ĭ�ϵ�ǰ�滮λ��Ϊԭ��λ��;1:�û�ָ��ԭ��λ��
    long originPos[8];    // �û�ָ����ԭ��λ��
} TCrdPrm;

// ����ϵ�����ṹ��
typedef struct _CrdPrmEx {
    short dimension;      // ����ϵά��
    short profile[32];    // ����profile��������(��1��ʼ)
    double synVelMax;     // ���ϳ��ٶ�
    double synAccMax;     // ���ϳɼ��ٶ�
    short evenTime;       // ��С����ʱ��
    short setOriginFlag;  // ����ԭ������ֵ��־,0:Ĭ�ϵ�ǰ�滮λ��Ϊԭ��λ��;1:�û�ָ��ԭ��λ��
    long originPos[32];   // �û�ָ����ԭ��λ��
} TCrdPrmEx;

// ��������
enum _CMD_TYPE {
    // ��������
    CMD_G00 = 1,  // ���ٶ�λ
    CMD_G01,      // ֱ�߲岹
    CMD_G02,      // ˳Բ���岹
    CMD_G03,      // ��Բ���岹
    CMD_G04,      // ��ʱ,G04 P1000����ͣ1��(��λΪms),G04 X2.0����ͣ2��
    CMD_G05,      // �����Զ���岹�ζκ�
    CMD_G54,
    CMD_HELIX_G02,  // ˳Բ�������岹
    CMD_HELIX_G03,  // ��Բ�������岹
    CMD_CRD_PRM,    // ��������ϵ

    CMD_M00 = 11,  // ��ͣ
    CMD_M30,       // ����
    CMD_M31,       // �л���XY1Z����ϵ
    CMD_M32,       // �л���XY2Z����ϵ
    CMD_M99,       // ѭ��

    CMD_SET_IO = 101,         // ����IO
    CMD_WAIT_IO,              // �ȴ�IO
    CMD_BUFFER_MOVE_SET_POS,  // CMD_BUFFER_MOVE_SET_POS
    CMD_BUFFER_MOVE_SET_VEL,  // CMD_BUFFER_MOVE_SET_VEL
    CMD_BUFFER_MOVE_SET_ACC,  // CMD_BUFFER_MOVE_SET_ACC
    CMD_BUFFER_GEAR,          // BUFFER_GEAR
};


// G00(���ٶ�λ)�������
struct _G00PARA {
    float synVel;              // �岹�κϳ��ٶ�
    float synAcc;              // �岹�κϳɼ��ٶ�
    long lX;                   // X�ᵽ��λ�þ���λ��(��λ��pluse)
    long lY;                   // Y�ᵽ��λ�þ���λ��(��λ��pluse)
    long lZ;                   // Z�ᵽ��λ�þ���λ��(��λ��pluse)
    long lA;                   // A�ᵽ��λ�þ���λ��(��λ��pluse)
    unsigned char iDimension;  // ����岹��������
    unsigned char cFuncFlag;   // ��λΪ0X01,��������lDisMask
    long segNum;
    long lB;  // B�ᵽ��λ�þ���λ��(��λ��pluse)(������������ϰ汾��λ�ò�������ƶ�)
    long lDisMask;  // �������룬��ӦλΪ1�������᲻�˶�
};
// G01(ֱ�߲岹)�������(����2��3�ᣬ��λ����֤)
struct _G01PARA {
    float synVel;  // �岹�κϳ��ٶ�
    float synAcc;  // �岹�κϳɼ��ٶ�
    float velEnd;  // �岹�ε��յ��ٶ�
    long lX;       // X�ᵽ��λ�þ���λ��(��λ��pluse)
    long lY;       // Y�ᵽ��λ�þ���λ��(��λ��pluse)
    long lZ;       // Z�ᵽ��λ�þ���λ��(��λ��pluse)
    long lA;       // A�ᵽ��λ�þ���λ��(��λ��pluse)
    long segNum;
    unsigned char iDimension;  // ����岹��������
    unsigned char
        iPreciseStopFlag;  // ��׼��λ��־λ�����Ϊ1���յ㰴���յ�������
    long lB;  // B�ᵽ��λ�þ���λ��(��λ��pluse)(������������ϰ汾��λ�ò�������ƶ�)
};

// G02_G03(˳Բ���岹)�������(����2�ᣬ��λ����֤)
struct _G02_3PARA {
    float synVel;      // �岹�κϳ��ٶ�
    float synAcc;      // �岹�κϳɼ��ٶ�
    float velEnd;      // �岹�ε��յ��ٶ�
    int iPlaneSelect;  // ƽ��ѡ��0��XYƽ�� 1��XZƽ�� 2��YZƽ��
    int iEnd1;         // ��һ���յ����꣨��λum��
    int iEnd2;         // �ڶ����յ����꣨��λum��
    int iI;            // Բ�����꣨��λum��(��������)
    int iJ;            // Բ�����꣨��λum��(��������)
    long segNum;
    unsigned char
        iPreciseStopFlag;  // ��׼��λ��־λ�����Ϊ1���յ㰴���յ�������
};

// G04��ʱ
struct _G04PARA {
    unsigned long ulDelayTime;  // ��ʱʱ��,��λMS
    long segNum;
};

// G05�����û��Զ���κ�
struct _G05PARA {
    long lUserSegNum;  // �û��Զ���κ�
};

// BufferMove�������(���֧��8��)
struct _BufferMoveGearPARA {
    long lAxis1Pos[8];  // ��Ŀ��λ�ã����֧��8�ᡣ��ļ��ٶȺ��ٶȲ��õ�λ�˶��ٶȺͼ��ٶȡ�������봦�ڵ�λģʽ�Ҳ��ǲ岹��
    long lUserSegNum;   // �û��Զ����к�
    unsigned char cAxisMask;  // �����룬bit0������1��bit1������2��.......��ӦλΪ1��������ҪbufferMove
    unsigned char cModalMask;  // �����룬bit0������1��bit1������2��.......��ӦλΪ1��������Ϊ���������ᵽλ��Ž�����һ��
};

// BufferMove����Vel��Acc�������(���֧��8��)
struct _BufferMoveVelAccPARA {
    float dVelAcc[8];         // ���ٶȼ����ٶȣ����֧��8�ᡣ
    long lUserSegNum;         // �û��Զ����к�
    unsigned char cAxisMask;  // �����룬bit0������1��bit1������2��.......��ӦλΪ1��������ҪbufferMove
};

// SetIO��������IO
struct _SetIOPara {
    unsigned short nCarkIndex;  // �忨������0����������1������չ��1��2������չ��2......��������
    unsigned short nDoMask;
    unsigned short nDoValue;
    long lUserSegNum;
};

// SetIO��������IO
struct _SetIOReversePara {
    unsigned short nCarkIndex;  // �忨������0����������1������չ��1��2������չ��2......��������
    unsigned short nDoMask;
    unsigned short nDoValue;
    unsigned short nReverseTime;
    long lUserSegNum;
};

// G�������
union _CMDPara {
    struct _G00PARA G00PARA;
    struct _G01PARA G01PARA;
    struct _G02_3PARA G02_3PARA;
    struct _G04PARA G04PARA;
    struct _G05PARA G05PARA;
    struct _BufferMoveGearPARA BufferMoveGearPARA;
    struct _BufferMoveVelAccPARA BufferMoveVelAccPARA;
    struct _SetIOPara SetIOPara;
};

// ÿһ�г���ṹ��
typedef struct _CrdData {
    unsigned char CMDType;  // ָ�����ͣ�֧�����255��ָ��0��GOO 1��G01 2��G02 FF:�ļ�����
    union _CMDPara CMDPara;  // ָ���������ͬ�����Ӧ��ͬ����
} TCrdData;

// ǰհ�����ṹ��
typedef struct _LookAheadPrm {
    int lookAheadNum;          // ǰհ����
    double dSpeedMax[32];      // ���������ٶ�(p/ms)
    double dAccMax[32];        // ����������ٶ�
    double dMaxStepSpeed[32];  // ���������ٶȱ仯�����൱�������ٶȣ�
    double dScale[32];         // ��������嵱��

    // ���ָ�����һ��Ҫ�ŵ������Ϊָ�������32λϵͳ�³�����32����64λϵͳ�³�����64
    TCrdData* pLookAheadBuf;  // ǰհ������ָ��
} TLookAheadPrm;

// ��������
typedef struct _AxisHomeParm {
    short nHomeMode;  // ���㷽ʽ��0--�� 1--HOME��ԭ��	2--HOME��Index��ԭ��3----Z����
    short nHomeDir;  // ���㷽��1-������㣬0-�������
    long lOffset;    // ����ƫ�ƣ��ص���λ������һ��Offset��Ϊ��λ

    double dHomeRapidVel;  // ��������ٶȣ���λ��Pluse/ms
    double dHomeLocatVel;  // ���㶨λ�ٶȣ���λ��Pluse/ms
    double dHomeIndexVel;  // ����Ѱ��INDEX�ٶȣ���λ��Pluse/ms
    double dHomeAcc;       // ����ʹ�õļ��ٶ�

    long ulHomeIndexDis;  // ��Index������
    long ulHomeBackDis;   // ����ʱ����һ��������λ��Ļ��˾���
    unsigned short nDelayTimeBeforeZero;  // λ������ǰ����ʱʱ�䣬��λms
    unsigned long ulHomeMaxDis;           // �������Ѱ�ҷ�Χ����λ����
} TAxisHomePrm;

// ϵͳ״̬�ṹ��
typedef struct _AllSysStatusData {
    double dAxisEncPos[9];         // �������λ�ã�����һ������
    double dAxisPrfPos[8];         // ��滮λ��
    unsigned long lAxisStatus[8];  // ��״̬
    short nADCValue[2];            // ADCֵ
    long lUserSegNum[2];           // ��������ϵ���û��κ�
    long lRemainderSegNum[2];      // ��������ϵ��ʣ��κ�
    short nCrdRunStatus[2];        // ��������ϵ������ϵ״̬
    long lCrdSpace[2];             // ��������ϵ��ʣ��ռ�
    double dCrdVel[2];             // ��������ϵ���ٶ�
    double dCrdPos[2][5];          // ��������ϵ������
    long lLimitPosRaw;             // ��Ӳ��λ
    long lLimitNegRaw;             // ��Ӳ��λ
    long lAlarmRaw;                // ��������
    long lHomeRaw;                 // ��λ����
    long lMPG;                     // �����ź�
    long lGpiRaw[4];  // ͨ��IO���루�������⣬���֧��3����չģ�飩
} TAllSysStatusData;

// 16������ϵͳ״̬�ṹ��
typedef struct _AllSysStatusDataEX {
    long lAxisEncPos[16];           // �������λ��
    long lAxisPrfPos[16];           // ��滮λ��
    unsigned long lAxisStatus[16];  // ��״̬
    short nADCValue[2];             // ADCֵ
    long lUserSegNum[2];            // ��������ϵ���û��κ�
    long lRemainderSegNum[2];       // ��������ϵ��ʣ��κ�
    short nCrdRunStatus[2];         // ��������ϵ������ϵ״̬
    long lCrdSpace[2];              // ��������ϵ��ʣ��ռ�
    double dCrdVel[2];              // ��������ϵ���ٶ�
    long lCrdPos[2][5];             // ��������ϵ������
    long lLimitPosRaw;              // ��Ӳ��λ
    long lLimitNegRaw;              // ��Ӳ��λ
    long lAlarmRaw;                 // ��������
    long lHomeRaw;                  // ��λ����
    long lMPGEncPos;                // ���ֱ�����
    long lMPG;                      // IO�����ź�
    long lGpiRaw[8];  // ͨ��IO���루�������⣬���֧��7����չģ�飩
} TAllSysStatusDataEX;

// 16������ϵͳ״̬�ṹ��(�����IO)
typedef struct _AllSysStatusDataSX {
    long lAxisEncPos[16];           // �������λ��
    long lAxisPrfPos[16];           // ��滮λ��
    unsigned long lAxisStatus[16];  // ��״̬
    short nADCValue[2];             // ADCֵ
    long lUserSegNum[2];            // ��������ϵ���û��κ�
    short lRemainderSegNum[2];      // ��������ϵ��ʣ��κ�
    short nCrdRunStatus[2];         // ��������ϵ������ϵ״̬
    short lCrdSpace[2];             // ��������ϵ��ʣ��ռ�
    float dCrdVel[2];               // ��������ϵ���ٶ�
    long lCrdPos[2][5];             // ��������ϵ������
    short lLimitPosRaw;             // ��Ӳ��λ
    short lLimitNegRaw;             // ��Ӳ��λ
    short lAlarmRaw;                // ��������
    short lHomeRaw;                 // ��λ����
    long lMPGEncPos;                // ���ֱ�����
    int lMPG;                       // ����IO�ź�
    long lGpiRaw[8];  // ͨ��IO���루�������⣬���֧��7����չģ�飩
    long lGpoRaw[8];  // ͨ��IO������������⣬���֧��7����չģ�飩
} TAllSysStatusDataSX;

// ͨѶ֡ͷ
typedef struct _ComDataFrameHead {
    char nCardNum;               // ���ƿ��������
    char nType;                  // ֡��Ϣ����
    char nSubType;               // ֡��Ϣ������
    char nResult;                // ִ�н��
    unsigned long ulAxisMask;    // ������    �����֧��һ��忨32���ᣩ
    unsigned char nCrdMask;      // ����ϵ���루���֧��8������ϵ��
    unsigned char nFrameCount;   // ��ѯУ��λ������һ֡��1��
    unsigned short nDataBufLen;  // ��Ч�����򳤶�
    unsigned long ulCRC;         // У���
} TComDataFrameHead;

// ǰհ״̬����
typedef struct _LookAheadState {
    int iFirstTime;  // ��һ��ǰհ��־
    int iWriteIndex;  // �����������ȳ���дָ����ܱ仯����ָ��ʼ�������һ֡
    int iNeedLookAhead;  // ��Ҫ��ǰհ����������ǰհ����
    int iNeedAutoSendAllDataInBuf;  // ��Ҫ��ǰհ������������ȫ�����ͳ�ȥ���Զ����ͣ�
    double dTotalLength;  // ��ǰ�ڻ������е��������ݶγ����ܺ�
    double dStartSpeed;  // ǰհ��������ǰ�ٶȣ���һ��Ϊ0������Ϊ������ͳ�ȥ��һ�ε��յ��ٶ�

    double dStartPos[32];  // ����ϵ�ոս���ʱ��������岹��ĵ�ǰλ�á�ÿѹ��һ��������һ��
    double dModalPos
        [32];  // ǰհʱ�õ�����¼������һ���˶���ɺ󣬸��岹��Ӧ�����ڵ�λ��
    double dEndPos[32];  // ���ѹ��Ĳ岹�˽��������λ��

    TCrdDataState* pCrdDataState;  // ָ��岹����״̬��ָ��
    int iNeedLookAheadFlag;        // ��Ҫǰհ��־λ
} TLookAheadState;

// ͨѶ֡
typedef struct _ComDataFrame {
    TComDataFrameHead Head;
    unsigned char nDataBuf[1100];
} TComDataFrame;

// TXPDO��RXPDO����
typedef struct _ECatPDOParm {
    unsigned char cTXPDOCount;  // TXPDO��Ŀ����
    unsigned char cRXPDOCount;  // RXPDO��Ŀ����

    unsigned long lTXPDOItem[10];  // TXPDO��Ŀ
    unsigned long lRXPDOItem[10];  // RXPDO��Ŀ
} TECatPDOParm;

// Delta����
typedef struct _DeltaParm {
    long lPlusePerCircle[3];    // �ؽڵ��ÿȦ����������λ����
    double dRotateAngle[3];     // �ؽ�ƽ�����XZƽ�����ת�Ƕȣ���λ����
    double dDisFixPlatform[3];  // ��ƽ̨���ĵ㵽���ӵ�ĳ��ȣ���λmm
    double dLengthArm1[3];      // ��ؽ�1�ı۳�
    double dLengthArm2[3];      // ��ؽ�2�ı۳�
    double dDisMovPlatform[3];  // ��ƽ̨���ĵ㵽���ӵ�ĳ��ȣ���λmm
} DELTA_PARM;

// XYZAC˫ҡ������
typedef struct _XYZAC_PRRA {
    double dCX;               // C����ת����X���꣬��λmm
    double dCY;               // C����ת����Y���꣬��λmm
    double dAY;               // A����ת����Y���꣬��λmm
    double dAZ;               // A����ת����Z���꣬��λmm
    long lPlusePerCircle[5];  // ����ÿȦ��������
    double dPitch[5];         // �����ݾ�,��λ����
} XYZAC_PARM;

// XYZTATC˫��ͷ����
typedef struct _XYZTATC_PRRA {
    double dDX;         // C�����ʱ����ת�ؽ��������Z��ľ���X,��λmm
    double dDY;         // C�����ʱ����ת�ؽ��������Z��ľ���Y,��λmm
    double dR;          // ĩ����ת�뾶����λmm
    double dOrgAngleC;  // ������ɺ�C��Ƕȣ���λ�ȣ�ȡֵ��Χ0~360
    long lPlusePerCircle[5];  // ����ÿȦ��������
    double dPitch[5];         // �����ݾ�,��λ����
} XYZTATC_PARM;

#define CRDSYS_MAX_COUNT (16)  // �������ϵ����(���ͨ������)
#define AXIS_MAX 8

class MultiCard {
   public:
    MultiCard();

    ~MultiCard();

   private:
    short m_nCardNum;
    short m_nOpenFlag;  // �忨�򿪳ɹ���־λ

    TCrdPrmEx m_LookAheadCrdPrm[CRDSYS_MAX_COUNT];  // ģ���������������ϵ����ʱ���һ������ϵ��������Ҫ��ǰհ��������

    // һ����������ϵ��ÿ������ϵ����ǰհ������
    // ����ǰհ����������
    TLookAheadPrm m_LookAheadPrm[CRDSYS_MAX_COUNT][2];
    // ����ǰհ������״̬
    TLookAheadState mLookAheadState[CRDSYS_MAX_COUNT][2];

    int ComWaitForResponseData(TComDataFrame* pDataFrame, TComDataFrame* pRecFrame);
    int ComSendData(TComDataFrame* pSendFrame, TComDataFrame* pRecFrame);
    int ComSendDataOpen(char* cString, int iLen);
    int WriteFrameToLookAheadBuf(short iCrdIndex, short FifoIndex, TCrdData* pCrdData);
    int InitLookAheadBufCtrlData(short iCrdIndex, short FifoIndex);
    int LookAhead(short iCrdIndex, short FifoIndex);
    int ReadFrameFromLookAheadBuf(short iCrdIndex, short FifoIndex, TCrdData* pCrdData);
    int ClearLookAheadBuf(short iCrdIndex, short FifoIndex);
    double CalConSpeed(short iCrdIndex,
                       short FifoIndex,
                       TCrdDataState* pCurState,
                       TCrdDataState* pNextState,
                       TCrdData* pCrdDataCur,
                       TCrdData* pCrdDataNext);
    int IsLookAheadBufEmpty(short iCrdIndex, short FifoIndex);
    int IsLookAheadBufFull(short iCrdIndex, short FifoIndex);
    float CalculateAngleByRelativePos(double x, double y);
    double CalEndSpeed(double dStartSpeed, double dAccDec, double dLength);
    void GetCoordAfterRotate90(double i, double j, double& iAfterRotate, double& jAfterRotate, int iDir);
    int GetLookAheadBufRemainDataNum(short iCrdIndex, short FifoIndex);
    int ReadPitchErrorTableInfo(short nTableNum, short* pPointNum, long* pStartPos, long* pEndPos);
    int ReadPitchErrorTableValue(
        short nTableNum, short nStartPointNum, short nLen, short* pErrValue1, short* pErrValue2);

    int MC_GetClockHighPrecision(double* pClock);
    int MC_GetClock(double* pClock);
    int MC_LoadConfig(char* pFile);
    int MC_GetConfig();
    int MC_GpiSns(unsigned long sense);
    int MC_GetGpiSns(unsigned long* pSense);
    int MC_GetProfileScale(short iAxis, short* pAlpha, short* pBeta);
    int MC_SetMtrLmt(short dac, short limit);
    int MC_GetMtrLmt(short dac, short* pLimit);
    int MC_PrfFollow(short profile, short dir = 0);
    int MC_SetFollowMaster(short profile, short masterIndex, short masterType, short masterItem);
    int MC_GetFollowMaster(short profile, short* pMasterIndex, short* pMasterType, short* pMasterItem);
    int MC_SetFollowLoop(short profile, short loop);
    int MC_GetFollowLoop(short profile, long* pLoop);
    int MC_SetFollowEvent(short profile, short nEvent, short masterDir, long pos);
    int MC_GetFollowEvent(short profile, short* pEvent, short* pMasterDir, long* pPos);
    int MC_FollowSpace(short profile, short* pSpace, short FifoIndex);
    int MC_FollowData(short profile, long masterSegment, double slaveSegment, short type, short FifoIndex);
    int MC_FollowClear(short profile, short FifoIndex);
    int MC_FollowStart(long mask, long option);
    int MC_FollowSwitch(long mask);
    int MC_SetFollowMemory(short profile, short memory);
    int MC_GetFollowMemory(short profile, short* pMemory);
    int MC_SetPtLoop(short nAxisNum);
    int MC_GetPtLoop(short nAxisNum);
    int MC_PrfPvt(short profile);
    int MC_SetPvtLoop(short profile, long loop);
    int MC_GetPvtLoop(short profile, long* pLoopCount, long* pLoop);
    int MC_PvtTable(short tableId, long lCount, double* pTime, double* pPos, double* pVel);
    int MC_PvtTableComplete(short tableId,
                            long lCount,
                            double* pTime,
                            double* pPos,
                            double* pA,
                            double* pB,
                            double* pC,
                            double velBegin,
                            double velEnd);
    int MC_PvtTablePercent(short tableId, long lCount, double* pTime, double* pPos, double* pPercent, double velBegin);
    int MC_PvtPercentCalculate(
        long lCount, double* pTime, double* pPos, double* pPercent, double velBegin, double* pVel);
    int MC_PvtTableContinuous(short tableId,
                              long lCount,
                              double* pPos,
                              double* pVel,
                              double* pPercent,
                              double* pVelMax,
                              double* pAcc,
                              double* pDec,
                              double timeBegin);
    int MC_PvtContinuousCalculate(long lCount,
                                  double* pPos,
                                  double* pVel,
                                  double* pPercent,
                                  double* pVelMax,
                                  double* pAcc,
                                  double* pDec,
                                  double* pTime);
    int MC_PvtTableSelect(short profile, short tableId);
    int MC_PvtStart(long mask);
    int MC_PvtStatus(short profile, short* pTableId, double* pTime, short nCount);
    int MC_IntConfig(short nCardIndex, short nBitIndex, short nIntLogic);
    int MC_GetIntConfig(short nCardIndex, short nBitIndex, short* nIntLogic);
    int MC_IntEnable(short nCardIndex, GAS_IOCallBackFun IntCallBack);
    int MC_GetControlInfo(short control);

    int MC_ArcXYR(short nCrdNum,
                  long x,
                  long y,
                  double radius,
                  short circleDir,
                  double synVel,
                  double synAcc,
                  double velEnd = 0,
                  short FifoIndex = 0,
                  long segNum = 0);
    int MC_ArcYZR(short nCrdNum,
                  long y,
                  long z,
                  double radius,
                  short circleDir,
                  double synVel,
                  double synAcc,
                  double velEnd = 0,
                  short FifoIndex = 0,
                  long segNum = 0);
    int MC_ArcZXR(short nCrdNum,
                  long z,
                  long x,
                  double radius,
                  short circleDir,
                  double synVel,
                  double synAcc,
                  double velEnd = 0,
                  short FifoIndex = 0,
                  long segNum = 0);
    int MC_BufLaserFollowRatio(
        short nCrdNum, double dRatio, double dMinPower, double dMaxPower, short nFifoIndex, short nChannel);
    int MC_BufLmtsOn(short nCrdNum, short nAxisNum, short limitType, short FifoIndex = 0, long segNum = 0);
    int MC_BufLmtsOff(short nCrdNum, short nAxisNum, short limitType, short FifoIndex = 0, long segNum = 0);
    int MC_BufSetStopIo(short nCrdNum,
                        short nAxisNum,
                        short stopType,
                        short inputType,
                        short inputIndex,
                        short FifoIndex = 0,
                        long segNum = 0);
    int MC_BufGearPercent(short nCrdNum,
                          short gearAxis,
                          long pos,
                          short accPercent,
                          short decPercent,
                          short FifoIndex = 0,
                          long segNum = 0);
    int MC_BufJumpNextSeg(short nCrdNum, short nAxisNum, short limitType, short FifoIndex = 0);
    int MC_BufSynchPrfPos(short nCrdNum, short nEncodeNum, short profile, short FifoIndex = 0);
    int MC_BufVirtualToActual(short nCrdNum, short FifoIndex = 0);
    int MC_BufSetLongVar(short nCrdNum, short index, long value, short FifoIndex = 0);
    int MC_BufSetDoubleVar(short nCrdNum, short index, double value, short FifoIndex = 0);
    int MC_CrdStartStep(short mask, short option);
    int MC_CrdStepMode(short mask, short option);
    int MC_GetUserTargetVel(short nCrdNum, double* pTargetVel);
    int MC_GetSegTargetPos(short nCrdNum, long* pTargetPos);
    int MC_G001PreData(short nCrdNum, long lC, long* plEnd, short FifoIndex = 0, long segNum = -1);
    int MC_EllipticXYCPreData(short nCrdNum, long R1, long R2, short FifoIndex = 0, long segNum = -1);
    int MC_HelixXYRZ(short nCrdNum,
                     long x,
                     long y,
                     long z,
                     double radius,
                     short circleDir,
                     double synVel,
                     double synAcc,
                     double velEnd,
                     short FifoIndex = 0,
                     long segNum = 0);
    int MC_HelixYZRX(short nCrdNum,
                     long x,
                     long y,
                     long z,
                     double radius,
                     short circleDir,
                     double synVel,
                     double synAcc,
                     double velEnd = 0,
                     short FifoIndex = 0,
                     long segNum = 0);
    int MC_HelixZXRY(short nCrdNum,
                     long x,
                     long y,
                     long z,
                     double radius,
                     short circleDir,
                     double synVel,
                     double synAcc,
                     double velEnd = 0,
                     short FifoIndex = 0,
                     long segNum = 0);

    int MC_HomeInit();
    int MC_Home(short nAxisNum, long pos, double vel, double acc, long offset);
    int MC_Index(short nAxisNum, long pos, long offset);
    int MC_HomeSts(short nAxisNum, unsigned short* pStatus);

    int MC_HandwheelInit();
    int MC_SetHandwheelStopDec(short nAxisNum, double decSmoothStop, double decAbruptStop);

    int MC_ECatSendAdoAddr(short nStationNum, short nAdoAddr);
    int MC_ECatSendSdoAddr(short nStationNum, short nSdoIndex, short nSdoSubIndex, short* pPDOFlag, long* pPDOValue);
    int MC_BeginWriteCamTable();
    int MC_EndWriteCamTable();

   public:
    // ����ָ���б�
    int MC_Open(short nCardNum,
                char* cPCEthernetIP,
                unsigned short nPCEthernetPort,
                char* cCardEthernetIP,
                unsigned short nCardEthernetPort);
    int MC_Close(void);
    int MC_Reset();
    int MC_GetVersion(char* pVersion);
    int MC_SetPrfPos(short profile, long prfPos);
    int MC_SynchAxisPos(long mask);
    int MC_ZeroPos(short nAxisNum, short nCount = 1);
    int MC_SetAxisBand(short nAxisNum, long lBand, long lTime);
    int MC_GetAxisBand(short nAxisNum, long* pBand, long* pTime);
    int MC_SetBacklash(short nAxisNum, long lCompValue, double dCompChangeValue, long lCompDir);
    int MC_GetBacklash(short nAxisNum, long* pCompValue, double* pCompChangeValue, long* pCompDir);
    int MC_SendString(char* cString, int iLen, int iOpenFlag = 0);
    int MC_FwUpdate(char* File, unsigned long ulFileLen, int* pProgress);

    // ϵͳ������Ϣ
    int MC_AlarmOn(short nAxisNum);
    int MC_AlarmOff(short nAxisNum);
    int MC_GetAlarmOnOff(short nAxisNum, short* pAlarmOnOff);
    int MC_AlarmSns(unsigned short nSense);
    int MC_GetAlarmSns(unsigned short* pSense);
    int MC_HomeSns(unsigned long sense);
    int MC_GetHomeSns(unsigned long* pSense);
    int MC_LmtsOn(short nAxisNum, short limitType = -1);
    int MC_LmtsOff(short nAxisNum, short limitType = -1);
    int MC_GetLmtsOnOff(short nAxisNum, short* pPosLmtsOnOff, short* pNegLmtsOnOff);
    int MC_LmtSns(unsigned short nSense);
    int MC_LmtSnsEX(unsigned long lSense);
    int MC_GetLmtSns(unsigned long* pSense);
    int MC_SetLmtSnsSingle(short nAxisNum, short nPosSns, short nNegSns);
    int MC_GetLmtSnsSingle(short nAxisNum, short* pPosSns, short* pNegSns);
    int MC_ProfileScale(short nAxisNum, short alpha, short beta);
    int MC_EncScale(short nAxisNum, short alpha, short beta);
    int MC_GetEncScale(short iAxis, short* pAlpha, short* pBeta);
    int MC_StepDir(short step);
    int MC_StepPulse(short step);
    int MC_GetStep(short nAxisNum, short* pStep);
    int MC_StepSns(unsigned long sense);
    int MC_GetStepSns(long* pSense);
    int MC_SetMtrBias(short dac, short bias);
    int MC_GetMtrBias(short dac, short* pBias);
    int MC_EncSns(unsigned long ulSense);
    int MC_GetEncSns(short* pSense);
    int MC_EncOn(short nEncoderNum);
    int MC_EncOff(short nEncoderNum);
    int MC_GetEncOnOff(short nAxisNum, short* pEncOnOff);
    int MC_SetPosErr(short nAxisNum, long lError);
    int MC_GetPosErr(short nAxisNum, long* pError);
    int MC_SetStopDec(short nAxisNum, double decSmoothStop, double decAbruptStop);
    int MC_GetStopDec(short nAxisNum, double* pDecSmoothStop, double* pDecAbruptStop);
    int MC_CtrlMode(short nAxisNum, short mode);
    int MC_GetCtrlMode(short nAxisNum, short* pMode);
    int MC_SetStopIo(short nAxisNum, short stopType, short inputType, short inputIndex);
    int MC_SetAdcFilter(short nAdcNum, short nFilterTime);
    int MC_SetSmoothTime(short nAxisNum, short nSmoothTime);
    int MC_SetAdcBias(short nAdcNum, short nBias);
    int MC_GetAdcBias(short nAdcNum, short* pBias);
    int MC_AxisSMoveEnable(short nAxisNum, double dJ);
    int MC_AxisSMoveDisable(short nAxisNum);
    int MC_CrdSMoveEnable(short nCrdNum, double dJ);
    int MC_CrdSMoveDisable(short nCrdNum);

    // �˶�״̬���ָ���б�
    int MC_GetSts(short nAxisNum, long* pSts, short nCount = 1, unsigned long* pClock = NULL);
    int MC_ClrSts(short nAxisNum, short nCount = 1);
    int MC_GetPrfMode(short profile, long* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_GetPrfPos(short nAxisNum, double* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_GetPrfVel(short nAxisNum, double* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_GetPrfAcc(short nAxisNum, double* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_GetAxisPrfPos(short nAxisNum, double* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_GetAxisPrfVel(short nAxisNum, double* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_GetAxisPrfAcc(short nAxisNum, double* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_GetAxisEncPos(short nAxisNum, double* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_GetAxisEncVel(short nAxisNum, double* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_GetAxisEncAcc(short nAxisNum, double* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_GetAxisError(short nAxisNum, double* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_Stop(long lMask, long lOption);
    int MC_StopEx(long lCrdMask, long lCrdOpion, long lAxisMask0, long lAxisOption0);
    int MC_AxisOn(short nAxisNum);
    int MC_AxisOff(short nAxisNum);
    int MC_GetAllSysStatus(TAllSysStatusData* pAllSysStatusData);
    int MC_GetAllSysStatusEX(TAllSysStatusDataEX* pAllSysStatusData);
    int MC_GetAllSysStatusSX(TAllSysStatusDataSX* pAllSysStatusData);

    // ��λ�˶�ָ���б���������λ���ٶ�ģʽ��
    int MC_PrfTrap(short nAxisNum);
    int MC_SetTrapPrm(short nAxisNum, TTrapPrm* pPrm);
    int MC_SetTrapPrmSingle(short nAxisNum, double dAcc, double dDec, double dVelStart, short dSmoothTime);
    int MC_GetTrapPrm(short nAxisNum, TTrapPrm* pPrm);
    int MC_GetTrapPrmSingle(short nAxisNum, double* dAcc, double* dDec, double* dVelStart, short* dSmoothTime);
    int MC_PrfJog(short nAxisNum);
    int MC_SetJogPrm(short nAxisNum, TJogPrm* pPrm);
    int MC_SetJogPrmSingle(short nAxisNum, double dAcc, double dDec, double dSmooth);
    int MC_GetJogPrm(short nAxisNum, TJogPrm* pPrm);
    int MC_GetJogPrmSingle(short nAxisNum, double* dAcc, double* dDec, double* dSmooth);
    int MC_SetPos(short nAxisNum, long pos);
    int MC_GetPos(short nAxisNum, long* pPos);
    int MC_SetVel(short nAxisNum, double vel);
    int MC_GetVel(short nAxisNum, double* pVel);
    int MC_SetMultiVel(short nAxisNum, double* pVel, short nCount = 1);
    int MC_SetMultiPos(short nAxisNum, long* pPos, short nCount = 1);
    int MC_Update(long mask);
    int MC_SetTrapPosAndUpdate(short nAxisNum,
                               long long llPos,
                               double dVel,
                               double dAcc,
                               double dDec,
                               double dVelStart,
                               short nSmoothTime,
                               short nBlock);

    // ���ӳ���ģʽָ���б�
    int MC_PrfGear(short nAxisNum, short dir = 0);
    int MC_SetGearMaster(short nAxisNum, short nMasterAxisNum, short masterType = GEAR_MASTER_PROFILE);
    int MC_GetGearMaster(short nAxisNum, short* nMasterAxisNum, short* pMasterType = NULL);
    int MC_SetGearRatio(
        short nAxisNum, long masterEven, long slaveEven, long masterSlope = 0, long lStopSmoothTime = 200);
    int MC_GetGearRatio(
        short nAxisNum, long* pMasterEven, long* pSlaveEven, long* pMasterSlope = NULL, long* pStopSmoothTime = NULL);
    int MC_GearStart(long mask);
    int MC_GearStop(long lAxisMask, long lEMGMask);
    int MC_SetGearEvent(short nAxisNum, short nEvent, double startPara0, double startPara1);
    int MC_GetGearEvent(short nAxisNum, short* pEvent, double* pStartPara0, double* pStartPara1);
    int MC_SetGearIntervalTime(short nAxisNum, short nIntervalTime);
    int MC_GetGearIntervalTime(short nAxisNum, short* nIntervalTime);
    int MC_GearSetMaxVel(short nAxisNum, double dMaxVel);

    // ����͹��ģʽָ���б�
    int MC_PrfCam(short nAxisNum, short nTableNum);
    int MC_SetCamMaster(short nAxisNum, short nMasterAxisNum, short nMasterType);
    int MC_GetCamMaster(short nAxisNum, short* pnMasterAxisNum, short* pMasterType);
    int MC_SetCamEvent(short nAxisNum, short nEvent, double startPara0, double startPara1);
    int MC_GetCamEvent(short nAxisNum, short* pEvent, double* pStartPara0, double* pStartPara1);
    int MC_SetCamIntervalTime(short nAxisNum, short nIntervalTime);
    int MC_GetCamIntervalTime(short nAxisNum, short* nIntervalTime);
    int MC_SetUpCamTable(short nCamTableNum, long lMasterValueMax, long* plSlaveCamData, long lCamTableLen);
    int MC_SetUpCamTableByKeyPoint(
        short nCamTableNum, long* plData, long lKeyPointNum, short nDynamicStaticFlag, short nNextCycleFlag);
    int MC_DownCamTable(short nTableNum, int* pProgress);
    int MC_CamStart(long lMask);
    int MC_CamStop(long lAxisMask, long lEMGMask);

    // PTģʽָ���б�
    int MC_PrfPt(short nAxisNum, short mode = PT_MODE_STATIC);
    int MC_PtSpace(short nAxisNum, long* pSpace, short nCount);
    int MC_PtRemain(short nAxisNum, long* pRemainSpace, short nCount);
    int MC_PtData(short nAxisNum, short* pData, long lLength, double dDataID);
    int MC_PtClear(long lAxisMask);
    int MC_PtStart(long lAxisMask);

    // �岹�˶�ģʽָ���б�
    int MC_StartDebugLog(short nFlag);
    int MC_StopDebugLog();
    int MC_SetCrdPrm(short nCrdNum, TCrdPrm* pCrdPrm);
    int MC_SetCrdPrmEX(short nCrdNum, TCrdPrmEx* pCrdPrm);
    int MC_GetCrdPrm(short nCrdNum, TCrdPrm* pCrdPrm);
    int MC_SetCrdPrmSingle(short nCrdNum,
                           short dimension,
                           short* profile,
                           double synVelMax,
                           double synAccMax,
                           short evenTime,
                           short setOriginFlag,
                           long* originPos);
    int MC_SetAddAxis(short nAxisNum, short nAddAxisNum);
    int MC_SetCrdOffset(
        short nCrdNum, long lOffsetX, long lOffsetY, long lOffsetZ, long lOffsetA, long lOffsetB, double dOffsetAngle);
    int MC_SetCrdStopDec(short nCrdNum, double decSmoothStop, double decAbruptStop);
    int MC_GetCrdStopDec(short nCrdNum, double* pDecSmoothStop, double* pDecAbruptStop);
    int MC_SetConstLinearVelFlag(short nCrdNum, short nFifoIndex, short nConstLinearVelFlag, long lRotateAxisMask);
    int MC_InitLookAhead(short nCrdNum, short FifoIndex, TLookAheadPrm* plookAheadPara);
    int MC_InitLookAheadSingle(short nCrdNum,
                               short FifoIndex,
                               int lookAheadNum,
                               double* dSpeedMax,
                               double* dAccMax,
                               double* dMaxStepSpeed,
                               double* dScale);
    int MC_CrdClear(short nCrdNum, short FifoIndex);
    int MC_LnXYG0(short nCrdNum, long x, long y, double synVel, double synAcc, short FifoIndex = 0, long segNum = 0);
    int MC_LnXYZG0(
        short nCrdNum, long x, long y, long z, double synVel, double synAcc, short FifoIndex = 0, long segNum = 0);
    int MC_LnXYZAG0(short nCrdNum,
                    long x,
                    long y,
                    long z,
                    long a,
                    double synVel,
                    double synAcc,
                    short FifoIndex = 0,
                    long segNum = 0);
    int MC_LnXYZABG0(short nCrdNum,
                     long x,
                     long y,
                     long z,
                     long a,
                     long b,
                     double synVel,
                     double synAcc,
                     short FifoIndex = 0,
                     long segNum = 0);
    int MC_LnXYZABCG0(short nCrdNum,
                      long x,
                      long y,
                      long z,
                      long a,
                      long b,
                      long c,
                      double synVel,
                      double synAcc,
                      short FifoIndex = 0,
                      long segNum = 0);
    int MC_LnAllG0(
        short nCrdNum, long* pPos, short nDim, double synVel, double synAcc, short FifoIndex = 0, long segNum = 0);
    int MC_LnX(
        short nCrdNum, long x, double synVel, double synAcc, double velEnd = 0, short FifoIndex = 0, long segNum = 0);
    int MC_LnXY(short nCrdNum,
                long x,
                long y,
                double synVel,
                double synAcc,
                double velEnd = 0,
                short FifoIndex = 0,
                long segNum = 0);
    int MC_LnXYZ(short nCrdNum,
                 long x,
                 long y,
                 long z,
                 double synVel,
                 double synAcc,
                 double velEnd = 0,
                 short FifoIndex = 0,
                 long segNum = 0);
    int MC_LnXYZA(short nCrdNum,
                  long x,
                  long y,
                  long z,
                  long a,
                  double synVel,
                  double synAcc,
                  double velEnd = 0,
                  short FifoIndex = 0,
                  long segNum = 0);
    int MC_LnXYZAB(short nCrdNum,
                   long x,
                   long y,
                   long z,
                   long a,
                   long b,
                   double synVel,
                   double synAcc,
                   double velEnd = 0,
                   short FifoIndex = 0,
                   long segNum = 0);
    int MC_LnXYZABC(short nCrdNum,
                    long x,
                    long y,
                    long z,
                    long a,
                    long b,
                    long c,
                    double synVel,
                    double synAcc,
                    double velEnd = 0,
                    short FifoIndex = 0,
                    long segNum = 0);
    int MC_LnAll(short nCrdNum,
                 long* pPos,
                 short nDim,
                 double synVel,
                 double synAcc,
                 double velEnd,
                 short FifoIndex = 0,
                 long segNum = 0);
    int MC_LnXYCmpPluse(short nCrdNum,
                        long x,
                        long y,
                        double synVel,
                        double synAcc,
                        double velEnd,
                        short nChannelMask,
                        short nPluseType,
                        short nTime,
                        short nTimerFlag,
                        short FifoIndex = 0,
                        long segNum = -1);
    int MC_ArcXYC(short nCrdNum,
                  long x,
                  long y,
                  double xCenter,
                  double yCenter,
                  short circleDir,
                  double synVel,
                  double synAcc,
                  double velEnd = 0,
                  short FifoIndex = 0,
                  long segNum = 0);
    int MC_ArcYZC(short nCrdNum,
                  long y,
                  long z,
                  double yCenter,
                  double zCenter,
                  short circleDir,
                  double synVel,
                  double synAcc,
                  double velEnd = 0,
                  short FifoIndex = 0,
                  long segNum = 0);
    int MC_ArcXZC(short nCrdNum,
                  long x,
                  long z,
                  double xCenter,
                  double zCenter,
                  short circleDir,
                  double synVel,
                  double synAcc,
                  double velEnd = 0,
                  short FifoIndex = 0,
                  long segNum = 0);
    int MC_HelixPreData(short nCrdNum, long x, long y, long z, short FifoIndex = 0, long segNum = -1);
    int MC_HelixXYCZ(short nCrdNum,
                     long x,
                     long y,
                     long z,
                     double xCenter,
                     double yCenter,
                     float k,
                     short circleDir,
                     double synVel,
                     double synAcc,
                     double velEnd = 0,
                     short FifoIndex = 0,
                     long segNum = -1);
    int MC_HelixYZCX(short nCrdNum,
                     long x,
                     long y,
                     long z,
                     double yCenter,
                     double zCenter,
                     float k,
                     short circleDir,
                     double synVel,
                     double synAcc,
                     double velEnd = 0,
                     short FifoIndex = 0,
                     long segNum = -1);
    int MC_HelixXZCY(short nCrdNum,
                     long x,
                     long y,
                     long z,
                     double xCenter,
                     double zCenter,
                     float k,
                     short circleDir,
                     double synVel,
                     double synAcc,
                     double velEnd = 0,
                     short FifoIndex = 0,
                     long segNum = -1);
    int MC_HelixXYCCount(short nCrdNum,
                         double xCenter,
                         double yCenter,
                         float k,
                         float CirlceCount,
                         short circleDir,
                         double synVel,
                         double synAcc,
                         double velEnd = 0,
                         short FifoIndex = 0,
                         long segNum = -1);
    int MC_HelixXZCCount(short nCrdNum,
                         double xCenter,
                         double zCenter,
                         float k,
                         float CirlceCount,
                         short circleDir,
                         double synVel,
                         double synAcc,
                         double velEnd = 0,
                         short FifoIndex = 0,
                         long segNum = -1);
    int MC_HelixYZCCount(short nCrdNum,
                         double yCenter,
                         double zCenter,
                         float k,
                         float CirlceCount,
                         short circleDir,
                         double synVel,
                         double synAcc,
                         double velEnd = 0,
                         short FifoIndex = 0,
                         long segNum = -1);
    int MC_BufIO(short nCrdNum,
                 unsigned short nDoType,
                 unsigned short nCardIndex,
                 unsigned short doMask,
                 unsigned short doValue,
                 short FifoIndex = 0,
                 long segNum = 0);
    int MC_BufIOEx(short nCrdNum,
                   unsigned short nDoType,
                   unsigned short nCardIndex,
                   unsigned long doMask,
                   unsigned long doValue,
                   short FifoIndex = 0,
                   long segNum = 0);
    int MC_BufIOReverse(short nCrdNum,
                        unsigned short nDoType,
                        unsigned short nCardIndex,
                        unsigned short doMask,
                        unsigned short doValue,
                        unsigned short nReverseTime,
                        short FifoIndex = 0,
                        long segNum = 0);
    int MC_BufWaitIO(short nCrdNum,
                     unsigned short nCardIndex,
                     unsigned short nIOPortIndex,
                     unsigned short nLevel,
                     unsigned long lWaitTimeMS,
                     unsigned short nFilterTime,
                     short FifoIndex = 0,
                     long segNum = -1);
    int MC_BufDelay(short nCrdNum, unsigned long ulDelayTime, short FifoIndex = 0, long segNum = 0);
    int MC_BufPWM(short nCrdNum, short nPwmNum, double dFreq, double dDuty, short nFifoIndex, long lUserSegNum = -1);
    int MC_BufDA(short nCrdNum, short nDacNum, short nValue, short nFifoIndex, long lUserSegNum = -1);
    int MC_BufZeroPos(short nCrdNum, short nAxisNum, short nFifoIndex, long lUserSegNum = -1);
    int MC_BufSetM(short nCrdNum, int iMAddr, short nMValue, short nFifoIndex, long lUserSegNum = -1);
    int MC_BufWaitM(short nCrdNum, int iMAddr, short nMValue, short nFifoIndex, long lUserSegNum = -1);
    int MC_LnXYZABMaskG0(short nCrdNum,
                         long x,
                         long y,
                         long z,
                         long a,
                         long b,
                         long lEnableMask,
                         double synVel,
                         double synAcc,
                         short FifoIndex = 0,
                         long segNum = -1);
    int MC_CrdData(short nCrdNum, void* pCrdData, short FifoIndex = 0);
    int MC_CrdStart(short mask, short option);
    int MC_SetOverride(short nCrdNum, double synVelRatio);
    int MC_G00SetOverride(short nCrdNum, double synVelRatio);
    int MC_GetCrdPos(short nCrdNum, double* pPos);
    int MC_GetCrdVel(short nCrdNum, double* pSynVel);
    int MC_CrdSpace(short nCrdNum, long* pSpace, short FifoIndex = 0);
    int MC_CrdStatus(short nCrdNum, short* pCrdStatus, long* pSegment, short FifoIndex = 0);
    int MC_SetUserSegNum(short nCrdNum, long segNum, short FifoIndex = 0);
    int MC_GetUserSegNum(short nCrdNum, long* pSegment, short FifoIndex = 0);
    int MC_GetRemainderSegNum(short nCrdNum, long* pSegment, short FifoIndex = 0);
    int MC_GetLookAheadSegCount(short nCrdNum, long* pSegCount, short FifoIndex = 0);
    int MC_GetLookAheadSpace(short nCrdNum, long* pSpace, short FifoIndex = 0);
    int MC_BufCmpData(short nCrdNum,
                      short nCmpEncodeNum,
                      short nPluseType,
                      short nStartLevel,
                      short nTime,
                      long* pBuf,
                      short nBufLen,
                      short nAbsPosFlag,
                      short nTimerFlag,
                      short nFifoIndex,
                      long lSegNum = -1);
    int MC_BufCmpPluse(short nCrdNum,
                       short nChannel,
                       short nPluseType,
                       short nTime,
                       short nTimerFlag,
                       short nFifoIndex,
                       long lSegNum = -1);
    int MC_BufCmpRpt(short nCrdNum,
                     short nCmpNum,
                     unsigned long lIntervalTime,
                     short nTime,
                     short nTimeFlag,
                     unsigned long ulRptTime,
                     short FifoIndex = 0,
                     long segNum = -1);
    int MC_BufMoveVel(short nCrdNum, short nAxisMask, float* pVel, short nFifoIndex = 0, long lSegNum = -1);
    int MC_BufMoveVelSingle(short nCrdNum,
                            short nAxisMask,
                            float dVel0,
                            float dVel1,
                            float dVel2,
                            float dVel3,
                            float dVel4,
                            float dVel5,
                            float dVel6,
                            float dVel7,
                            short nFifoIndex = 0,
                            long lSegNum = -1);
    int MC_BufMoveVelEX(short nCrdNum, short nAxisMask, float* pVel, short nFifoIndex = 0, long lSegNum = -1);
    int MC_BufMoveVel32(short nCrdNum, long lAxisMask, float* pVel, short nFifoIndex = 0, long lSegNum = -1);
    int MC_BufMoveAcc(short nCrdNum, short nAxisMask, float* pAcc, short nFifoIndex = 0, long lSegNum = -1);
    int MC_BufMoveAccSingle(short nCrdNum,
                            short nAxisMask,
                            float dAcc0,
                            float dAcc1,
                            float dAcc2,
                            float dAcc3,
                            float dAcc4,
                            float dAcc5,
                            float dAcc6,
                            float dAcc7,
                            short nFifoIndex = 0,
                            long lSegNum = -1);
    int MC_BufMoveAccEX(short nCrdNum, short nAxisMask, float* pAcc, short nFifoIndex = 0, long lSegNum = -1);
    int MC_BufMoveAcc32(short nCrdNum, long lAxisMask, float* pAcc, short nFifoIndex = 0, long lSegNum = -1);
    int MC_BufMoveDec(short nCrdNum, short nAxisMask, float* pDec, short nFifoIndex = 0, long lSegNum = -1);
    int MC_BufMoveDecEX(short nCrdNum, short nAxisMask, float* pDec, short nFifoIndex = 0, long lSegNum = -1);
    int MC_BufMoveDec32(short nCrdNum, long lAxisMask, float* pDec, short nFifoIndex = 0, long lSegNum = -1);
    int MC_BufMove(short nCrdNum, short nAxisMask, long* pPos, short nModalMask, short nFifoIndex, long lSegNum);
    int MC_BufMoveSingle(short nCrdNum,
                         short nAxisMask,
                         long lPos0,
                         long lPos1,
                         long lPos2,
                         long lPos3,
                         long lPos4,
                         long lPos5,
                         long lPos6,
                         long lPos7,
                         short nModalMask,
                         short nFifoIndex = 0,
                         long lSegNum = -1);
    int MC_BufMoveEX(short nCrdNum, short nAxisMask, long* pPos, short nModalMask, short nFifoIndex, long lSegNum);
    int MC_BufMove32(short nCrdNum, long lAxisMask, long* pPos, long lModalMask, short nFifoIndex, long lSegNum);
    int MC_BufGear(short nCrdNum, short nAxisMask, long* pPos, short nFifoIndex, long lSegNum);
    int MC_BufGearSingle(short nCrdNum,
                         short nAxisMask,
                         long lPos0,
                         long lPos1,
                         long lPos2,
                         long lPos3,
                         long lPos4,
                         long lPos5,
                         long lPos6,
                         long lPos7,
                         short nFifoIndex = 0,
                         long lSegNum = -1);
    int MC_BufJog(
        short nCrdNum, short nAxisNum, double dAccDec, double dVel, short nBlock, short nFifoIndex, long lUserSegNum);
    int MC_GetCrdErrStep(short nCrdNum, unsigned long* pulErrStep);
    int MC_EllipticXYC(short nCrdNum,
                       long x,
                       long y,
                       double xCenter,
                       double yCenter,
                       short circleDir,
                       long R1,
                       long R2,
                       double synVel,
                       double synAcc,
                       double velEnd = 0,
                       short FifoIndex = 0,
                       long segNum = -1);

    // ����Ӳ����Դָ���б�
    int MC_GetDi(short nDiType, long* pValue);
    int MC_GetDiRaw(short nDiType, long* pValue);
    int MC_GetDiReverseCount(short nDiType,
                             short diIndex,
                             unsigned long* pReverseCount,
                             short nCount = 1,
                             short nCardIndex = 0,
                             short nRiseSinkType = 0);
    int MC_SetDiReverseCount(
        short nDiType, short diIndex, unsigned long ReverseCount, short nCount = 1, short nCardIndex = 0);
    int MC_SetDo(short nDoType, long value);
    int MC_SetDoBit(short nDoType, short nDoNum, short value);
    int MC_SetDoBitReverse(short nDoType, short nDoNum, short nValue, short nReverseTime);
    int MC_SetDoBitReverseEx(unsigned short nCardIndex, short nDoType, short nDoNum, short nValue, short nReverseTime);
    int MC_GetDo(short nDoType, long* pValue);
    int MC_GetEncPos(short nEncodeNum, double* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_GetEncVel(short nEncodeNum, double* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_SetEncPos(short nEncodeNum, long encPos);
    int MC_GetMPGVel(double* pValue, unsigned long* pClock = NULL);
    int MC_SetMPGPos(long lMPGPos);
    int MC_SetDac(short nDacNum, short* pValue, short nCount = 1);
    int MC_GetAdc(short nADCNum, short* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_SetPwm(short nPwmNum, double dFreq, double dDuty);
    int MC_GetPwm(short nPwmNum, double* pFreq, double* pDuty);
    int MC_SetExtDoValue(short nCardIndex, unsigned long* value, short nCount = 1);
    int MC_GetExtDiValue(short nCardIndex, unsigned long* pValue, short nCount = 1);
    int MC_GetExtDoValue(short nCardIndex, unsigned long* pValue, short nCount = 1);
    int MC_SetExtDoBit(short nCardIndex, short nBitIndex, unsigned short nValue);
    int MC_GetExtDiBit(short nCardIndex, short nBitIndex, unsigned short* pValue);
    int MC_GetExtDoBit(short nCardIndex, short nBitIndex, unsigned short* pValue);
    int MC_SetExtDiFilter(short nCardIndex, short FilterTime, long ulIOMask);
    int MC_SendEthToUartString(short nUartNum, unsigned char* pSendBuf, short nLength);
    int MC_ReadUartToEthString(short nUartNum, unsigned char* pRecvBuf, short* pLength);
    int MC_SetExDac(short nCardIndex, short nDacNum, short* pValue, short nCount = 1);
    int MC_GetExAdc(short nCardIndex, short nADCNum, short* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_SetIOEventTrigger(short nEventNum,
                             short nIOIndex,
                             short nTriggerSense,
                             long lFilterTimer,
                             short nEventType,
                             double dEventParm1,
                             double dEventParm2);
    int MC_GetIOEventTrigger(short nEventNum, short* pTriggerFlag, short nCount);
    int MC_GetDac(short nDacNum, short* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_GetExDac(short nCardIndex, short nDacNum, short* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_UartConfig(unsigned short nUartNum,
                      unsigned long uLBaudRate,
                      unsigned short nDataLength,
                      unsigned short nVerifyType,
                      unsigned short nStopBitLen);
    int MC_SetPWMOffset(short nPwmNum, short nOffset);
    int MC_GpioCmpBufData(short nCmpGpoIndex,
                          short nCmpSource,
                          short nPluseType,
                          short nStartLevel,
                          short nTime,
                          short nTimerFlag,
                          short nAbsPosFlag,
                          short nBufLen,
                          long* pBuf);
    int MC_GpioCmpBufSts(unsigned short* pStatus, unsigned short* pRemainData, unsigned short* pRemainSpace);
    int MC_GpioCmpBufStop(long* pGpioMask, short nCount);

    // �Ƚ����ָ��
    int MC_CmpPluse(short nChannelMask,
                    short nPluseType1,
                    short nPluseType2,
                    short nTime1,
                    short nTime2,
                    short nTimeFlag1,
                    short nTimeFlag2);
    int MC_CmpBufSetChannel(short nBuf1ChannelNum, short nBuf2ChannelNum);
    int MC_CmpBufData(short nCmpEncodeNum,
                      short nPluseType,
                      short nStartLevel,
                      short nTime,
                      long* pBuf1,
                      short nBufLen1,
                      long* pBuf2,
                      short nBufLen2,
                      short nAbsPosFlag = 0,
                      short nTimerFlag = 0);
    int MC_CmpBufSts(unsigned short* pStatus,
                     unsigned short* pRemainDaga1,
                     unsigned short* pRemainDaga2,
                     unsigned short* pRemainSpace1,
                     unsigned short* pRemainSpace2);
    int MC_CmpBufStop(short nChannelMask);
    int MC_CmpSetHighSpeedIOTrigger(
        short nChannelNum, short nTriggerSense, short nPluseType, short nStartLevel, short nTime, short nTimerFlag = 0);
    int MC_CmpRstFpgaCount(unsigned long ulMask);
    int MC_CmpGetFpgaCount(unsigned short* pFPGACount, unsigned short nCount);
    int MC_CmpSetTriggerCount(long lTriggerCount1, long lTriggerCount2);
    int MC_CmpGetTriggerCount(long* plTriggerCount1, long* plTriggerCount2);
    int MC_CmpRpt(short nCmpNum, unsigned long lIntervalTime, short nTime, short nTimeFlag, unsigned long ulRptTime);
    int MC_CmpBufRpt(short nEncNum,
                     short nDir,
                     short nEncFlag,
                     long lTrigValue,
                     short nCmpNum,
                     unsigned long lIntervalTime,
                     short nTime,
                     short nTimeFlag,
                     unsigned long ulRptTime);

    // ����Ӳ������ָ���б�
    int MC_SetCaptureMode(short nEncodeNum, short mode);
    int MC_GetCaptureMode(short nEncodeNum, short* pMode, short nCount = 1);
    int MC_GetCaptureStatus(
        short nEncodeNum, short* pStatus, long* pValue, short nCount = 1, unsigned long* pClock = NULL);
    int MC_SetCaptureSense(short nEncodeNum, short mode, short sense);
    int MC_GetCaptureSense(short nEncodeNum, short mode, short* sense);
    int MC_ClearCaptureStatus(short nEncodeNum);
    int MC_SetContinueCaptureMode(short nEncodeNum, short nMode, short nContinueMode, short nFilterTime);
    int MC_GetContinueCaptureData(short nEncodeNum, long* pCapturePos, short* pCaptureCount);

    // ��ȫ����ָ���б�
    int MC_SetSoftLimit(short nAxisNum, long lPositive, long lNegative);
    int MC_GetSoftLimit(short nAxisNum, long* pPositive, long* pNegative);
    int MC_SetHardLimP(short nAxisNum, short nType, short nCardIndex, short nIOIndex);
    int MC_SetHardLimN(short nAxisNum, short nType, short nCardIndex, short nIOIndex);
    int MC_EStopSetIO(short nCardIndex, short nIOIndex, short nEStopSns, unsigned long lFilterTime);
    int MC_EStopOnOff(short nEStopOnOff);
    int MC_EStopGetSts(short* nEStopSts);
    int MC_EStopClrSts();
    int MC_EStopConfig(unsigned int ulEnableMask,
                       unsigned int ulEnableValue,
                       short nAdcMask,
                       short nAdcValue,
                       unsigned int ulIOMask,
                       unsigned int ulIOValue);
    int MC_CrdHlimEnable(short nCrdNum, short nEnableFlag);

    // �Զ��������API
    int MC_HomeStart(short nAxisNum);
    int MC_HomeStop(short nAxisNum);
    int MC_HomeSetPrm(short nAxisNum, TAxisHomePrm* pAxisHomePrm);
    int MC_HomeSetPrmSingle(short iAxisNum,
                            short nHomeMode,
                            short nHomeDir,
                            long lOffset,
                            double dHomeRapidVel,
                            double dHomeLocatVel,
                            double dHomeIndexVel,
                            double dHomeAcc);
    int MC_HomeGetPrm(short nAxisNum, TAxisHomePrm* pAxisHomePrm);
    int MC_HomeGetPrmSingle(short nAxisNum,
                            short* nHomeMode,
                            short* nHomeDir,
                            long* lOffset,
                            double* dHomeRapidVel,
                            double* dHomeLocatVel,
                            double* dHomeIndexVel,
                            double* dHomeAcc);
    int MC_HomeGetSts(short nAxisNum, unsigned short* pStatus);
    int MC_HomeGetFailReason(short nAxisNum, unsigned short* pFailReason);
    int MC_HomeSetFinishFlag(short nAxisNum, unsigned short nValue);

    // �������
    int MC_StartHandwheel(short nAxisNum,
                          short nMasterAxisNum = 9,
                          long lMasterEven = 1,
                          long lSlaveEven = 1,
                          short nIntervalTime = 0,
                          double dAcc = 0.1,
                          double dDec = 0.1,
                          double dVel = 50,
                          short nStopWaitTime = 0);
    int MC_EndHandwheel(short nAxisNum);

    // �������
    int MC_LaserPowerMode(short nChannelIndex, short nPowerMode, double dMaxValue, double dMinValue, short nDelayMode);
    int MC_LaserSetPower(short nChannelIndex, double dPower);
    int MC_LaserOn(short nChannelIndex);
    int MC_LaserOff(short nChannelIndex);
    int MC_LaserGetPowerAndOnOff(short nChannelIndex, double* dPower, short* pOnOff);
    int MC_LaserFollowRatio(
        short nChannelIndex, double dMinSpeed, double dMaxSpeed, double dMinPower, double dMaxPower, short nFifoIndex);

    // EtherCAT�������
    int MC_ECatInit();
    int MC_ECatGetInitStep(short* pCutInitSlaveNum, short* pMode, short* pModeStep);
    int MC_ECatGetSlaveCount(short* pCount);
    int MC_ECatSetPluseAxisNum(short* pAxisNum, short nCount);
    int MC_ECatSetAdoValue(short nStationNum, short nAdoAddr, short nAdoValue);
    int MC_ECatGetAdoValue(short nStationNum, short nAdoAddr, short* pAdoValue);
    int MC_ECatSetSdoValue(short nStationNum, short nSdoIndex, short nSdoSubIndex, long lSdoValue, short nLen);
    int MC_ECatGetSdoValue(short nStationNum,
                           short nSdoIndex,
                           short nSdoSubIndex,
                           long* pSdoValue,
                           short* pPdoFlag,
                           short nLen,
                           short nSignFlag);
    int MC_ECatSetProbeCaptureStart(short nStationNum,
                                    short nProbeNum,
                                    short nProbeSource,
                                    short nProbeSense,
                                    short nContinueFlag,
                                    short nAutoStopFlag);
    int MC_ECatGetProbeCaptureStatus(short nStationNum, short nProbeNum, short* nSatus, long* pValueP, long* pValueF);
    int MC_ECatSetPDOConfig(short nStationNum, short nGroupNum, TECatPDOParm* pEcatPrm);
    int MC_ECatGetPDOConfig(short nStationNum, short nGroupNum, TECatPDOParm* pEcatPrm);
    int MC_ECatResetPDOConfig(short nStationNum);
    int MC_ECatLoadPDOConfig(short nStationNum);
    int MC_ECatSetCtrlBit(short nStationNum, unsigned long ulMask, unsigned long ulValue);
    int MC_ECatSetCtrlMode(short nStationNum, unsigned short nMode);
    int MC_ECatHomeStart(short nStationNum,
                         short nHomeMode,
                         double dHomeRapidVel,
                         double dHomeLocatVel,
                         double dHomeAcc,
                         long lOffset,
                         unsigned short nDelayTime);
    int MC_ECatHomeStop(unsigned long ulAxisMask);
    int MC_ECatGetStatusWord(short nStationNum, short* pEcatStatusValue);
    int MC_ECatSetAllPDOData(short nStationNum, unsigned char* pData, short nLen);
    int MC_ECatGetAllPDOData(short nStationNum, unsigned char* pData, short* pLen);

    int MC_ECatSetOrgPosAbs(short nStationNum, long lOrgPosAbs);
    int MC_ECatSetOrgPosCur(short nStationNum);
    int MC_ECatGetOrgPosAbs(short nStationNum, long* plOrgPosAbs);
    int MC_ECatLoadOrgPosAbs(short nStationNum);
    int MC_ECatGetDCOffset(long* lDCOffset);
    int MC_ECatSetDCOffset(short nStationNum, long lDCOffset);
    int MC_ECatSetPlusePerCircle(short nAxisNum, long long lPlusePerCircleOrg, long long lPlusePerCircle);

    // Robot���API
    int MC_RobotSetPrm(unsigned short RobotID,
                       unsigned long ulRobotType,
                       short nJogAxisCount,
                       short* pJogAxisList,
                       short nVirAxisCount,
                       short* pVirAxisList,
                       void* RobotParm);
    int MC_RobotSetForward(short nRobotID);
    int MC_RobotSetInverse(short nRobotID);

    // Dxf���API
    int MC_DxfLoadFile(char* pFile);
    int MC_DxfGetCircleCount(short nLayerIndex, long* pCount);
    int MC_DxfGetMultiLineCount(short nLayerIndex, long* pCount);
    int MC_DxfGetCircleCenterR(short nLayerIndex, long lCricleIndex, double* dX, double* dY, double* dZ, double* dR);
    int MC_DxfGetCircleAllCenterR(short nLayerIndex, double* dX, double* dY, double* dZ, double* dR, long* pCount);
    int MC_DxfGetMultiLineInfo(short nLayerIndex,
                               long lMultiLineIndex,
                               double* dSX,
                               double* dSY,
                               double* dSZ,
                               double* dEX,
                               double* dEY,
                               double* dEZ,
                               long* pCount);
    int MC_DxfGetMultiLinePoint(short nLayerIndex,
                                long lMultiLineIndex,
                                long lPointIndex,
                                double* dX,
                                double* dY,
                                double* dZ,
                                short* nArcFlag,
                                short* nArcDir,
                                double* dCenterX,
                                double* dCenterY,
                                double* dR);
    int MC_DxfGetMultiLineAllPoint(short nLayerIndex,
                                   long lMultiLineIndex,
                                   double* dX,
                                   double* dY,
                                   double* dZ,
                                   short* nArcFlag,
                                   short* nArcDir,
                                   double* dCenterX,
                                   double* dCenterY,
                                   double* dR,
                                   long* pCount);

    // ����API
    int MC_GetIP(unsigned long* pIP);
    int MC_SetIP(unsigned long ulIP);
    int MC_GetID(unsigned long* pID);
    int MC_WriteInterFlash(unsigned char* pData, short nLength);
    int MC_ReadInterFlash(unsigned char* pData, short nLength);
    int MC_SetPLCShortD(long lAdd, short* pData, short nCount);
    int MC_GetPLCShortD(long lAdd, short* pData, short nCount);
    int MC_SetPLCLongD(long lAdd, long* pData, short nCount);
    int MC_GetPLCLongD(long lAdd, long* pData, short nCount);
    int MC_SetPLCFloatD(long lAdd, float* pData, short nCount);
    int MC_GetPLCFloatD(long lAdd, float* pData, short nCount);
    int MC_SetPLCM(long lAdd, char* pData, short nCount);
    int MC_GetPLCM(long lAdd, char* pData, short nCount);
    int MC_ResetAllM();
    int MC_GetCardMessage(unsigned char* cMessage);
    int MC_ClrCardMessage();
    int MC_SetCommuTimer(int iCommuTimer);
    int MC_SetKeepAlive(long AliveTime,
                        long ulEnableMask,
                        long ulEnableValue,
                        short* nAdcMask,
                        short* nAdcValue,
                        long* ulIOMask,
                        long* ulIOValue,
                        short nModuleNum);
    int MC_DownPitchErrorTable(
        short nTableNum, short nPointNum, long lStartPos, long lEndPos, short* pErrValue1, short* pErrValue2);
    int MC_ReadPitchErrorTable(
        short nTableNum, short* pPointNum, long* pStartPos, long* pEndPos, short* pErrValue1, short* pErrValue2);
    int MC_AxisErrPitchOn(short nAxisNum);
    int MC_AxisErrPitchOff(short nAxisNum);
    int MC_StartWatch(long lAxisMask, long lPackageCountFlag, long lUserSegNumFlag, long lReserve, char* FilePath);
    int MC_StopWatch();
};

#pragma pack(pop)
