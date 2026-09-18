#include "network/wifi_sta.h"
#include "basic_include.h"
#include "syscfg.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "hal/netdev.h"

static int g_sta_connected = 0;
static char g_sta_ip[16] = {0};

/* Resolve the STA's lwIP netif without relying on netif_find("w0"). lwIP's
 * netif_add() auto-increments netif->num to avoid collisions, so the first
 * real netif (created after the loopif in netif_init()) ends up with num=1,
 * not 0. netif_find() matches on name+num, so netif_find("w0") (which wants
 * num==0) returns NULL and the UI never sees the DHCP-assigned IP. Reach the
 * netif through the netdev abstraction (ndev->stack_data) instead, which is
 * the exact pointer lwip_netif_add() stored for HG_WIFI0_DEVID. */
static struct netif *wifi_sta_netif(void)
{
    struct netdev *ndev = (struct netdev *)dev_get(HG_WIFI0_DEVID);
    return ndev ? (struct netif *)ndev->stack_data : NULL;
}

void wifi_sta_get_status(char *buf, int buf_len)
{
    /* Connection state is driven by WiFi association (sys_status.wifi_connected),
     * NOT by the netif IP. This way the UI shows real signal strength as soon as
     * the STA associates, even before DHCP completes. */
    struct netif *netif = wifi_sta_netif();
    if (sys_status.wifi_connected) {
        g_sta_connected = 1;
        if (netif && netif_is_up(netif) && !ip4_addr_isany_val(*netif_ip4_addr(netif))) {
            os_strncpy(g_sta_ip, ip4addr_ntoa(netif_ip4_addr(netif)), sizeof(g_sta_ip) - 1);
        } else {
            g_sta_ip[0] = '\0';
        }
        os_snprintf(buf, buf_len, "CONNECTED,%s,%d",
                    g_sta_ip[0] ? g_sta_ip : "0.0.0.0", (int)sys_status.rssi);
    } else {
        g_sta_connected = 0;
        g_sta_ip[0] = '\0';
        os_snprintf(buf, buf_len, "DISCONNECTED");
    }
}

int wifi_sta_is_connected(void)
{
    return g_sta_connected;
}

const char *wifi_sta_get_ip(void)
{
    /* Query the live netif IP on each call so DHCP-obtained addresses are
     * reflected immediately, even if wifi_sta_get_status() was last called
     * before DHCP completed. */
    struct netif *netif = wifi_sta_netif();
    if (netif && netif_is_up(netif) && !ip4_addr_isany_val(*netif_ip4_addr(netif))) {
        os_strncpy(g_sta_ip, ip4addr_ntoa(netif_ip4_addr(netif)), sizeof(g_sta_ip) - 1);
    } else {
        g_sta_ip[0] = '\0';
    }
    return g_sta_ip;
}

int wifi_sta_get_rssi(void)
{
    /* RSSI is kept current by the IEEE80211_EVENT_RSSI handler in sys_event_hdl.
     * Only meaningful while associated; report 0 (unknown) otherwise. */
    if (!g_sta_connected) {
        return 0;
    }
    return (int)sys_status.rssi;
}