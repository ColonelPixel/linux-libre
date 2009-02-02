/* mga_warp.c -- Matrox G200/G400 WARP engine management -*- linux-c -*-
 * Created: Thu Jan 11 21:29:32 2001 by gareth@valinux.com
 *
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Gareth Hughes <gareth@valinux.com>
 */

#include "drmP.h"
#include "drm.h"
#include "mga_drm.h"
#include "mga_drv.h"
#include "mga_ucode.h"

#define MGA_WARP_CODE_ALIGN		256	/* in bytes */

/*(DEBLOBBED)*/

unsigned int mga_warp_microcode_size(const drm_mga_private_t * dev_priv)
{
	switch (dev_priv->chipset) {
	/*(DEBLOBBED)*/
	default:
		return 0;
	}
}

/*(DEBLOBBED)*/

int mga_warp_install_microcode(drm_mga_private_t * dev_priv)
{
	const unsigned int size = mga_warp_microcode_size(dev_priv);

	DRM_DEBUG("MGA ucode size = %d bytes\n", size);
	if (size > dev_priv->warp->size) {
		DRM_ERROR("microcode too large! (%u > %lu)\n",
			  size, dev_priv->warp->size);
		return -ENOMEM;
	}

	switch (dev_priv->chipset) {
	/*(DEBLOBBED)*/
	default:
		return -EINVAL;
	}
}

#define WMISC_EXPECTED		(MGA_WUCODECACHE_ENABLE | MGA_WMASTER_ENABLE)

int mga_warp_init(drm_mga_private_t * dev_priv)
{
	u32 wmisc;

	/* FIXME: Get rid of these damned magic numbers...
	 */
	switch (dev_priv->chipset) {
	case MGA_CARD_TYPE_G400:
	case MGA_CARD_TYPE_G550:
		MGA_WRITE(MGA_WIADDR2, MGA_WMODE_SUSPEND);
		MGA_WRITE(MGA_WGETMSB, 0x00000E00);
		MGA_WRITE(MGA_WVRTXSZ, 0x00001807);
		MGA_WRITE(MGA_WACCEPTSEQ, 0x18000000);
		break;
	case MGA_CARD_TYPE_G200:
		MGA_WRITE(MGA_WIADDR, MGA_WMODE_SUSPEND);
		MGA_WRITE(MGA_WGETMSB, 0x1606);
		MGA_WRITE(MGA_WVRTXSZ, 7);
		break;
	default:
		return -EINVAL;
	}

	MGA_WRITE(MGA_WMISC, (MGA_WUCODECACHE_ENABLE |
			      MGA_WMASTER_ENABLE | MGA_WCACHEFLUSH_ENABLE));
	wmisc = MGA_READ(MGA_WMISC);
	if (wmisc != WMISC_EXPECTED) {
		DRM_ERROR("WARP engine config failed! 0x%x != 0x%x\n",
			  wmisc, WMISC_EXPECTED);
		return -EINVAL;
	}

	return 0;
}
