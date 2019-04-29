// SPDX-License-Identifier: GPL-2.0
/*
 * soc-apci-intel-cnl-match.c - tables and support for CNL ACPI enumeration.
 *
 * Copyright (c) 2018, Intel Corporation.
 *
 */

#include <sound/soc-acpi.h>
#include <sound/soc-acpi-intel-match.h>
#include "../skylake/skl.h"

static struct skl_machine_pdata cnl_pdata = {
	.use_tplg_pcm = true,
};

struct snd_soc_acpi_mach snd_soc_acpi_intel_cnl_machines[] = {
	{
		.id = "INT34C2",
		.drv_name = "cnl_rt274",
		.fw_filename = "/*(DEBLOBBED)*/",
		.pdata = &cnl_pdata,
		.sof_fw_filename = "/*(DEBLOBBED)*/",
		.sof_tplg_filename = "/*(DEBLOBBED)*/",
	},
	{},
};
EXPORT_SYMBOL_GPL(snd_soc_acpi_intel_cnl_machines);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Intel Common ACPI Match module");
