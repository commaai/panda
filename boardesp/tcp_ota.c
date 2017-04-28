/*
 * tcp_ota.c: Over The Air (OTA) firmware upgrade via direct TCP/IP connection.
 *
 * NOTE that this does not perform any security checks, so don't rely on this for production use!
 *
 * Author: Ian Marshall
 * Date: 28/05/2016
 */
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"    
#include "ip_addr.h"
#include "espconn.h"
#include "mem.h"
#include "spi_flash.h"
#include "user_interface.h"
#include "upgrade.h"
#include "driver/uart.h"
#include "espmissingincludes.h"
#include "tcp_ota.h"

#include "crypto/rsa.h"
#include "crypto/sha.h"

#include "cert.h"

#define min(a,b) ((a) < (b) ? (a) : (b))

#define FIRMWARE_SIZE 503808

// The TCP port used to listen to for connections.
#define OTA_PORT 65056

// The number of bytes to use for the OTA message buffer (NOT the firmware buffer).
#define OTA_BUFFER_LEN 32

// Structure holding the TCP connection information for the OTA connection.
LOCAL struct espconn ota_conn;

// TCP specific protocol structure for the OTA connection.
LOCAL esp_tcp ota_proto;

// Timer used for rebooting the ESP8266 after an OTA upgrade is complete.
LOCAL os_timer_t ota_reboot_timer;

// Buffer used to hold the new firmware until we have received it all. This is not statically allocated, to avoid 
// constantly blocking out the memory used, even when no OTA upgrade is in progress.
LOCAL uint8_t *ota_firmware = NULL;

// The total number of bytes expected for the firmware image that is to be flashed.
LOCAL uint32_t ota_firmware_size = 0;

// The total number of bytes received for the firmware image that is to be flashed.
LOCAL uint32_t ota_firmware_received = 0;

// The number of bytes that have currently been received into the "ota_firmware" buffer, which is reset every 4KB.
LOCAL uint32_t ota_firmware_len = 0;

// Buffer used for receiving header information via TCP, allowing the header information to be split over multiple 
// packets.
LOCAL uint8_t ota_buffer[OTA_BUFFER_LEN];

// The number of bytes currently used in the OTA buffer.
LOCAL uint8_t ota_buffer_len = 0;

// Type used to define the possible status values of OTA upgrades.
typedef enum {
    NOT_STARTED,
    CONNECTION_ESTABLISHED,
    RECEIVING_HEADER,
    RECEIVING_FIRMWARE,
    REBOOTING,
    ERROR
} ota_state_t;

// The current status of the OTA flashing. This is required as multiple transmissions will be required to send through 
// the firmware data.
LOCAL ota_state_t ota_state = NOT_STARTED;

// The IP address of the host sending the OTA data to us. Needed to avoid corruption if two hosts try to OTA upgrade at
// the same time.
LOCAL uint32_t ota_ip = 0;

// The TCP port of the host sending the OTA data to us.
LOCAL uint16_t ota_port = 0;

// Forward definitions.
LOCAL uint8_t ICACHE_FLASH_ATTR parse_header_line();

/*
 * Handles the receiving of information for the OTA update process.
 */
LOCAL void ICACHE_FLASH_ATTR ota_rx_cb(void *arg, char *data, uint16_t len) {
    // Store the IP address from the sender of this data.
    struct espconn *conn = (struct espconn *)arg;
    uint8_t *addr_array = NULL;
    addr_array = conn->proto.tcp->remote_ip;
    ip_addr_t addr;
    IP4_ADDR(&addr, conn->proto.tcp->remote_ip[0], conn->proto.tcp->remote_ip[1], conn->proto.tcp->remote_ip[2], 
             conn->proto.tcp->remote_ip[3]);
    if (ota_ip == 0) {
        // There is no previously stored IP address, so we have it.
        ota_ip = addr.addr;
        ota_port = conn->proto.tcp->remote_port;
        ota_state = CONNECTION_ESTABLISHED;
    } else if ((ota_ip != addr.addr) || (ota_port != conn->proto.tcp->remote_port)) {
        // This connection is not the one curently sending OTA data.
        espconn_send(conn, "ERR: Connection Already Exists\r\n", 32);
        return;
    }

    //os_printf("Rx packet - %d bytes, state=%d, size=%d, received=%d, len=%d.\r\n", 
    //          len, ota_state, ota_firmware_size, ota_firmware_received, ota_firmware_len);
    // OTA message sequence:
    // Rx: "OTA\r\n"
    // Rx: "GetNextFlash\r\n"
    // Tx: "user1.bin\r\n" or "user2.bin\r\n", depending on which binary is the next one to be flashed.
    // Rx: "FirmwareLength: <len>\r\n", where "<len>" is the number of bytes (in ASCII) to be sent in the firmware.
    // Tx: "Ready\r\n"
    // Rx: <Firmware>, for "<len>" bytes.
    // Tx: "Flashing\r\n" or "Invalid\r\n".
    // Tx: "Rebooting\r\n"
    uint16_t unbuffered_start = 0;
    if ((ota_state == CONNECTION_ESTABLISHED) || (ota_state == RECEIVING_HEADER)) {
        // Store the received bytes into the buffer.
        for (uint16_t ii = 0; ii < len; ii++) {
            if (ota_buffer_len < (OTA_BUFFER_LEN - 1)) {
                ota_buffer[ota_buffer_len++] = data[ii];
            } else {
                // The buffer has overflowed, remember where we left off.
                unbuffered_start = ii;
                break;
            }
        }
    } else if (ota_state == RECEIVING_FIRMWARE) {
        // Store received bytes in the firmware buffer.
        uint32_t copy_len = (uint32_t)len;
        if ((copy_len + ota_firmware_len) > SPI_FLASH_SEC_SIZE) {
            copy_len = SPI_FLASH_SEC_SIZE - ota_firmware_len;
        }
        if ((copy_len + ota_firmware_received) > ota_firmware_size) {
            copy_len = ota_firmware_size - ota_firmware_len;
        }
        os_memmove(&ota_firmware[ota_firmware_len], data, copy_len);
        ota_firmware_len += copy_len;
        ota_firmware_received += copy_len;
        if (copy_len < len) {
            unbuffered_start = copy_len;
        }
    }

    bool repeat = true;
    while (repeat) {
        uint8_t eol = 0;
        switch (ota_state) {
            case CONNECTION_ESTABLISHED: {
                // A connection has just been established. We expect an initial line of "OTA".
                eol = parse_header_line();
                if (eol > 0) {
                    // We have a line, it should be "OTA".
                    if (strncmp("OTA", ota_buffer, eol - 2)) {
                        // Oh dear, it's not.
                        espconn_send(conn, "ERR: Invalid protocol\r\n", 23);
                        ota_state = ERROR;
                        return;
                    } else {
                        // We do, move to the next line in the header.
                        ota_state = RECEIVING_HEADER;
                    }
                }
                break;
            }
            case RECEIVING_HEADER: {
                // We are now receiving header lines.
                eol = parse_header_line();
                if (eol > 0) {
                    // We have a line, see what it is.
                    if (!strncmp("GetNextFlash", ota_buffer, eol - 2)) {
                        // The remote device has requested to know what the next flash unit is.
                        uint8_t unit = system_upgrade_userbin_check(); // Note, returns the current unit!
                        if (unit == UPGRADE_FW_BIN1) {
                            espconn_send(conn, "user2.bin\r\n", 11);
                        } else {
                            espconn_send(conn, "user1.bin\r\n", 11);
                        }
                    } else if ((eol > 17) && (!strncmp("FirmwareLength:", ota_buffer, 15))) {
                        // The remote system is preparing to send the firmware. The expected length is supplied here.
                        uint32_t size = 0;
                        for (uint8_t ii = 16; ii < ota_buffer_len; ii++) {
                            if ((ota_buffer[ii] >= '0') && (ota_buffer[ii] <= '9')) {
                                size *= 10;
                                size += ota_buffer[ii] - '0';
                            } else if ((ota_buffer[ii] == '\r') || (ota_buffer[ii] == '\n')) {
                                // We have finished the firmware size.
                                break;
                            } else if ((ota_buffer[ii] != ' ') && (ota_buffer[ii] != ',')) {
                                // Anything that's not a number, space or new-line is invalid.
                                size = 0;
                                break;
                            }
                        }

                        if (size == 0) {
                            // We either didn't get a length, or the length is invalid.
                            espconn_send(conn, "ERR: Invalid firmware length\r\n", 30);
                            ota_state = ERROR;
                            return;
                        } else if (size > FIRMWARE_SIZE) {
                            // The size of the incoming firmware image is too big to fit.
                            espconn_send(conn, "ERR: Firmware length is too big\r\n", 33);
                            ota_state = ERROR;
                            return;
                        } else {
                            // Ready to begin flashing!
                            ota_firmware = (uint8_t *)os_malloc(SPI_FLASH_SEC_SIZE);
                            if (ota_firmware == NULL) {
                                espconn_send(conn, "ERR: Unable to allocate OTA buffer.\r\n", 37);
                                ota_state = ERROR;
                                return;
                            }
                            ota_firmware_size = size;
                            ota_firmware_received = 0;
                            ota_firmware_len = 0;  
                            ota_state = RECEIVING_FIRMWARE;

                            // Copy any remaining bytes from the OTA buffer to the firmware buffer.
                            uint8_t remaining = ota_buffer_len - eol - 1;
                            if (remaining > 0) {
                                os_memmove(ota_firmware, &ota_buffer[eol + 1], remaining);
                                ota_firmware_received = ota_firmware_len = (uint32_t)remaining;
                            }

                            espconn_send(conn, "Ready\r\n", 7);
                        }
                    } else {
                        // We received an unexpected header line, abort.
                        espconn_send(conn, "ERR: Unexpected header.\r\n", 25);
                        ota_state = ERROR;
                        return;
                    }
                }
                break;
            }
            case RECEIVING_FIRMWARE: {
                // We are now receiving the firmware image.
                if ((ota_firmware_len == SPI_FLASH_SEC_SIZE) || (ota_firmware_received == ota_firmware_size)) {
                    // We have received a sector's worth of data, or the remainder of the flash image, flash it.
                    if (ota_firmware_received <= SPI_FLASH_SEC_SIZE) {
                        // This is the first block, check the header.
                        if (ota_firmware[0] != 0xEA) {
                            espconn_send(conn, "ERR: IROM magic missing.\r\n", 26);
                            ota_state = ERROR;
                            return;
                        } else if ((ota_firmware[1] != 0x04) || (ota_firmware[2] > 0x03) || 
                                   ((ota_firmware[3] >> 4) > 0x06)) {
                            espconn_send(conn, "ERR: Flash header invalid.\r\n", 28);
                            ota_state = ERROR;
                            return;
                        } else if (((uint16_t *)ota_firmware)[3] != 0x4010) {
                            espconn_send(conn, "ERR: Invalid entry address.\r\n", 29);
                            ota_state = ERROR;
                            return;
                        } else if (((uint32_t *)ota_firmware)[2] != 0x00000000) {
                            espconn_send(conn, "ERR: Invalid start offset.\r\n", 28);
                            ota_state = ERROR;
                            return;
                        }
                    }

                    // Zero out any remaining bytes in the last block, to avoid writing dirty data.
                    if (ota_firmware_len < SPI_FLASH_SEC_SIZE) {
                        os_memset(&ota_firmware[ota_firmware_len], 0, SPI_FLASH_SEC_SIZE - ota_firmware_len);
                    }

                    // Find out the starting address for the flash write.
                    int address, start_address;
                    uint8_t current = system_upgrade_userbin_check();
                    if (current == UPGRADE_FW_BIN1) {
                        // The next flash, user2.bin, will start after 4KB boot, user1, 16KB user params, 4KB reserved.
                        address = 4*1024 + FIRMWARE_SIZE + 16*1024 + 4*1024;
                    } else {
                        // The next flash, user1.bin, will start after 4KB boot.
                        address = 4*1024;
                    }
                    start_address = address;
                    address += ota_firmware_received - ota_firmware_len;


                    // Erase the flash block.
                    if ((address % SPI_FLASH_SEC_SIZE) == 0) {
                        spi_flash_erase_sector(address / SPI_FLASH_SEC_SIZE);
                    }

                    // Write the new flash block.
                    //os_printf("Flashing address %05x, total received = %d.\n", address, ota_firmware_received);
                    SpiFlashOpResult res = spi_flash_write(address, (uint32_t *)ota_firmware, SPI_FLASH_SEC_SIZE);
                    ota_firmware_len = 0;
                    if (res != SPI_FLASH_RESULT_OK) {
                        espconn_send(conn, "ERR: Flash failed.\r\n", 20);
                        ota_state = ERROR;
                        return;
                    }

                    if (ota_firmware_received == ota_firmware_size) {
                        char digest[SHA_DIGEST_SIZE];
                        uint32_t rsa[RSANUMBYTES/4];
                        uint32_t dat[0x80/4];
                        int ll;
                        spi_flash_read(start_address+ota_firmware_size-RSANUMBYTES, rsa, RSANUMBYTES);

                        // 32-bit aligned accesses only
                        SHA_CTX ctx;
                        SHA_init(&ctx);
                        for (ll = 0; ll < ota_firmware_size-RSANUMBYTES; ll += 0x80) {
                          spi_flash_read(start_address + ll, dat, 0x80);
                          SHA_update(&ctx, dat, min((ota_firmware_size-RSANUMBYTES)-ll, 0x80));
                        }
                        memcpy(digest, SHA_final(&ctx), SHA_DIGEST_SIZE);

                        /*char buf[0x20];
                        os_sprintf(buf, "%d: %02x %02x %02x %02x", ota_firmware_size-RSANUMBYTES, digest[0], digest[1], digest[2], digest[3]);
                        espconn_send(conn, buf, strlen(buf));*/

                        if (!RSA_verify(&debugesp_rsa_key, rsa, RSANUMBYTES, digest, SHA_DIGEST_SIZE)) {
                          espconn_send(conn, "Signature check FAILED. OTA fail.......\r\n", 41);
                        } else {
                          // We've flashed all of the firmware now, reboot into the new firmware.
                          os_printf("Preparing to update firmware.\n");

                          espconn_send(conn, "Signature check true.  Rebooting in 2s.\r\n", 41);
                          os_free(ota_firmware);
                          ota_firmware_size = 0;
                          ota_firmware_received = 0;
                          ota_firmware_len = 0;
                          ota_state = REBOOTING;
                          system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
                          os_printf("Scheduling reboot.\n");
                          os_timer_disarm(&ota_reboot_timer);
                          os_timer_setfn(&ota_reboot_timer, (os_timer_func_t *)system_upgrade_reboot, NULL);
                          os_timer_arm(&ota_reboot_timer, 2000, 1);
                        }
                    }
                }
                break;
            }
        }

        // Clear out the processed bytes from the buffer, if any.
        repeat = false;
        if ((ota_state == CONNECTION_ESTABLISHED) || (ota_state == RECEIVING_HEADER)) {
            // In these states, we're still going to be using the buffer.
            if (eol < (ota_buffer_len - 1)) {
                // There are still more characters in the buffer yet to process, move them to the start of the buffer.
                os_memmove(&ota_buffer[0], &ota_buffer[eol + 1], ota_buffer_len - eol - 1);
                ota_buffer_len = ota_buffer_len - eol - 1;
                repeat = true;
            } else {
                ota_buffer_len = 0;
            }

            if (unbuffered_start > 0) {
                // Store unbuffered bytes to the end of the buffer.
                for (uint16_t ii = unbuffered_start; ii < len; ii++) {
                    if (ota_buffer_len < (OTA_BUFFER_LEN - 1)) {
                        ota_buffer[ota_buffer_len++] = data[ii];
                        unbuffered_start = 0;
                        repeat = true;
                    } else {
                        // The buffer has overflowed again, remember where we left off.
                        unbuffered_start = ii;
                        break;
                    }
                }
            }
        } else if (ota_state == RECEIVING_FIRMWARE) {
            if (unbuffered_start > 0) {
                // Store unbuffered bytes in the firmware buffer.
                uint32_t copy_len = (uint32_t)(len - unbuffered_start);
                if ((copy_len + ota_firmware_len) > SPI_FLASH_SEC_SIZE) {
                    copy_len = SPI_FLASH_SEC_SIZE - ota_firmware_len;
                }
                if ((copy_len + ota_firmware_received) > ota_firmware_size) {
                    copy_len = ota_firmware_size - ota_firmware_len;
                }
                os_memmove(&ota_firmware[ota_firmware_len], &data[unbuffered_start], copy_len);
                ota_firmware_len += copy_len;
                ota_firmware_received += copy_len;
                if (copy_len < (len - unbuffered_start)) {
                    unbuffered_start += copy_len;
                } else {
                    unbuffered_start = 0;
                }
                repeat = true;
            }
        }
    }
}

// Returns the number of bytes in the message buffer for a single header line, or zero if no header is found.
LOCAL uint8_t ICACHE_FLASH_ATTR parse_header_line() {
    for (uint8_t ii = 0; ii < ota_buffer_len - 1; ii++) {
        if ((ota_buffer[ii] == '\r') && (ota_buffer[ii + 1] == '\n')) {
            // We have found the end of line markers.
            return ii + 1;
        }
    }

    // If we get here, we didn't find the end of line markers.
    return 0;
}

/*
 * Call-back for when a TCP connection has been disconnected.
 */
LOCAL void ICACHE_FLASH_ATTR ota_disc_cb(void *arg) {
    // Reset the connection information, if we haven't progressed far enough.
    if ((ota_state != NOT_STARTED) && (ota_state != REBOOTING)) {
        ota_ip = 0;
        ota_port = 0;
        ota_state = NOT_STARTED;

        ota_buffer_len = 0;
        if (ota_firmware != NULL) {
            os_free(ota_firmware);
            ota_firmware = NULL;
            ota_firmware_size = 0;
            ota_firmware_len = 0;
        }
    }
}

/*
 * Call-back for when a TCP connection has failed - reconnected is a misleading name, sadly.
 */
LOCAL void ICACHE_FLASH_ATTR ota_recon_cb(void *arg, int8_t err) {
    // Use the disconnect call-back to process this event.
    ota_disc_cb(arg);
}

/*
 * Call-back for when an incoming TCP connection has been established.
 */
LOCAL void ICACHE_FLASH_ATTR ota_tcp_connect_cb(void *arg) {
    struct espconn *conn = (struct espconn *)arg;
    os_printf("TCP OTA connection received from "IPSTR":%d\n",
              IP2STR(conn->proto.tcp->remote_ip), conn->proto.tcp->remote_port);

    // See if this connection is allowed.
    if (ota_ip == 0) {
        // Now that we have a connection, register some call-backs.
        espconn_regist_recvcb(conn, ota_rx_cb);
        espconn_regist_disconcb(conn, ota_disc_cb);
        espconn_regist_reconcb(conn, ota_recon_cb);
    }
}

/*
 * Initialises the required connection information to listen for OTA messages.
 * WiFi must first have been set up for this to succeed.
 */
void ICACHE_FLASH_ATTR ota_init() {
    ota_proto.local_port = OTA_PORT;
    ota_conn.type = ESPCONN_TCP;
    ota_conn.state = ESPCONN_NONE;
    ota_conn.proto.tcp = &ota_proto;
    espconn_regist_connectcb(&ota_conn, ota_tcp_connect_cb);
    espconn_accept(&ota_conn);
}
