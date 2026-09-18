#ifndef __AI_ALBUM_WIFI_CREDENTIALS_H__
#define __AI_ALBUM_WIFI_CREDENTIALS_H__

#include <stdint.h>

/*
 * Persistent Wi-Fi credentials for ai_album.
 *
 * Stores SSID and password so the device can auto-connect on boot.
 * Uses syscfg with magic/checksum validation.
 */

/* Initialize WiFi credentials from persistent storage. */
void wifi_credentials_init(void);

/* Get the stored SSID (may be empty if no credentials saved). */
const char *wifi_credentials_get_ssid(void);

/* Get the stored password. */
const char *wifi_credentials_get_password(void);

/* Check if valid credentials are stored. */
uint8_t wifi_credentials_is_valid(void);

/* Save new credentials. Returns 0 on success. */
int wifi_credentials_save(const char *ssid, const char *password);

/* Clear stored credentials. */
void wifi_credentials_clear(void);

/* Attempt to connect using stored credentials. Returns 0 on success. */
int wifi_credentials_connect_stored(void);

#endif /* __AI_ALBUM_WIFI_CREDENTIALS_H__ */
