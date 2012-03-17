/**************************************************************************
 *                  QLOGIC LINUX SOFTWARE
 *
 * Copyright (C) 2004 QLogic Corporation
 * (www.qlogic.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 *************************************************************************/

/************************************************************************
 *									*
 * 	        --- ISP1040 Initiator/Target Firmware ---               *
 *			    32 LUN Support				*
 *									*
 ************************************************************************
 */

/*
 *	Firmware Version 7.65.06 (14:38 Jan 07, 2002)
 */

static unsigned char firmware_version[] = {7,65,6};

#define FW_VERSION_STRING "7.65.06"

static unsigned short risc_code_addr01 = 0x1000 ;

/*(DEBLOBBED)*/
