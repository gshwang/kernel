/*
 * arch/arm/mach-ixp4xx/ez_ixp42x-pci.c 
 *
 * ez_ixp42x board-level PCI initialization
 *
 * Copyright (C) 2002 Intel Corporation.
 * Copyright (C) 2003-2004 MontaVista Software, Inc.
 *
 * Maintainer: Deepak Saxena <dsaxena@plexity.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/delay.h>

#include <asm/mach/pci.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>

void __init ez_ixp42x_pci_preinit(void)
{
	set_irq_type(IRQ_EZ_IXP42X_PCI_INTA, IRQT_LOW);
	set_irq_type(IRQ_EZ_IXP42X_PCI_INTB, IRQT_LOW);
	set_irq_type(IRQ_EZ_IXP42X_PCI_INTC, IRQT_LOW);
	set_irq_type(IRQ_EZ_IXP42X_PCI_INTD, IRQT_LOW);

	ixp4xx_pci_preinit();
}

static int __init ez_ixp42x_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	static int pci_irq_table[EZ_IXP42X_PCI_IRQ_LINES] = {
		IRQ_EZ_IXP42X_PCI_INTA,
		IRQ_EZ_IXP42X_PCI_INTB,
		IRQ_EZ_IXP42X_PCI_INTC,
		IRQ_EZ_IXP42X_PCI_INTD
	};

	int irq = -1;

#if 0
	if (slot >= 1 && slot <= EZ_IXP42X_PCI_MAX_DEV && 
		pin >= 1 && pin <= EZ_IXP42X_PCI_IRQ_LINES) {
		irq = pci_irq_table[(slot + pin - 2) % 4];
	}

	return irq;
#endif

    if ( slot >= 1 && pin >= 1 )
    {
        int  idx;

        slot = 32-slot;

        idx = (slot + pin) % 4;

        irq = pci_irq_table[ idx ];
        printk( " ... slot=%d, pin=%d >> irq=%d (%d)\n", slot, pin, irq, idx );
    }
	
	return irq;
}

struct hw_pci ez_ixp42x_pci __initdata = {
	.nr_controllers = 1,
	.preinit	= ez_ixp42x_pci_preinit,
	.swizzle	= pci_std_swizzle,
	.setup		= ixp4xx_setup,
	.scan		= ixp4xx_scan_bus,
	.map_irq	= ez_ixp42x_map_irq,
};

int __init ez_ixp42x_pci_init(void)
{
	//if (machine_is_ez_ixp42x() || machine_is_ixcdp1100() || machine_is_ixdp465())
	if ( machine_is_ez_ixp42x() )
		pci_common_init(&ez_ixp42x_pci);
	return 0;
}

subsys_initcall(ez_ixp42x_pci_init);
