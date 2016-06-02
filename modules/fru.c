/*
 *   openMMC -- Open Source modular IPM Controller firmware
 *
 *   Copyright (C) 2015-2016  Henrique Silva <henrique.silva@lnls.br>
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
 *
 *   @license GPL-3.0+ <http://spdx.org/licenses/GPL-3.0+>
 */

/*! @file fru.c
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"

#include "fru.h"
#include "at24mac.h"
#include "utils.h"
#include "ipmi.h"
#include "fru_editor.h"
#include "i2c_mapping.h"

void fru_init( uint8_t fru_id )
{
    uint8_t eeprom_id;
    uint8_t **fru_info;
    size_t *fru_size;
    bool *fru_runtime;

    if( fru_id == FRU_AMC ) {
        eeprom_id = CHIP_ID_EEPROM;
        fru_info = &amc_fru_info;
        fru_size = &amc_fru_info_size;
        fru_runtime = &fru_amc_runtime;
#ifdef FRU_WRITE_EEPROM
        *fru_size = amc_fru_info_build( fru_info );
        at24mac_write( eeprom_id, 0x00, *fru_info, *fru_size, 0 );
#endif
#ifdef MODULE_RTM
    } else if ( fru_id == FRU_RTM ) {
        eeprom_id = CHIP_ID_RTM_EEPROM;
        fru_info = &rtm_fru_info;
        fru_size = &rtm_fru_info_size;
        fru_runtime = &fru_rtm_runtime;
#ifdef FRU_WRITE_EEPROM
        *fru_size = rtm_fru_info_build( fru_info );
        at24mac_write( eeprom_id, 0x00, *fru_info, *fru_size, 0 );
#endif
#endif
    } else {
	return;
    }

#ifdef MODULE_EEPROM_AT24MAC
    /* Read FRU info Common Header */
    uint8_t common_header[8];

    if ( at24mac_read( eeprom_id, 0x00, &common_header[0], 8, 0 ) == 8 ) {
        if ( (calculate_chksum( &common_header[0], 7 ) == common_header[7]) && common_header[0] == 1 ) {
            /* We have a valid FRU image in the SEEPROM */
            *fru_runtime = false;
            return;
        }
    }
#endif
    /* Could not access the SEEPROM, create a runtime fru info */
    if ( fru_id == FRU_AMC ) {
        *fru_size = amc_fru_info_build( fru_info );
#ifdef MODULE_RTM
    } else if ( fru_id == FRU_RTM ) {
        *fru_size = rtm_fru_info_build( fru_info );
#endif
    }
    *fru_runtime = true;
}

size_t fru_read( uint8_t id, uint8_t *rx_buff, uint16_t offset, size_t len )
{
    uint16_t i;
    uint16_t j = offset;

    size_t ret_val = 0;

    if ( fru_amc_runtime ) {
        for (i = 0; i < len; i++, j++ ) {
            if ( id == FRU_AMC ) {
                if (j < amc_fru_info_size) {
                    rx_buff[i] = amc_fru_info[j];
                } else {
                    rx_buff[i] = 0xFF;
                }
#ifdef MODULE_RTM
            } else if ( id == FRU_RTM ) {
                if (j < rtm_fru_info_size) {
                    rx_buff[i] = rtm_fru_info[j];
                } else {
                    rx_buff[i] = 0xFF;
                }
#endif
            }
        }
        ret_val = i;
    } else {
#ifdef MODULE_EEPROM_AT24MAC
        ret_val = at24mac_read( CHIP_ID_EEPROM, offset, rx_buff, len, 0 );
#endif
    }
    return ret_val;
}

size_t fru_write( uint8_t id, uint8_t *tx_buff, uint16_t offset, size_t len )
{
    size_t ret_val = 0;

    if ( fru_amc_runtime ) {
        uint8_t i;
        for (i = 0; i < len; i++) {
            if ( id == FRU_AMC ) {
                amc_fru_info[offset+i] = tx_buff[i];
#ifdef MODULE_RTM
            } else if ( id == FRU_RTM ) {
                rtm_fru_info[offset+i] = tx_buff[i];
#endif
            }
        }
        ret_val = i;
    } else {
#ifdef MODULE_EEPROM_AT24MAC
        ret_val = at24mac_write( CHIP_ID_EEPROM, offset, tx_buff, len, 0 );
#endif
    }
    return ret_val;
}


/* IPMI Handlers */
IPMI_HANDLER(ipmi_storage_get_fru_info, NETFN_STORAGE, IPMI_GET_FRU_INVENTORY_AREA_INFO_CMD, ipmi_msg * req, ipmi_msg * rsp )
{
    uint8_t len = rsp->data_len = 0;

    uint8_t fru_id = req->data[0];

    rsp->completion_code = IPMI_CC_OK;

    if (fru_id == 0) {
        rsp->data[len++] = amc_fru_info_size & 0xFF;
        rsp->data[len++] = (amc_fru_info_size & 0xFF00) >> 8;
        rsp->data[len++] = 0x00; /* Device accessed by bytes */
    } else if (fru_id == 1) {
#ifdef MODULE_RTM
        rsp->data[len++] = rtm_fru_info_size & 0xFF;
        rsp->data[len++] = (rtm_fru_info_size & 0xFF00) >> 8;
        rsp->data[len++] = 0x00; /* Device accessed by bytes */
#else
        rsp->completion_code = IPMI_CC_INV_DATA_FIELD_IN_REQ;
#endif
    }
    rsp->data_len = len;
}

IPMI_HANDLER(ipmi_storage_read_fru_data_cmd, NETFN_STORAGE, IPMI_READ_FRU_DATA_CMD, ipmi_msg * req, ipmi_msg * rsp )
{
    uint32_t offset;
    uint8_t len = rsp->data_len = 0;
    uint8_t fru_id = req->data[0];

    /* Count byte on the request is "1" based */
    uint8_t count = req->data[3];

    if ( (count-1) > IPMI_MSG_MAX_LENGTH ) {
        rsp->completion_code = IPMI_CC_CANT_RET_NUM_REQ_BYTES;
        return;
    }

    offset = (req->data[2] << 8) | (req->data[1]);


    count = fru_read( fru_id, &(rsp->data[len+1]), offset, count );
    rsp->data[len++] = count;

    rsp->data_len = len + count;
    rsp->completion_code = IPMI_CC_OK;
}

IPMI_HANDLER(ipmi_storage_write_fru_data_cmd, NETFN_STORAGE, IPMI_WRITE_FRU_DATA_CMD, ipmi_msg * req, ipmi_msg * rsp )
{
    uint8_t len = rsp->data_len = 0;
    uint16_t offset =  (req->data[2] << 8) | (req->data[1]);
    uint8_t fru_id = req->data[0];
    uint8_t count;

    rsp->completion_code = IPMI_CC_OK;

    /* Use signed comparison here in case offset + data_len < 3 */
    if ((offset + req->data_len - 3) < (int16_t)amc_fru_info_size) {
        /* Write data to the FRU */
        count = fru_write( fru_id, &req->data[3], offset, req->data_len - 3);

        /* Count written (1 based) */
        rsp->data[len++] = count +1;
    } else {
        /* Count written (1 based) */
        rsp->data[len++] = 0;
        rsp->completion_code = IPMI_CC_PARAM_OUT_OF_RANGE;
    }
    rsp->data_len = len;
}
