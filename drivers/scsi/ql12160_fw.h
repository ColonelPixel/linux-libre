/*****************************************************************************
 *                  QLOGIC LINUX SOFTWARE
 *
 * QLogic ISP12160 device driver for Linux 2.2.x and 2.4.x
 * Copyright (C) 2002 Qlogic Corporation (www.qlogic.com)
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
 * 	           --- ISP12160A Initiator Firmware ---                 *
 *			      32 LUN Support                            *
 ************************************************************************/

/*
 *	Firmware Version 10.04.42 (15:44 Apr 18, 2003)
 */

#ifdef UNIQUE_FW_NAME
static unsigned char fw12160i_version_str[] = {10,4,42};
#else
static unsigned char firmware_version[] = {10,4,42};
#endif

#ifdef UNIQUE_FW_NAME
#define fw12160i_VERSION_STRING "10.04.42"
#else
#define FW_VERSION_STRING "10.04.42"
#endif

#ifdef UNIQUE_FW_NAME
static unsigned short fw12160i_addr01 = 0x1000;
#else
static unsigned short risc_code_addr01 = 0x1000;
#endif

/*(DEBLOBBED)*/

