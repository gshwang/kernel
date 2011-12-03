/*
 * include/asm-arm/arch-ixp4xx/ez_ixp42x.h
 *
 * EZ-IXP42X platform specific definitions
 *
 * Author: You young chang <frog@falinux.com>
 *
 * This source base is ixdp425.h
 *
 * Copyright 2005 (c) FALinux.Co.Ltd.
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __ASM_ARCH_HARDWARE_H__
#error "Do not include this directly, instead #include <asm/hardware.h>"
#endif

#define		EZ_IXP42X_SDA_PIN		7
#define		EZ_IXP42X_SCL_PIN		6

#define		EZ_IXP42X_WDOG_PIN		14

/*
 * IXDP425 PCI IRQs
 */
#define 	EZ_IXP42X_PCI_MAX_DEV		8
#define 	EZ_IXP42X_PCI_IRQ_LINES		4

/* PCI controller GPIO to IRQ pin mappings */
#define 	EZ_IXP42X_PCI_INTA_PIN 		11
#define 	EZ_IXP42X_PCI_INTB_PIN 		10
#define    	EZ_IXP42X_PCI_INTC_PIN  	9
#define    	EZ_IXP42X_PCI_INTD_PIN  	8
