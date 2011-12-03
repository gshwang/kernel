/*
 *  linux/arch/arm/mach-pxa/ez_x5.c
 *
 *  Support for the FALinux EZ-X5 Development Platform.
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


static void __init falinux_pxa255_init_irq(void)
{
	pxa_init_irq();

	set_irq_type(IRQ_GPIO(19), IRQT_RISING); // smsc9111
	set_irq_type(IRQ_GPIO(20), IRQT_RISING); // smsc9111

	set_irq_type(IRQ_GPIO(23), IRQT_RISING); // 16550A	attatch 74hc32
	set_irq_type(IRQ_GPIO(24), IRQT_RISING); // 16550A
	set_irq_type(IRQ_GPIO(25), IRQT_RISING); // 16550A
	set_irq_type(IRQ_GPIO(26), IRQT_RISING); // 16550A
}

// Ethernet smsc91c111 ----------------------------------------------------
static struct resource smc91xA_resources[] = {
	[0] = {
		.start	= PXA_CS3_PHYS+0x00300,
		.end	= PXA_CS3_PHYS+0xfffff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_GPIO(19),
		.end	= IRQ_GPIO(19),
		.flags	= IORESOURCE_IRQ,
	},
};
static struct resource smc91xB_resources[] = {
	[0] = {
		.start	= PXA_CS3_PHYS+0x400300,
		.end	= PXA_CS3_PHYS+0x40ffff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_GPIO(20),
		.end	= IRQ_GPIO(20),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device smc91xA_device = {
	.name          = "smc91x",
	.id            = -1,
	.num_resources = ARRAY_SIZE(smc91xA_resources),
	.resource      = smc91xA_resources,
};
static struct platform_device smc91xB_device = {
	.name          = "smc91xB",
	.id            = -1,
	.num_resources = ARRAY_SIZE(smc91xB_resources),
	.resource      = smc91xB_resources,
};	


// UART 16c550  ----------------------------------------------
#include <asm/serial.h>
#include <linux/serial_8250.h>

#define	UART_SETUP( _addr, _irq, _regshift )       \
	{                                              \
		.membase	= 0,                           \
		.mapbase	= (_addr),                     \
		.irq		= (_irq),                      \
		.flags		= UPF_SKIP_TEST | UPF_IOREMAP, \
		.iotype		= UPIO_MEM,                    \
		.regshift	= (_regshift),                 \
		.uartclk	= 3686400,                     \
	}

static struct plat_serial8250_port serial_platform_data[] = {

	UART_SETUP( PXA_CS2_PHYS + 0x000, IRQ_GPIO(23), 1 ),
	UART_SETUP( PXA_CS2_PHYS + 0x100, IRQ_GPIO(23), 1 ),
	UART_SETUP( PXA_CS2_PHYS + 0x200, IRQ_GPIO(23), 1 ),
	UART_SETUP( PXA_CS2_PHYS + 0x300, IRQ_GPIO(23), 1 ),
	UART_SETUP( PXA_CS2_PHYS + 0x400, IRQ_GPIO(24), 1 ),
	UART_SETUP( PXA_CS2_PHYS + 0x500, IRQ_GPIO(24), 1 ),
	UART_SETUP( PXA_CS2_PHYS + 0x600, IRQ_GPIO(24), 1 ),
	UART_SETUP( PXA_CS2_PHYS + 0x700, IRQ_GPIO(24), 1 ),
	
	{(struct plat_serial8250_port *)NULL}
};


static struct platform_device serial_device = {
	.name   = "serial8250",
	.id	    = 0,
	.dev    = { 
		.platform_data = serial_platform_data,
    },
};


static struct platform_device *devices[] __initdata = {
	&smc91xA_device,
	&smc91xB_device,
	&serial_device
};

static void __init falinux_pxa255_init(void)
{
	platform_add_devices(devices, ARRAY_SIZE(devices));
	
}

static struct map_desc falinux_pxa255_io_desc[] __initdata = {
  	{	/* nCS0 Boot Flash  -- slow RD/WR */
		.virtual	= 0xf0000000,
		.pfn		= __phys_to_pfn(PXA_CS0_PHYS+0x000000),
		.length		= 0x00400000,
		.type		= MT_DEVICE
	}
};

static void __init falinux_pxa255_map_io(void)
{
	pxa_map_io();
	iotable_init(falinux_pxa255_io_desc, ARRAY_SIZE(falinux_pxa255_io_desc));

	// FF-UART
	pxa_gpio_mode(GPIO34_FFRXD_MD);
	pxa_gpio_mode(GPIO39_FFTXD_MD);
	
	// ST-UART
	pxa_gpio_mode(GPIO46_STRXD_MD);
	pxa_gpio_mode(GPIO47_STTXD_MD);
}

MACHINE_START(FALINUX_PXA255, "FALinux ESP-CX Development Platform")
	/* Maintainer: FALINUX Software Inc. */
	.phys_io       = 0x40000000,
	.boot_params   = 0xa0000100,	/* EZBOOT boot parameter setting */	
	.io_pg_offst   = (io_p2v(0x40000000) >> 18) & 0xfffc,
	.map_io        = falinux_pxa255_map_io,
	.init_irq      = falinux_pxa255_init_irq,
	.timer		   = &pxa_timer,
	.init_machine  = falinux_pxa255_init,
MACHINE_END
