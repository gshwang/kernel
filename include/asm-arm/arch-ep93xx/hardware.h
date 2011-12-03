/*
 * linux/include/asm-arm/arch-ep93xx/hardware.h
 */
#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include "ep93xx-regs.h"
#include "regs_syscon.h"
#include "regs_irda.h"
#include "regs_ide.h"
#include "regs_dma.h"
#include "regs_raster.h"
#include "regs_ac97.h"
#include "regs_uart.h"
#include "regs_smc.h"
#include "regs_touch.h"
#include "regs_spi.h"

#include "memory.h"

#define pcibios_assign_all_busses()	0

#include "platform.h"

#include "gesbc9312.h"
#include "ts72xx.h"
#include "edb9301.h"
#include "edb9302.h"
#include "edb9307.h"
#include "edb9312.h"
#include "edb9315.h"
#include "edb9302a.h"
#include "edb9307a.h"
#include "edb9315a.h"
#include "falinux.h"

#if (defined(CONFIG_MACH_EDB9315A) || defined(CONFIG_MACH_EDB9307A) || defined(CONFIG_MACH_EDB9302A) || defined(CONFIG_FALINUX_EP9312))
#define EP93XX_PARAMS_PHYS      0xc0000100
#define EP93XX_ZREL_ADDR        0xc0008000
#define EP93XX_PHYS_ADDR        0xc0000000
/*#define EP93XX_INITRD_PHYS    0x01000000*/

#else
#define EP93XX_PARAMS_PHYS      0x00000100
#define EP93XX_ZREL_ADDR        0x00008000
#define EP93XX_PHYS_ADDR        0x00000000
/*#define EP93XX_INITRD_PHYS    0x01000000*/
#endif

#ifndef MSECS_TO_JIFFIES
#define MSECS_TO_JIFFIES(ms)    (((ms)*HZ+999)/1000)
#endif

#if (defined(CONFIG_MACH_EDB9312) || defined(CONFIG_MACH_EDB9315) || defined(CONFIG_MACH_EDB9307) )
#define EP93XX_FLASH_BASE       0x60000000
#define EP93XX_FLASH_WIDTH      4
#define EP93XX_FLASH_SIZE       0x02000000
#else
#define EP93XX_FLASH_BASE       0x60000000
#define EP93XX_FLASH_WIDTH      2
#define EP93XX_FLASH_SIZE       0x00800000
#endif


#endif

