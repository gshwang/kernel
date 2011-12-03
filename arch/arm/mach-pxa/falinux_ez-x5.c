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


static void __init ez_x5_init_irq(void)
{
	pxa_init_irq();
	set_irq_type(IRQ_GPIO(21), IRQT_RISING); // CS8900A
	set_irq_type(IRQ_GPIO(12), IRQT_RISING); // SL811
}


// SL811 USB Host ----------------------------------------------------
static struct resource sl811_resources[] = {
	[0] = {
		.start	= PXA_CS2_PHYS+0x00,
		.end	= PXA_CS2_PHYS+0x00,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= PXA_CS2_PHYS+0x04,
		.end	= PXA_CS2_PHYS+0x04,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= IRQ_GPIO(12),
		.end	= IRQ_GPIO(12),
		.flags	= IORESOURCE_IRQ,
	}
};

static struct platform_device sl811_device = {
	.name		= "sl811-hcd",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(sl811_resources),
	.resource	= sl811_resources,
};

// FB   --------------------------------------------------------------
static struct pxafb_mode_info ezlcd_640_480_mode = {
   .pixclock       = 25000,
   .xres           = 640,
   .yres           = 480,
   .bpp            = 16,
   .hsync_len      = 8,
   .left_margin    = 0x59,
   .right_margin   = 0x19,
   .vsync_len      = 8,
   .upper_margin   = 0x12,
   .lower_margin   = 0x00,
   .sync           = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
   .cmap_greyscale	   = 0,
};

static struct pxafb_mode_info ezlcd_480_272_mode = {
   .pixclock       = 81000,
   .xres           = 480,
   .yres           = 272,
   .bpp            = 16,
   .hsync_len      = 1,
   .left_margin    = 89,
   .right_margin   = 25,
   .vsync_len      = 1,
   .upper_margin   = 12,
   .lower_margin   = 4,
   .sync           = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
   .cmap_greyscale	   = 0,
};



static struct pxafb_mach_info falinux_ezlcd = {
   	.modes          = 0,
   	.num_modes      = 1,
	.lccr0			= LCCR0_Act | LCCR0_Sngl | LCCR0_Color,
	.lccr3			= LCCR3_PCP,
};


static struct pxafb_mach_info *ez_lcd_to_use;

static int __init ezlcd_set_lcd(void) {
	ez_lcd_to_use 		= &falinux_ezlcd;	
// Select LCD Type	
#ifdef	 CONFIG_FB_EZ_PXA_640X480
	ez_lcd_to_use->modes = &ezlcd_640_480_mode;
#else if CONFIG_FB_EZ_PXA_480X272
	ez_lcd_to_use->modes = &ezlcd_480_272_mode;
#endif
   
   return 1;
}

static struct platform_device *devices[] __initdata = {
	&sl811_device,
//	&serial_device
};

static void __init ez_x5_init(void)
{
	ezlcd_set_lcd();							// FB init
	
	platform_add_devices(devices, ARRAY_SIZE(devices));
	
}

static struct map_desc ez_x5_io_desc[] __initdata = {
  	{	/* nCS0 Boot Flash  -- slow RD/WR */
		.virtual	= 0xf0000000,
		.pfn		= __phys_to_pfn(PXA_CS0_PHYS+0x000000),
		.length		= 0x00400000,
		.type		= MT_DEVICE
	},
  	{	/* nCS0 CS8900   	-- slow RD/WR */
		.virtual	= 0xf1000000,
		.pfn		= __phys_to_pfn(PXA_CS0_PHYS+0x400000),
		.length		= 0x00100000,
		.type		= MT_DEVICE
	},
  	{	/* nCS1 CS8900   	-- slow RD/WR */
		.virtual	= 0xf1100000,
		.pfn		= __phys_to_pfn(PXA_CS0_PHYS+0x800000),
		.length		= 0x00100000,
		.type		= MT_DEVICE
	},
  	{	/* nCS1 REV         -- fast RD/WR  CPLD.REV2 */
		.virtual	=  0xf1500000,
		.pfn		= __phys_to_pfn(PXA_CS1_PHYS+0x800000),
		.length		= 0x00100000,
		.type		= MT_DEVICE
	},
  	{	/* nCS1 REV         -- fast RD/WR  CPLD.REV2 */
		.virtual	=  0xf1600000,
		.pfn		= __phys_to_pfn(PXA_CS1_PHYS+0xC00000),
		.length		= 0x00100000,
		.type		= MT_DEVICE
	}
};

static void __init ez_x5_map_io(void)
{
	pxa_map_io();
	iotable_init(ez_x5_io_desc, ARRAY_SIZE(ez_x5_io_desc));

	// FF-UART
	pxa_gpio_mode(GPIO34_FFRXD_MD);
	pxa_gpio_mode(GPIO39_FFTXD_MD);
	
	// ST-UART
	pxa_gpio_mode(GPIO46_STRXD_MD);
	pxa_gpio_mode(GPIO47_STTXD_MD);

	// BT-UART
	pxa_gpio_mode(GPIO42_BTRXD_MD);
	pxa_gpio_mode(GPIO43_BTTXD_MD);
	
	// FB
	set_pxa_fb_info(&falinux_ezlcd);
}

MACHINE_START(FALINUX_PXA255, "FALinux EZ-X5 Development Platform")
	/* Maintainer: FALINUX Software Inc. */
	.phys_io      = 0x40000000,
	.boot_params  = 0xa0000100,	/* EZBOOT boot parameter setting */	
	.io_pg_offst  = (io_p2v(0x40000000) >> 18) & 0xfffc,
	.map_io       = ez_x5_map_io,
	.init_irq     = ez_x5_init_irq,
	.timer        = &pxa_timer,
	.init_machine = ez_x5_init,
MACHINE_END
