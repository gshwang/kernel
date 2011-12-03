/*======================================================================
 
   drivers/mtd/nand/ep93xx_nand.c: EDB93xx NAND flash driver
 
   Copyright (C) 2006 Cirrus Logic, Inc.
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 
======================================================================*/
#include <linux/autoconf.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <asm/sizes.h>


/*
 *  There is a jumper select the ALE connect to A24 or EGPIO4.
 *  If ALE is connected to A24, define NAND_ALE_ON_A24.
 *  If ALE is connected to EGPIO4, un-define NAND_ALE_ON_A24.
 */
#undef NAND_ALE_ON_A24

/*
 *  There is a jumper select the CLE connect to A25 or EGPIO5.
 *  If CLE is connected to A25, define NAND_CLE_ON_A25.
 *  If CLE is connected to EGPIO5, un-define NAND_CLE_ON_A25.
 */
#undef NAND_CLE_ON_A25

/*
 *  There is a jumper select the CE connect to CS3 or EGPIO8.
 *  If CE is connected to CS3, define NAND_CE_DONNT_CARE.
 *  If CE is connected to EGPIO8, un-define NAND_CE_DONNT_CARE.
 *  
 *  NOTICE: CE DON'T CARE mode doesn't work for all nand chips.
 */
#undef NAND_CE_DONNT_CARE

/*
 *  There is a jumper select the R/B connect to /WAIT or EGPIO9.
 *  If CE is connected to /WAIT, define NAND_RB_ON_WAIT.
 *  If CE is connected to EGPIO9, un-define NAND_RB_ON_WAIT.
 *  
 */
#undef NAND_RB_ON_WAIT


static struct mtd_info *ep93xx_mtd = NULL;

/*
 *  CS3 is used for NAND flash.
 */
#define EP93xx_FIO_PBASE 0x30000000


#ifdef NAND_ALE_ON_A24

    static int ale_status = 0;
    static int ale_address_offset = 0;

#endif

#ifdef NAND_CLE_ON_A25

    static int cle_status = 0;
    static int cle_address_offset = 0;
#endif


static unsigned long ep93xx_fio_pbase = EP93xx_FIO_PBASE;
/*
#ifdef MODULE
MODULE_PARM(ep93xx_fio_pbase, "i");

__setup("ep93xx_fio_pbase=",ep93xx_fio_pbase);
#endif
*/

#ifdef CONFIG_MTD_PARTITIONS

#define BOOT_PARTITION_SIZE         (0x800000)
/*
 * Define static partitions for flash device
 */
static struct mtd_partition partition_info[] =
{
    {
        .name = "EP93xx Nand Flash Boot Partition",
        .offset = 0,
        .size = BOOT_PARTITION_SIZE
    },
    {
        .name = "EP93xx Nand Flash",
        .offset = BOOT_PARTITION_SIZE,
        .size = 32*1024*1024
    }
};

#define NUM_PARTITIONS 2

#endif

int EGpio_out (int line, int value)
{
    unsigned long uTmp;
    unsigned long PDR = GPIO_PADR;
    unsigned long PDDR = GPIO_PADDR;

    if(line&0xff00)
    {
        PDR = GPIO_PBDR;
        PDDR = GPIO_PBDDR;
        line = line>>8;
    }

    uTmp = inl(PDDR);
    if( ( uTmp & line) == 0)
    {
        outl(uTmp|line, PDDR);
    }


    uTmp = inl(PDR);
    if(value)
        outl(uTmp|line, PDR);
    else
        outl(uTmp&(~line), PDR);

    return value;
}

int EGpio_in (int line)
{
    unsigned long uTmp;
    unsigned long PDR = GPIO_PADR;
    unsigned long PDDR = GPIO_PADDR;

    if(line&0xff00)
    {
        PDR = GPIO_PBDR;
        PDDR = GPIO_PBDDR;
        line = line>>8;
    }

    uTmp = inl(PDDR);
    if( uTmp & line )
    {
        outl(uTmp&(~line), PDDR);
    }

    return (inl(PDR) & line)?1:0;
}

#ifndef NAND_RB_ON_WAIT
int ep93xx_device_ready(struct mtd_info *mtd)
{
    return EGpio_in(1<<9);
}
#endif

static void ep93xx_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
    int i;
    struct nand_chip *this = mtd->priv;
    
    /*printk("readbuf of %d bytes: \n", len);*/
    
    for (i=0; i<len; i++)
        buf[i] = readb(this->IO_ADDR_R);
}

#if defined(NAND_CLE_ON_A25) || defined(NAND_ALE_ON_A24)

static void ep93xx_nand_write_byte(struct mtd_info *mtd, u_char byte)
{
    struct nand_chip *this = mtd->priv;

#ifdef NAND_ALE_ON_A24
    if(ale_status)
    {
        writeb(byte, ale_address_offset);
        return;
    }
#endif

#ifdef NAND_CLE_ON_A25
    if(cle_status)
    {
        writeb(byte, cle_address_offset);
        return;
    }
#endif
    writeb(byte, this->IO_ADDR_W);
}

#endif



/* Select the chip by setting nCE to low */
#define NAND_CTL_SETNCE         1
/* Deselect the chip by setting nCE to high */
#define NAND_CTL_CLRNCE         2
/* Select the command latch by setting CLE to high */
#define NAND_CTL_SETCLE         3
/* Deselect the command latch by setting CLE to low */
#define NAND_CTL_CLRCLE         4
/* Select the address latch by setting ALE to high */
#define NAND_CTL_SETALE         5
/* Deselect the address latch by setting ALE to low */
#define NAND_CTL_CLRALE         6


/*
 *      hardware specific access to control-lines
 */
static void ep93xx_hwcontrol(struct mtd_info *mtd, int cmd)
{
    switch(cmd)
    {

    case NAND_CTL_SETCLE:
#ifdef NAND_CLE_ON_A25

        cle_status = 1;
#else

        EGpio_out(1<<5, 1);
#endif

        break;
    case NAND_CTL_CLRCLE:
#ifdef NAND_CLE_ON_A25

        cle_status = 0;
#else

        EGpio_out(1<<5, 0);
#endif

        break;

    case NAND_CTL_SETALE:
#ifdef NAND_ALE_ON_A24

        ale_status = 1;
#else

        EGpio_out(1<<4, 1);
#endif

        break;
    case NAND_CTL_CLRALE:
#ifdef NAND_ALE_ON_A24

        ale_status = 0;
#else

        EGpio_out(1<<4, 0);
#endif

        break;

    case NAND_CTL_SETNCE:
#ifndef NAND_CE_DONNT_CARE

        EGpio_out(1<<8, 0);
#endif

        break;
    case NAND_CTL_CLRNCE:
#ifndef NAND_CE_DONNT_CARE

        EGpio_out(1<<8, 1);
#endif

        break;
    }
}


/*
 *	hardware specific access to control-lines
 *
 *	NAND_NCE: bit 0 -> bit 7
 *	NAND_CLE: bit 1 -> bit 4
 *	NAND_ALE: bit 2 -> bit 5
 */
static void ep93xx_cmd_ctrl(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct nand_chip *chip = mtd->priv;

        if (ctrl & NAND_CTRL_CHANGE) {
                /*----------------------------*/
                /*SET the control line to high*/
                /*this->write_byte*/
                /*----------------------------*/
                if((ctrl & NAND_NCE) == NAND_NCE){
                /*set the NCE hight*/
                        //printk("set nce high\n");
                        ep93xx_hwcontrol(mtd, NAND_CTL_SETNCE);
                }
                else{
                        ep93xx_hwcontrol(mtd, NAND_CTL_CLRNCE);
                }

                if((ctrl & NAND_CLE) == NAND_CLE){
                /*set the CLE hight*/
                        //printk("set cle high\n");
                        ep93xx_hwcontrol(mtd, NAND_CTL_SETCLE);
                }
                else{
                        ep93xx_hwcontrol(mtd, NAND_CTL_CLRCLE);

                }

                if((ctrl & NAND_ALE) == NAND_ALE){
                /*set the ALE hight*/
                        //printk("set ale high\n");
                        ep93xx_hwcontrol(mtd, NAND_CTL_SETALE);

                }
                else{
                        ep93xx_hwcontrol(mtd, NAND_CTL_CLRALE);
                }

        }

        if(cmd != NAND_CMD_NONE){
                //printk("nand cmd none\n");
                #if defined(NAND_CLE_ON_A25) || defined(NAND_ALE_ON_A24)
                        ep93xx_nand_write_byte(mtd,(unsigned char)(cmd & 0xFF));
                #else
                        writeb((unsigned char)(cmd & 0xFF), chip->IO_ADDR_W);
                                //ep93xx_nand_write_byte(mtd,(unsigned char)(cmd & 0xFF));
                #endif
                //printk("write byte exit\n");
        }

	
}

#ifdef CONFIG_MTD_PARTITIONS
const char *part_probes[] =
    { "cmdlinepart", NULL
    };
#endif

/*
 * Main initialization routine
 */
static int __init ep93xx_init (void)
{
    int ret=0;
    struct nand_chip *chip;
    const char *part_type = 0;
    int mtd_parts_nb = 0;
    struct mtd_partition *mtd_parts = 0;
    void __iomem * ep93xx_fio_base = 0;

    /* Allocate memory for MTD device structure and private data */
    ep93xx_mtd = kmalloc(sizeof(struct mtd_info) +
                         sizeof(struct nand_chip),
                         GFP_KERNEL);
    if (!ep93xx_mtd)
    {
        printk("Unable to allocate EDB93xx NAND MTD device structure.\n");
        ret = -ENOMEM;
        goto fail_return;
    }

    /* map physical adress */
    ep93xx_fio_base = ioremap(ep93xx_fio_pbase, SZ_1K);
    if(!ep93xx_fio_base)
    {
        printk("ioremap EDB93xx NAND flash failed\n");
        ret = -EIO;
        goto fail_return;
    }
    
#ifdef NAND_ALE_ON_A24
    ale_address_offset = ioremap(ep93xx_fio_pbase | 0x1000000, SZ_1K);
    if(!ale_address_offset)
    {
        printk("ioremap EDB93xx NAND flash failed\n");
        ret = -EIO;
        goto fail_return;
    }
#endif

#ifdef NAND_CLE_ON_A25

    cle_address_offset = ioremap(ep93xx_fio_pbase | 0x2000000, SZ_1K);
    if(!cle_address_offset)
    {
        printk("ioremap EDB93xx NAND flash failed\n");
        ret = -EIO;
        goto fail_return;
    }

#endif

    /* Get pointer to private data */
    chip = (struct nand_chip *) (&ep93xx_mtd[1]);

    /* Initialize structures */
    memset((char *) ep93xx_mtd, 0, sizeof(struct mtd_info));
    memset((char *) chip, 0, sizeof(struct nand_chip));

    /* Link the private data with the MTD structure */
    ep93xx_mtd->priv = chip;
    ep93xx_mtd->owner = THIS_MODULE;
    /* insert callbacks */
    chip->IO_ADDR_R = ep93xx_fio_base;
    chip->IO_ADDR_W = ep93xx_fio_base;
    chip->cmd_ctrl = ep93xx_cmd_ctrl;
    /*chip->hwcontrol = ep93xx_hwcontrol; shrek change it for 2.6.20.4*/

#ifdef NAND_RB_ON_WAIT
    chip->dev_ready = NULL;
#else
    chip->dev_ready = ep93xx_device_ready;
#endif
    chip->read_buf = ep93xx_read_buf;
    /* 15 us command delay time */
    chip->chip_delay = 100;
    /*chip->eccmode = NAND_ECC_SOFT;  shrek change it for 2.6.20.4*/
    chip->ecc.mode = NAND_ECC_SOFT;
    
/*
#if defined(NAND_CLE_ON_A25) || defined(NAND_ALE_ON_A24)
    chip->write_byte = ep93xx_nand_write_byte;
#endif
*/

    /* Scan to find existence of the device */
    printk("Scan the NAND device for EDB93xx.\n");
    if (nand_scan (ep93xx_mtd, 1))
    {
        ret = -ENXIO;
        goto fail_return;
    }

    /* Allocate memory for internal data buffer */
    /*
    chip->data_buf = kmalloc (sizeof(u_char) * (ep93xx_mtd->oobblock + ep93xx_mtd->oobsize), GFP_KERNEL);
    if (!chip->data_buf)
    {
        ret = -ENOMEM;
        goto fail_return;
    }
    */					/*shrek change it for 2.6.20*/
#ifdef CONFIG_MTD_PARTITIONS
    ep93xx_mtd->name = "ep93xx_nand";
    mtd_parts_nb = parse_mtd_partitions(ep93xx_mtd, part_probes,
                                        &mtd_parts, 0);
    if (mtd_parts_nb > 0)
        part_type = "command line";
    else
        mtd_parts_nb = 0;
#endif

    if (mtd_parts_nb == 0)
    {
        mtd_parts = partition_info;
        mtd_parts_nb = NUM_PARTITIONS;
        part_type = "static";
    }

    /* Register the partitions */
    printk(KERN_NOTICE "Using %s partition definition\n", part_type);
    add_mtd_partitions(ep93xx_mtd, mtd_parts, mtd_parts_nb);

    /* Return happy */
    return ret;

fail_return:

#ifdef NAND_ALE_ON_A24
    if(ale_address_offset)
        iounmap((void *)ale_address_offset);
#endif

#ifdef NAND_CLE_ON_A25
    if(cle_address_offset)
        iounmap((void *)cle_address_offset);
#endif
    
    if(ep93xx_fio_base)
        iounmap((void *)ep93xx_fio_base);

    if (ep93xx_mtd)
        kfree (ep93xx_mtd);

    return ret;
}
module_init(ep93xx_init);

/*
 * Clean up routine
 */
static void __exit ep93xx_cleanup (void)
{
    /*struct nand_chip *chip = (struct nand_chip *) &ep93xx_mtd[1];*/	/*shrek change it for 2.6.20*/

    /* Unregister the device */
    nand_release(ep93xx_mtd);

    /* Free internal data buffer */
    /*kfree(chip->data_buf);*/		/*shrek change it for 2.6.20*/

    /* Free the MTD device structure */
    kfree(ep93xx_mtd);
}
module_exit(ep93xx_cleanup);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MTD map driver for EDB93xx NAND daughter board");
