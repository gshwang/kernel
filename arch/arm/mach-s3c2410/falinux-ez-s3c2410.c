/* linux/arch/arm/mach-s3c2440/falinux-ez-s3c2440.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 
 *  CONFIG_MACH_EZ_S3C2440
 *
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>


#include <linux/mmc/protocol.h>
#include <linux/mmc/host.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include <asm/arch/regs-serial.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-lcd.h>

#include <asm/arch/idle.h>
#include <asm/arch/fb.h>
#include <asm/arch/mci.h>
#include <asm/arch/ts.h>
#include <asm/arch/udc.h>

#ifdef CONFIG_SERIAL_8250
#include <asm/serial.h>
#include <linux/serial_8250.h>
#endif

#include <asm/plat-s3c24xx/s3c2410.h>
#include <asm/plat-s3c24xx/clock.h>
#include <asm/plat-s3c24xx/devs.h>
#include <asm/plat-s3c24xx/cpu.h>

#include <asm/plat-s3c24xx/common-falinux.h>

static struct map_desc falinuxs3c2410_iodesc[] __initdata = {
	[0] = {		// nCS1 CS8900 -- slow RD/WR
		.virtual    = 0xf4000000,
		.pfn        = __phys_to_pfn(S3C2410_CS1+0x000000),
		.length     = 0x00400000,
		.type       = MT_DEVICE
	},
};

#define UCON S3C2410_UCON_DEFAULT | S3C2410_UCON_UCLK
#define ULCON S3C2410_LCON_CS8 | S3C2410_LCON_PNONE | S3C2410_LCON_STOPB
#define UFCON S3C2410_UFCON_RXTRIG8 | S3C2410_UFCON_FIFOMODE

static struct s3c2410_uartcfg falinuxs3c2410_uartcfgs[] __initdata = {
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x03,
		.ufcon	     = 0x51,
	},
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x03,
		.ufcon	     = 0x51,
	},
	/* IR port */
	[2] = {
		.hwport	     = 2,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x03,	// 0x43
		.ufcon	     = 0x51,
	}
};

// UDC PullUp
static void pullup(unsigned char cmd)
{
	printk(KERN_DEBUG "udc: pullup(%d)\n",cmd);
	switch (cmd)
	{
		case S3C2410_UDC_P_ENABLE :
			break;
		case S3C2410_UDC_P_DISABLE :
			break;
		case S3C2410_UDC_P_RESET :
			break;
		default: break;
	}
}

static struct s3c2410_udc_mach_info falinuxs3c2410_udc_cfg __initdata = {
	.udc_command = pullup,
};

// LCD driver info 
#include "falinux-lcd.h"

// TouchScreen Setup
static struct s3c2410_ts_mach_info falinux_s24xx_ts_cfg = {
	.delay = 10000,
	.presc = 49, 
	.oversampling_shift = 2,
};


static struct platform_device *falinuxs3c2410_devices[] __initdata = {
	&s3c_device_usb,
	&s3c_device_lcd,
	&s3c_device_wdt,
	&s3c_device_i2c,
	&s3c_device_rtc,
	&s3c_device_iis,
	&s3c_device_sdi,
	&s3c_device_ts,
};

static struct s3c24xx_board falinuxs3c2410_board __initdata = {
	.devices       = falinuxs3c2410_devices,
	.devices_count = ARRAY_SIZE(falinuxs3c2410_devices)
};


static void __init falinuxs3c2410_map_io(void)
{
	s3c24xx_init_io(falinuxs3c2410_iodesc, ARRAY_SIZE(falinuxs3c2410_iodesc));
	s3c24xx_init_clocks(0);
	s3c24xx_init_uarts(falinuxs3c2410_uartcfgs, ARRAY_SIZE(falinuxs3c2410_uartcfgs));
	s3c24xx_set_board(&falinuxs3c2410_board);

	set_s3c2410ts_info(&falinux_s24xx_ts_cfg);
}


// 2��° usb ��Ʈ�� ȣ��Ʈ�� �����Ѵ�.
static void __init set_usb_2ndport_host(void)
{
	unsigned long tmp;

	tmp  = readl(S3C2410_MISCCR);
	tmp |= S3C2410_MISCCR_USBHOST;
	__raw_writel(tmp, S3C2410_MISCCR);
}
#if 0
// 2��° usb ��Ʈ�� ����̽��� �����Ѵ�.
static void __init set_usb_2ndport_device(void)
{
	unsigned long tmp;

	tmp  = readl(S3C2410_MISCCR);
	tmp &= ~( S3C2410_MISCCR_USBSUSPND1 | S3C2410_MISCCR_USBHOST);
	__raw_writel(tmp, S3C2410_MISCCR);
}
#endif

static void __init falinuxs3c2410_machine_init(void)
{
	s3c24xx_fb_set_platdata(&falinuxs3c2410_lcd_cfg);
	s3c24xx_udc_set_platdata(&falinuxs3c2410_udc_cfg);

	// MISCELLANEOUS CONTROL REGISTER (MISCCR) USB Host, USB Device
	// USB Host/Device  겸용 사용 Port 설정
	
	falinuxs24xx_machine_init();	// arch/arm/plat-s3c24xx/common-falinux.c
	
	set_usb_2ndport_host();

	// setup irq  ------------------------------
	// IRQT_RISING, IRQT_FALLING, IRQT_HIGH, IRQT_LOW
	set_irq_type( IRQ_EINT9, IRQT_RISING);		// CS8900A - EtherNet Interrupt Init
	
}

MACHINE_START(FALINUX_S3C2410, "www.falinux.com EZ-S3C2410 for S3C2410 Board")
	.phys_io		= S3C2410_PA_UART,
	.io_pg_offst	= (((u32)S3C24XX_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S3C2410_SDRAM_PA + 0x100,

	.init_irq		= s3c24xx_init_irq,
	.map_io			= falinuxs3c2410_map_io,
	.init_machine	= falinuxs3c2410_machine_init,
	.timer			= &s3c24xx_timer,
MACHINE_END
