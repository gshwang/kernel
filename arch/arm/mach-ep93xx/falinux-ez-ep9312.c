/*
 * arch/arm/mach-ep93xx/edb9312.c
 * Cirrus Logic EDB9312 support.
 *
 * Copyright (C) 2006 Infosys Technologies Limited
 * 	Toufeeq Hussain	<toufeeq_hussain@infosys.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/mtd/physmap.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

static void __init falinuxep9312_init_machine(void)
{
	ep93xx_init_devices();
}

MACHINE_START(FALINUX_EP9312, "Falinux EZ-EP9312 for EP9312 Boar")
	/* Maintainer: Toufeeq Hussain <toufeeq_hussain@infosys.com> */
	.phys_io		= EP93XX_APB_PHYS_BASE,
	.io_pg_offst	= ((EP93XX_APB_VIRT_BASE) >> 18) & 0xfffc,
	.boot_params	= EP93XX_PARAMS_PHYS,
	.map_io			= ep93xx_map_io,
	.init_irq		= ep93xx_init_irq,
	.timer			= &ep93xx_timer,
	.init_machine	= falinuxep9312_init_machine,
MACHINE_END

