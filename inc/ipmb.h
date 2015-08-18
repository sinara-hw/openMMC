/*
 * ipmb.h
 *
 *   AFCIPMI  --
 *
 *   Copyright (C) 2015  Henrique Silva  <henrique.silva@lnls.br>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IPMB_H_
#define IPMB_H_

#define IPMB_I2C                I2C0

#define IPMB_TXTASK_PRIORITY    3
#define IPMB_RXTASK_PRIORITY    IPMB_TXTASK_PRIORITY
#define IPMB_TXQUEUE_LEN        5
#define IPMB_CLIENT_QUEUE_LEN   5

#define IPMB_MAX_RETRIES        3
#define IPMB_MSG_TIMEOUT        250/portTICK_PERIOD_MS
#define CLIENT_NOTIFY_TIMEOUT   5

#define IPMI_HEADER_CHECKSUM_POSITION      2
#define IPMI_MSG_MAX_LENGTH         32
#define IPMB_REQ_HEADER_LENGTH      6
/* Response header is 1 byte longer because it must include the completion code */
#define IPMB_RESP_HEADER_LENGTH     7


#define IPMB_NETFN_MASK         0xFC
#define IPMB_DEST_LUN_MASK      0x3
#define IPMB_SEQ_MASK           0xFC
#define IPMB_SRC_LUN_MASK       0x3

#define MCH_ADDRESS             0x20

typedef struct ipmi_msg {
    uint8_t dest_addr;
    uint8_t netfn;
    uint8_t dest_LUN;
    uint8_t hdr_chksum;
    uint8_t src_addr;
    uint8_t seq;
    uint8_t src_LUN;
    uint8_t cmd;
    uint8_t completion_code;
/* Data field has 24 bytes:
 * 32 (Max IPMI msg len) - 7 header bytes - 1 final chksum byte */
    uint8_t data_len;
    uint8_t data[IPMI_MSG_MAX_LENGTH];
    uint8_t msg_chksum;
} ipmi_msg;

typedef struct ipmi_msg_cfg {
    ipmi_msg buffer;
    TaskHandle_t caller_task;
    uint8_t retries;
uint32_t timestamp;
} ipmi_msg_cfg;

typedef enum ipmb_error {
    ipmb_error_success = 0,
    ipmb_error_failure,
    ipmb_error_timeout,
    ipmb_error_hdr_chksum,
    ipmb_error_msg_chksum,
    ipmb_error_queue_creation
} ipmb_error;

/* Function Prototypes */
void IPMB_TXTask ( void *pvParameters );
void IPMB_RXTask ( void *pvParameters );
void ipmb_init ( void );
ipmb_error ipmb_send_request ( uint8_t netfn, uint8_t cmd, uint8_t * data, uint8_t data_len );
ipmb_error ipmb_send_response ( ipmi_msg * req, uint8_t cc, uint8_t * data, uint8_t data_len );
ipmb_error ipmb_register_rxqueue ( QueueHandle_t * queue );
/* Maybe put these prototypes on the ipmb.c since they're "private" functions */
ipmb_error ipmb_notify_client ( ipmi_msg_cfg * msg_cfg );
ipmb_error ipmb_assert_chksum ( uint8_t * buffer, uint8_t buffer_len );
uint8_t ipmb_calculate_chksum ( uint8_t * buffer, uint8_t range );
ipmb_error ipmb_encode ( uint8_t * buffer, ipmi_msg * msg );
ipmb_error ipmb_decode ( ipmi_msg * msg, uint8_t * buffer, uint8_t len );

#endif
