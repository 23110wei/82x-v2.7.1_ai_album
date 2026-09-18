#ifndef __BLE_SETTINGS_H__
#define __BLE_SETTINGS_H__

#include "typesdef.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize BLE settings module.
 * @details Reads persistent state from syscfg. Must be called after
 *          app_net_handler_init() (for proper event registration).
 */
void ble_settings_init(void);

/**
 * @brief Check if BLE is currently enabled.
 * @return 1 = BLE active, 0 = BLE disabled
 */
uint8_t ble_settings_is_enabled(void);

/**
 * @brief Enable or disable BLE advertising.
 * @param enable 1 = enable BLE, 0 = disable BLE
 */
void ble_settings_set_enabled(uint8_t enable);

/**
 * @brief Get the current BLE state for UI display.
 * @return 1 = ON, 0 = OFF
 */
uint8_t ble_settings_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* __BLE_SETTINGS_H__ */