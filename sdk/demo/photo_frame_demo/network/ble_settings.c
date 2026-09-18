#include "network/ble_settings.h"

#include "basic_include.h"
#include "lib/bluetooth/uble/ble_demo.h"
#include "syscfg.h"

/*
 * 蓝牙开关真实现:UBLE栈由开机sys_ble_init(BLE_SUPPORT=1)拉起,
 * 这里只做运行时广播开关(ble_set_mode)与持久化(sys_cfgs.album_ble)。
 * 原工程的命名syscfg记录("album_bt")在vendor库上不可用,改存主记录。
 */

#define BLE_SETTINGS_ADV_CHAN 38U

static uint8_t g_ble_current;

void ble_settings_init(void)
{
    if (sys_cfgs.album_ble == 1U) {
        if (ble_set_mode(1, BLE_SETTINGS_ADV_CHAN) == RET_OK) {
            g_ble_current = 1U;
            os_printf("ai_album: BLE auto-started on boot\r\n");
        } else {
            os_printf("ai_album: BLE auto-start failed\r\n");
        }
    }
}

uint8_t ble_settings_is_enabled(void)
{
    return g_ble_current;
}

void ble_settings_set_enabled(uint8_t enable)
{
    uint8_t old = sys_cfgs.album_ble;

    if (enable && !g_ble_current) {
        if (ble_set_mode(1, BLE_SETTINGS_ADV_CHAN) != RET_OK) {
            os_printf("ai_album: BLE enable failed\r\n");
            return;
        }
        g_ble_current = 1U;
    } else if (!enable && g_ble_current) {
        (void)ble_set_mode(0, BLE_SETTINGS_ADV_CHAN);
        g_ble_current = 0U;
    } else {
        return;
    }

    sys_cfgs.album_ble = g_ble_current;
    if (syscfg_save() != RET_OK) {
        sys_cfgs.album_ble = old;
        os_printf("ai_album: BLE state save failed\r\n");
        return;
    }
    os_printf("ai_album: BLE %s by user\r\n",
              g_ble_current ? "enabled" : "disabled");
}

uint8_t ble_settings_get_state(void)
{
    return g_ble_current;
}
