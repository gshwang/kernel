 /*  File:       linux/include/asm-arm/arch-ep93xx/io.h
 *
 *  Copyright (C) 1999 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

#define IO_SPACE_LIMIT		0xffffffff

#define __io(p)			((void __iomem *)(p))
#define __mem_pci(p)		(p)

/*
 * These routines are used to hack the default ide i/o routines in
 * drivers/ide/ide-iops.h so they work with PCMCIA IDE.  Sad, but
 * necessary.   They are fleshed out in arch/arm/mach-ep93xx/pcmcia_io.c.
 */
extern u8 ep93xx_pcmcia_io_inb (unsigned long port);
extern u16 ep93xx_pcmcia_io_inw (unsigned long port);
extern void ep93xx_pcmcia_io_insw (unsigned long port, void *addr, u32 count);
extern void ep93xx_pcmcia_io_outb (u8 addr, unsigned long port);
extern void ep93xx_pcmcia_io_outw (u16 addr, unsigned long port);
extern void ep93xx_pcmcia_io_outsw (unsigned long port, void *addr, u32 count);

#endif
