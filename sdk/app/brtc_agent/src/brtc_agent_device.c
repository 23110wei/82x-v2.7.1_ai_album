#include "brtc_agent_internal.h"

#include "osal/time.h"

const char *brtc_agent_device_id(void)
{
    static char device_id[16];
    static bool initialized;
    uint8 mac[6] = {0};
    uint32 nonzero;
    uint32 hash;
    int i;
    extern void sysctrl_efuse_mac_addr_calc(uint8 *addr_buf);

    if (initialized) {
        return device_id;
    }
    sysctrl_efuse_mac_addr_calc(mac);
    nonzero = ((uint32)mac[3] << 16) | ((uint32)mac[4] << 8) | mac[5];
    if (nonzero == 0U) {
        nonzero = ((uint32)mac[0] << 16) | ((uint32)mac[1] << 8) | mac[2];
    }

    if (nonzero == 0U) {
        hash = (uint32)os_jiffies();
        os_printf("[BRTC_AGENT] warning: efuse MAC is empty; device id is not stable\r\n");
    } else {
        hash = 2166136261U;
        for (i = 0; i < 6; ++i) {
            hash ^= mac[i];
            hash *= 16777619U;
        }
    }
    os_snprintf(device_id, sizeof(device_id), "%u", (unsigned)hash);
    initialized = true;
    return device_id;
}
