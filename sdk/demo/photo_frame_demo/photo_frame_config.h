#ifndef __PHOTO_FRAME_CONFIG_H__
#define __PHOTO_FRAME_CONFIG_H__


/*****************************************************************************
 * AI数码相框(ai_album) 1024x600 RGB888屏
 * 主控板:TXW827-RGB888_GQ_XC001
 * 屏:B101B545C-27A(HX8282,DE模式,RGB888,48M pclk)
 * 当前阶段:点屏验证(显示+按键+LVGL),后续再开SD/网络/AI
 *****************************************************************************/

/***************************************************************
 * 打开PIN_FROM_PARAM,通过脚本和config.cfg去生成对应的io配置信息
 * 请查看重要的文件:pin_param.h、config.cfg两个文件
 * config.cfg中已按相框板配置RGB888引脚,并关闭与本板冲突的外设引脚
 *************************************************************/
#define PIN_FROM_PARAM
/*****************************************
 * 打开对应demo的宏
 ****************************************/
#define PHOTO_FRAME_DEMO

/**********************************************************************
 * 系统必要信息宏
 * CONFIG_PSRAM_AVHEAP_SIZE:为应用分配的psram宏,需要根据应用场景分配
 * PSRAM_HEAP:如果需要用到psram,需要打开PSRAM_HEAP
 * AV堆6.25MB构成(峰值6.03M时的余量账):
 *   解码双槽2×1.2M + YUV持久0.94M + osd_tmp_buf 1.2M
 *   + OSD编码静态缓冲~1.05M + JPEG输入峰值0.4M
 ********************************************************************/
#define DEFAULT_SYS_CLK                 (192*1000000)
#define PSRAM_HEAP          //如果需要psram当作heap,需要打开这个宏
#define AV_PSRAM_HEAP
#define AV_HEAP

#define CONFIG_PSRAM_AVHEAP_SIZE        (6*1024*1024+256*1024)
#define CONFIG_AVHEAP_SIZE              (100*1024)

/*****************************************************************
 * 电源域配置:本板无摄像头,VCAM/VCAM2均关闭
 *****************************************************************/
#define VCAM_EN                         0
#define VCAM_VOL                        VCAM_VOL_3V00
#define VCAM2_EN                        0
#define VCAM2_VOL                       VCC_LDO_VOL_1V80

/************************************************************************
* 打开hx8282 rgb888屏支持
************************************************************************ */
#define LCD_HX8282_EN 1

/***************************************************************************
* 支持的输入设备类型,暂时只有按键和触摸(LVGL_INDEY_KEY|LVGL_INDEY_TOUCHPAD)
* 相框用6个AD实体按键(PA15分压),无触摸
************************************************************************** */
#define LVGL_INPUTDEV_SUPPORT (LVGL_INDEY_KEY)

/*************************************************************
 * 支持LCD:SUPPORT_LCD(支持显示到屏)
************************************************************ */
#define SUPPORT_LCD

/**********************************************************************
 * 蓝牙:开机拉起UBLE栈(sys_ble_init->ble_demo_init),设置页用
 * ble_set_mode做运行时广播开关(见network/ble_settings.c)
 *********************************************************************/
#define BLE_SUPPORT                     1

/**********************************************************************
 * 开发调试:默认WiFi热点(烧录后免设置页配网,syscfg重建默认时自动预填;
 * syscfg_default只在flash无有效记录或记录尺寸变化时执行,改这里后
 * 需重烧才会生效)。量产前删除这两行宏。
 *********************************************************************/
#define PHOTO_FRAME_WIFI_DEFAULT_SSID   "wei"
#define PHOTO_FRAME_WIFI_DEFAULT_PASSWD "qwerrewq"

/**********************************************************************
 * SD卡/文件系统:SDIO 1-bit(CLK=PB7 CMD=PC12 DAT0=PC11),
 * 挂载FatFS到VFS("0:"驱动器),相册/字体/存储信息依赖
 * USE_FAT_CACHE:FatFS读缓存(相册缩略图批量读卡性能需要)
 *********************************************************************/
#define FS_EN                           1
#define USE_FAT_CACHE                   1
#define STARTUP_OTA                     0

/**********************************************************************
 * BRTC语音服务配置(来自ai_album工程project_config.h,当前brtc为桩未使用;
 * 接入真实语音服务时由brtc实现读取)
 *********************************************************************/
#define AI_ALBUM_BRTC_PLATFORM_URL    "http://106.12.120.112:8936/api/v1/aiagent"
#define AI_ALBUM_BRTC_CREATE_URL      AI_ALBUM_BRTC_PLATFORM_URL "/generateAIAgentCall"
#define AI_ALBUM_BRTC_STOP_URL        AI_ALBUM_BRTC_PLATFORM_URL "/stopAIAgentInstance"
#define AI_ALBUM_BRTC_APP_ID          "appse5qw343eugn"
#define AI_ALBUM_BRTC_LICENSE_KEY     "a271f35c6d5f45b184d455a1d9ca22b5_19e0eecdd2"
#define AI_ALBUM_BRTC_LLM             "LLMRacing"
#define AI_ALBUM_BRTC_LANGUAGE        "zh"
#define AI_ALBUM_BRTC_SCREEN_WIDTH    1024U
#define AI_ALBUM_BRTC_SCREEN_HEIGHT   600U

/**********************************************************************
 * 音频coder运行位置(brtc语音PCM解码依赖,照AI对话demo配方):
 * 默认AUCODER_NO_RUN时 pcm_dec_msi_init 不注册pcmdec组件,
 * brtc_agent_audio_start 会报 "provider audio MSI is not ready"。
 * RUN_IN_CPU1 = PCM解码跑CPU1(经aurpc RPC,app_audio_init建堆)。
 *********************************************************************/
#define PCM_DEC_CTRL                    AUCODER_RUN_IN_CPU1

/**********************************************************************
 * WiFi skb池 200KB→96KB:给PSRAM系统堆腾空间供百度引擎。第一轮
 * 128KB后RTP会话已能建立,但语音会话进行中系统堆实测只剩~15KB,
 * 进AI CHAT页/字体位图/lwip pbuf分配失败(psram remain 288B)。
 * 相框WiFi负载(天气/NTP/语音RTP ~71kbps)96KB仍充裕;若出现WiFi
 * 吞吐异常(天气超时/语音断续)再回128KB。
 *********************************************************************/
#define CONFIG_CORE_SKB_POOL_SIZE       (96*1024)

#endif
