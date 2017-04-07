/*
 * tcp_ota.h: Over The Air (OTA) firmware upgrade via direct TCP/IP connection.
 *
 * Author: Ian Marshall
 * Date: 28/05/2016
 */

#ifndef TCP_OTA_H
#define TCP_OTA_H

/*
 * Initialises the required connection information to listen for OTA messages.
 * WiFi must first have been set up for this to succeed.
 */
void ICACHE_FLASH_ATTR ota_init();

#endif
