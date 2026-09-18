#ifndef AI_ALBUM_BATTERY_DETECT_H
#define AI_ALBUM_BATTERY_DETECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "typesdef.h"

/* 采样VBAT分压(PB6, 原理图R54/R58 1M/1M分压)估算电量0-100%。
 * ADC驱动自带互斥,与AD按键扫描共用ADC0串行访问。由单一轮询上下文
 * 周期调用(共享标题栏秒级定时器);内部EMA平滑读数 */
uint8_t battery_detect_percent(void);

/* 上屏显示电量:在EMA读数上再叠一层迟滞/爬升,避免秒级跳变。
 * external_power 为VBUS在位标志(power_ctrl_charging()),充电时爬升更快;
 * 与 battery_detect_percent() 同属单一轮询上下文。 */
uint8_t battery_detect_display_percent(uint8_t external_power);

#ifdef __cplusplus
}
#endif

#endif
