/* linux/arch/arm/plat-s3c24xx/common-smdk.c
 *
 * Copyright (c) 2006 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * Common code for SMDK2410 and SMDK2440 boards
 *
 * http://www.fluff.org/ben/smdk2440/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/platform_device.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <asm/arch/regs-gpio.h>
#include <asm/arch/leds-gpio.h>

#include <asm/arch/nand.h>

#include <asm/plat-s3c24xx/common-falinux.h>
#include <asm/plat-s3c24xx/devs.h>
#include <asm/plat-s3c24xx/pm.h>

// ------------------------------------------------------------------
// NAND Flash Setup
// ------------------------------------------------------------------
#define NUM_PARTITIONS      8

//------------------------------------------------------------------------------
// 설명 : 디폴트 파티션 정보
//------------------------------------------------------------------------------
static struct mtd_partition falinux_s24xx_default_nand_part[NUM_PARTITIONS] = {
	[0] = {
		.name   = "falinux boot/config/logo partition",
		.offset = 0,
		.size   = 2*SZ_1M,
		.mask_flags = 0,
	},
	[1] = {
		.name   = "falinux kernel/ramdisk partition",
		.offset = 2*SZ_1M,
		.size   = 8*SZ_1M,
		.mask_flags = 0,
	},
	[2] = {
		.name   = "falinux yaffs partition",
		.offset = 10*SZ_1M,
		.size   = 54*SZ_1M,
		.mask_flags = 0,
	},
	[3] = {
		.name   = "falinux logging partition",
		.offset = 64*SZ_1M,
		.size   = 0*SZ_1M,
		.mask_flags = 0,
	},
};

static struct s3c2410_nand_set falinuxs24xx_nand_sets[] = {
	[0] = {
		.name		    = "FALINUX",
		.nr_chips	    = 1,
		.nr_map		    = NULL,
		.nr_partitions  = 3,
		.partitions     = falinux_s24xx_default_nand_part
	},
};

static struct s3c2410_platform_nand falinuxs24xx_nand_info = {
	.tacls		    = 20,
	.twrph0		    = 60,
	.twrph1		    = 20,
	.nr_sets	    = ARRAY_SIZE(falinuxs24xx_nand_sets),
	.sets		    = falinuxs24xx_nand_sets,
	.select_chip	= NULL,
};

/* devices we initialise */
static struct platform_device __initdata *falinuxs24xx_devs[] = {
	&s3c_device_nand,
};

void __init falinuxs24xx_machine_init(void)
{
	s3c_device_nand.dev.platform_data = &falinuxs24xx_nand_info;

	platform_add_devices(falinuxs24xx_devs, ARRAY_SIZE(falinuxs24xx_devs));

//	s3c2410_pm_init();
}
