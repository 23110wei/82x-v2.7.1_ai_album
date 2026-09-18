#include "network/wifi_credentials.h"

#include "basic_include.h"
#include "network/wifi_provision.h"
#include "syscfg.h"

/*
 * 移植适配:凭据直接存于项目"syscfg"主记录(sys_cfgs.ssid/passwd/psk,
 * wifi_mode=STA),由wifi_provision_connect完成写入并触发连接——
 * 开机自动连接由SDK既有的sys_cfg_load+WiFi初始化流程天然实现。
 */

void wifi_credentials_init(void)
{
}

const char *wifi_credentials_get_ssid(void)
{
    return (const char *)sys_cfgs.ssid;
}

const char *wifi_credentials_get_password(void)
{
    return (const char *)sys_cfgs.passwd;
}

uint8_t wifi_credentials_is_valid(void)
{
    return (uint8_t)(sys_cfgs.ssid[0] != '\0');
}

int wifi_credentials_save(const char *ssid, const char *password)
{
    return wifi_provision_connect(ssid, password);
}

void wifi_credentials_clear(void)
{
    sys_cfgs.ssid[0] = '\0';
    sys_cfgs.passwd[0] = '\0';
    memset(sys_cfgs.psk, 0, sizeof(sys_cfgs.psk));
    (void)syscfg_save();
}

int wifi_credentials_connect_stored(void)
{
    if (sys_cfgs.ssid[0] == '\0') {
        return -1;
    }
    wifi_provision_retry();
    return 0;
}
