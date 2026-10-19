#ifndef WIFI_H
#define WIFI_H

/*
 * Start WiFi in station mode and return immediately.
 * Connecting happens in the background; it retries forever on disconnect.
 * Network name and password come from menuconfig (WiFi station).
 * Requires nvs_flash_init() to have been called first.
 */
void wifi_init_sta(void);

#endif
