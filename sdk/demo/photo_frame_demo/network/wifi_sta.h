#ifndef WIFI_STA_H
#define WIFI_STA_H

void wifi_sta_get_status(char *buf, int buf_len);
int wifi_sta_is_connected(void);
const char *wifi_sta_get_ip(void);
int wifi_sta_get_rssi(void);          /* dBm, 0 when unknown/not connected */

#endif