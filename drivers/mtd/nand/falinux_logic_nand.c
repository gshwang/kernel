/*
 *  drivers/mtd/nand/falinux_logic_nand.c
 *
 *  Copyright (C) 2007 jaekyoung oh (freefrug@falinux.com)
 *
 *  Derived from drivers/mtd/nand/edb7312.c
 *       Copyright (C) 2002 Marius Gröger (mag@sysgo.de)
 *       Copyright (c) 2001 Thomas Gleixner (gleixner@autronix.de)
 *
 * $Id: h1910.c,v 1.6 2005/11/07 11:14:30 gleixner Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>

/*
 * MTD structure 
 */
static struct mtd_info *falinux_mtd = NULL;


// drivr/mtd/nand/falinux_ezboot_part.c
extern int    ezboot_part_nr;
extern struct mtd_partition ezboot_partition_info[];


/*
 * Module stuff
 */
#if		defined(CONFIG_MACH_EZ_X5) || defined(CONFIG_MACH_ESP_NS) || defined(CONFIG_MACH_ESP_CX) || defined(CONFIG_MACH_EZ_PXA270)
#include <asm/arch/pxa-regs.h>
#define	NAND_BASE_PHY	PXA_CS1_PHYS
#elif	defined(CONFIG_MACH_EZ_EP9312) || defined(CONFIG_MACH_ESP_MMI)
#include <asm/arch/ep93xx-regs.h>
#include <asm/arch/gpio.h>
#define	NAND_BASE_PHY	EP9312_CS1_PHYS
#else
#define	NAND_BASE_PHY		0
#endif

#define NUM_PARTITIONS 		3


/**
 * nand_select_chip - control CE line
 * @mtd:	MTD device structure
 * @chipnr:	chipnumber to select, -1 for deselect
 *
 * Default select function for 1 chip devices.
 */
static void falinux_nand_select_chip(struct mtd_info *mtd, int chipnr)
{
	struct nand_chip *chip = mtd->priv;

#if		defined(CONFIG_MACH_EZ_X5) || defined(CONFIG_MACH_ESP_NS) || defined(CONFIG_MACH_EZ_EP9312)

	switch (chipnr) 
	{
	case 0:
		writeb( 0  , chip->IO_ADDR_W + 0x300 );
		writeb( 0  , chip->IO_ADDR_W + 0x000 );
		break;
	default :
		writeb( 0  , chip->IO_ADDR_W + 0x300 );
		break;
	}

#elif	defined(CONFIG_MACH_ESP_MMI)

	gpio_line_config(0, GPIO_OUT);	// PADDR 0-bit를 출력으로 설정한다.

	switch (chipnr) 
	{
	case 0:
		gpio_line_set(0, EP93XX_GPIO_LOW);
		break;
	default:
		gpio_line_set(0, EP93XX_GPIO_HIGH);
		break;
	}	

#elif	defined(CONFIG_MACH_EZ_PXA270)

	switch (chipnr) 
	{
	case 0:
		pxa_gpio_set_value(0, 0);	// low
		break;
	default:
		pxa_gpio_set_value(0, 1);	// high
		break;
	}	

#elif   defined(CONFIG_MACH_ESP_CX)

	switch (chipnr) 
	{
	case 0:
		GPCR(81) = GPIO_bit(81);
		break;
	default:
		GPSR(81) = GPIO_bit(81);
		break;
	}

#endif
}

/*
 *	hardware specific access to control-lines
 *
 *	NAND_NCE: bit 0 - don't care
 *	NAND_CLE: bit 1 - address bit 2
 *	NAND_ALE: bit 2 - address bit 3
 */
static void falinux_nand_hwcontrol(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct nand_chip *chip = mtd->priv;

	if ( cmd != NAND_CMD_NONE )
	{
#if		defined(CONFIG_MACH_EZ_X5) || defined(CONFIG_MACH_ESP_NS) || defined(CONFIG_MACH_ESP_CX) || defined(CONFIG_MACH_EZ_PXA270) || defined(CONFIG_MACH_EZ_EP9312)

		if      ( ctrl & NAND_CLE ) { writeb( cmd, chip->IO_ADDR_W + 0x100   );  } 
		else if ( ctrl & NAND_ALE ) { writeb( cmd, chip->IO_ADDR_W + 0x200   );  } 

#elif	defined(CONFIG_MACH_ESP_MMI)

		if      ( ctrl & NAND_CLE ) { writeb( cmd, chip->IO_ADDR_W + 0x10000 );  } 
		else if ( ctrl & NAND_ALE ) { writeb( cmd, chip->IO_ADDR_W + 0x20000 );  } 

#else

		if      ( ctrl & NAND_CLE ) { writeb( cmd, chip->IO_ADDR_W + 0x100   );  } 
		else if ( ctrl & NAND_ALE ) { writeb( cmd, chip->IO_ADDR_W + 0x200   );  } 

#endif
	}
}

/*
 * Main initialization routine
 */
static int __init falinux_nand_init(void)
{
	struct nand_chip *this;
	int mtd_parts_nb = 0;
	struct mtd_partition *mtd_parts = 0;
	void __iomem *nandaddr;

#if		defined(CONFIG_MACH_EZ_X5) || defined(CONFIG_MACH_ESP_NS) || defined(CONFIG_MACH_ESP_CX) || defined(CONFIG_MACH_EZ_PXA270) || defined(CONFIG_MACH_EZ_EP9312)
	nandaddr = (unsigned long)ioremap(NAND_BASE_PHY, 0x1000  );
#elif	defined(CONFIG_MACH_ESP_MMI)
	nandaddr = (unsigned long)ioremap(NAND_BASE_PHY, 0x100000);
#else
	nandaddr = (unsigned long)ioremap(NAND_BASE_PHY, 0x1000  );
#endif	
	if (!nandaddr) {
		printk("Failed to ioremap nand flash.\n");
		return -ENOMEM;
	}

	/* Allocate memory for MTD device structure and private data */
	falinux_mtd = kmalloc(sizeof(struct mtd_info) + sizeof(struct nand_chip), GFP_KERNEL);
	if (!falinux_mtd) {
		printk("Unable to allocate h1910 NAND MTD device structure.\n");
		iounmap((void *)nandaddr);
		return -ENOMEM;
	}

	/* Get pointer to private data */
	this = (struct nand_chip *)(&falinux_mtd[1]);

	/* Initialize structures */
	memset(falinux_mtd, 0, sizeof(struct mtd_info));
	memset(this, 0, sizeof(struct nand_chip));

	/* Link the private data with the MTD structure */
	falinux_mtd->priv = this;
	falinux_mtd->owner = THIS_MODULE;

	/* insert callbacks */
	this->IO_ADDR_R  = nandaddr;
	this->IO_ADDR_W  = nandaddr;
	this->cmd_ctrl   = falinux_nand_hwcontrol;
	this->dev_ready  = NULL;	// unknown whether that was correct or not so we will just do it like this 
	this->chip_delay = 50;		// 15 us command delay time 
	this->ecc.mode   = NAND_ECC_SOFT;
	this->options    = NAND_NO_AUTOINCR;

	this->select_chip = falinux_nand_select_chip;

	/* Scan to find existence of the device */
	if (nand_scan(falinux_mtd, 1)) 
	{
		printk( "No NAND device - returning -ENXIO\n");
		kfree(falinux_mtd);
		iounmap((void *)nandaddr);
		return -ENXIO;
	}

	// 파티션 정보를 설정한다.
	add_mtd_partitions( falinux_mtd, ezboot_partition_info, ezboot_part_nr );
	

	/* Return happy */
	return 0;
}

module_init(falinux_nand_init);

/*
 * Clean up routine
 */
static void __exit falinux_nand_cleanup(void)
{
	struct nand_chip *this = (struct nand_chip *)&falinux_mtd[1];

	/* Release resources, unregister device */
	nand_release(falinux_mtd);

	/* Release io resource */
	iounmap((void *)this->IO_ADDR_W);

	/* Free the MTD device structure */
	kfree(falinux_mtd);
}

module_exit(falinux_nand_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jaekyoung oh (freefrug@falinux.com)");
MODULE_DESCRIPTION("NAND flash driver for falinux board (EZ-X5,ESP-CX,ESP-NS) ");
