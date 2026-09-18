#include "sys_config.h"
#include "typesdef.h"
#include "adkey.h"
#include "keyScan.h"
 
#include "dev/adc/hgadc_v0.h"
#include "hal/gpio.h"
#ifdef PIN_FROM_PARAM
#include "pin_param.h"
#endif

static adkey_t adkey= {
  .priv = NULL,
  .pull = GPIO_PULL_NONE,
  .pull_level = GPIO_PULL_LEVEL_NONE,
};

static void key_adkey_init(key_channel_t *key)
{
    adkey_t *adkey = (adkey_t *)key->priv;
    struct hgadc_v0 *adc = (struct hgadc_v0*)dev_get(HG_ADC0_DEVID);
	adkey->pin = MACRO_PIN(PIN_ADKEY1);
	adc_open((struct adc_device *)adc);	
	gpio_set_mode(adkey->pin,adkey->pull,adkey->pull_level);
	adc_add_channel((struct adc_device *)adc, adkey->pin);	
    adkey->priv = (void*)adc;
	key->enable = 1;

}

static uint8 key_adkey_scan(key_channel_t *key)
{
    adkey_t *adkey = (adkey_t *)key->priv;
    uint32 vol;
    struct adkey_scan_code *key_scan = (struct adkey_scan_code*)key->key_table;
    adc_get_value((struct adc_device *)adkey->priv, adkey->pin, &vol);
	// printf("vol:%d\n",vol);
    //记录当前adc的值,用与发送到应用层,至于应用层是否需要,由应用层去管理
    key->extern_value = vol;
    for(;;)
    {
        if(vol>=key_scan->adc)
        {
            key_scan++;
        }
        else
        {
            key_scan--;
            break;
        }
    }
    // printf("key_scan->key:%d\tvol:%d\n",key_scan->key,vol);
    return key_scan->key;
}








/*********************************************************
 *                      adkey的参数配置
 * 
    default: 2117
    up:910
    down:462
    left:6
    right:1368
    press:1826

默认开发板先检查每一个按键的值,然后大概每一个ad-100填到下表
************************************************************/
#ifdef SYS_APP_WALKIE_TALKIE
static const struct adkey_scan_code adkey_table[] =
{
	{0,     AD_DOWN},
	{200,   AD_DOWN},
	{635,   AD_SPEACH},
	{1275,  AD_UP},
    {1620,  AD_RIGHT},
	{2047,  KEY_NONE},
	{3200,  KEY_NONE},
    {3800,  KEY_NONE},
    {4000,  KEY_NONE},
    {4096,  KEY_NONE},
};
#elif defined(PHOTO_FRAME_DEMO)
/*********************************************************
 * AI数码相框板(TXW827-RGB888_GQ_XC001)6+1键阶梯,PA15。
 * ADC返回11位原始值(0~2047,驱动内已钳位),阈值换算自实测mV:
 *   raw = mv * 2047 / 3300 (参考ai_album key_input中间件换算)
 *   电源0mV→0  OK670→416  右1620→1005  M1843→1143
 *   左2226→1381  上2600→1613  下2929→1817  空闲3300→2047
 * 档位取相邻中心值中点。键id与photo_frame_key_input.c映射一致。
 * 若上板后个别键错档:看日志"key id=.. raw=.."实测值微调本表。
 ********************************************************/
static const struct adkey_scan_code adkey_table[] =
{
	{0,     AD_A},       /* 电源键 0~208 */
	{209,   AD_PRESS},   /* OK 209~710 */
	{711,   AD_RIGHT},   /* 右 711~1074 */
	{1075,  AD_B},       /* M菜单 1075~1262 */
	{1263,  AD_LEFT},    /* 左 1263~1497 */
	{1498,  AD_UP},      /* 上 1498~1715 */
	{1716,  AD_DOWN},    /* 下 1716~1932 */
	{1933,  KEY_NONE},
    {3400,  KEY_NONE},
    {4096,  KEY_NONE},
};
#else
static const struct adkey_scan_code adkey_table[] = 
{
	{0,     AD_LEFT},
	{350,   AD_DOWN},
	{800,   AD_UP},
	{1200,  AD_RIGHT},
    {1700,  AD_PRESS},
	{2000,  KEY_NONE},
	{3200,  KEY_NONE},
    {3800,  KEY_NONE},
    {4000,  KEY_NONE},
    {4096,  KEY_NONE},
};
#endif

static const keys_t adkey_arg = 
{
    .period_long     = 500,
    .period_repeat   = 1000,
    .period_dither = 80,
};

//外部调用
key_channel_t adkey_key = 
{
  .init       = key_adkey_init,
  .scan       = key_adkey_scan,
  .prepare    = NULL,
  .priv       = (void*)&adkey,
  .key_arg    = &adkey_arg,//按键的参数,可能不同的类型按键,参数不一样
  .key_table  = &adkey_table,
};



