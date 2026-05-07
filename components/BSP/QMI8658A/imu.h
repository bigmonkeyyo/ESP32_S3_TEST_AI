/**
 ****************************************************************************************************
 * @file        imu.h
 * @author      姝ｇ偣鍘熷瓙鍥㈤槦(ALIENTEK)
 * @version     V1.0
 * @date        2024-05-21
 * @brief       濮挎€佽В绠?浠ｇ爜
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

#ifndef __IMU_H
#define __IMU_H

#include <math.h>
#include <stdbool.h>

#define IMU_DELTA_T     0.1000f         /* 閲囨牱鏃堕棿 */
#define DEG2RAD         0.017453293f    /* 搴﹁浆寮у害 蟺/180 */
#define RAD2DEG         57.29578f       /* 寮у害杞害 180/蟺 */


void imu_get_eulerian_angles(float acc[3], float gyro[3], float *rpy, float dt) ; /* 鑾峰彇娆ф媺瑙?*/
void imu_get_quaternion(float quat_wxyz[4]); /* 鑾峰彇鍥涘厓鏁帮紙w,x,y,z锛?*/

#endif
