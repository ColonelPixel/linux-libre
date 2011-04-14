/*
 * Copyright (c) 1996, 2003 VIA Networking Technologies, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *
 * File: baseband.c
 *
 * Purpose: Implement functions to access baseband
 *
 * Author: Yiching Chen
 *
 * Date: May 20, 2004
 *
 * Functions:
 *
 * Revision History:
 *
 */

#include "firmware.h"
#include "control.h"
#include "rndis.h"

/*---------------------  Static Definitions -------------------------*/

static int          msglevel                =MSG_LEVEL_INFO;
//static int          msglevel                =MSG_LEVEL_DEBUG;
/*---------------------  Static Classes  ----------------------------*/

/*---------------------  Static Variables  --------------------------*/

/*---------------------  Static Functions  --------------------------*/

/*---------------------  Export Variables  --------------------------*/

/*
 *  This is firmware version 1.51
 */
#define FIRMWARE_VERSION        0x133

/*(DEBLOBBED)*/

/*---------------------  Export Functions  --------------------------*/


BOOL
FIRMWAREbDownload(
     PSDevice pDevice
    )
{
    int NdisStatus;
    PBYTE abyFirmware[1] = { 0 };
    PBYTE       pBuffer = NULL;
    WORD        wLength;
    int         ii;

    printk("vt6656: missing Free firmware\n");
    return (FALSE);
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"---->Download firmware\n");
    spin_unlock_irq(&pDevice->lock);
    pBuffer = kmalloc(sizeof(abyFirmware), GFP_KERNEL);
    if (pBuffer != NULL) {

        for (ii=0;ii<sizeof(abyFirmware);ii++)
            pBuffer[ii] = abyFirmware[ii];

        for (ii=0;ii<sizeof(abyFirmware);ii+=0x400) {

            if ((sizeof(abyFirmware) - ii) < 0x400)
                wLength = (sizeof(abyFirmware) - ii);
            else
                wLength = 0x400;

            NdisStatus = CONTROLnsRequestOutAsyn(pDevice,
                                            0,
                                            0x1200+ii,
                                            0x0000,
                                            wLength,
                                            &(pBuffer[ii])
                                            );

            DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Download firmware...%d %zu\n", ii, sizeof(abyFirmware));
            if (NdisStatus != STATUS_SUCCESS) {
                if (pBuffer)
                    kfree(pBuffer);
                spin_lock_irq(&pDevice->lock);
		return FALSE;
            }
        }
    }

    if (pBuffer)
        kfree(pBuffer);

    spin_lock_irq(&pDevice->lock);
    return (TRUE);
}

BOOL
FIRMWAREbBrach2Sram(
     PSDevice pDevice
    )
{
    int NdisStatus;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"---->Branch to Sram\n");

    NdisStatus = CONTROLnsRequestOut(pDevice,
                                    1,
                                    0x1200,
                                    0x0000,
                                    0,
                                    NULL
                                    );

    if (NdisStatus != STATUS_SUCCESS) {
        return (FALSE);
    } else {
        return (TRUE);
    }
}


BOOL
FIRMWAREbCheckVersion(
     PSDevice pDevice
    )
{
	int ntStatus;

    ntStatus = CONTROLnsRequestIn(pDevice,
                                    MESSAGE_TYPE_READ,
                                    0,
                                    MESSAGE_REQUEST_VERSION,
                                    2,
                                    (PBYTE) &(pDevice->wFirmwareVersion));

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Firmware Version [%04x]\n", pDevice->wFirmwareVersion);
    if (ntStatus != STATUS_SUCCESS) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Firmware Invalid.\n");
        return FALSE;
    }
    if (pDevice->wFirmwareVersion == 0xFFFF) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"In Loader.\n");
        return FALSE;
    }
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"Firmware Version [%04x]\n", pDevice->wFirmwareVersion);
    if (pDevice->wFirmwareVersion != FIRMWARE_VERSION) {
        // branch to loader for download new firmware
        FIRMWAREbBrach2Sram(pDevice);
        return FALSE;
    }
    return TRUE;
}
