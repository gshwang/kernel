/******************************************************************************
 * drivers/ide/arm/ide-ep93xx.c
 *
 * Support for IDE PIO
 * Version 1.0 for EP93XX-E1
 *
 * Copyright (C) 2005  Cirrus Logic
 *
 * A large portion of this file is based on the ide-io.c
 * and the respective pmac version of it
 *
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
 *
 ****************************************************************************/
#include <linux/autoconf.h>
#include <linux/ide.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include <asm/ide.h>
#include <asm/irq.h>
#include <asm/arch/ide.h>
#include <asm/arch/dma.h>
#include <asm/hardware.h>

extern void ep93xx_dma_init(ide_hwif_t *hwif);

/*****************************************************************************
 *
 *  functions to set up the IDE control register and data register to read
 *  or write a byte of data to/from the specified IDE device register.
 *
 ****************************************************************************/
static void
ep93xx_ide_outb(u8 b, unsigned long addr)
{
	unsigned int uiIDECR;
	
	/*
	 * Write the address out.
	 */
	uiIDECR = IDECtrl_DIORn | IDECtrl_DIOWn | ((addr & 7) << 2) |
		  (addr >> 10);
	outl(uiIDECR, IDECR);

	/*
	 * Write the data out.
	 */
	outl(b, IDEDATAOUT);

	/*
	 * Toggle the write signal.
	 */
	outl(uiIDECR & ~IDECtrl_DIOWn, IDECR);

	outl(uiIDECR, IDECR);
}

static void
ep93xx_ide_outbsync(ide_drive_t *drive, u8 b, unsigned long addr)
{
	unsigned int uiIDECR;
	/*
	 * Write the address out.
	 */
	uiIDECR = IDECtrl_DIORn | IDECtrl_DIOWn | ((addr & 7) << 2) |
		  (addr >> 10);
	outl(uiIDECR, IDECR);

	/*
	 * Write the data out.p
	 */
	outl(b, IDEDATAOUT);
	/*
	 * Toggle the write signal.
	 */
	outl(uiIDECR & ~IDECtrl_DIOWn, IDECR);
	outl(uiIDECR, IDECR);
}

static unsigned char
ep93xx_ide_inb(unsigned long addr)
{
	unsigned int uiIDECR;

	/*
	 * Write the address out.
	 */
	uiIDECR = IDECtrl_DIORn | IDECtrl_DIOWn | ((addr & 7) << 2) |
		  (addr >> 10);
	outl(uiIDECR, IDECR);

	/*
	 * Toggle the read signal.
	 */
	outl(uiIDECR & ~IDECtrl_DIORn, IDECR);
	outl(uiIDECR, IDECR);

	/*
	 * Read the data in.
	 */
	return(inl(IDEDATAIN) & 0xff);
}

/*****************************************************************************
 *
 *  functions to set up the IDE control register and data restister to read
 *  or write 16 bits of data to/from the specified IDE device register.
 *  These functions should only be used when reading/writing data to/from
 *  the data register.
 *
 ****************************************************************************/
static void
ep93xx_ide_outw(u16 w, unsigned long addr)
{
	unsigned int uiIDECR;

	/*
	 * Write the address out.
	 */
	uiIDECR = IDECtrl_DIORn | IDECtrl_DIOWn | ((addr & 7) << 2) |
		  (addr >> 10);
	outl(uiIDECR, IDECR);

	/*
	 * Write the data out.
	 */
	outl(w, IDEDATAOUT);

	/*
	 * Toggle the write signal.
	 */
	outl(uiIDECR & ~IDECtrl_DIOWn, IDECR);	
	outl(uiIDECR, IDECR);
}

static u16
ep93xx_ide_inw(unsigned long addr)
{
	unsigned int uiIDECR;

	/*
	 * Write the address out.
	 */
	uiIDECR = IDECtrl_DIORn | IDECtrl_DIOWn | ((addr & 7) << 2) |
		  (addr >> 10);
	outl(uiIDECR, IDECR);

	/*
	 * Toggle the read signal.
	 */
	outl(uiIDECR & ~IDECtrl_DIORn, IDECR);
	outl(uiIDECR, IDECR);

	/*
	 * Read the data in.
	 */
	return(inl(IDEDATAIN) & 0xffff);
}

/*****************************************************************************
 *
 *  functions to read/write a block of data to/from the ide device using
 *  PIO mode.
 *
 ****************************************************************************/
static void
ep93xx_ide_insw(unsigned long addr, void *buf, u32 count)
{
	unsigned short *data = (unsigned short *)buf;

	/*
	 * Read in data from the data register 16 bits at a time.
	 */
	for (; count; count--)
		*data++ = ep93xx_ide_inw(addr);
}

static void
ep93xx_ide_outsw(unsigned long addr, void *buf, u32 count)
{
	unsigned short *data = (unsigned short *)buf;

	/*
	 * Write out data to the data register 16 bits at a time.
	 */
	for (; count; count--)
		ep93xx_ide_outw(*data++, addr);
}

static void
ep93xx_ata_input_data(ide_drive_t *drive, void *buffer, u32 count)
{
	/*
	 * Read in the specified number of half words from the ide interface.
	 */
	ep93xx_ide_insw(IDE_DATA_REG, buffer, count << 1);
}

static void
ep93xx_ata_output_data(ide_drive_t *drive, void *buffer, u32 count)
{
	/*
	 * write the specified number of half words from the ide interface
	 * to the ide device.
	 */
	ep93xx_ide_outsw(IDE_DATA_REG, buffer, count << 1);
}

static void
ep93xx_atapi_input_bytes(ide_drive_t *drive, void *buffer, u32 count)
{
	/*
	 * read in the specified number of bytes from the ide interface.
	 */
	ep93xx_ide_insw(IDE_DATA_REG, buffer, (count >> 1) + (count & 1));
}

static void
ep93xx_atapi_output_bytes(ide_drive_t *drive, void *buffer, u32 count)
{
	/*
	 * Write the specified number of bytes from the ide interface
	 * to the ide device.
	 */
	ep93xx_ide_outsw(IDE_DATA_REG, buffer, (count >> 1) + (count & 1));
}




void
ep93xx_ide_init(struct hwif_s *hwif)
{
      
    u32 uiTemp;         
	      
	/*
	 * Make sure the GPIO on IDE bits in the DEVCFG register are not set.
	 */
	uiTemp = inl(EP93XX_SYSCON_DEVICE_CONFIG) & ~(SYSCON_DEVCFG_EonIDE |
					SYSCON_DEVCFG_GonIDE |
					SYSCON_DEVCFG_HonIDE);
	SysconSetLocked( EP93XX_SYSCON_DEVICE_CONFIG, uiTemp );

	/*
	 * Insure that neither device is selected. -wlg
	 */
        outl(inl(IDECR) | (IDECtrl_CS0n | IDECtrl_CS1n), IDECR);

	/*
	 * Make sure that MWDMA and UDMA are disabled.
	 */
	outl(0, IDEMDMAOP);
	outl(0, IDEUDMAOP);

	/*
	 * Set up the IDE interface for PIO transfers, using the default PIO
	 * mode.
	 */
	outl(IDECfg_IDEEN | IDECfg_PIO | (4 << IDECfg_MODE_SHIFT) |
	     (1 << IDECfg_WST_SHIFT), IDECFG);

	/*
	 * Setup the ports.
	 */
	ide_init_hwif_ports(&(hwif->hw), 0x800, 0x406, NULL);
	      
	/*
	 *  Set up the HW interface function pointers with the ep93xx specific
	 *  function.
	 */
	hwif->ata_input_data = ep93xx_ata_input_data;
	hwif->ata_output_data = ep93xx_ata_output_data;
	hwif->atapi_input_bytes = ep93xx_atapi_input_bytes;
	hwif->atapi_output_bytes = ep93xx_atapi_output_bytes;

	hwif->OUTB = ep93xx_ide_outb;
	hwif->OUTBSYNC = ep93xx_ide_outbsync;
	hwif->OUTW = ep93xx_ide_outw;
	hwif->OUTSW = ep93xx_ide_outsw;

	hwif->INB = ep93xx_ide_inb;
	hwif->INW = ep93xx_ide_inw;
	hwif->INSW = ep93xx_ide_insw;

#ifdef CONFIG_BLK_DEV_IDE_DMA_EP93XX
	ep93xx_dma_init(hwif);
#endif

}



