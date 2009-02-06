/*
 * Copyright (c) 2005-2008 Chelsio, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "common.h"
#include "regs.h"

enum {
	PMD_RSD     = 10,   /* PMA/PMD receive signal detect register */
	PCS_STAT1_X = 24,   /* 10GBASE-X PCS status 1 register */
	PCS_STAT1_R = 32,   /* 10GBASE-R PCS status 1 register */
	XS_LN_STAT  = 24    /* XS lane status register */
};

enum {
	AEL100X_TX_DISABLE = 9,
	AEL100X_TX_CONFIG1 = 0xc002,
	AEL1002_PWR_DOWN_HI = 0xc011,
	AEL1002_PWR_DOWN_LO = 0xc012,
	AEL1002_XFI_EQL = 0xc015,
	AEL1002_LB_EN = 0xc017,
	AEL_OPT_SETTINGS = 0xc017,
	AEL_I2C_CTRL = 0xc30a,
	AEL_I2C_DATA = 0xc30b,
	AEL_I2C_STAT = 0xc30c,
	AEL2005_GPIO_CTRL = 0xc214,
	AEL2005_GPIO_STAT = 0xc215,
};

enum { edc_none, edc_sr, edc_twinax };

/* PHY module I2C device address */
#define MODULE_DEV_ADDR 0xa0

#define AEL2005_MODDET_IRQ 4

struct reg_val {
	unsigned short mmd_addr;
	unsigned short reg_addr;
	unsigned short clear_bits;
	unsigned short set_bits;
};

static int set_phy_regs(struct cphy *phy, const struct reg_val *rv)
{
	int err;

	for (err = 0; rv->mmd_addr && !err; rv++) {
		if (rv->clear_bits == 0xffff)
			err = mdio_write(phy, rv->mmd_addr, rv->reg_addr,
					 rv->set_bits);
		else
			err = t3_mdio_change_bits(phy, rv->mmd_addr,
						  rv->reg_addr, rv->clear_bits,
						  rv->set_bits);
	}
	return err;
}

static void ael100x_txon(struct cphy *phy)
{
	int tx_on_gpio = phy->addr == 0 ? F_GPIO7_OUT_VAL : F_GPIO2_OUT_VAL;

	msleep(100);
	t3_set_reg_field(phy->adapter, A_T3DBG_GPIO_EN, 0, tx_on_gpio);
	msleep(30);
}

static int ael1002_power_down(struct cphy *phy, int enable)
{
	int err;

	err = mdio_write(phy, MDIO_DEV_PMA_PMD, AEL100X_TX_DISABLE, !!enable);
	if (!err)
		err = t3_mdio_change_bits(phy, MDIO_DEV_PMA_PMD, MII_BMCR,
					  BMCR_PDOWN, enable ? BMCR_PDOWN : 0);
	return err;
}

static int ael1002_reset(struct cphy *phy, int wait)
{
	int err;

	if ((err = ael1002_power_down(phy, 0)) ||
	    (err = mdio_write(phy, MDIO_DEV_PMA_PMD, AEL100X_TX_CONFIG1, 1)) ||
	    (err = mdio_write(phy, MDIO_DEV_PMA_PMD, AEL1002_PWR_DOWN_HI, 0)) ||
	    (err = mdio_write(phy, MDIO_DEV_PMA_PMD, AEL1002_PWR_DOWN_LO, 0)) ||
	    (err = mdio_write(phy, MDIO_DEV_PMA_PMD, AEL1002_XFI_EQL, 0x18)) ||
	    (err = t3_mdio_change_bits(phy, MDIO_DEV_PMA_PMD, AEL1002_LB_EN,
				       0, 1 << 5)))
		return err;
	return 0;
}

static int ael1002_intr_noop(struct cphy *phy)
{
	return 0;
}

/*
 * Get link status for a 10GBASE-R device.
 */
static int get_link_status_r(struct cphy *phy, int *link_ok, int *speed,
			     int *duplex, int *fc)
{
	if (link_ok) {
		unsigned int stat0, stat1, stat2;
		int err = mdio_read(phy, MDIO_DEV_PMA_PMD, PMD_RSD, &stat0);

		if (!err)
			err = mdio_read(phy, MDIO_DEV_PCS, PCS_STAT1_R, &stat1);
		if (!err)
			err = mdio_read(phy, MDIO_DEV_XGXS, XS_LN_STAT, &stat2);
		if (err)
			return err;
		*link_ok = (stat0 & stat1 & (stat2 >> 12)) & 1;
	}
	if (speed)
		*speed = SPEED_10000;
	if (duplex)
		*duplex = DUPLEX_FULL;
	return 0;
}

static struct cphy_ops ael1002_ops = {
	.reset = ael1002_reset,
	.intr_enable = ael1002_intr_noop,
	.intr_disable = ael1002_intr_noop,
	.intr_clear = ael1002_intr_noop,
	.intr_handler = ael1002_intr_noop,
	.get_link_status = get_link_status_r,
	.power_down = ael1002_power_down,
};

int t3_ael1002_phy_prep(struct cphy *phy, struct adapter *adapter,
			int phy_addr, const struct mdio_ops *mdio_ops)
{
	cphy_init(phy, adapter, phy_addr, &ael1002_ops, mdio_ops,
		  SUPPORTED_10000baseT_Full | SUPPORTED_AUI | SUPPORTED_FIBRE,
		   "10GBASE-R");
	ael100x_txon(phy);
	return 0;
}

static int ael1006_reset(struct cphy *phy, int wait)
{
	return t3_phy_reset(phy, MDIO_DEV_PMA_PMD, wait);
}

static int ael1006_power_down(struct cphy *phy, int enable)
{
	return t3_mdio_change_bits(phy, MDIO_DEV_PMA_PMD, MII_BMCR,
				   BMCR_PDOWN, enable ? BMCR_PDOWN : 0);
}

static struct cphy_ops ael1006_ops = {
	.reset = ael1006_reset,
	.intr_enable = t3_phy_lasi_intr_enable,
	.intr_disable = t3_phy_lasi_intr_disable,
	.intr_clear = t3_phy_lasi_intr_clear,
	.intr_handler = t3_phy_lasi_intr_handler,
	.get_link_status = get_link_status_r,
	.power_down = ael1006_power_down,
};

int t3_ael1006_phy_prep(struct cphy *phy, struct adapter *adapter,
			     int phy_addr, const struct mdio_ops *mdio_ops)
{
	cphy_init(phy, adapter, phy_addr, &ael1006_ops, mdio_ops,
		  SUPPORTED_10000baseT_Full | SUPPORTED_AUI | SUPPORTED_FIBRE,
		   "10GBASE-SR");
	ael100x_txon(phy);
	return 0;
}

static int ael2005_setup_sr_edc(struct cphy *phy)
{
	static struct reg_val regs[] = {
		{ MDIO_DEV_PMA_PMD, 0xc003, 0xffff, 0x181 },
		{ MDIO_DEV_PMA_PMD, 0xc010, 0xffff, 0x448a },
		{ MDIO_DEV_PMA_PMD, 0xc04a, 0xffff, 0x5200 },
		{ 0, 0, 0, 0 }
	};
	/*(DEBLOBBED)*/
	int i, err;

	err = set_phy_regs(phy, regs);
	if (err)
		return err;

	msleep(50);

	CH_ERR(phy->adapter, "Missing Free firwmare\n");
	err = -EINVAL;
	/*(DEBLOBBED)*/
	if (!err)
		phy->priv = edc_sr;
	return err;
}

static int ael2005_setup_twinax_edc(struct cphy *phy, int modtype)
{
	static struct reg_val regs[] = {
		{ MDIO_DEV_PMA_PMD, 0xc04a, 0xffff, 0x5a00 },
		{ 0, 0, 0, 0 }
	};
	static struct reg_val preemphasis[] = {
		{ MDIO_DEV_PMA_PMD, 0xc014, 0xffff, 0xfe16 },
		{ MDIO_DEV_PMA_PMD, 0xc015, 0xffff, 0xa000 },
		{ 0, 0, 0, 0 }
	};
	/*(DEBLOBBED)*/
	int i, err;

	err = set_phy_regs(phy, regs);
	if (!err && modtype == phy_modtype_twinax_long)
		err = set_phy_regs(phy, preemphasis);
	if (err)
		return err;

	msleep(50);

	CH_ERR(phy->adapter, "Missing Free firwmare\n");
	err = -EINVAL;
	/*(DEBLOBBED)*/
	if (!err)
		phy->priv = edc_twinax;
	return err;
}

static int ael2005_i2c_rd(struct cphy *phy, int dev_addr, int word_addr)
{
	int i, err;
	unsigned int stat, data;

	err = mdio_write(phy, MDIO_DEV_PMA_PMD, AEL_I2C_CTRL,
			 (dev_addr << 8) | (1 << 8) | word_addr);
	if (err)
		return err;

	for (i = 0; i < 5; i++) {
		msleep(1);
		err = mdio_read(phy, MDIO_DEV_PMA_PMD, AEL_I2C_STAT, &stat);
		if (err)
			return err;
		if ((stat & 3) == 1) {
			err = mdio_read(phy, MDIO_DEV_PMA_PMD, AEL_I2C_DATA,
					&data);
			if (err)
				return err;
			return data >> 8;
		}
	}
	CH_WARN(phy->adapter, "PHY %u I2C read of addr %u timed out\n",
		phy->addr, word_addr);
	return -ETIMEDOUT;
}

static int get_module_type(struct cphy *phy, int delay_ms)
{
	int v;
	unsigned int stat;

	v = mdio_read(phy, MDIO_DEV_PMA_PMD, AEL2005_GPIO_CTRL, &stat);
	if (v)
		return v;

	if (stat & (1 << 8))			/* module absent */
		return phy_modtype_none;

	if (delay_ms)
		msleep(delay_ms);

	/* see SFF-8472 for below */
	v = ael2005_i2c_rd(phy, MODULE_DEV_ADDR, 3);
	if (v < 0)
		return v;

	if (v == 0x10)
		return phy_modtype_sr;
	if (v == 0x20)
		return phy_modtype_lr;
	if (v == 0x40)
		return phy_modtype_lrm;

	v = ael2005_i2c_rd(phy, MODULE_DEV_ADDR, 6);
	if (v < 0)
		return v;
	if (v != 4)
		goto unknown;

	v = ael2005_i2c_rd(phy, MODULE_DEV_ADDR, 10);
	if (v < 0)
		return v;

	if (v & 0x80) {
		v = ael2005_i2c_rd(phy, MODULE_DEV_ADDR, 0x12);
		if (v < 0)
			return v;
		return v > 10 ? phy_modtype_twinax_long : phy_modtype_twinax;
	}
unknown:
	return phy_modtype_unknown;
}

static int ael2005_intr_enable(struct cphy *phy)
{
	int err = mdio_write(phy, MDIO_DEV_PMA_PMD, AEL2005_GPIO_CTRL, 0x200);
	return err ? err : t3_phy_lasi_intr_enable(phy);
}

static int ael2005_intr_disable(struct cphy *phy)
{
	int err = mdio_write(phy, MDIO_DEV_PMA_PMD, AEL2005_GPIO_CTRL, 0x100);
	return err ? err : t3_phy_lasi_intr_disable(phy);
}

static int ael2005_intr_clear(struct cphy *phy)
{
	int err = mdio_write(phy, MDIO_DEV_PMA_PMD, AEL2005_GPIO_CTRL, 0xd00);
	return err ? err : t3_phy_lasi_intr_clear(phy);
}

static int ael2005_reset(struct cphy *phy, int wait)
{
	static struct reg_val regs0[] = {
		{ MDIO_DEV_PMA_PMD, 0xc001, 0, 1 << 5 },
		{ MDIO_DEV_PMA_PMD, 0xc017, 0, 1 << 5 },
		{ MDIO_DEV_PMA_PMD, 0xc013, 0xffff, 0xf341 },
		{ MDIO_DEV_PMA_PMD, 0xc210, 0xffff, 0x8000 },
		{ MDIO_DEV_PMA_PMD, 0xc210, 0xffff, 0x8100 },
		{ MDIO_DEV_PMA_PMD, 0xc210, 0xffff, 0x8000 },
		{ MDIO_DEV_PMA_PMD, 0xc210, 0xffff, 0 },
		{ 0, 0, 0, 0 }
	};
	static struct reg_val regs1[] = {
		{ MDIO_DEV_PMA_PMD, 0xca00, 0xffff, 0x0080 },
		{ MDIO_DEV_PMA_PMD, 0xca12, 0xffff, 0 },
		{ 0, 0, 0, 0 }
	};

	int err, lasi_ctrl;

	err = mdio_read(phy, MDIO_DEV_PMA_PMD, LASI_CTRL, &lasi_ctrl);
	if (err)
		return err;

	err = t3_phy_reset(phy, MDIO_DEV_PMA_PMD, 0);
	if (err)
		return err;

	msleep(125);
	phy->priv = edc_none;
	err = set_phy_regs(phy, regs0);
	if (err)
		return err;

	msleep(50);

	err = get_module_type(phy, 0);
	if (err < 0)
		return err;
	phy->modtype = err;

	if (err == phy_modtype_twinax || err == phy_modtype_twinax_long)
		err = ael2005_setup_twinax_edc(phy, err);
	else
		err = ael2005_setup_sr_edc(phy);
	if (err)
		return err;

	err = set_phy_regs(phy, regs1);
	if (err)
		return err;

	/* reset wipes out interrupts, reenable them if they were on */
	if (lasi_ctrl & 1)
		err = ael2005_intr_enable(phy);
	return err;
}

static int ael2005_intr_handler(struct cphy *phy)
{
	unsigned int stat;
	int ret, edc_needed, cause = 0;

	ret = mdio_read(phy, MDIO_DEV_PMA_PMD, AEL2005_GPIO_STAT, &stat);
	if (ret)
		return ret;

	if (stat & AEL2005_MODDET_IRQ) {
		ret = mdio_write(phy, MDIO_DEV_PMA_PMD, AEL2005_GPIO_CTRL,
				 0xd00);
		if (ret)
			return ret;

		/* modules have max 300 ms init time after hot plug */
		ret = get_module_type(phy, 300);
		if (ret < 0)
			return ret;

		phy->modtype = ret;
		if (ret == phy_modtype_none)
			edc_needed = phy->priv;       /* on unplug retain EDC */
		else if (ret == phy_modtype_twinax ||
			 ret == phy_modtype_twinax_long)
			edc_needed = edc_twinax;
		else
			edc_needed = edc_sr;

		if (edc_needed != phy->priv) {
			ret = ael2005_reset(phy, 0);
			return ret ? ret : cphy_cause_module_change;
		}
		cause = cphy_cause_module_change;
	}

	ret = t3_phy_lasi_intr_handler(phy);
	if (ret < 0)
		return ret;

	ret |= cause;
	return ret ? ret : cphy_cause_link_change;
}

static struct cphy_ops ael2005_ops = {
	.reset           = ael2005_reset,
	.intr_enable     = ael2005_intr_enable,
	.intr_disable    = ael2005_intr_disable,
	.intr_clear      = ael2005_intr_clear,
	.intr_handler    = ael2005_intr_handler,
	.get_link_status = get_link_status_r,
	.power_down      = ael1002_power_down,
};

int t3_ael2005_phy_prep(struct cphy *phy, struct adapter *adapter,
			int phy_addr, const struct mdio_ops *mdio_ops)
{
	cphy_init(phy, adapter, phy_addr, &ael2005_ops, mdio_ops,
		  SUPPORTED_10000baseT_Full | SUPPORTED_AUI | SUPPORTED_FIBRE |
		  SUPPORTED_IRQ, "10GBASE-R");
	msleep(125);
	return t3_mdio_change_bits(phy, MDIO_DEV_PMA_PMD, AEL_OPT_SETTINGS, 0,
				   1 << 5);
}

/*
 * Get link status for a 10GBASE-X device.
 */
static int get_link_status_x(struct cphy *phy, int *link_ok, int *speed,
			     int *duplex, int *fc)
{
	if (link_ok) {
		unsigned int stat0, stat1, stat2;
		int err = mdio_read(phy, MDIO_DEV_PMA_PMD, PMD_RSD, &stat0);

		if (!err)
			err = mdio_read(phy, MDIO_DEV_PCS, PCS_STAT1_X, &stat1);
		if (!err)
			err = mdio_read(phy, MDIO_DEV_XGXS, XS_LN_STAT, &stat2);
		if (err)
			return err;
		*link_ok = (stat0 & (stat1 >> 12) & (stat2 >> 12)) & 1;
	}
	if (speed)
		*speed = SPEED_10000;
	if (duplex)
		*duplex = DUPLEX_FULL;
	return 0;
}

static struct cphy_ops qt2045_ops = {
	.reset = ael1006_reset,
	.intr_enable = t3_phy_lasi_intr_enable,
	.intr_disable = t3_phy_lasi_intr_disable,
	.intr_clear = t3_phy_lasi_intr_clear,
	.intr_handler = t3_phy_lasi_intr_handler,
	.get_link_status = get_link_status_x,
	.power_down = ael1006_power_down,
};

int t3_qt2045_phy_prep(struct cphy *phy, struct adapter *adapter,
		       int phy_addr, const struct mdio_ops *mdio_ops)
{
	unsigned int stat;

	cphy_init(phy, adapter, phy_addr, &qt2045_ops, mdio_ops,
		  SUPPORTED_10000baseT_Full | SUPPORTED_AUI | SUPPORTED_TP,
		  "10GBASE-CX4");

	/*
	 * Some cards where the PHY is supposed to be at address 0 actually
	 * have it at 1.
	 */
	if (!phy_addr && !mdio_read(phy, MDIO_DEV_PMA_PMD, MII_BMSR, &stat) &&
	    stat == 0xffff)
		phy->addr = 1;
	return 0;
}

static int xaui_direct_reset(struct cphy *phy, int wait)
{
	return 0;
}

static int xaui_direct_get_link_status(struct cphy *phy, int *link_ok,
				       int *speed, int *duplex, int *fc)
{
	if (link_ok) {
		unsigned int status;

		status = t3_read_reg(phy->adapter,
				     XGM_REG(A_XGM_SERDES_STAT0, phy->addr)) |
		    t3_read_reg(phy->adapter,
				XGM_REG(A_XGM_SERDES_STAT1, phy->addr)) |
		    t3_read_reg(phy->adapter,
				XGM_REG(A_XGM_SERDES_STAT2, phy->addr)) |
		    t3_read_reg(phy->adapter,
				XGM_REG(A_XGM_SERDES_STAT3, phy->addr));
		*link_ok = !(status & F_LOWSIG0);
	}
	if (speed)
		*speed = SPEED_10000;
	if (duplex)
		*duplex = DUPLEX_FULL;
	return 0;
}

static int xaui_direct_power_down(struct cphy *phy, int enable)
{
	return 0;
}

static struct cphy_ops xaui_direct_ops = {
	.reset = xaui_direct_reset,
	.intr_enable = ael1002_intr_noop,
	.intr_disable = ael1002_intr_noop,
	.intr_clear = ael1002_intr_noop,
	.intr_handler = ael1002_intr_noop,
	.get_link_status = xaui_direct_get_link_status,
	.power_down = xaui_direct_power_down,
};

int t3_xaui_direct_phy_prep(struct cphy *phy, struct adapter *adapter,
			    int phy_addr, const struct mdio_ops *mdio_ops)
{
	cphy_init(phy, adapter, phy_addr, &xaui_direct_ops, mdio_ops,
		  SUPPORTED_10000baseT_Full | SUPPORTED_AUI | SUPPORTED_TP,
		  "10GBASE-CX4");
	return 0;
}
