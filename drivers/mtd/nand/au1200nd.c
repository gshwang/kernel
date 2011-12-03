/*
 *  drivers/mtd/nand/au1200nd.c
 *
 *  Copyright (C) 2004 Embedded Edge, LLC
 *
 * $Id: au1200nd.c,v 1.13 2005/11/07 11:14:30 gleixner Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/version.h>
#include <asm/io.h>

#include <asm/mach-au1x00/au1xxx.h>

/*
 * MTD structure for NAND controller
 */
static struct mtd_info *au1200_mtd = NULL;
static void __iomem *p_nand;
static int nand_width = 1;	/* default x8 */
static void (*au1200_write_byte)(struct mtd_info *, u_char);


/**
 * au_read_byte -  read one byte from the chip
 * @mtd:	MTD device structure
 *
 *  read function for 8bit buswith
 */
static u_char au_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *this = mtd->priv;
	u_char ret;
	ret = readb(this->IO_ADDR_R);
	au_sync();
	return ret;
}

/**
 * au_write_byte -  write one byte to the chip
 * @mtd:	MTD device structure
 * @byte:	pointer to data byte to write
 *
 *  write function for 8it buswith
 */
static void au_write_byte(struct mtd_info *mtd, u_char byte)
{
	struct nand_chip *this = mtd->priv;

	au_writeb(byte, this->IO_ADDR_W);
	au_sync();
}

/**
 * au_read_byte16 -  read one byte endianess aware from the chip
 * @mtd:	MTD device structure
 *
 *  read function for 16bit buswith with
 * endianess conversion
 */
static u_char au_read_byte16(struct mtd_info *mtd)
{
	struct nand_chip *this = mtd->priv;
	u_char ret;
	ret = (u_char) cpu_to_le16(readw(this->IO_ADDR_R));
	au_sync();
	return ret;
}

/**
 * au_write_byte16 -  write one byte endianess aware to the chip
 * @mtd:	MTD device structure
 * @byte:	pointer to data byte to write
 *
 *  write function for 16bit buswith with
 * endianess conversion
 */
static void au_write_byte16(struct mtd_info *mtd, u_char byte)
{
	struct nand_chip *this = mtd->priv;
	writew(le16_to_cpu((u16) byte), this->IO_ADDR_W);
	au_sync();
}

/**
 * au_read_word -  read one word from the chip
 * @mtd:	MTD device structure
 *
 *  read function for 16bit buswith without
 * endianess conversion
 */
static u16 au_read_word(struct mtd_info *mtd)
{
	struct nand_chip *this = mtd->priv;
	u16 ret;
	ret = readw(this->IO_ADDR_R);
	au_sync();
	return ret;
}

/**
 * au_write_buf -  write buffer to chip
 * @mtd:	MTD device structure
 * @buf:	data buffer
 * @len:	number of bytes to write
 *
 *  write function for 8bit buswith
 */
static void au_write_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;

	for (i = 0; i < len; i++) {
		writeb(buf[i], this->IO_ADDR_W);
		au_sync();
	}
}

/**
 * au_read_buf -  read chip data into buffer
 * @mtd:	MTD device structure
 * @buf:	buffer to store date
 * @len:	number of bytes to read
 *
 *  read function for 8bit buswith
 */
static void au_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;

	for (i = 0; i < len; i++) {
		buf[i] = readb(this->IO_ADDR_R);
		au_sync();
	}
}

/**
 * au_verify_buf -  Verify chip data against buffer
 * @mtd:	MTD device structure
 * @buf:	buffer containing the data to compare
 * @len:	number of bytes to compare
 *
 *  verify function for 8bit buswith
 */
static int au_verify_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;

	for (i = 0; i < len; i++) {
		if (buf[i] != readb(this->IO_ADDR_R))
			return -EFAULT;
		au_sync();
	}

	return 0;
}

/**
 * au_write_buf16 -  write buffer to chip
 * @mtd:	MTD device structure
 * @buf:	data buffer
 * @len:	number of bytes to write
 *
 *  write function for 16bit buswith
 */
static void au_write_buf16(struct mtd_info *mtd, const u_char *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;
	u16 *p = (u16 *) buf;
	len >>= 1;

	for (i = 0; i < len; i++) {
		writew(p[i], this->IO_ADDR_W);
		au_sync();
	}

}

/**
 * au_read_buf16 -  read chip data into buffer
 * @mtd:	MTD device structure
 * @buf:	buffer to store date
 * @len:	number of bytes to read
 *
 *  read function for 16bit buswith
 */
static void au_read_buf16(struct mtd_info *mtd, u_char *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;
	u16 *p = (u16 *) buf;
	len >>= 1;
	
	for (i = 0; i < len; i++) {
		p[i] = readw(this->IO_ADDR_R);
		au_sync();
	}
}

/**
 * au_verify_buf16 -  Verify chip data against buffer
 * @mtd:	MTD device structure
 * @buf:	buffer containing the data to compare
 * @len:	number of bytes to compare
 *
 *  verify function for 16bit buswith
 */
static int au_verify_buf16(struct mtd_info *mtd, const u_char *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;
	u16 *p = (u16 *) buf;
	len >>= 1;

	for (i = 0; i < len; i++) {
		if (p[i] != readw(this->IO_ADDR_R))
			return -EFAULT;
		au_sync();
	}
	return 0;
}

static inline void nand_nCS_active(void)
{
	au_writel((1<<7), SYS_OUTPUTCLR);				au_sync();
}

static void nand_nCS_deactive(void)
{
	au_writel((1<<7), SYS_OUTPUTSET);				au_sync();
}

static void au1200_hwcontrol(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct nand_chip* this = mtd->priv;

	if (ctrl & NAND_CTRL_CHANGE) 
	{
		if ( ctrl & NAND_NCE ) { this->IO_ADDR_W = p_nand + MEM_STNAND_DATA; nand_nCS_active();     } 
		else                   {                                             nand_nCS_deactive();   } 
                                                                                                      
		if ( ctrl & NAND_CLE ) { this->IO_ADDR_W = p_nand + MEM_STNAND_CMD;                         } 
		if ( ctrl & NAND_ALE ) { this->IO_ADDR_W = p_nand + MEM_STNAND_ADDR;                        } 

	}

	if (cmd != NAND_CMD_NONE) writeb(cmd & 0xFF, this->IO_ADDR_W); // @C FALINUX

	/* Drain the writebuffer */
	au_sync();
	
}
 
int au1200_device_ready(struct mtd_info *mtd)
{
	int ret = (au_readl(MEM_STSTAT) & 0x1) ? 1 : 0;
	au_sync();
	return ret;
}

/*
 * Main initialization routine
 */
static int __init au1xxx_nand_init(void)
{
	struct nand_chip *this;
	u16 boot_swapboot = 0;	/* default value */
	int retval;
	u32 mem_staddr;
	u32 nand_phys;

	/* Allocate memory for MTD device structure and private data */
	au1200_mtd = kmalloc(sizeof(struct mtd_info) + sizeof(struct nand_chip), GFP_KERNEL);
	if (!au1200_mtd) {
		printk("Unable to allocate NAND MTD dev structure.\n");
		return -ENOMEM;
	}

	/* Get pointer to private data */
	this = (struct nand_chip *)(&au1200_mtd[1]);

	/* Initialize structures */
	memset(au1200_mtd, 0, sizeof(struct mtd_info));
	memset(this, 0, sizeof(struct nand_chip));

	/* Link the private data with the MTD structure */
	au1200_mtd->priv = this;
	au1200_mtd->owner = THIS_MODULE;


	/* MEM_STNDCTL: disable ints, disable nand boot */
	au_writel(0x0, MEM_STNDCTL);

	/* Configure chip-select; normally done by boot code, e.g. YAMON */
#ifdef NAND_STCFG
	if (NAND_CS == 0) {
		au_writel(NAND_STCFG,  MEM_STCFG0);
		au_writel(NAND_STTIME, MEM_STTIME0);
		au_writel(NAND_STADDR, MEM_STADDR0);
	}
	if (NAND_CS == 1) {
		au_writel(NAND_STCFG,  MEM_STCFG1);
		au_writel(NAND_STTIME, MEM_STTIME1);
		au_writel(NAND_STADDR, MEM_STADDR1);
	}
	if (NAND_CS == 2) {
		au_writel(NAND_STCFG,  MEM_STCFG2);
		au_writel(NAND_STTIME, MEM_STTIME2);
		au_writel(NAND_STADDR, MEM_STADDR2);
	}
	if (NAND_CS == 3) {
		au_writel(NAND_STCFG,  MEM_STCFG3);
		au_writel(NAND_STTIME, MEM_STTIME3);
		au_writel(NAND_STADDR, MEM_STADDR3);
	}
#endif

	/* Locate NAND chip-select in order to determine NAND phys address */
	mem_staddr = 0x00000000;
	if (((au_readl(MEM_STCFG0) & 0x7) == 0x5) && (NAND_CS == 0))
		mem_staddr = au_readl(MEM_STADDR0);
	else if (((au_readl(MEM_STCFG1) & 0x7) == 0x5) && (NAND_CS == 1))
		mem_staddr = au_readl(MEM_STADDR1);
	else if (((au_readl(MEM_STCFG2) & 0x7) == 0x5) && (NAND_CS == 2))
		mem_staddr = au_readl(MEM_STADDR2);
	else if (((au_readl(MEM_STCFG3) & 0x7) == 0x5) && (NAND_CS == 3))
		mem_staddr = au_readl(MEM_STADDR3);

	if (mem_staddr == 0x00000000) {
		printk("Au1xxx NAND: ERROR WITH NAND CHIP-SELECT\n");
		kfree(au1200_mtd);
		return 1;
	}
	nand_phys = (mem_staddr << 4) & 0xFFFC0000;
	p_nand = (void __iomem *)ioremap(nand_phys, 0x1000);

	/* make controller and MTD agree */
	if (NAND_CS == 0)
		nand_width = au_readl(MEM_STCFG0) & (1 << 22);
	if (NAND_CS == 1)
		nand_width = au_readl(MEM_STCFG1) & (1 << 22);
	if (NAND_CS == 2)
		nand_width = au_readl(MEM_STCFG2) & (1 << 22);
	if (NAND_CS == 3)
		nand_width = au_readl(MEM_STCFG3) & (1 << 22);

{	// @C FALINUX
	// NAND �� OE ��ȣ�� ���� �ʱ� ������ �߰��� ��ƾ 
	// ������ ��Ȯ�� �𸣰ڴ�. 
	// �ƴ� ����� ���߿� ���� �ֱ� �ٶ���. 
	// �̰��� �ذ��ϱ� ���ؼ��� 
	// MEMSTCFG0 ��  BEB �ʵ尪�� 2 ���� 1�� �������� nCS0 ������ �ѹ� �о� �־�� �Ѵ�.
	u32 memstcfg0_val;
	memstcfg0_val = au_readl(MEM_STCFG0);
	memstcfg0_val &= (~0x00060000);
	memstcfg0_val |= ( 0x00020000);
	au_writel(memstcfg0_val, MEM_STCFG0 );
	readb( 0xBE000000 ); 
}
	this->IO_ADDR_R = p_nand + MEM_STNAND_DATA;
	this->IO_ADDR_W = p_nand + MEM_STNAND_DATA;

	this->cmd_ctrl	= au1200_hwcontrol;
	this->dev_ready	= au1200_device_ready;

	/* 30 us command delay time */
	this->chip_delay = 30;  // @FALINUX
	this->ecc.mode = NAND_ECC_SOFT;

	this->options = NAND_NO_AUTOINCR;

	if (!nand_width)
		this->options |= NAND_BUSWIDTH_16;

	this->read_byte = (!nand_width) ? au_read_byte16 : au_read_byte;
	au1200_write_byte = (!nand_width) ? au_write_byte16 : au_write_byte;
	this->read_word = au_read_word;
	this->write_buf = (!nand_width) ? au_write_buf16 : au_write_buf;
	this->read_buf = (!nand_width) ? au_read_buf16 : au_read_buf;
	this->verify_buf = (!nand_width) ? au_verify_buf16 : au_verify_buf;

	/* Scan to find existence of the device */
	if (nand_scan(au1200_mtd, 1)) {
		retval = -ENXIO;
		goto outio;
	}

	/* Register the partitions */
	add_mtd_partitions( au1200_mtd, ezboot_partition_info, ARRAY_SIZE(ezboot_partition_info) );
	
	return 0;

 outio:
	iounmap((void *)p_nand);

 outmem:
	kfree(au1200_mtd);
	return retval;
}

module_init(au1xxx_nand_init);

/*
 * Clean up routine
 */
static void __exit au1200_cleanup(void)
{
	struct nand_chip *this = (struct nand_chip *)&au1200_mtd[1];

	/* Release resources, unregister device */
	nand_release(au1200_mtd);

	/* Free the MTD device structure */
	kfree(au1200_mtd);

	/* Unmap */
	iounmap((void *)p_nand);
}

module_exit(au1200_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("falinux.com");
MODULE_DESCRIPTION("Board-specific glue layer for NAND flash on EM-AU1200 board");
