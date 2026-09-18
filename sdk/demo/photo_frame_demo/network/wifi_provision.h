#ifndef __DUAL_SCREEN_WIFI_PROVISION_H__
#define __DUAL_SCREEN_WIFI_PROVISION_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_PROVISION_STATE_UNCONFIGURED = 0,
    WIFI_PROVISION_STATE_DISCONNECTED,
    WIFI_PROVISION_STATE_CONNECTING,
    WIFI_PROVISION_STATE_DHCP,
    WIFI_PROVISION_STATE_ONLINE,
    WIFI_PROVISION_STATE_FAILED,
} wifi_provision_state_t;

typedef struct {
    wifi_provision_state_t state;
    char ssid[33];
    char ip[16];
    int rssi;
} wifi_provision_status_t;

#define WIFI_PROVISION_MAX_NETWORKS 10

typedef struct {
    char ssid[33];
    int rssi;
    unsigned char encrypted;
} wifi_provision_network_t;

/* Configure, persist, and immediately connect the application STA. */
int wifi_provision_connect(const char *ssid, const char *password);
void wifi_provision_get_status(wifi_provision_status_t *status);
void wifi_provision_retry(void);

/* Starts an asynchronous all-channel scan; results are read by polling. */
void wifi_provision_scan_start(void);
int wifi_provision_get_networks(wifi_provision_network_t *networks, int max_networks);

#ifdef __cplusplus
}
#endif

#endif
