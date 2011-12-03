/*
 *  linux/arch/arm/mach-pxa/ez_pxa255.c
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


static void __init ez_pxa255_init_irq(void)
{
	pxa_init_irq();
	set_irq_type(IRQ_GPIO(53), IRQT_FALLING); // ax88796b
}

// Ethernet AX88796B ----------------------------------------------------
static struct resource ax88796b_resources[] = {
	[0] = {
		.start	= PXA_CS2_PHYS+0x00000,
		.end	= PXA_CS2_PHYS+0xfffff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_GPIO(53),
		.end	= IRQ_GPIO(53),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device ax88796b_device = {
    .name           = "ax88796b",
    .id             = -1,
    .num_resources  = ARRAY_SIZE(ax88796b_resources),
    .resource       = ax88796b_resources,
};

// Frame buffer ----------------------------------------------------
static struct pxafb_mode_info ezlcd_640_480_mode = {
   .pixclock       = 25000,
   .xres           = 640,
   .yres           = 480,
   .bpp            = 16,
   .hsync_len          = 8,
   .left_margin        = 0x59,
   .right_margin       = 0x19,
   .vsync_len          = 8,
   .upper_margin       = 0x12,
   .lower_margin       = 0x00,
   .sync           = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
   .cmap_greyscale	   = 0,
};

static struct pxafb_mode_info ezlcd_480_272_mode = {
   .pixclock       = 81000,
   .xres           = 480,
   .yres           = 272,
   .bpp            = 16,
   .hsync_len          = 1,
   .left_margin        = 89,
   .right_margin       = 25,
   .vsync_len          = 1,
   .upper_margin       = 12,
   .lower_margin       = 4,
   .sync           = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
   .cmap_greyscale	   = 0,
};

static struct pxafb_mode_info ezlcd_800_600_mode = {
   .pixclock       = 25000,
   .xres           = 800,
   .yres           = 600,
   .bpp            = 16,
   .hsync_len          = 8,
   .left_margin        = 108,
   .right_margin       = 108,
   .vsync_len          = 8,
   .upper_margin       = 14,
   .lower_margin       = 14,
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
#else if CONFIG_FB_EZ_PXA_800X600
	ez_lcd_to_use->modes = &ezlcd_800_600_mode;
#endif

	set_pxa_fb_info(&falinux_ezlcd);
   
   return 1;
}

static void pxafb_set_gpio(void)
{
	pxa_gpio_mode(GPIO58_LDD_0_MD);
	pxa_gpio_mode(GPIO59_LDD_1_MD);
	pxa_gpio_mode(GPIO60_LDD_2_MD);
	pxa_gpio_mode(GPIO61_LDD_3_MD);
	pxa_gpio_mode(GPIO62_LDD_4_MD);
	pxa_gpio_mode(GPIO63_LDD_5_MD);
	pxa_gpio_mode(GPIO64_LDD_6_MD);
	pxa_gpio_mode(GPIO65_LDD_7_MD);
	pxa_gpio_mode(GPIO66_LDD_8_MD);
	pxa_gpio_mode(GPIO67_LDD_9_MD);
	pxa_gpio_mode(GPIO68_LDD_10_MD);
	pxa_gpio_mode(GPIO69_LDD_11_MD);
	pxa_gpio_mode(GPIO70_LDD_12_MD);
	pxa_gpio_mode(GPIO71_LDD_13_MD);
	pxa_gpio_mode(GPIO72_LDD_14_MD);
	pxa_gpio_mode(GPIO73_LDD_15_MD);
	
}
//__setup("lcd=", ezpxa255_set_lcd);

// Platform µî·Ï ----------------------------------------------------
static struct platform_device *devices[] __initdata = {
	&ax88796b_device
//	&sl811_device,
//	&serial_device
};

static void __init ez_pxa255_init(void)
{
	platform_add_devices(devices, ARRAY_SIZE(devices));
	
	
}

static struct map_desc ez_pxa255_io_desc[] __initdata = {
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

static void __init ez_pxa255_map_io(void)
{
	pxa_map_io();
	iotable_init(ez_pxa255_io_desc, ARRAY_SIZE(ez_pxa255_io_desc));

	// FF-UART
	pxa_gpio_mode(GPIO34_FFRXD_MD);
	pxa_gpio_mode(GPIO39_FFTXD_MD);
	
	// ST-UART
	pxa_gpio_mode(GPIO46_STRXD_MD);
	pxa_gpio_mode(GPIO47_STTXD_MD);

	// BT-UART
	pxa_gpio_mode(GPIO42_BTRXD_MD);
	pxa_gpio_mode(GPIO43_BTTXD_MD);
	
	// PXA FB
	pxafb_set_gpio();
//	pxa_gpio_mode(52|GPIO_OUT);
//	pxa_gpio_set_value(52,1);
//	pxa_gpio_mode(16|GPIO_OUT);
//	pxa_gpio_set_value(16,1);

//	ezlcd_set_lcd();
	
	set_pxa_fb_info(&falinux_ezlcd);
}

MACHINE_START(FALINUX_PXA255, "FALinux EZ-PXA255 Development Platform")
	/* Maintainer: FALINUX Software Inc. */
	.phys_io      = 0x40000000,
	.boot_params  = 0xa0000100,	/* EZBOOT boot parameter setting */	
	.io_pg_offst  = (io_p2v(0x40000000) >> 18) & 0xfffc,
	.map_io       = ez_pxa255_map_io,
	.init_irq     = ez_pxa255_init_irq,
	.timer        = &pxa_timer,
	.init_machine = ez_pxa255_init,
MACHINE_END
