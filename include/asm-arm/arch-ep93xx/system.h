/*
 * linux/include/asm-arm/arch-ep93xx/system.h
 */

#include <asm/hardware.h>
#include <asm/io.h>
static inline void arch_idle(void)
{
	cpu_do_idle();
}

static inline void arch_reset(char mode)
{
	u32 devicecfg;

	local_irq_disable();

#ifdef CONFIG_ESP_MMI
	int uiRegTemp;
	unsigned long val = inl(SYSCON_DEVCFG);

	// FALINUX ESP-MMI EGPIO-14 RESET 
	uiRegTemp = inl(GPIO_PBDDR);
	outl(uiRegTemp | 0x40, GPIO_PBDDR);
	
	uiRegTemp = inl(GPIO_PBDR);
	outl(uiRegTemp & ~0x40, GPIO_PBDR);
	msleep(1);
#endif

	devicecfg = __raw_readl(EP93XX_SYSCON_DEVICE_CONFIG);
	__raw_writel(0xaa, EP93XX_SYSCON_SWLOCK);
	__raw_writel(devicecfg | 0x80000000, EP93XX_SYSCON_DEVICE_CONFIG);
	__raw_writel(0xaa, EP93XX_SYSCON_SWLOCK);
	__raw_writel(devicecfg & ~0x80000000, EP93XX_SYSCON_DEVICE_CONFIG);

	while (1)
		;
}
