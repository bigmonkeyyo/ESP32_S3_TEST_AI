/**
 ****************************************************************************************************
 * @file        qmi8658a.h
 * @author      姝ｇ偣鍘熷瓙鍥㈤槦(ALIENTEK)
 * @version     V1.0
 * @date        2024-06-25
 * @brief       QMI8658A椹卞姩浠ｇ爜
 *
 * @license     Copyright (c) 2020-2032, 骞垮窞甯傛槦缈肩數瀛愮鎶€鏈夐檺鍏徃
 ****************************************************************************************************
 * @attention
 *
 * 瀹為獙骞冲彴:姝ｇ偣鍘熷瓙 ESP32S3 BOX 寮€鍙戞澘
 * 鍦ㄧ嚎瑙嗛:www.yuanzige.com
 * 鎶€鏈鍧?www.openedv.com
 * 鍏徃缃戝潃:www.alientek.com
 * 璐拱鍦板潃:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#ifndef __QMI8658A_H__
#define __QMI8658A_H__

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "iic.h"
#include "driver/gpio.h"
#include "esp_log.h"


/****************************************************************************************************************/
/* QMI8658A鐩稿叧瀹忓畾涔?*/

#define QMI8658_ADDR            0X6A                        /* SA0=1 鍦板潃涓猴細0X6A, SA0=0 鍦板潃涓猴細0X6B */        
#define ONE_G                   9.807f                      /* 鍔犻€熷害鍗曚綅杞崲浣跨敤 */
#ifndef M_PI
#define M_PI                    (3.14159265358979323846f)
#endif
#define MAX_CALI_COUNT          100                         /* 閲囨牱娆℃暟 */

#define QMI8658_DISABLE_ALL     (0x0)                       /* 闄€铻轰华鍜屽姞閫熷害璁″潎涓嶄娇鑳?*/
#define QMI8658_ACC_ENABLE      (0x1)                       /* 浣胯兘鍔犻€熷害璁?*/
#define QMI8658_GYR_ENABLE      (0x2)                       /* 浣胯兘闄€铻轰华 */
#define QMI8658_ACCGYR_ENABLE   (QMI8658_ACC_ENABLE | QMI8658_GYR_ENABLE)/* 闄€铻轰华鍜屽姞閫熷害璁″潎浣胯兘 */
//#define QMI8658_SYNC_SAMPLE_MODE                          /* 鍚屾閲囨牱妯″紡 */
extern uint8_t g_imu_init;

/****************************************************************************************************************/
/* QMI8658A鐨勫瘎瀛樺櫒鍦板潃 */
/* 璇︾粏璇存槑鍙傜収QMI8658A鏁版嵁鎵嬪唽 */
enum Qmi8658AReg
{
    Register_WhoAmI = 0,
    Register_Revision,
    Register_Ctrl1,
    Register_Ctrl2,
    Register_Ctrl3,
    Register_Reserved,
    Register_Ctrl5,
    Register_Reserved1,
    Register_Ctrl7,
    Register_Ctrl8,
    Register_Ctrl9,
    Register_Cal1_L = 11,
    Register_Cal1_H,
    Register_Cal2_L,
    Register_Cal2_H,
    Register_Cal3_L,
    Register_Cal3_H,
    Register_Cal4_L,
    Register_Cal4_H,
    Register_FifoWmkTh = 19, 
    Register_FifoCtrl = 20,
    Register_FifoCount = 21,
    Register_FifoStatus = 22,
    Register_FifoData = 23,
    Register_StatusInt = 45,
    Register_Status0,
    Register_Status1,
    Register_Timestamp_L = 48,
    Register_Timestamp_M,
    Register_Timestamp_H,
    Register_Tempearture_L = 51,
    Register_Tempearture_H,
    Register_Ax_L = 53,
    Register_Ax_H,
    Register_Ay_L,
    Register_Ay_H,
    Register_Az_L,
    Register_Az_H,
    Register_Gx_L = 59,
    Register_Gx_H,
    Register_Gy_L,
    Register_Gy_H,
    Register_Gz_L,
    Register_Gz_H,
    Register_COD_Status = 70,
    Register_dQW_L = 73,
    Register_dQW_H,
    Register_dQX_L,
    Register_dQX_H,
    Register_dQY_L,
    Register_dQY_H,
    Register_dQZ_L,
    Register_dQZ_H,
    Register_dVX_L,
    Register_dVX_H,
    Register_dVY_L,
    Register_dVY_H,
    Register_dVZ_L,
    Register_dVZ_H,
    Register_TAP_Status = 89,
    Register_Step_Cnt_L = 90,
    Register_Step_Cnt_M = 91,
    Register_Step_Cnt_H = 92,
    Register_Reset = 96
};

/****************************************************************************************************************/
/* 璇︾粏璇存槑鍙傜収QMI8658A鏁版嵁鎵嬪唽 Ctrl9璇︾粏鍛戒护璇存槑 */
enum Ctrl9Command
{
    Ctrl9_Cmd_Ack                   = 0X00,
    Ctrl9_Cmd_RstFifo               = 0X04,
    Ctrl9_Cmd_ReqFifo               = 0X05, /* Get FIFO data from Device */
    Ctrl9_Cmd_WoM_Setting           = 0x08, /* 璁剧疆骞跺惎鐢ㄨ繍鍔ㄥ敜閱?*/
    Ctrl9_Cmd_AccelHostDeltaOffset  = 0x09, /* 鏇存敼鍔犻€熷害璁″亸绉?*/
    Ctrl9_Cmd_GyroHostDeltaOffset   = 0x0A, /* 鏇存敼闄€铻轰华鍋忕Щ */
    Ctrl9_Cmd_CfgTap                = 0x0C, /* 閰嶇疆TAP妫€娴?*/
    Ctrl9_Cmd_CfgPedometer          = 0x0D, /* 閰嶇疆璁℃鍣?*/
    Ctrl9_Cmd_Motion                = 0x0E, /* 閰嶇疆浠讳綍杩愬姩/鏃犺繍鍔?鏄剧潃杩愬姩妫€娴?*/
    Ctrl9_Cmd_RstPedometer          = 0x0F, /* 閲嶇疆璁℃鍣ㄨ鏁帮紙姝ユ暟锛?*/
    Ctrl9_Cmd_CopyUsid              = 0x10, /* 灏?USID 鍜?FW 鐗堟湰澶嶅埗鍒?UI 瀵勫瓨鍣?*/
    Ctrl9_Cmd_SetRpu                = 0x11, /* 閰嶇疆 IO 涓婃媺 */
    Ctrl9_Cmd_AHBClockGating        = 0x12, /* 鍐呴儴 AHB 鏃堕挓闂ㄦ帶寮€鍏?*/
    Ctrl9_Cmd_OnDemandCalivration   = 0xA2, /* 闄€铻轰华鎸夐渶鏍″噯 */
    Ctrl9_Cmd_ApplyGyroGains        = 0xAA  /* 鎭㈠淇濆瓨鐨勯檧铻轰华澧炵泭 */
};

/* 浣庨€氭护娉㈠櫒鐨勫紑鍚拰鍏抽棴 */
enum qmi8658_LpfConfig
{
    Qmi8658Lpf_Disable,
    Qmi8658Lpf_Enable
};

/* 鑷妯″紡鐨勫紑鍚拰鍏抽棴 */
enum qmi8658_StConfig
{
    Qmi8658St_Disable,
    Qmi8658St_Enable
};

/* 鍔犻€熷害璁″拰闄€铻轰华鐨勪綆閫氳繃婊ゅ櫒妯″紡閫夋嫨 */
enum qmi8658_LpfMode
{
    A_LSP_MODE_0 = 0x00 << 1,
    A_LSP_MODE_1 = 0x01 << 1,
    A_LSP_MODE_2 = 0x02 << 1,
    A_LSP_MODE_3 = 0x03 << 1,

    G_LSP_MODE_0 = 0x00 << 5,
    G_LSP_MODE_1 = 0x01 << 5,
    G_LSP_MODE_2 = 0x02 << 5,
    G_LSP_MODE_3 = 0x03 << 5
};

/* 鍔犻€熷害璁￠噺绋嬮€夋嫨 */
enum qmi8658_accrange
{
    Qmi8658accrange_2g = 0x00 << 4,
    Qmi8658accrange_4g = 0x01 << 4,
    Qmi8658accrange_8g = 0x02 << 4,
    Qmi8658accrange_16g = 0x03 << 4
};

/* 鍔犻€熷害璁DR杈撳嚭閫熺巼閫夋嫨 */
enum qmi8658_accodr
{
    Qmi8658accodr_8000Hz = 0x00,
    Qmi8658accodr_4000Hz = 0x01,
    Qmi8658accodr_2000Hz = 0x02,
    Qmi8658accodr_1000Hz = 0x03,
    Qmi8658accodr_500Hz = 0x04,
    Qmi8658accodr_250Hz = 0x05,
    Qmi8658accodr_125Hz = 0x06,
    Qmi8658accodr_62_5Hz = 0x07,
    Qmi8658accodr_31_25Hz = 0x08,
    Qmi8658accodr_LowPower_128Hz = 0x0c,
    Qmi8658accodr_LowPower_21Hz = 0x0d,
    Qmi8658accodr_LowPower_11Hz = 0x0e,
    Qmi8658accodr_LowPower_3Hz = 0x0f
};

/* 闄€铻轰华閲忕▼閫夋嫨 */
enum qmi8658_gyrrange
{
    Qmi8658gyrrange_16dps = 0 << 4,
    Qmi8658gyrrange_32dps = 1 << 4,
    Qmi8658gyrrange_64dps = 2 << 4,
    Qmi8658gyrrange_128dps = 3 << 4,
    Qmi8658gyrrange_256dps = 4 << 4,
    Qmi8658gyrrange_512dps = 5 << 4,
    Qmi8658gyrrange_1024dps = 6 << 4,
    Qmi8658gyrrange_2048dps = 7 << 4
};

/* 闄€铻轰华杈撳嚭閫熺巼閫夋嫨 */
enum qmi8658_gyrodr
{
    Qmi8658gyrodr_8000Hz = 0x00,
    Qmi8658gyrodr_4000Hz = 0x01,
    Qmi8658gyrodr_2000Hz = 0x02,
    Qmi8658gyrodr_1000Hz = 0x03,
    Qmi8658gyrodr_500Hz = 0x04,
    Qmi8658gyrodr_250Hz = 0x05,
    Qmi8658gyrodr_125Hz = 0x06,
    Qmi8658gyrodr_62_5Hz = 0x07,
    Qmi8658gyrodr_31_25Hz = 0x08
};

typedef struct
{
    unsigned char ensensors;        /* 浼犳劅鍣ㄤ娇鑳芥爣蹇?*/
    enum qmi8658_accrange accrange; /* 璁剧疆鍔犻€熷害璁＄殑閲忕▼ */
    enum qmi8658_accodr accodr;     /* 璁剧疆鍔犻€熷害璁＄殑ODR杈撳嚭閫熺巼 */     
    enum qmi8658_gyrrange gyrrange; /* 璁剧疆闄€铻轰华鐨勯噺绋?*/
    enum qmi8658_gyrodr gyrodr;     /* 璁剧疆闄€铻轰华鐨凮DR杈撳嚭閫熺巼 */     
    unsigned char syncsample;       /* 鍚屾閲囨牱鏍囧織 */
} qmi8658_config;

typedef struct
{
    qmi8658_config cfg;             /* 璁板綍浼犳劅鍣ㄥ弬鏁?*/
    unsigned short ssvt_a;          /* 鍔犻€熷害璁″崟浣嶆崲绠楀弬鏁?*/
    unsigned short ssvt_g;          /* 闄€铻轰华鍗曚綅鎹㈢畻鍙傛暟 */
    float imu[6];                   /* 璁板綍涓婁竴娆＄殑鍔犻€熷害璁″拰闄€铻轰华鐨勪笁杞村弬鏁?*/
} qmi8658_state;

/****************************************************************************************************************/
/* 鍑芥暟澹版槑 */
uint8_t qmi8658_init(i2c_obj_t self);                                         /* 鍒濆鍖朡MI8658 */
float qmi8658_get_temperature(void);                                /* 鑾峰彇浼犳劅鍣ㄦ俯搴?*/
uint8_t get_euler_angles(float *pitch, float *roll, float *yaw);    /* 鑾峰彇娆ф媺瑙掓暟鎹?*/
void qmi8658_reset(void);                                           /* 澶嶄綅浼犳劅鍣?*/
void qmi8658_read_xyz(float *acc, float *gyro);                     /* 鑾峰彇鍔犻€熷害璁″拰闄€铻轰华鐨勪笁杞存暟鎹?*/

#endif

