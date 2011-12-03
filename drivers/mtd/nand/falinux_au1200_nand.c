/*
 *  drivers/mtd/nand/falinux_au1200_nand.c
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

// drivr/mtd/nand/falinux_ezboot_part.c
extern int    ezboot_part_nr;
extern struct mtd_partition ezboot_partition_info[];

// @C FALINUX
// NAND 의 OE 신호가 뜨지 않기 때문에 추가한 루틴 
// 이유는 정확히 모르겠다. 
// 아는 사람이 나중에 고쳐 주기 바란다. 
// 이것을 해결하기 위해서는 
// MEMSTCFG0 의  BEB 필드값을 2 에서 1로 수정한후 nCS0 영역을 한번 읽어 주어야 한다.
inline void falinux_au1200_nand_bug_fix(void)
{
	u32 memstcfg0_val;
	
	memstcfg0_val = au_readl(MEM_STCFG0);
	memstcfg0_val &= (~0x00060000);
	memstcfg0_val |= ( 0x00020000);
	au_writel(memstcfg0_val, MEM_STCFG0 );
	readb( 0xBE000000 ); 
	
	au_sync();
}

//
// au1200 nand 제어시 에러가 발생하여 처리한 코드이다.
// au1250 일 경우 0 으로 한다
// 추후 nand_cs 핀은 gpio 가 아닌 CS1 으로 변경될 예정이다.(2009-02-12)
#define AU1200_NAND_ACCESS_BUGFIX       0

#if (AU1200_NAND_ACCESS_BUGFIX)
  static unsigned long __flags;
  static unsigned int  __enter = 0;
#endif

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

	switch (chipnr) 
	{
	case 0:

#if (AU1200_NAND_ACCESS_BUGFIX)
		if ( __enter ) return;
		__enter	= 1;
		local_irq_save( __flags );		
		au_sync();
		falinux_au1200_nand_bug_fix();		
#endif
		
		au_writel((1<<7), SYS_OUTPUTCLR);	au_sync();
		break;

	default :
		au_writel((1<<7), SYS_OUTPUTSET);	au_sync();

#if (AU1200_NAND_ACCESS_BUGFIX)
		__enter	= 0;
		falinux_au1200_nand_bug_fix();		
		local_irq_restore(__flags );
#endif		
		break;
	}
}

/**
 * nand_wait - [DEFAULT]  wait until the command is done
 * @mtd:	MTD device structure
 * @chip:	NAND chip structure
 *
 * Wait for command done. This applies to erase and program only
 * Erase can take up to 400ms and program up to 20ms according to
 * general NAND and SmartMedia specs
 */
static int nand_wait(struct mtd_info *mtd, struct nand_chip *chip)
{

	unsigned long timeo = jiffies;
	int status, state = chip->state;

	if (state == FL_ERASING)
		timeo += (HZ * 400) / 1000;
	else
		timeo += (HZ * 20) / 1000;

	/* Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine. */
	ndelay(100);

	if ((state == FL_ERASING) && (chip->options & NAND_IS_AND))
		chip->cmdfunc(mtd, NAND_CMD_STATUS_MULTI, -1, -1);
	else
		chip->cmdfunc(mtd, NAND_CMD_STATUS, -1, -1);

#if (AU1200_NAND_ACCESS_BUGFIX)
	if ( __enter )
	{
		local_irq_restore(__flags );
		__enter	= 0;
	}
#endif

	while (time_before(jiffies, timeo)) {
		
		if (chip->dev_ready) {
			if (chip->dev_ready(mtd))
				break;
		} else {
			if (chip->read_byte(mtd) & NAND_STATUS_READY)
				break;
		}
		cond_resched();
	}

#if (AU1200_NAND_ACCESS_BUGFIX)
	if ( 0 == __enter )
	{
		local_irq_save( __flags );		
		__enter	= 1;
		falinux_au1200_nand_bug_fix();
	}
#endif

	status = (int)chip->read_byte(mtd);
	return status;
}

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
	ret = au_readb(this->IO_ADDR_R);
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
		au_writeb(buf[i], this->IO_ADDR_W);
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
		buf[i] = au_readb(this->IO_ADDR_R);
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
		if (buf[i] != au_readb(this->IO_ADDR_R))
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

static void au1200_hwcontrol(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct nand_chip* this = mtd->priv;


	if ( cmd != NAND_CMD_NONE )
	{
		if      ( ctrl & NAND_CLE ) { writeb( cmd & 0xFF, this->IO_ADDR_W - MEM_STNAND_DATA + MEM_STNAND_CMD  ); au_sync(); } 
		else if ( ctrl & NAND_ALE ) { writeb( cmd & 0xFF, this->IO_ADDR_W - MEM_STNAND_DATA + MEM_STNAND_ADDR ); au_sync(); } 
	}
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

	falinux_au1200_nand_bug_fix();

	this->IO_ADDR_R = p_nand + MEM_STNAND_DATA;
	this->IO_ADDR_W = p_nand + MEM_STNAND_DATA;

	this->cmd_ctrl	= au1200_hwcontrol;
	this->dev_ready	= au1200_device_ready;

	/* 30 us command delay time */
	this->chip_delay = 50;  // @FALINUX
	this->ecc.mode = NAND_ECC_SOFT;

	this->options = NAND_NO_AUTOINCR;
	
	// [FG]
	this->select_chip = falinux_nand_select_chip;	
	this->waitfunc    = nand_wait;

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
	add_mtd_partitions( au1200_mtd, ezboot_partition_info, ezboot_part_nr );
	
	return 0;

 outio:
	iounmap((void *)p_nand);

 //outmem:
	kfree(au1200_mtd);
	return retval;
}

module_init(au1xxx_nand_init);

/*
 * Clean up routine
 */
static void __exit au1200_cleanup(void)
{
	//struct nand_chip *this = (struct nand_chip *)&au1200_mtd[1];

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
