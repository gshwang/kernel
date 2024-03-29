/*
 * arch/arm/mach-ixp4xx/ez_ixp42x-setup.c
 *
 * EZ_IXP42X board-setup 
 *
 * Copyright (C) 2003-2005 MontaVista Software, Inc.
 *
 * Author: Deepak Saxena <dsaxena@plexity.net>
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/serial_8250.h>
#include <linux/slab.h>

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach/flash.h>

static struct flash_platform_data ez_ixp42x_flash_data = {
	.map_name	= "cfi_probe",
	.width		= 2,
};

static struct resource ez_ixp42x_flash_resource = {
	.flags		= IORESOURCE_MEM,
};

static struct platform_device ez_ixp42x_flash = {
	.name		= "IXP4XX-Flash",
	.id		= 0,
	.dev		= {
		.platform_data = &ez_ixp42x_flash_data,
	},
	.num_resources	= 1,
	.resource	= &ez_ixp42x_flash_resource,
};

static struct ixp4xx_i2c_pins ez_ixp42x_i2c_gpio_pins = {
	.sda_pin	= EZ_IXP42X_SDA_PIN,
	.scl_pin	= EZ_IXP42X_SCL_PIN,
};

static struct platform_device ez_ixp42x_i2c_controller = {
	.name		= "IXP4XX-I2C",
	.id		= 0,
	.dev		= {
		.platform_data = &ez_ixp42x_i2c_gpio_pins,
	},
	.num_resources	= 0
};

static struct resource ez_ixp42x_uart_resources[] = {
	{
		.start		= IXP4XX_UART1_BASE_PHYS,
		.end		= IXP4XX_UART1_BASE_PHYS + 0x0fff,
		.flags		= IORESOURCE_MEM
	},
	{
		.start		= IXP4XX_UART2_BASE_PHYS,
		.end		= IXP4XX_UART2_BASE_PHYS + 0x0fff,
		.flags		= IORESOURCE_MEM
	}
};

static struct plat_serial8250_port ez_ixp42x_uart_data[] = {
	{
		.mapbase	= IXP4XX_UART1_BASE_PHYS,
		.membase	= (char *)IXP4XX_UART1_BASE_VIRT + REG_OFFSET,
		.irq		= IRQ_IXP4XX_UART1,
		.flags		= UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
		.uartclk	= IXP4XX_UART_XTAL,
	},
	{
		.mapbase	= IXP4XX_UART2_BASE_PHYS,
		.membase	= (char *)IXP4XX_UART2_BASE_VIRT + REG_OFFSET,
		.irq		= IRQ_IXP4XX_UART2,
		.flags		= UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
		.uartclk	= IXP4XX_UART_XTAL,
	},
	{ },
};

static struct platform_device ez_ixp42x_uart = {
	.name			= "serial8250",
	.id			= PLAT8250_DEV_PLATFORM,
	.dev.platform_data	= ez_ixp42x_uart_data,
	.num_resources		= 2,
	.resource		= ez_ixp42x_uart_resources
};

static struct platform_device *ez_ixp42x_devices[] __initdata = {
	&ez_ixp42x_i2c_controller,
	&ez_ixp42x_flash,
	&ez_ixp42x_uart
};

static void __init ez_ixp42x_init(void)
{
	ixp4xx_sys_init();

	//ez_ixp42x_flash_resource.start = IXP4XX_EXP_BUS_BASE(0);
	//ez_ixp42x_flash_resource.end =
	//	IXP4XX_EXP_BUS_BASE(0) + ixp4xx_exp_bus_size - 1;

	platform_add_devices(ez_ixp42x_devices, ARRAY_SIZE(ez_ixp42x_devices));
}

#ifdef CONFIG_MACH_EZ_IXP42X
MACHINE_START(EZ_IXP42X, "Intel EZ_IXP42X Development Platform")
	/* Maintainer: MontaVista Software, Inc. */
	.phys_io	= IXP4XX_PERIPHERAL_BASE_PHYS,
	.io_pg_offst	= ((IXP4XX_PERIPHERAL_BASE_VIRT) >> 18) & 0xfffc,
	.map_io		= ixp4xx_map_io,
	.init_irq	= ixp4xx_init_irq,
	.timer		= &ixp4xx_timer,
	.boot_params	= 0x0100,
	.init_machine	= ez_ixp42x_init,
MACHINE_END
#endif
