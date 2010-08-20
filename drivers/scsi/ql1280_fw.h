/*****************************************************************************
 *                  QLOGIC LINUX SOFTWARE
 *
 * QLogic ISP1280/ device driver for Linux 2.2.x and 2.4.x
 * Copyright (C) 2001 Qlogic Corporation (www.qlogic.com)
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
 *****************************************************************************/

/************************************************************************
 * 	         --- ISP1240/1080/1280 Initiator Firmware ---           *
 *			      32 LUN Support				*
 ************************************************************************/


/*
 *	Firmware Version 8.15.11 (10:20 Jan 02, 2002)
 */

#ifdef UNIQUE_FW_NAME
static unsigned char fw1280ei_version_str[] = {8,15,11};
#else
static unsigned char firmware_version[] = {8,15,11};
#endif

#ifdef UNIQUE_FW_NAME
#define fw1280ei_VERSION_STRING "8.15.11"
#else
#define FW_VERSION_STRING "8.15.11"
#endif

#ifdef UNIQUE_FW_NAME
static unsigned short fw1280ei_addr01 = 0x1000 ;
#else
static unsigned short risc_code_addr01 = 0x1000 ;
#endif

/*(DEBLOBBED)*/

