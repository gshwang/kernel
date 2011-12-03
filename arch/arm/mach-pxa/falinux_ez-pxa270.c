/*
 *  linux/arch/arm/mach-pxa/falinux_ez_pxa270.c
 *
 *  Support for the FALinux EZ-PXA270 Development Platform.
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
#include <asm/arch/ez_pxa270.h>
#include <asm/arch/audio.h>
#include <asm/arch/pxafb.h>
#include <asm/arch/mmc.h>
#include <asm/arch/ohci.h>

#include "generic.h"
#include <asm/delay.h>

static void __init ez_pxa270_init_irq(void)
{
	pxa_init_irq();
	set_irq_type(IRQ_GPIO(21), IRQT_RISING); // CS8900A
//	set_irq_type(IRQ_GPIO(12), IRQT_RISING); // SL811

	set_irq_type(IRQ_GPIO(23), IRQT_RISING); // 16550A	attatch 74hc32
	set_irq_type(IRQ_GPIO(24), IRQT_RISING); // 16550A
	set_irq_type(IRQ_GPIO(25), IRQT_RISING); // 16550A
	set_irq_type(IRQ_GPIO(26), IRQT_RISING); // 16550A
	set_irq_type(IRQ_GPIO(114), IRQT_FALLING); // ax88796B
}
// Ethernet AX88796B ----------------------------------------------------
//#ifdef CONFIG_AX88796B
static struct resource ax88796b_resources[] = {
	[0] = {
		.start	= PXA_CS2_PHYS+0x00000,
		.end	= PXA_CS2_PHYS+0xfffff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_GPIO(114),
		.end	= IRQ_GPIO(114),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device ax88796b_device = {
    .name           = "ax88796b",
    .id             = -1,
    .num_resources  = ARRAY_SIZE(ax88796b_resources),
    .resource       = ax88796b_resources,
};
//#endif
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
	.name		= "smc91xA",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(smc91xA_resources),
	.resource	= smc91xA_resources,
};
static struct platform_device smc91xB_device = {
	.name		= "smc91xB",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(smc91xB_resources),
	.resource	= smc91xB_resources,
};	


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

// AC97 ARM ------------------------------------------------------
static struct platform_device ac97_device = {
	.name	= "pxa2xx-ac97",
	.id		= -1,
};

// wm9712 touch --------------------------------------------------------
static struct resource wm9712ts_resources[] = {
[0] = {
	.name   = "wm9712ts-int",
	.start  = IRQ_GPIO(84),
	.end    = IRQ_GPIO(84),
	.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device wm9712ts_device = {
	.name		   = "wm9712ts",
	.id            = -1,
	.num_resources = ARRAY_SIZE(wm9712ts_resources),
	.resource      = wm9712ts_resources,
};




// SD / MMC ------------------------------------------------------
#define GPIO_IRQ_DETECT	IRQ_GPIO(10)
#define GPIO10_MMC_CD				10
#define GPIO1_MMC_WP				1
#define GPIO10_MMC_CD_MD 			GPIO10_MMC_CD|GPIO_ALT_FN_1_IN			
#define GPIO1_MMC_WP_MD				GPIO1_MMC_WP|GPIO_ALT_FN_1_IN



static int ezpxa270_mci_init(struct device *dev, irq_handler_t ezpxa270_detect_int, void *data)
{
	int err;

	// set GPIO
	pxa_gpio_mode(GPIO32_MMCCLK_MD);
	pxa_gpio_mode(GPIO112_MMCCMD_MD);
	pxa_gpio_mode(GPIO92_MMCDAT0_MD);
	pxa_gpio_mode(GPIO109_MMCDAT1_MD);
	pxa_gpio_mode(GPIO110_MMCDAT2_MD);
	pxa_gpio_mode(GPIO111_MMCDAT3_MD);

	
	pxa_gpio_mode(GPIO1_MMC_WP_MD);
	pxa_gpio_mode(GPIO10_MMC_CD_MD);
	set_irq_type(GPIO_IRQ_DETECT, IRQT_FALLING); // MMC CD

	err = request_irq(GPIO_IRQ_DETECT , ezpxa270_detect_int, IRQF_DISABLED,
			     "MMC card detect", data);
	if (err) {
		printk(KERN_ERR "ezpxa270_mci_init: MMC/SD: can't request MMC card detect IRQ\n");
		return -1;
	}

	return 0;
}

static int ezpxa270_mci_get_ro( struct device *dev )
{
	int gpio;

	pxa_gpio_mode(GPIO1_MMC_WP_MD);
	gpio = pxa_gpio_get_value( GPIO1_MMC_WP );

	// Write Protect
	return gpio;
}

static void ezpxa270_mci_setpower(struct device *dev, unsigned int vdd)
{
	struct pxamci_platform_data* p_d = dev->platform_data;
// not used!!	
}

static void ezpxa270_mci_exit(struct device *dev, void *data)
{
	free_irq(GPIO_IRQ_DETECT, data);
}


static struct pxamci_platform_data ezpxa270_mci_platform = {
	.ocr_mask	= MMC_VDD_32_33|MMC_VDD_33_34,
	.init 		= ezpxa270_mci_init,
	.get_ro		= ezpxa270_mci_get_ro,
//	.setpower 	= ezpxa270_mci_setpower,
	.exit		= ezpxa270_mci_exit,
};

// USB HOST ------------------------------------------------------
static int ohci_init(struct device *dev)
{
	/* setup Port1 GPIO pin. */
	pxa_gpio_mode( 88 | GPIO_ALT_FN_1_IN);	/* USBHPWR1 */
	//pxa_gpio_mode( 89 | GPIO_ALT_FN_2_OUT);	/* USBHPEN1 */

	/* Set the Power Control Polarity Low and Power Sense
	   Polarity Low to active low. */
	UHCHR = (UHCHR | UHCHR_PCPL | UHCHR_PSPL) &
		~(UHCHR_SSEP1 | UHCHR_SSEP2 | UHCHR_SSEP3 | UHCHR_SSE);

	return 0;
}

static struct pxaohci_platform_data ohci_device = {
	//.port_mode	= PMM_PERPORT_MODE,
	.port_mode	= PMM_NPS_MODE,
	.init		= ohci_init,
};


// pxa reset  --------------------------------------------------------
static void falinux_reset(char mode)
{
	RCSR = RCSR_HWR | RCSR_WDR | RCSR_SMR | RCSR_GPR;
	arm_machine_restart('H');
}



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

// Serial 16552 ------------------------------------------------------
//#include "esp-serial.h"
//
//static struct platform_device serial_device = {
//	.name		= "serial8250",
//	.id		= 0,
//	.dev		= { 
//		.platform_data  = serial_platform_data,
//    	},
//};


static struct platform_device *devices[] __initdata = {
	&wm9712ts_device,
	&ac97_device,
	&ax88796b_device
};

static void __init ez_pxa270_init(void)
{

	// 
	arm_pm_restart	= falinux_reset;			// Reset init
	ezlcd_set_lcd();							// FB init

	// register platform bus	
	pxa_set_ohci_info(&ohci_device);			// USB Host ( OHCI )
	pxa_set_mci_info(&ezpxa270_mci_platform);	// SD /MMC

	platform_add_devices(devices, ARRAY_SIZE(devices));

}

static struct map_desc ez_pxa270_io_desc[] __initdata = {
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
	,
  	{	/* nCS5 REV			-- fast */
		.virtual	=  0x14000000,
		.pfn		= __phys_to_pfn(PXA_CS5_PHYS),
		.length		= 0x00100000,
		.type		= MT_DEVICE
	}
};

static void __init ez_pxa270_map_io(void)
{
	pxa_map_io();
	iotable_init(ez_pxa270_io_desc, ARRAY_SIZE(ez_pxa270_io_desc));

	// FF-UART
	pxa_gpio_mode(GPIO34_FFRXD_MD);
	pxa_gpio_mode(GPIO39_FFTXD_MD);
	
	// ST-UART
	pxa_gpio_mode(GPIO46_STRXD_MD);
	pxa_gpio_mode(GPIO47_STTXD_MD);

	// BT-UART
	pxa_gpio_mode(GPIO42_BTRXD_MD);
	pxa_gpio_mode(GPIO43_BTTXD_MD);

	// AC97
	pxa_gpio_mode(GPIO89_SYSCLK_AC97_MD);
	
	// FB
	pxafb_set_gpio();
//	pxa_gpio_mode(19|GPIO_OUT);
//	pxa_gpio_set_value(19, 1);

	set_pxa_fb_info(&falinux_ezlcd);

}

MACHINE_START(FALINUX_PXA270, "FALinux EZ-PXA270 Development Platform")
	/* Maintainer: FALINUX Software Inc. */
	.phys_io	= 0x40000000,
	.boot_params	= 0xa0000100,	/* EZBOOT boot parameter setting */	
	.io_pg_offst	= (io_p2v(0x40000000) >> 18) & 0xfffc,
	.map_io		= ez_pxa270_map_io,
	.init_irq	= ez_pxa270_init_irq,
	.timer		= &pxa_timer,
	.init_machine	= ez_pxa270_init,
MACHINE_END
