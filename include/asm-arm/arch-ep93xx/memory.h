/*
 * linux/include/asm-arm/arch-ep93xx/memory.h
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#if (defined(CONFIG_MACH_EDB9315A) || defined(CONFIG_MACH_EDB9307A) || defined(CONFIG_MACH_EDB9302A) || defined(CONFIG_FALINUX_EP9312) )
#define PHYS_OFFSET		UL(0xc0000000)
#else
#define PHYS_OFFSET		UL(0x00000000)
#endif


#define __bus_to_virt(x)	__phys_to_virt(x)
#define __virt_to_bus(x)	__virt_to_phys(x)


#endif
