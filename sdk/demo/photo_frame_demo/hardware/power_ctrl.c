#include "hardware/power_ctrl.h"

#include "basic_include.h"
#include "lib/fs/fatfs/ff.h"
#include "chip/txw82x/io_function.h"
#include "display/ai_album_brightness.h"
#include "ui/fonts/ai_album_sd_font.h"

/* gpio_iomap_output声明在chip/txw82x/io_function.h(经sdk/include根
 * 以相对路径包含);gpio_set_val等在basic_include->hal/gpio.h已带 */

/* 移植自旁系ai_album工程src/hardware/power_ctrl.c(TXW827-RGB888_GQ_XC001
 * 同板),按本工程实际适配:关屏序列改为背光PWM关断+AVDD_EN(PD13)拉低,
 * 增加SD字体引擎文件句柄关闭;brtc语音未接入,无PTT会话需停。 */

/* config.cfg不注册SYSTEM_PWR_HOLD,应用直接驱动PA5(电路见power_ctrl.h) */
#define POWER_CTRL_HOLD_PIN PA_5

/* SW8开机键同时经D5拉低PA15,数字低=用户正按着电源键(外部R26 20K上拉)。
 * 仅开机锁存阶段读取,此时keyWork(AD按键扫描)尚未启动,无复用冲突 */
#define POWER_CTRL_ADKEY_PIN PA_15

/* PA10_USB_DET: VBUS经R56/R64兆欧级高压分压后到脚(5V时约2.5V)。
 * 注意:本芯片hgadc_v1通道表**无PA_10**(PA9/PA10无ADC功能),只能
 * 数字GPIO读取;高阻分压+输入漏电流使电平在阈值附近可能临界(用户
 * 板测曾观察到充电标识偶发翻转),故所有判定都带去抖,且开机路径
 * 绝不因PA10读低而死锁(见power_ctrl_hold的复查循环)。JTAG TCK与
 * 本脚共点,接调试器时读数无效。TP4056的CHRG/STDBY脚未接SoC,语义
 * =外部供电(VBUS)在位,"真在充电/已充满"不可区分 */
#define POWER_CTRL_VBUS_PIN PA_10
/* 疑似变化时的一致性确认:50ms窗口连读N次全部一致才翻转(滤除临界
 * 电平的单次误读);仅在变化沿发生,UI线程最多阻塞~50ms */
#define POWER_CTRL_VBUS_CONFIRM_COUNT 5u
#define POWER_CTRL_VBUS_CONFIRM_GAP_MS 10u

/* 真实开机需持续按住SW8超过该时长才锁存电源,防误碰开机 */
#define POWER_CTRL_MIN_HOLD_MS 500u

/* 屏模拟电源使能,与photo_frame_demo.c的PHOTO_FRAME_AVDD_EN_PIN同脚 */
#define POWER_CTRL_AVDD_EN_PIN PD_13

#define POWER_CTRL_UNMOUNT_VOLUME "0:"

static void power_ctrl_gpio_write(uint32 pin, uint8 val)
{
    gpio_iomap_output(pin, GPIO_IOMAP_OUTPUT);
    gpio_set_mode(pin, GPIO_PULL_NONE, GPIO_PULL_LEVEL_NONE);
    gpio_set_dir(pin, GPIO_DIR_OUTPUT);
    gpio_set_val(pin, val);
}

static uint8_t power_ctrl_key_pressed(void)
{
    gpio_set_mode(POWER_CTRL_ADKEY_PIN, GPIO_PULL_NONE, GPIO_PULL_LEVEL_NONE);
    gpio_set_dir(POWER_CTRL_ADKEY_PIN, GPIO_DIR_INPUT);
    return (uint8_t)(gpio_get_val(POWER_CTRL_ADKEY_PIN) == 0);
}

static void power_ctrl_latch(void)
{
    power_ctrl_gpio_write(POWER_CTRL_HOLD_PIN, 1U);
}

static uint8_t power_ctrl_vbus_present_raw(void)
{
    /* 采样前先把节点泄放:无VBUS时分压节点是纯高阻(上下臂均1MΩ量
     * 级),泄漏/近旁走线感应会把电荷慢慢充过数字阈值(表现为"没插
     * USB过一会充电图标自己亮")。先输出低1ms放掉漂移电荷,再切输
     * 入等1ms:真VBUS经上臂1MΩ毫秒内把节点充回分压电平(读高),
     * 无源节点保持低。经上臂的泄放电流仅~5µA,无害 */
    gpio_set_mode(POWER_CTRL_VBUS_PIN, GPIO_PULL_NONE,
                  GPIO_PULL_LEVEL_NONE);
    gpio_set_val(POWER_CTRL_VBUS_PIN, 0U);
    gpio_set_dir(POWER_CTRL_VBUS_PIN, GPIO_DIR_OUTPUT);
    os_sleep_ms(1);
    gpio_set_dir(POWER_CTRL_VBUS_PIN, GPIO_DIR_INPUT);
    os_sleep_ms(1);
    return (uint8_t)(gpio_get_val(POWER_CTRL_VBUS_PIN) == 1);
}

/* 50ms窗口连读N次全部一致返回1(滤除临界电平的单次误读) */
static uint8_t power_ctrl_vbus_confirm(uint8_t value)
{
    uint8_t i;

    for (i = 0U; i + 1U < POWER_CTRL_VBUS_CONFIRM_COUNT; ++i) {
        os_sleep_ms(POWER_CTRL_VBUS_CONFIRM_GAP_MS);
        if ((uint8_t)power_ctrl_vbus_present_raw() != value) {
            return 0U;
        }
    }
    return 1U;
}

static uint8_t g_booted_with_key;

/* 无按键/短按后的兜底循环:每200ms复查VBUS,一旦在位立即锁存并继续
 * 开机——PA10读数不准(临界电平/共点干扰)绝不允许把板子锁死,插一下
 * USB即可救活;电池幽灵重启场景VBUS恒低,循环直到电源轨耗尽真关机 */
static void power_ctrl_wait_external_or_die(void)
{
    os_printf("power_ctrl: waiting for VBUS\r\n");
    while (1) {
        os_sleep_ms(200);
        if (power_ctrl_vbus_present_raw()) {
            power_ctrl_latch();
            return;
        }
    }
}

void power_ctrl_hold(void)
{
    /* 开机判定(按键PA15优先,VBUS兜底):
     * 1. 没按键+VBUS在位 = 外部供电上电(USB插电/调试器供轨,开发台
     *    即此形态)→ 直接锁存,插电即用;
     * 2. 按着键 = 按键开机,须保持超过最短窗口(防误碰短按);
     * 3. 没按键+无VBUS = 电池关机后放电过程的幽灵重启 → 兜底循环
     *    等断电(期间插入USB会立即开机) */
    if (!power_ctrl_key_pressed()) {
        if (power_ctrl_vbus_present_raw()) {
            power_ctrl_latch();
            return;
        }
        power_ctrl_wait_external_or_die();
        return;
    }

    os_sleep_ms(POWER_CTRL_MIN_HOLD_MS);
    if (!power_ctrl_key_pressed()) {
        os_printf("power_ctrl: key released too early\r\n");
        power_ctrl_wait_external_or_die();
        return;
    }

    /* 标记本次开机由按键按住触发:按键桥接用它抑制"开机按住"被
     * keyWork识别为长按而弹出关机弹窗(首次松开前的电源键事件吞掉) */
    g_booted_with_key = 1U;
    power_ctrl_latch();
}

uint8_t power_ctrl_booted_with_key(void)
{
    return g_booted_with_key;
}

void power_ctrl_release(void)
{
    gpio_set_val(POWER_CTRL_HOLD_PIN, 0U);
}

uint8_t power_ctrl_charging(void)
{
    static uint8 configured;
    static uint8 reported;
    uint8_t raw;

    /* 由单一轮询上下文调用(home runtime秒级定时器)。
     * 初值判定不信任单次读数:电池按键开机时这里是PA10上电后第一次
     * 被配置读取,复位默认上拉/复用暂态会把高阻分压节点充到高电平
     * (实测:不插USB开机⚡也会先闪)。先等50ms放稳再突发确认,确认
     * 高电平后隔200ms复验,双稳才报外部供电;低电平直接确认即可。
     * 全部在首次调用(主页首帧前)完成,一次性阻塞<=~330ms */
    if (!configured) {
        configured = 1u;
        reported = 0U;
        os_sleep_ms(50);
        raw = power_ctrl_vbus_present_raw();
        if (raw != 0U && power_ctrl_vbus_confirm(raw)) {
            os_sleep_ms(200);
            if (power_ctrl_vbus_present_raw() != 0U &&
                power_ctrl_vbus_confirm(1U)) {
                reported = 1U;
            }
        }
        os_printf("power_ctrl: vbus initial=%u\r\n", (unsigned)reported);
        return reported;
    }
    raw = power_ctrl_vbus_present_raw();
    if (raw == reported) {
        return reported;
    }
    if (power_ctrl_vbus_confirm(raw)) {
        reported = raw;
        os_printf("power_ctrl: vbus -> %u\r\n", (unsigned)reported);
    }
    return reported;
}

void power_ctrl_shutdown_sequence(void)
{
    /* brtc语音未接入,无PTT/音频会话需要停,接入后在此补 */

    /* 关闭SD字体引擎常开的2个文件句柄,再强制卸载FatFS卷让缓冲落盘 */
    ai_album_sd_font_close_files();
    f_mount(NULL, POWER_CTRL_UNMOUNT_VOLUME, 0);
    os_printf("power_ctrl: SD volume unmounted\r\n");

    /* 逆上电顺序关屏:背光(PWM占空0+停PWM) -> AVDD_EN(PD13)。
     * VGH/VGL由屏模组从AVDD自举,无独立使能脚,AVDD断即全掉 */
    ai_album_brightness_set_output_enabled(0);
    os_sleep_ms(50);
    power_ctrl_gpio_write(POWER_CTRL_AVDD_EN_PIN, 0);

    os_sleep_ms(100);
    os_printf("power_ctrl: releasing power hold\r\n");

    power_ctrl_release();

    /* 锁存释放后数毫秒内VIN塌掉;若板仍活着(如USB直供变体焊了D4)
     * 原地等待,不返回到UI。注意:若USB供电下测试关机,电源不会断,
     * 屏灭后固件停留在此循环——这是预期行为 */
    while (1) {
        os_sleep_ms(1000);
    }
}
