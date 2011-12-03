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
#include <linux/irq.h>

#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <asm/arch/ep93xx-regs.h>

// Serial 16552 ------------------------------------------------------
#include <linux/serial_8250.h>
static struct plat_serial8250_port serial_platform_data[] = {
	{
		.membase    = 0,
		.mapbase    = (EP9312_CS2_PHYS +0x100000),      // port-0
		.irq        = IRQ_EP93XX_EXT0,
		.flags      = UPF_SKIP_TEST | UPF_IOREMAP,
		.iotype     = UPIO_MEM,
		.regshift   = 1,
		.uartclk    = 3686400,
	} , {
		.membase    = 0,
		.mapbase    = (EP9312_CS2_PHYS +0x180000),      // port-1
		.irq        = IRQ_EP93XX_EXT1,
		.flags      = UPF_SKIP_TEST | UPF_IOREMAP,
		.iotype     = UPIO_MEM,
		.regshift   = 1,
		.uartclk    = 3686400,
	}, {NULL},
};

static struct platform_device serial_device = {
	.name   = "serial8250",
	.id     = 0,
	.dev    = {
		.platform_data  = serial_platform_data,
	},
};
// =====================================================================

static struct platform_device *devices[] __initdata = {
	&serial_device,
};

static void __init falinuxep9312_init_machine(void)
{
	ep93xx_init_devices();

	platform_add_devices(devices, ARRAY_SIZE(devices));

	// IRQ Init
	// set_irq_type( IRQ_EP93XX_GPIO(13), IRQT_FALLING ); 
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

