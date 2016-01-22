/*
 * hpm.c
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

/* Project includes */
#include "ipmi.h"
#include "hpm.h"
#include "utils.h"
#include "string.h"
#include "led.h"
#include "payload.h"

/* Local Variables */

/* Stores the code of the current or last completed long duration command */
static uint8_t cmd_in_progress;

/* Stores the completion code of the current or last completed long duration command */
static uint8_t last_cmd_cc;

/*Current component under upgrade */
static uint8_t active_id;

/* IPMC Capabilities */
t_ipmc_capabilities ipmc_cap = {
    .flags = { .upgrade_undesirable = 0,
               .automatic_rollback_overridden = 0,
               .ipmc_degraded_in_update = 0,
               .deferred_activation = 1,
               .services_affected = 1,
               .manual_rollback = 1,
               .automatic_rollback = 1,
               .self_test = 1
    }
};

/* Components properties */
t_component hpm_components[HPM_MAX_COMPONENTS] = {
    [HPM_BOOTLOADER_COMPONENT_ID] = {
        .properties = {
            .flags = {
                .reserved = 0x00,
                .cold_reset_required = 1,
                .deferred_activation_supported = 0,
                .comparison_supported = 1,
                .preparation_support = 0,
                .rollback_backup_support = 0x01
            }
        },
    },
    [HPM_IPMC_COMPONENT_ID] = {
        .properties = {
            .flags = {
                .reserved = 0x00,
                .cold_reset_required = 1,
                .deferred_activation_supported = 0,
                .comparison_supported = 1,
                .preparation_support = 0,
                .rollback_backup_support = 0x01
            }
        },
    },
    [HPM_PAYLOAD_COMPONENT_ID] = {
        .properties = {
            .flags = {
                .reserved = 0x00,
                .cold_reset_required = 1,
                .deferred_activation_supported = 0,
                .comparison_supported = 1,
                .preparation_support = 1,
                .rollback_backup_support = 0x01
            }
        },
        .hpm_prepare_comp_f = payload_hpm_prepare_comp,
        .hpm_upload_block_f = payload_hpm_upload_block,
        .hpm_finish_upload_f = payload_hpm_finish_upload
    }
};

/*******************************************************/

void hpm_init( void )
{
    memcpy(hpm_components[HPM_BOOTLOADER_COMPONENT_ID].description, "Bootloader", sizeof("Bootloader"));
    memcpy(hpm_components[HPM_IPMC_COMPONENT_ID].description, "AFC IPMC", sizeof("AFC IPMC"));
    memcpy(hpm_components[HPM_PAYLOAD_COMPONENT_ID].description, "FPGA Payload", sizeof("FPGA Payload"));
#if 0
    sprintf(hpm_components[HPM_BOOTLOADER_COMPONENT_ID].description, "%s", "Bootloader");
    sprintf(hpm_components[HPM_IPMC_COMPONENT_ID].description, "%s", "AFC IPMC");
    sprintf(hpm_components[HPM_PAYLOAD_COMPONENT_ID].description, "%s", "FPGA Payload");*/
#endif

}

IPMI_HANDLER(ipmi_picmg_get_upgrade_capabilities, NETFN_GRPEXT, IPMI_PICMG_CMD_HPM_GET_UPGRADE_CAPABILITIES, ipmi_msg *req, ipmi_msg* rsp)
{
    uint8_t len = rsp->data_len = 0;

    rsp->data[len++] = IPMI_PICMG_GRP_EXT;
    rsp->data[len++] = HPM_SUPPORTED_VERSION;
    rsp->data[len++] = ipmc_cap.byte;
    rsp->data[len++] = HPM_UPGRADE_TIMEOUT;
    rsp->data[len++] = HPM_SELF_TEST_TIMEOUT;
    rsp->data[len++] = HPM_ROLLBACK_TIMEOUT;
    rsp->data[len++] = HPM_INACCESSIBILITY_TIMEOUT;
    rsp->data[len++] = (1 << HPM_BOOTLOADER_COMPONENT_ID | 1 << HPM_IPMC_COMPONENT_ID | 1 << HPM_PAYLOAD_COMPONENT_ID);

    rsp->data_len = len;
    rsp->completion_code = IPMI_CC_OK;

    /* This is not a long-duration command, so we don't need to update neither cmd_in_progress nor last_cmd_cc variables */
}

IPMI_HANDLER(ipmi_picmg_get_component_properties, NETFN_GRPEXT, IPMI_PICMG_CMD_HPM_GET_COMPONENT_PROPERTIES, ipmi_msg *req, ipmi_msg* rsp)
{
    uint8_t len = rsp->data_len = 0;

    uint8_t comp_id = req->data[1];
    uint8_t comp_properties_selector = req->data[2];

    rsp->completion_code = IPMI_CC_UNSPECIFIED_ERROR;

    if (comp_id > 7) {
        rsp->data[len++] = IPMI_PICMG_GRP_EXT;
        /* Return command-specific completion code: 0x82 (Invalid Component ID) */
        rsp->data_len = len;
        rsp->completion_code = 0x82;
        return;
    }

    rsp->data[len++] = IPMI_PICMG_GRP_EXT;

    switch (comp_properties_selector) {
    case 0x00:
        /* General component properties */
        rsp->data[len++] = hpm_components[comp_id].properties.byte;
        rsp->completion_code = IPMI_CC_OK;
        break;
    case 0x01:
        /* Firmware current version */
        /* TODO: Read fw revision from flash */
        rsp->data[len++] = (0x7F & FW_REV_MAJOR);
        rsp->data[len++] = FW_REV_MINOR;
        rsp->data[len++] = FW_REV_AUX_0;
        rsp->data[len++] = FW_REV_AUX_1;
        rsp->data[len++] = FW_REV_AUX_2;
        rsp->data[len++] = FW_REV_AUX_3;
        rsp->completion_code = IPMI_CC_OK;
        break;
    case 0x02:
        /* Description string */
        memcpy( &rsp->data[len], &hpm_components[comp_id].description[0], 12 );
        len += 12;
        rsp->completion_code = IPMI_CC_OK;
        break;
    case 0x03:
        /* Rollback Firmware version */
        /* TODO: Read fw revision from flash */
        rsp->data[len++] = (0x7F & FW_REV_MAJOR);
        rsp->data[len++] = FW_REV_MINOR;
        rsp->data[len++] = FW_REV_AUX_0;
        rsp->data[len++] = FW_REV_AUX_1;
        rsp->data[len++] = FW_REV_AUX_2;
        rsp->data[len++] = FW_REV_AUX_3;
        rsp->completion_code = IPMI_CC_OK;
        break;
    case 0x04:
        /* Deferred upgrade Firmware version */
        /* TODO: Read fw revision from flash */
        rsp->data[len++] = (0x7F & FW_REV_MAJOR);
        rsp->data[len++] = FW_REV_MINOR;
        rsp->data[len++] = FW_REV_AUX_0;
        rsp->data[len++] = FW_REV_AUX_1;
        rsp->data[len++] = FW_REV_AUX_2;
        rsp->data[len++] = FW_REV_AUX_3;
        rsp->completion_code = IPMI_CC_OK;
        break;
    default:
        /* Return command-specific completion code: 0x83 (Invalid Component properties selector) */
        rsp->completion_code = 0x83;
    }

    rsp->data_len = len;
    /* This is not a long-duration command, so we don't need to update neither cmd_in_progress nor last_cmd_cc variables */
}

IPMI_HANDLER(ipmi_picmg_initiate_upgrade_action, NETFN_GRPEXT, IPMI_PICMG_CMD_HPM_INITIATE_UPGRADE_ACTION, ipmi_msg *req, ipmi_msg* rsp)
{
    uint8_t len = rsp->data_len = 0;

    uint8_t comp_id = req->data[1];
    uint8_t upgrade_action = req->data[2];

    rsp->completion_code = IPMI_CC_UNSPECIFIED_ERROR;

    if (comp_id > 7) {
        /* Component ID out of range */
        rsp->data[len++] = IPMI_PICMG_GRP_EXT;
        /* Return command-specific completion code: 0x82 (Invalid Component ID) */
        rsp->data_len = len;
        rsp->completion_code = 0x82;
        return;
    }

    active_id = comp_id;

#if 0
    if (((upgrade_action == 0x02)|(upgrade_action == 0x03))
        && !isPowerofTwo(comp_id)) {
        /* More than one component selected */
        rsp->data[len++] = IPMI_PICMG_GRP_EXT;
        rsp->data_len = len;
        rsp->completion_code = IPMI_CC_INV_DATA_FIELD_IN_REQ;
        return;
    }
#endif
    rsp->data[len++] = IPMI_PICMG_GRP_EXT;

    switch (upgrade_action) {
    case 0x00:
        /* Backup component */
        break;
    case 0x01:
        /* Prepare Component */
        if (hpm_components[active_id].hpm_prepare_comp_f) {
            rsp->completion_code = hpm_components[active_id].hpm_prepare_comp_f();
        }
        break;
    case 0x02:
        /* Upload for upgrade */
        /* Set the component that'll be upgraded */
        active_id = comp_id;
        if (hpm_components[active_id].hpm_prepare_comp_f) {
            rsp->completion_code = hpm_components[active_id].hpm_prepare_comp_f();
        }
        break;
    case 0x03:
        /* Upload for compare */
        break;
    default:
        break;
    }

    rsp->data_len = len;

    /* This is a long-duration command, update both cmd_in_progress and last_cmd_cc */
    cmd_in_progress = req->cmd;
    last_cmd_cc = rsp->completion_code;
}

IPMI_HANDLER(ipmi_picmg_get_upgrade_status, NETFN_GRPEXT, IPMI_PICMG_CMD_HPM_GET_UPGRADE_STATUS, ipmi_msg *req, ipmi_msg* rsp)
{
    uint8_t len = rsp->data_len = 0;

    if (hpm_components[active_id].hpm_get_upgrade_status_f) {
        /* WARNING: This function can't block! */
        last_cmd_cc = hpm_components[active_id].hpm_get_upgrade_status_f();
    } else {
        /* Returning IPMI_CC_OK for debug purposes only, should be IPMI_CC_UNSPECIFIED_ERROR */
        last_cmd_cc = IPMI_CC_OK;
    }

    rsp->data[len++] = IPMI_PICMG_GRP_EXT;
    rsp->data[len++] = cmd_in_progress;
    rsp->data[len++] = last_cmd_cc;
    rsp->data_len = len;
    rsp->completion_code = IPMI_CC_OK;

    /* This is not a long-duration command, so we don't need to update neither cmd_in_progress nor last_cmd_cc variables */
}

IPMI_HANDLER(ipmi_picmg_abort_firmware_upgrade, NETFN_GRPEXT, IPMI_PICMG_CMD_HPM_ABORT_FIRMWARE_UPGRADE, ipmi_msg *req, ipmi_msg* rsp)
{
    uint8_t len = rsp->data_len = 0;

    rsp->data[len++] = IPMI_PICMG_GRP_EXT;

    rsp->data_len = len;
    rsp->completion_code = IPMI_CC_OK;
    /* This is not a long-duration command, so we don't need to update neither cmd_in_progress nor last_cmd_cc variables */
}

IPMI_HANDLER(ipmi_picmg_upload_firmware_block, NETFN_GRPEXT, IPMI_PICMG_CMD_HPM_UPLOAD_FIRMWARE_BLOCK, ipmi_msg *req, ipmi_msg* rsp)
{
    uint8_t len = rsp->data_len = 0;
    uint8_t block_data[HPM_BLOCK_SIZE];

    if (active_id > 7) {
        /* Component ID out of range */
        rsp->data[len++] = IPMI_PICMG_GRP_EXT;
        /* Return command-specific completion code: 0x81 (Invalid Component ID) */
        rsp->data_len = len;
        rsp->completion_code = 0x81;
        return;
    }

    uint8_t block_num = req->data[1];
    memcpy(&block_data[0], &req->data[2], req->data_len-2);

    /* TODO: perform checksum of the block */

    if (hpm_components[active_id].hpm_upload_block_f) {
        /* WARNING: This function can't block! */
        rsp->completion_code = hpm_components[active_id].hpm_upload_block_f(&block_data[0], sizeof(block_data));
    } else {
        rsp->completion_code = IPMI_CC_UNSPECIFIED_ERROR;
    }

    rsp->data[len++] = IPMI_PICMG_GRP_EXT;
    rsp->data_len = len;

    /* This is a long-duration command, update both cmd_in_progress and last_cmd_cc */
    cmd_in_progress = req->cmd;
    last_cmd_cc = rsp->completion_code;
}

IPMI_HANDLER(ipmi_picmg_finish_firmware_upload, NETFN_GRPEXT, IPMI_PICMG_CMD_HPM_FINISH_FIRMWARE_UPLOAD, ipmi_msg *req, ipmi_msg* rsp)
{
    uint8_t len = rsp->data_len = 0;

    uint32_t image_len = (req->data[5] << 24) | (req->data[4] << 16) | (req->data[3] << 8) | (req->data[2]);

    /* TODO: compare image_len with the actual data received */
    /* TODO: implement HPM.1 REQ3.59 */

    if ( hpm_components[active_id].hpm_finish_upload_f) {
        rsp->completion_code = hpm_components[active_id].hpm_finish_upload_f( image_len );
    } else {
        rsp->completion_code = IPMI_CC_UNSPECIFIED_ERROR;
    }

    rsp->data[len++] = IPMI_PICMG_GRP_EXT;

    rsp->data_len = len;


    /* This is a long-duration command, update both cmd_in_progress and last_cmd_cc */
    cmd_in_progress = req->cmd;
    last_cmd_cc = rsp->completion_code;
}

IPMI_HANDLER(ipmi_picmg_activate_firmware, NETFN_GRPEXT, IPMI_PICMG_CMD_HPM_ACTIVATE_FIRMWARE, ipmi_msg *req, ipmi_msg* rsp)
{
    uint8_t len = rsp->data_len = 0;

    /* TODO: Compare firmware revisions before activating */

    if (hpm_components[active_id].hpm_activate_firmware_f) {
        rsp->completion_code = hpm_components[active_id].hpm_activate_firmware_f();
    } else {
        rsp->completion_code = IPMI_CC_UNSPECIFIED_ERROR;
    }

    rsp->data[len++] = IPMI_PICMG_GRP_EXT;

    rsp->data_len = len;

    /* This is a long-duration command, update both cmd_in_progress and last_cmd_cc */
    cmd_in_progress = req->cmd;
    last_cmd_cc = rsp->completion_code;
}
