/*
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2017 SiFive
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#include <linux/clocksource.h>
#include <linux/delay.h>
#include <asm/sbi.h>

unsigned long riscv_timebase;

void __init time_init(void)
{
	struct device_node *cpu;
	u32 prop;

	cpu = of_find_node_by_path("/cpus");
	if (!cpu || of_property_read_u32(cpu, "timebase-frequency", &prop))
		panic(KERN_WARNING "RISC-V system with no 'timebase-frequency' in DTS\n");
	of_node_put(cpu);
	riscv_timebase = prop;

	lpj_fine = riscv_timebase / HZ;
	timer_probe();
}
