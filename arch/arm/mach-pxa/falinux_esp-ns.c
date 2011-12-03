/*
 *  linux/arch/arm/mach-pxa/falinux_esp-ns.c
 *
 *  Support for the FALinux ESP-NS Development Platform.
 *
 *  Author:	You young-chang
 *  Copyright:	FALinux.Co.Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/sysdev.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/bitops.h>
#include <linux/fb.h>
#include <linux/ioport.h>

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/sizes.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/arch/pxa-regs.h>
#include <asm/arch/audio.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/mmc.h>

#include "generic.h"


static void __init esp_ns_init_irq(void)
{
	pxa_init_irq();
	set_irq_type(IRQ_GPIO(21), IRQT_RISING); // CS8900A #1
	set_irq_type(IRQ_GPIO(22), IRQT_RISING); // CS8900A #2

	set_irq_type(IRQ_GPIO(23), IRQT_RISING); // 16550A
	set_irq_type(IRQ_GPIO(24), IRQT_RISING); // 16550A
	set_irq_type(IRQ_GPIO(25), IRQT_RISING); // 16550A
	set_irq_type(IRQ_GPIO(26), IRQT_RISING); // 16550A
}

// Serial 16552 ------------------------------------------------------
#include <linux/serial_8250.h>
static struct plat_serial8250_port serial_platform_data[] = {
	{
		.membase	= 0,
		.mapbase	= (PXA_CS0_PHYS + 0xC00000),		// port-0
		.irq		= IRQ_GPIO(23),
		.flags		= UPF_SKIP_TEST | UPF_IOREMAP,
		.iotype		= UPIO_MEM,
		.regshift	= 1,
		.uartclk	= 3686400,
	}, {	
		.membase	= 0,
		.mapbase	= (PXA_CS0_PHYS + 0xC00100),		// port-1
		.irq		= IRQ_GPIO(24),
		.flags		= UPF_SKIP_TEST | UPF_IOREMAP,
		.iotype		= UPIO_MEM,
		.regshift	= 1,
		.uartclk	= 3686400,
	}, {	
		.membase	= 0,
		.mapbase	= (PXA_CS0_PHYS + 0xC00200),		// port-2
		.irq		= IRQ_GPIO(25),
		.flags		= UPF_SKIP_TEST | UPF_IOREMAP,
		.iotype		= UPIO_MEM,
		.regshift	= 1,
		.uartclk	= 3686400,
	}, {	
		.membase	= 0,
		.mapbase	= (PXA_CS0_PHYS + 0xC00300),		// port-3
		.irq		= IRQ_GPIO(26),
		.flags		= UPF_SKIP_TEST | UPF_IOREMAP,
		.iotype		= UPIO_MEM,
		.regshift	= 1,
		.uartclk	= 3686400,
	}, 
	{ NULL }
};

static struct platform_device serial_device = {
	.name	= "serial8250",
	.id		= 0,
	.dev	= { 
		.platform_data  = serial_platform_data,
	},
};


// =====================================================================

static struct platform_device *esp_ns_devices[] __initdata = {
	&serial_device
};

static void __init esp_ns_init(void)
{
	platform_add_devices(esp_ns_devices, ARRAY_SIZE(esp_ns_devices));
	
}

static struct map_desc esp_ns_io_desc[] __initdata = {
	
  	{	// nCS0 Boot Flash  -- slow RD/WR
		.virtual	= 0xf0000000,
		.pfn		= __phys_to_pfn(PXA_CS0_PHYS+0x000000),
		.length		= 0x00400000,
		.type		= MT_DEVICE
	},
  	{	// nCS0 CS8900 #1
		.virtual	= 0xf1000000,
		.pfn		= __phys_to_pfn(PXA_CS0_PHYS+0x400000),
		.length		= 0x00100000,
		.type		= MT_DEVICE
	},
  	{	// nCS1 CS8900 #2
		.virtual	= 0xf1100000,
		.pfn		= __phys_to_pfn(PXA_CS0_PHYS+0x800000),
		.length		= 0x00100000,
		.type		= MT_DEVICE
	},
};

static void __init esp_ns_map_io(void)
{
	pxa_map_io();
	iotable_init(esp_ns_io_desc, ARRAY_SIZE(esp_ns_io_desc));

	// FF-UART
	pxa_gpio_mode(GPIO34_FFRXD_MD);
	pxa_gpio_mode(GPIO39_FFTXD_MD);
	
	// ST-UART
	pxa_gpio_mode(GPIO46_STRXD_MD);
	pxa_gpio_mode(GPIO47_STTXD_MD);
}

MACHINE_START(FALINUX_PXA255, "FALinux ESP-NS Development Platform")
	// Maintainer: FALINUX Software Inc.
	.phys_io	  = 0x40000000,
	.boot_params  = 0xa0000100,			// EZBOOT boot parameter setting
	.io_pg_offst  = (io_p2v(0x40000000) >> 18) & 0xfffc,
	.map_io		  = esp_ns_map_io,
	.init_irq	  = esp_ns_init_irq,
	.timer		  = &pxa_timer,
	.init_machine = esp_ns_init,
MACHINE_END
