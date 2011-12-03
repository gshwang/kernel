/*
 * arch/arm/mach-ep93xx/core.c
 * Core routines for Cirrus EP93xx chips.
 *
 * Copyright (C) 2006 Lennert Buytenhek <buytenh@wantstofly.org>
 *
 * Thanks go to Michael Burian and Ray Lehtiniemi for their key
 * role in the ep93xx linux community.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */
#include <linux/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/bitops.h>
#include <linux/serial.h>
#include <linux/serial_8250.h>
#include <linux/serial_core.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/delay.h>
#include <linux/termios.h>
#include <linux/amba/bus.h>
#include <linux/amba/serial.h>
#include <linux/utsname.h>

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/memory.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/tlbflush.h>
#include <asm/pgtable.h>
#include <asm/io.h>

#include <asm/mach/flash.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <asm/mach/irq.h>
#include <asm/arch/gpio.h>
#include <asm/arch/ssp.h>
#include <asm/arch/crunch.h>

#include <asm/hardware/vic.h>

#ifdef CONFIG_EP93XX_FPU
extern void crunch_init(void);
int crunch_is_enabled=0;
#endif

/*************************************************************************
 * Static I/O mappings that are needed for all EP93xx platforms
 *************************************************************************/
static struct map_desc ep93xx_io_desc[] __initdata = {
	{
		.virtual	= EP93XX_AHB_VIRT_BASE,
		.pfn		= __phys_to_pfn(EP93XX_AHB_PHYS_BASE),
		.length		= EP93XX_AHB_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= EP93XX_APB_VIRT_BASE,
		.pfn		= __phys_to_pfn(EP93XX_APB_PHYS_BASE),
		.length		= EP93XX_APB_SIZE,
		.type		= MT_DEVICE,
	},
};

void __init ep93xx_map_io(void)
{
	iotable_init(ep93xx_io_desc, ARRAY_SIZE(ep93xx_io_desc));
}

/*************************************************************************
 * Timer handling for EP93xx
 *************************************************************************
 * The ep93xx has four internal timers.  Timers 1, 2 (both 16 bit) and
 * 3 (32 bit) count down at 508 kHz, are self-reloading, and can generate
 * an interrupt on underflow.  Timer 4 (40 bit) counts down at 983.04 kHz,
 * is free-running, and can't generate interrupts.
 *
 * The 508 kHz timers are ideal for use for the timer interrupt, as the
 * most common values of HZ divide 508 kHz nicely.  We pick one of the 16
 * bit timers (timer 1) since we don't need more than 16 bits of reload
 * value as long as HZ >= 8.
 *
 * The higher clock rate of timer 4 makes it a better choice than the
 * other timers for use in gettimeoffset(), while the fact that it can't
 * generate interrupts means we don't have to worry about not being able
 * to use this timer for something else.  We also use timer 4 for keeping
 * track of lost jiffies.
 */
/*
static unsigned int last_jiffy_time;
static unsigned int next_jiffy_time;
static unsigned int accumulator;

static int after_eq(unsigned long a, unsigned long b)
{
	return ((signed long)(a - b)) >= 0;
}
*/
#define TIMER4_TICKS_PER_JIFFY          (CLOCK_TICK_RATE / HZ)
#define TIMER4_TICKS_MOD_JIFFY          (CLOCK_TICK_RATE % HZ)


static int ep93xx_timer_interrupt(int irq, void *dev_id)
{
	write_seqlock(&xtime_lock);

	__raw_writel(1, EP93XX_TIMER1_CLEAR);
	timer_tick();

	write_sequnlock(&xtime_lock);

	return IRQ_HANDLED;
}

static struct irqaction ep93xx_timer_irq = {
	.name		= "ep93xx timer",
	.flags		= IRQF_DISABLED | IRQF_TIMER,
	.handler	= ep93xx_timer_interrupt,
};

static void __init ep93xx_timer_init(void)
{
	__raw_writel(0, TIMER1CONTROL);
	__raw_writel(LATCH - 1, TIMER1LOAD);
	__raw_writel(0xc8, TIMER1CONTROL);

	setup_irq(IRQ_EP93XX_TIMER1, &ep93xx_timer_irq);
}

static unsigned long ep93xx_gettimeoffset(void)
{
	unsigned long hwticks;
	hwticks = LATCH - (inl(TIMER1VALUE) & 0xffff);
	return (hwticks * (tick_nsec / 1000)) / LATCH;
}

struct sys_timer ep93xx_timer = {
	.init		= ep93xx_timer_init,
	.offset		= ep93xx_gettimeoffset,
};

/*************************************************************************
 * GPIO handling for EP93xx
 *************************************************************************/
static unsigned char gpio_int_unmasked[3];
static unsigned char gpio_int_enabled[3];
static unsigned char gpio_int_debounce[2];
static unsigned char gpio_int_type1[3];
static unsigned char gpio_int_type2[3];

static void update_gpio_int_params(int abf)
{
	if (abf == 0) {
		__raw_writeb(0, EP93XX_GPIO_A_INT_ENABLE);
		__raw_writeb(gpio_int_debounce[0], EP93XX_GPIO_A_INT_DEBOUNCE);
		__raw_writeb(gpio_int_type2[0], EP93XX_GPIO_A_INT_TYPE2);
		__raw_writeb(gpio_int_type1[0], EP93XX_GPIO_A_INT_TYPE1);
		__raw_writeb(gpio_int_unmasked[0] & gpio_int_enabled[0], EP93XX_GPIO_A_INT_ENABLE);
	} else if (abf == 1) {
		__raw_writeb(0, EP93XX_GPIO_B_INT_ENABLE);
		__raw_writeb(gpio_int_debounce[1], EP93XX_GPIO_B_INT_DEBOUNCE);
		__raw_writeb(gpio_int_type2[1], EP93XX_GPIO_B_INT_TYPE2);
		__raw_writeb(gpio_int_type1[1], EP93XX_GPIO_B_INT_TYPE1);
		__raw_writeb(gpio_int_unmasked[1] & gpio_int_enabled[1], EP93XX_GPIO_B_INT_ENABLE);
	} else if (abf == 2) {
		__raw_writeb(0, EP93XX_GPIO_F_INT_ENABLE);
		__raw_writeb(gpio_int_type2[2], EP93XX_GPIO_F_INT_TYPE2);
		__raw_writeb(gpio_int_type1[2], EP93XX_GPIO_F_INT_TYPE1);
		__raw_writeb(gpio_int_unmasked[2] & gpio_int_enabled[2], EP93XX_GPIO_F_INT_ENABLE);
	} else {
		BUG();
	}
}

static unsigned char data_register_offset[8] = {
	0x00, 0x04, 0x08, 0x0c, 0x20, 0x30, 0x38, 0x40,
};

static unsigned char data_direction_register_offset[8] = {
	0x10, 0x14, 0x18, 0x1c, 0x24, 0x34, 0x3c, 0x44,
};

void gpio_line_config(int line, int direction)
{
	unsigned int data_direction_register;
	unsigned long flags;
	unsigned char v;

	data_direction_register =
		EP93XX_GPIO_REG(data_direction_register_offset[line >> 3]);

	local_irq_save(flags);
	if (direction == GPIO_OUT) {
		if (line >= 0 && line < 16) {
			/* Port A/B.  */
			gpio_int_unmasked[line >> 3] &= ~(1 << (line & 7));
			update_gpio_int_params(line >> 3);
		} else if (line >= 40 && line < 48) {
			/* Port F.  */
			gpio_int_unmasked[2] &= ~(1 << (line & 7));
			update_gpio_int_params(2);
		}

		v = __raw_readb(data_direction_register);
		v |= 1 << (line & 7);
		__raw_writeb(v, data_direction_register);
	} else if (direction == GPIO_IN) {
		v = __raw_readb(data_direction_register);
		v &= ~(1 << (line & 7));
		__raw_writeb(v, data_direction_register);
	}
	local_irq_restore(flags);
}
EXPORT_SYMBOL(gpio_line_config);

int gpio_line_get(int line)
{
	unsigned int data_register;

	data_register = EP93XX_GPIO_REG(data_register_offset[line >> 3]);

	return !!(__raw_readb(data_register) & (1 << (line & 7)));
}
EXPORT_SYMBOL(gpio_line_get);

void gpio_line_set(int line, int value)
{
	unsigned int data_register;
	unsigned long flags;
	unsigned char v;

	data_register = EP93XX_GPIO_REG(data_register_offset[line >> 3]);

	local_irq_save(flags);
	if (value == EP93XX_GPIO_HIGH) {
		v = __raw_readb(data_register);
		v |= 1 << (line & 7);
		__raw_writeb(v, data_register);
	} else if (value == EP93XX_GPIO_LOW) {
		v = __raw_readb(data_register);
		v &= ~(1 << (line & 7));
		__raw_writeb(v, data_register);
	}
	local_irq_restore(flags);
}
EXPORT_SYMBOL(gpio_line_set);



/*************************************************************************
 * EP93xx IRQ handling
 *************************************************************************/
static void ep93xx_gpio_ab_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	unsigned char status;
	int i;

	status = __raw_readb(EP93XX_GPIO_A_INT_STATUS);
	for (i = 0; i < 8; i++) {
		if (status & (1 << i)) {
			desc = irq_desc + IRQ_EP93XX_GPIO(0) + i;
			desc_handle_irq(IRQ_EP93XX_GPIO(0) + i, desc);
		}
	}

	status = __raw_readb(EP93XX_GPIO_B_INT_STATUS);
	for (i = 0; i < 8; i++) {
		if (status & (1 << i)) {
			desc = irq_desc + IRQ_EP93XX_GPIO(8) + i;
			desc_handle_irq(IRQ_EP93XX_GPIO(8) + i, desc);
		}
	}
}

static void ep93xx_gpio_f_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	int gpio_irq = IRQ_EP93XX_GPIO(16) + (((irq + 1) & 7) ^ 4);

	desc_handle_irq(gpio_irq, irq_desc + gpio_irq);
}

static void ep93xx_gpio_irq_mask_ack(unsigned int irq)
{
	int line = irq - IRQ_EP93XX_GPIO(0);
	int port = line >> 3;

	gpio_int_unmasked[port] &= ~(1 << (line & 7));
	update_gpio_int_params(port);

	if (port == 0) {
		__raw_writel(1 << (line & 7), EP93XX_GPIO_A_INT_ACK);
	} else if (port == 1) {
		__raw_writel(1 << (line & 7), EP93XX_GPIO_B_INT_ACK);
	} else if (port == 2) {
		__raw_writel(1 << (line & 7), EP93XX_GPIO_F_INT_ACK);
	}
}

static void ep93xx_gpio_irq_mask(unsigned int irq)
{
	int line = irq - IRQ_EP93XX_GPIO(0);
	int port = line >> 3;

	gpio_int_unmasked[port] &= ~(1 << (line & 7));
	update_gpio_int_params(port);
}

static void ep93xx_gpio_irq_unmask(unsigned int irq)
{
	int line = irq - IRQ_EP93XX_GPIO(0);
	int port = line >> 3;

	gpio_int_unmasked[port] |= 1 << (line & 7);
	update_gpio_int_params(port);
}

/*
 * gpio_int_type1 controls whether the interrupt is level (0) or
 * edge (1) triggered, while gpio_int_type2 controls whether it
 * triggers on low/falling (0) or high/rising (1).
 */
static int ep93xx_gpio_irq_type(unsigned int irq, unsigned int type)
{
	int port;
	int line;

	line = irq - IRQ_EP93XX_GPIO(0);

	if (line >= 0 && line < 16) {
		gpio_line_config(line, GPIO_IN);
	} else {
		gpio_line_config(EP93XX_GPIO_LINE_F(line), GPIO_IN);
	}

	port = line >> 3;
	line &= 7;

	if (type & IRQT_RISING) {
		gpio_int_enabled[port] |=   1 << line;
		gpio_int_type1[port]   |=   1 << line;
		gpio_int_type2[port]   |=   1 << line;
	} else if (type & IRQT_FALLING) {
		gpio_int_enabled[port] |=   1 << line;
		gpio_int_type1[port]   |=   1 << line;
		gpio_int_type2[port]   &= ~(1 << line);
	} else if (type & IRQT_HIGH) {
		gpio_int_enabled[port] |=   1 << line;
		gpio_int_type1[port]   &= ~(1 << line);
		gpio_int_type2[port]   |=   1 << line;
	} else if (type & IRQT_LOW) {
		gpio_int_enabled[port] |=   1 << line;
		gpio_int_type1[port]   &= ~(1 << line);
		gpio_int_type2[port]   &= ~(1 << line);
	} else {
		gpio_int_enabled[port] &= ~(1 << line);
	}
	update_gpio_int_params(port);

	return 0;
}

static struct irq_chip ep93xx_gpio_irq_chip = {
	.name		= "GPIO",
	.ack		= ep93xx_gpio_irq_mask_ack,
	.mask		= ep93xx_gpio_irq_mask,
	.unmask		= ep93xx_gpio_irq_unmask,
	.set_type	= ep93xx_gpio_irq_type,
};

void __init ep93xx_init_irq(void)
{
	int irq;

	vic_init((void *)EP93XX_VIC1_BASE, 0,  EP93XX_VIC1_VALID_IRQ_MASK);
	vic_init((void *)EP93XX_VIC2_BASE, 32, EP93XX_VIC2_VALID_IRQ_MASK);


#if defined(CONFIG_FALINUX_EP9312)
	for (irq = IRQ_EP93XX_GPIO(0); irq <= IRQ_EP93XX_GPIO(15); irq++) {
#else
	for (irq = IRQ_EP93XX_GPIO(0); irq <= IRQ_EP93XX_GPIO(23); irq++) {
#endif
		set_irq_chip(irq, &ep93xx_gpio_irq_chip);
		set_irq_flags(irq, IRQF_VALID);
		set_irq_handler(irq, handle_level_irq);
	}

#if defined(CONFIG_FALINUX_EP9312)	
	set_irq_chained_handler(IRQ_EP93XX_GPIO_AB,  ep93xx_gpio_ab_irq_handler);
#else
	set_irq_chained_handler(IRQ_EP93XX_GPIO_AB,  ep93xx_gpio_ab_irq_handler);
	set_irq_chained_handler(IRQ_EP93XX_GPIO0MUX, ep93xx_gpio_f_irq_handler);
	set_irq_chained_handler(IRQ_EP93XX_GPIO1MUX, ep93xx_gpio_f_irq_handler);
	set_irq_chained_handler(IRQ_EP93XX_GPIO2MUX, ep93xx_gpio_f_irq_handler);
	set_irq_chained_handler(IRQ_EP93XX_GPIO3MUX, ep93xx_gpio_f_irq_handler);
	set_irq_chained_handler(IRQ_EP93XX_GPIO4MUX, ep93xx_gpio_f_irq_handler);
	set_irq_chained_handler(IRQ_EP93XX_GPIO5MUX, ep93xx_gpio_f_irq_handler);
	set_irq_chained_handler(IRQ_EP93XX_GPIO6MUX, ep93xx_gpio_f_irq_handler);
	set_irq_chained_handler(IRQ_EP93XX_GPIO7MUX, ep93xx_gpio_f_irq_handler);
#endif
}

/*************************************************************************
 * EP93xx peripheral handling
 *************************************************************************/
#define EP93XX_UART_MCR_OFFSET		(0x0100)

static void ep93xx_uart_set_mctrl(struct amba_device *dev,
				  void __iomem *base, unsigned int mctrl)
{
	unsigned int mcr;

	if(dev->res.start==EP93XX_UART1_PHYS_BASE)
	{
		//printk("set mctrl:1 %x\n",(unsigned long)base);
		mcr = 0;
		if (!(mctrl & TIOCM_RTS))
			mcr |= 2;
		if (!(mctrl & TIOCM_DTR))
			mcr |= 1;

		__raw_writel(mcr, base + EP93XX_UART_MCR_OFFSET);
	}
	else
	{
		//printk("set mctrl :2 \n");
	}	
}

static struct amba_pl010_data ep93xx_uart_data = {
	.set_mctrl	= ep93xx_uart_set_mctrl,
};

static struct amba_device uart1_device = {
	.dev		= {
		.bus_id		= "apb:uart1",
		.platform_data	= &ep93xx_uart_data,
	},
	.res		= {
		.start	= EP93XX_UART1_PHYS_BASE,
		.end	= EP93XX_UART1_PHYS_BASE + 0x0fff,
		.flags	= IORESOURCE_MEM,
	},
	.irq		= { IRQ_EP93XX_UART1, NO_IRQ },
	.periphid	= 0x00041010,
};

static struct amba_device uart2_device = {
	.dev		= {
		.bus_id		= "apb:uart2",
		.platform_data	= &ep93xx_uart_data,
	},
	.res		= {
		.start	= EP93XX_UART2_PHYS_BASE,
		.end	= EP93XX_UART2_PHYS_BASE + 0x0fff,
		.flags	= IORESOURCE_MEM,
	},
	.irq		= { IRQ_EP93XX_UART2, NO_IRQ },
	.periphid	= 0x00041010,
};

static struct amba_device uart3_device = {
	.dev		= {
		.bus_id		= "apb:uart3",
		.platform_data	= &ep93xx_uart_data,
	},
	.res		= {
		.start	= EP93XX_UART3_PHYS_BASE,
		.end	= EP93XX_UART3_PHYS_BASE + 0x0fff,
		.flags	= IORESOURCE_MEM,
	},
	.irq		= { IRQ_EP93XX_UART3, NO_IRQ },
	.periphid	= 0x00041010,
};


static struct platform_device ep93xx_rtc_device = {
       .name           = "ep93xx-rtc",
       .id             = -1,
       .num_resources  = 0,
};

static struct resource ep93xx_ac97_resources[] = {
	[0] = {
		.start	= EP93XX_AC97_PHY_BASE,
		.end	= EP93XX_AC97_PHY_BASE + 0x6C,
 		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= NO_IRQ,
		.end	= NO_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 ep93xx_ac97_dma_mask = 0xffffffffUL;

static struct platform_device ep93xx_ac97_device = {
	.name           = "ep93xx-ac97",
	.id             = 0,
	.num_resources  = 2,
	.resource       = ep93xx_ac97_resources,
	.dev = {
		.dma_mask               = &ep93xx_ac97_dma_mask,
		.coherent_dma_mask      = 0xffffffffUL,
	},
};

static struct ep93xx_eth_data ep93xx_eth_data = {
	.phy_id			= 1,
};

static struct resource ep93xx_eth_resource[] = {
	{
		.start	= EP93XX_ETHERNET_PHYS_BASE,
		.end	= EP93XX_ETHERNET_PHYS_BASE + 0xffff,
		.flags	= IORESOURCE_MEM,
	}, {
		.start	= IRQ_EP93XX_ETHERNET,
		.end	= IRQ_EP93XX_ETHERNET,
		.flags	= IORESOURCE_IRQ,
	}
};

static struct platform_device ep93xx_eth_device = {
	.name	= "ep93xx-eth",
	.id		= -1,
	.dev	= {
		.platform_data	= &ep93xx_eth_data,
	},
	.num_resources	= 2,
	.resource	= ep93xx_eth_resource,
};

static struct resource ep93xx_usb_resources[] = {
	{
		.start	= EP93XX_USB_PHYS_BASE,
		.end	= EP93XX_USB_PHYS_BASE + 0x8C,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= IRQ_EP93XX_USB,
		.end	= IRQ_EP93XX_USB,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 ep93xx_usbhost_dma_mask = 0xffffffffUL;
static struct platform_device ep93xx_usb_device = {
	.name	= "ep93xx-usb",
	.id		= -1,
	.dev	= {
		.dma_mask	= &ep93xx_usbhost_dma_mask,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources  = 2,
	.resource       = ep93xx_usb_resources,
};

static struct resource ep93xx_gadget_resources[] = {
	[0] = {
		.start	= 0x70000000,
		.end	= 0x70000000 + 0xff,
		.flags	= IORESOURCE_MEM,
	},
#if (defined(CONFIG_MACH_EDB9315A) || defined(CONFIG_MACH_EDB9307A) || defined(CONFIG_MACH_EDB9302A))
	[1] = {
		.start	= IRQ_EP93XX_EXT0,
		.end	= IRQ_EP93XX_EXT0,
		.flags	= IORESOURCE_IRQ,
	},
#else
	[1] = {
		.start	= IRQ_EP93XX_EXT1,
		.end	= IRQ_EP93XX_EXT1,
		.flags	= IORESOURCE_IRQ,
	},
#endif
};

static u64 ep93xx_gadget_dma_mask = 0xffffffffUL;
static struct platform_device ep93xx_gadget_device = {
	.name	= "ep93xx-udc",
	.id		= -1,
	.num_resources	= 2,
	.resource		= ep93xx_gadget_resources,
	.dev = {
		.dma_mask			= &ep93xx_gadget_dma_mask,
		.coherent_dma_mask	= 0xffffffffUL,
	},
};

#if defined(CONFIG_MTD_EDB93XX)
static struct flash_platform_data ep93xx_flash_data = {
	.map_name	= "cfi_probe",
	.width		= EP93XX_FLASH_WIDTH,
};

static struct resource ep93xx_flash_resource = {
	.start		= EP93XX_FLASH_BASE,
	.end		= EP93XX_FLASH_BASE + EP93XX_FLASH_SIZE - 1,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device ep93xx_flash_device = {
	.name	= "edb93xxflash",
	.id		= 0,
	.dev	= {
		.platform_data	= &ep93xx_flash_data,
	},
	.num_resources	= 1,
	.resource	= &ep93xx_flash_resource,
};
#endif

static struct resource ep93xx_pcmcia_resources[] = {
	[0] = {
		.start	= PCMCIA_BASE_PHYS,
		.end	= PCMCIA_BASE_PHYS + PCMCIA_SIZE,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_EP93XX_GPIO0MUX,
		.end	= IRQ_EP93XX_GPIO7MUX,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 ep93xx_pcmcia_dma_mask = 0xffffffffUL;
static struct platform_device ep93xx_pcmcia_device = {
	.name           = "ep93xx-pcmcia",
	.id             = -1,
	.num_resources  = 2,
	.resource       = ep93xx_pcmcia_resources,
	.dev = {
		.dma_mask			= &ep93xx_pcmcia_dma_mask,
		.coherent_dma_mask	= 0xffffffffUL,
	},
};

void __init ep93xx_init_devices(void)
{
	unsigned int v;
	unsigned int uiTemp;
	int SSP_Handle;

	/*
	 * Disallow access to MaverickCrunch initially.
	 */
	v = __raw_readl(EP93XX_SYSCON_DEVICE_CONFIG);
	v = v | SYSCON_DEVCFG_U2EN;
	v &= ~EP93XX_SYSCON_DEVICE_CONFIG_CRUNCH_ENABLE;
	__raw_writel(0xaa, EP93XX_SYSCON_SWLOCK);
	__raw_writel(v, EP93XX_SYSCON_DEVICE_CONFIG);

	amba_device_register(&uart1_device, &iomem_resource);
	amba_device_register(&uart2_device, &iomem_resource);
	amba_device_register(&uart3_device, &iomem_resource);

	platform_device_register(&ep93xx_rtc_device);

	memcpy(ep93xx_eth_data.dev_addr, (void *)(EP93XX_ETHERNET_BASE + 0x50), 6);
	platform_device_register(&ep93xx_eth_device);
	platform_device_register(&ep93xx_usb_device);
//	platform_device_register(&ep93xx_gadget_device);

	#if defined(CONFIG_MTD_EDB93XX)
		platform_device_register(&ep93xx_flash_device);
	#endif

//	platform_device_register(&ep93xx_raster_device);
//	platform_device_register(&ep93xx_pcmcia_device);
	platform_device_register(&ep93xx_ac97_device);

	/*
	 * Get the hostname from the SPI FLASH if it has been programmed.
	 */
	SSP_Handle = SSPDriver->Open(SERIAL_FLASH, 0);
	if(SSP_Handle != -1) {
		SSPDriver->Read( SSP_Handle, 0x1000, &uiTemp );
		if (uiTemp == 0x43414d45) {
			SSPDriver->Read( SSP_Handle, 0x1010, &uiTemp );
			init_uts_ns.name.nodename[0] = uiTemp & 255;
			init_uts_ns.name.nodename[1] = (uiTemp >> 8) & 255;
			init_uts_ns.name.nodename[2] = (uiTemp >> 16) & 255;
			init_uts_ns.name.nodename[3] = uiTemp >> 24;
			SSPDriver->Read( SSP_Handle, 0x1014, &uiTemp );
			init_uts_ns.name.nodename[4] = uiTemp & 255;
			init_uts_ns.name.nodename[5] = (uiTemp >> 8) & 255;
			init_uts_ns.name.nodename[6] = (uiTemp >> 16) & 255;
			init_uts_ns.name.nodename[7] = uiTemp >> 24;
			SSPDriver->Read( SSP_Handle, 0x1018, &uiTemp );
			init_uts_ns.name.nodename[8] = uiTemp & 255;
			init_uts_ns.name.nodename[9] = (uiTemp >> 8) & 255;
			init_uts_ns.name.nodename[10] = (uiTemp >> 16) & 255;
			init_uts_ns.name.nodename[11] = uiTemp >> 24;
			SSPDriver->Read( SSP_Handle, 0x101c, &uiTemp );
			init_uts_ns.name.nodename[12] = uiTemp & 255;
			init_uts_ns.name.nodename[13] = (uiTemp >> 8) & 255;
			init_uts_ns.name.nodename[14] = (uiTemp >> 16) & 255;
			init_uts_ns.name.nodename[15] = uiTemp >> 24;
			init_uts_ns.name.nodename[16] = 0;
		}
		SSPDriver->Close( SSP_Handle );
	}
		 
	#ifdef CONFIG_CRUNCH
		elf_hwcap |= HWCAP_CRUNCH;
	#endif
	
	#ifdef CONFIG_EP93XX_FPU
		crunch_enable();
		crunch_init();
		crunch_is_enabled=1;
	#endif
}

