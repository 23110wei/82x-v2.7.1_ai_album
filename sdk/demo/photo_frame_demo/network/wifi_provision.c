#include "network/wifi_provision.h"

#include "network/wifi_sta.h"
#include "basic_include.h"
#include "syscfg.h"
#include "lib/umac/ieee80211.h"

static uint8_t g_connect_pending;
static wifi_provision_network_t g_networks[WIFI_PROVISION_MAX_NETWORKS];
static uint8_t g_network_count;

int wifi_provision_connect(const char *ssid, const char *password)
{
    size_t ssid_len;
    size_t password_len;

    if (ssid == NULL || password == NULL) {
        return -1;
    }
    ssid_len = os_strlen(ssid);
    password_len = os_strlen(password);
    if (ssid_len == 0 || ssid_len > SSID_MAX_LEN || password_len < 8 ||
        password_len > PASSWD_MAX_LEN) {
        return -2;
    }

    os_memset(sys_cfgs.ssid, 0, sizeof(sys_cfgs.ssid));
    os_memset(sys_cfgs.passwd, 0, sizeof(sys_cfgs.passwd));
    os_memset(sys_cfgs.psk, 0, sizeof(sys_cfgs.psk));
    os_strncpy((char *)sys_cfgs.ssid, ssid, SSID_MAX_LEN);
    os_strncpy((char *)sys_cfgs.passwd, password, PASSWD_MAX_LEN);
    if (wpa_passphrase(sys_cfgs.ssid, (char *)sys_cfgs.passwd, sys_cfgs.psk) != 0) {
        return -3;
    }

    sys_cfgs.wifi_mode = WIFI_MODE_STA;
    sys_cfgs.dhcpc_en = 1;
    sys_status.wifi_connected = 0;
    sys_status.dhcpc_done = 0;
    sys_status.wifi_status_code = 0;
    sys_status.wifi_reason_code = 0;
    if (syscfg_save() != RET_OK) {
        return -4;
    }

    /* Reapply the persistent STA configuration, then start an all-channel scan.
     * This is the same stack path used by the existing boot and AT workflows. */
    syscfg_flush(1);
    ieee80211_scan(WIFI_MODE_STA, 1, NULL);
    g_connect_pending = 1;
    return 0;
}

void wifi_provision_retry(void)
{
    if (sys_cfgs.ssid[0] != '\0') {
        sys_status.wifi_status_code = 0;
        sys_status.wifi_reason_code = 0;
        ieee80211_scan(WIFI_MODE_STA, 1, NULL);
        g_connect_pending = 1;
    }
}

void wifi_provision_scan_start(void)
{
    g_network_count = 0;
    os_memset(g_networks, 0, sizeof(g_networks));
    ieee80211_scan(WIFI_MODE_STA, 1, NULL);
}

int wifi_provision_get_networks(wifi_provision_network_t *networks, int max_networks)
{
    struct hgic_bss_info bss[WIFI_PROVISION_MAX_NETWORKS];
    int count;
    int i;
    int j;
    int out_count = 0;
    wifi_provision_network_t network;

    if (networks == NULL || max_networks <= 0) {
        return 0;
    }
    os_memset(bss, 0, sizeof(bss));
    count = ieee80211_get_bsslist(bss, WIFI_PROVISION_MAX_NETWORKS, 1);
    if (count > 0) {
        if (count > WIFI_PROVISION_MAX_NETWORKS) {
            count = WIFI_PROVISION_MAX_NETWORKS;
        }
        g_network_count = 0;
        for (i = 0; i < count; i++) {
            if (bss[i].ssid[0] == '\0') {
                continue;
            }
            for (j = 0; j < g_network_count; j++) {
                if (os_strncmp(g_networks[j].ssid, (const char *)bss[i].ssid,
                               sizeof(g_networks[j].ssid) - 1) == 0) {
                    break;
                }
            }
            if (j < g_network_count || g_network_count >= WIFI_PROVISION_MAX_NETWORKS) {
                continue;
            }
            os_memset(&g_networks[g_network_count], 0, sizeof(g_networks[g_network_count]));
            os_memcpy(g_networks[g_network_count].ssid, bss[i].ssid,
                      sizeof(g_networks[g_network_count].ssid) - 1);
            g_networks[g_network_count].rssi = bss[i].signal;
            g_networks[g_network_count].encrypted = bss[i].encrypt ? 1 : 0;
            g_network_count++;
        }

        /* Scan results may arrive in a different order every poll.  Keep the
         * visual list stable by ordering it by SSID, while retaining the
         * latest RSSI value in each row. */
        for (i = 1; i < g_network_count; i++) {
            network = g_networks[i];
            for (j = i; j > 0 && os_strcmp(g_networks[j - 1].ssid, network.ssid) > 0; j--) {
                g_networks[j] = g_networks[j - 1];
            }
            g_networks[j] = network;
        }
    }

    if (g_network_count > (uint8_t)max_networks) {
        out_count = max_networks;
    } else {
        out_count = g_network_count;
    }
    if (out_count > 0) {
        os_memcpy(networks, g_networks, (uint32_t)out_count * sizeof(g_networks[0]));
    }
    return out_count;
}

void wifi_provision_get_status(wifi_provision_status_t *status)
{
    char raw[48];
    const char *ip;

    if (status == NULL) {
        return;
    }
    os_memset(status, 0, sizeof(*status));
    os_strncpy(status->ssid, (const char *)sys_cfgs.ssid, sizeof(status->ssid) - 1);

    /* Refresh the existing STA wrapper before reading its cached RSSI state. */
    wifi_sta_get_status(raw, sizeof(raw));
    if (wifi_sta_is_connected()) {
        ip = wifi_sta_get_ip();
        os_strncpy(status->ip, ip, sizeof(status->ip) - 1);
        status->rssi = wifi_sta_get_rssi();
        if (sys_status.dhcpc_done && status->ip[0] != '\0') {
            status->state = WIFI_PROVISION_STATE_ONLINE;
            g_connect_pending = 0;
        } else {
            status->state = WIFI_PROVISION_STATE_DHCP;
        }
    } else if (status->ssid[0] == '\0') {
        status->state = WIFI_PROVISION_STATE_UNCONFIGURED;
    } else if (g_connect_pending && sys_status.wifi_status_code != 0) {
        status->state = WIFI_PROVISION_STATE_FAILED;
    } else if (g_connect_pending) {
        status->state = WIFI_PROVISION_STATE_CONNECTING;
    } else {
        status->state = WIFI_PROVISION_STATE_DISCONNECTED;
    }
}
