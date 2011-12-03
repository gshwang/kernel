/******************************************************************************
 * drivers/ide/arm/ide-dma-ep93xx.c
 *
 * Support for IDE UDMA
 * Version 1.0 for EP93XX-E1
 *
 * Copyright (C) 2005  Cirrus Logic
 *
 * A large portion of this file is based on the ide-dma.c
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

//#define EP93XX_IDE_DMA_DEBUG
//#define DEBUG_VADDR

#ifdef EP93XX_IDE_DMA_DEBUG
#define DPRINTK( fmt, arg... )  printk( fmt, ##arg )
#else
#define DPRINTK( fmt, arg... )
#endif

static void
ep93xx_set_pio(void);


#define ep93xx_ide_dma_intr 0
/*
//this functions comes from PCI PC land
ide_startstop_t ide_dma_intr (ide_drive_t *drive)
{
	u8 stat = 0, dma_stat = 0;
	
	DPRINTK("%s\n", __FUNCTION__);

	dma_stat = HWIF(drive)->ide_dma_end(drive);
	stat = HWIF(drive)->INB(IDE_STATUS_REG);	//get drive status 
	if (OK_STAT(stat,DRIVE_READY,drive->bad_wstat|DRQ_STAT)) {
		if (!dma_stat) {
			struct request *rq = HWGROUP(drive)->rq;

			DRIVER(drive)->end_request(drive, 1, rq->nr_sectors);
			return ide_stopped;
		}
		printk(KERN_ERR "%s: dma_intr: bad DMA status (dma_stat=%x)\n", 
		       drive->name, dma_stat);
	}
	return DRIVER(drive)->error(drive, "dma_intr", stat);
}
*/

/*****************************************************************************
 *
 * ep93xx_config_ide_device()
 *
 * This function sets up the ep93xx ide device for a dma transfer by first
 * probing to find the best dma mode supported by the device.
 *
 * Returns a 0 for success, and a 1 otherwise.
 *
 ****************************************************************************/
static unsigned int
ep93xx_config_ide_device(ide_drive_t *drive)
{
        unsigned int   ulChipID;
	byte transfer = 0;

	DPRINTK("%s: ep93xx_config_ide_device\n", drive->name);

	/*
	 * Determine the best transfer speed supported.  On Rev D1/E0
     * the maximum DMA mode is 2.  On Rev E1 the maximum UDMA mode is 3.
	 */
	transfer = ide_dma_speed(drive, 1); //mode1=udma0..2, mode2=udma2..4
  	
	/*
	 * Do nothing if a DMA mode is not supported or if the drive supports
         * MDMA.
	 */
	if(transfer == XFER_MW_DMA_2 || transfer == XFER_MW_DMA_1 || 
		transfer == XFER_MW_DMA_0 || transfer == 0) {
printk("	device only supports MDMA ? (we're hosed)\n");
                return 1;  
	}

	ulChipID = inl(SYSCON_CHIPID);
	if(transfer == XFER_UDMA_3 && ((ulChipID &  SYSCON_CHIPID_REV_MASK)>>SYSCON_CHIPID_REV_SHIFT) != 0x6)
	{
		transfer = XFER_UDMA_2; 
        }

	DPRINTK("configuring the HDD for this transfer: ");
	/*
	 * Configure the drive.
	 */
	if (ide_config_drive_speed(drive, transfer) == 0) {
		/*
		 * Hold on to this value for use later.
		 */
printk("	device configured for speed X%d\n", transfer ); 
		drive->current_speed = transfer;

		/*
		 * Success, so turn on DMA.
		 */
		return HWIF(drive)->ide_dma_on(drive);
	} 
	else
		return 1;  
}




static int g_prd_count=0;
static int g_pwr_count=0;



//from include/asm/dma-mapping.h
//static inline int
//dma_map_sg(struct device *dev, struct scatterlist *sg, int nents,
//	   enum dma_data_direction dir)

//#define MAP_SG(a,b,c,d)		dma_map_sg( (struct device *) (a), (b), (c), (d) )
//#define UNMAP_SG(a,b,c,d)	dma_unmap_sg( (struct device *) (a), (b), (c), (d) )

#define MAP_SG(a,b,c,d)		dma_map_sg( NULL, (b), (c), (d) )
#define UNMAP_SG(a,b,c,d)	dma_unmap_sg( NULL, (b), (c), (d) )

/*
 * Needed for allowing full modular support of ide-driver
 */
static int ep93xx_ide_release_dma_engine (ide_hwif_t *hwif)
{
	if (hwif->dmatable_cpu) 
	{
	    kfree(hwif->dmatable_cpu);
	    hwif->dmatable_cpu = NULL;
	}
	if (hwif->sg_table) 
	{
	    kfree(hwif->sg_table);
	    hwif->sg_table = NULL;
	}


	return 1;
}


int ep93xx_ide_allocate_dma_engine (ide_hwif_t *hwif)
{
	
	/*
	 * Allocate memory for the DMA table.
	 */
	hwif->dmatable_cpu = kmalloc(PRD_ENTRIES * PRD_BYTES, GFP_KERNEL);

	/*
	 * Check if we allocated memory for dma
	 */
	if (hwif->dmatable_cpu == NULL) 
	{
		printk("%s: SG-DMA disabled, UNABLE TO ALLOCATE DMA TABLES\n", hwif->name);
		return 1;
	}

	/*
	 * Allocate memory for the scatterlist structures.
	 */
	hwif->sg_table = kmalloc(sizeof(struct scatterlist) * PRD_ENTRIES, GFP_KERNEL);

	/*
	 * Check if we allocated the memory we expected to.
	 */
	if (hwif->sg_table == NULL) 
	{
		/*
		 *  Fail, so clean up.
		 */
		kfree(hwif->dmatable_cpu );
		printk("%s: SG-DMA disabled, UNABLE TO ALLOCATE DMA TABLES\n", hwif->name);
		return 1;
	}

    return 0;
}


/**
 *	config_drive_for_dma	-	attempt to activate IDE DMA
 *	@drive: the drive to place in DMA mode
 *
 *	If the drive supports at least mode 2 DMA or UDMA of any kind
 *	then attempt to place it into DMA mode. Drives that are known to
 *	support DMA but predate the DMA properties or that are known
 *	to have DMA handling bugs are also set up appropriately based
 *	on the good/bad drive lists.
 */
 
static int config_drive_for_dma (ide_drive_t *drive)
{
	struct hd_driveid *id = drive->id;
	ide_hwif_t *hwif = HWIF(drive);

        DPRINTK("%s\n", __FUNCTION__ );

	if ((id->capability & 1) && hwif->autodma) 
	{
		/* Consult the list of known "bad" drives */
		if (__ide_dma_bad_drive(drive))
			return __ide_dma_off(drive);

		DPRINTK("	drive seems ok\n");

#ifdef EP93XX_IDE_DMA_DEBUG
		if (id->field_valid & 4)
		{
		    printk("	supports UDMA : 0x%08x\n", id->dma_ultra );
		}
#endif

		/*
		 * Enable DMA on any drive that has
		 * UltraDMA (mode 0/1/2/3/4/5/6) enabled
		 * ep93xx supports up to udma2 
		 */
		 
		if ( (id->field_valid & 4) && ( id->dma_ultra & 0x7f) )
		{
			DPRINTK("enabling UDMA\n");
			return hwif->ide_dma_on(drive);
		}

		// ep93xx can't do mdma/sdma
	}

	return hwif->ide_dma_off_quietly(drive);
}



/**
 *	ep93xx_ide_dma_check		-	check DMA setup
 *	@drive: drive to check
 *
 */
 
static int ep93xx_ide_dma_check (ide_drive_t *drive)
{
        DPRINTK("%s\n", __FUNCTION__ );

	config_drive_for_dma(drive);
	return ep93xx_config_ide_device(drive);
}


/**
 *	ide_build_sglist	-	map IDE scatter gather for DMA I/O
 *	@drive: the drive to build the DMA table for
 *	@rq: the request holding the sg list
 *
 *	Perform the PCI mapping magic necessary to access the source or
 *	target buffers of a request via PCI DMA. The lower layers of the
 *	kernel provide the necessary cache management so that we can
 *	operate in a portable fashion
 */

static int ide_build_sglist(ide_drive_t *drive, struct request *rq)
{
	ide_hwif_t *hwif = HWIF(drive);
	struct scatterlist *sg = hwif->sg_table;
	int nents;
#ifdef DEBUG_VADDR
	u32 vaddr;
	vaddr = (u32) rq->buffer;
	printk("vaddr=0x%08x, paddr=0x%08x\n", (int) vaddr, (int) virt_to_dma(NULL,vaddr) );
#endif	
	//if (hwif->sg_dma_active)
	//	BUG();

	nents = blk_rq_map_sg(drive->queue, rq, sg);
		
	if (rq_data_dir(rq) == READ)
		hwif->sg_dma_direction = PCI_DMA_FROMDEVICE;
	else
		hwif->sg_dma_direction = PCI_DMA_TODEVICE;

	return MAP_SG(hwif->pci_dev, sg, nents, hwif->sg_dma_direction);
}


//comes from PCI PC land
/**
 *	ide_raw_build_sglist	-	map IDE scatter gather for DMA
 *	@drive: the drive to build the DMA table for
 *	@rq: the request holding the sg list
 *
 *	Perform the PCI mapping magic necessary to access the source or
 *	target buffers of a taskfile request via PCI DMA. The lower layers 
 *	of the  kernel provide the necessary cache management so that we can
 *	operate in a portable fashion
 */

static int ide_raw_build_sglist(ide_drive_t *drive, struct request *rq)
{
	ide_hwif_t *hwif = HWIF(drive);
	struct scatterlist *sg = hwif->sg_table;
	int nents = 0;
	ide_task_t *args = rq->special;
	u8 *virt_addr = rq->buffer;
	int sector_count = rq->nr_sectors;
#ifdef DEBUG_VADDR
	u32 vaddr;
	vaddr = (u32) rq->buffer;
	printk("vaddr=0x%08x, paddr=0x%08x\n", (int) vaddr, (int) virt_to_dma(NULL, vaddr) );
#endif	

	if (args->command_type == IDE_DRIVE_TASK_RAW_WRITE)
		hwif->sg_dma_direction = PCI_DMA_TODEVICE;
	else
		hwif->sg_dma_direction = PCI_DMA_FROMDEVICE;

#if 1
	if (sector_count > 256)
		BUG();

	if (sector_count > 128) {
#else
	while (sector_count > 128) {
#endif
		memset(&sg[nents], 0, sizeof(*sg));
		sg[nents].page = virt_to_page(virt_addr);
		sg[nents].offset = offset_in_page(virt_addr);
		sg[nents].length = 128  * SECTOR_SIZE;
		nents++;
		virt_addr = virt_addr + (128 * SECTOR_SIZE);
		sector_count -= 128;
	}
	memset(&sg[nents], 0, sizeof(*sg));
	sg[nents].page = virt_to_page(virt_addr);
	sg[nents].offset = offset_in_page(virt_addr);
	sg[nents].length =  sector_count  * SECTOR_SIZE;
	nents++;

	return MAP_SG(hwif->pci_dev, sg, nents, hwif->sg_dma_direction);
}

//comes from PCI PC land
/**
 *	ide_build_dmatable	-	build IDE DMA table
 *
 *	ide_build_dmatable() prepares a dma request. We map the command
 *	to get the pci bus addresses of the buffers and then build up
 *	the PRD table that the IDE layer wants to be fed. The code
 *	knows about the 64K wrap bug in the CS5530.
 *
 *	Returns 0 if all went okay, returns 1 otherwise.
 *	May also be invoked from trm290.c
 */
 
static int ep93xx_ide_build_dmatable (ide_drive_t *drive, struct request *rq)
{
	ide_hwif_t *hwif	= HWIF(drive);
	unsigned int *table	= hwif->dmatable_cpu;
	unsigned int count = 0;
	int i;
	struct scatterlist *sg;

	if (HWGROUP(drive)->rq->cmd_type & /*REQ_DRIVE_TASKFILE*/REQ_TYPE_ATA_TASKFILE)
		hwif->sg_nents = i = ide_raw_build_sglist(drive, rq);
	else
		hwif->sg_nents = i = ide_build_sglist(drive, rq);

	if (!i)
		return 0;

	sg = hwif->sg_table;
	while (i) {
		u32 cur_addr;
		u32 cur_len;

		cur_addr = sg_dma_address(sg);
		cur_len = sg_dma_len(sg);

		/*
		 * Fill in the dma table, without crossing any 64kB boundaries.
		 * Most hardware requires 16-bit alignment of all blocks,
		 * but the trm290 requires 32-bit alignment.
		 */

		while (cur_len) {
			if (count++ >= PRD_ENTRIES) {
				printk(KERN_ERR "%s: DMA table too small\n", drive->name);
				goto use_pio_instead;
			} else {
				u32 xcount, bcount = 0x10000 - (cur_addr & 0xffff);

				if (bcount > cur_len)
					bcount = cur_len;
				*table++ = cpu_to_le32(cur_addr);
				xcount = bcount & 0xffff;

				if (xcount == 0x0000) {
	/* 
	 * Most chipsets correctly interpret a length of 0x0000 as 64KB,
	 * but at least one (e.g. CS5530) misinterprets it as zero (!).
	 * So here we break the 64KB entry into two 32KB entries instead.
	 */
					if (count++ >= PRD_ENTRIES) {
						printk(KERN_ERR "%s: DMA table too small\n", drive->name);
						goto use_pio_instead;
					}
					*table++ = cpu_to_le32(0x8000);
					*table++ = cpu_to_le32(cur_addr + 0x8000);
					xcount = 0x8000;
				}
				*table++ = cpu_to_le32(xcount);
				cur_addr += bcount;
				cur_len -= bcount;
			}
		}

		sg++;
		i--;
	}

	if (count) {
			*--table |= cpu_to_le32(0x80000000);
		return count;
	}

	printk(KERN_ERR "%s: empty DMA table?\n", drive->name);

use_pio_instead:

	UNMAP_SG(hwif->pci_dev,
		     hwif->sg_table,
		     hwif->sg_nents,
		     hwif->sg_dma_direction);

	//hwif->sg_dma_active = 0;

	return 0; /* revert to PIO for this request */
}


/* Teardown mappings after DMA has completed.  */
static void
ep93xx_ide_destroy_dmatable (ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);
	struct scatterlist *sg;  
	
	sg = hwif->sg_table;

	if (hwif->sg_nents) {
		UNMAP_SG(NULL, sg, hwif->sg_nents, hwif->sg_dma_direction);
		//hwif->sg_dma_active = 0;
	}
}


static int __ide_dma_lostirq (ide_drive_t *drive)
{
	DPRINTK("%s: DMA interrupt recovery\n", drive->name);
	return 1;
}

// from PCI PC land
/**
 *	__ide_dma_on		-	Enable DMA on a device
 *	@drive: drive to enable DMA on
 *
 *	Enable IDE DMA for a device on this IDE controller.
 */
 
int __ide_dma_on (ide_drive_t *drive)
{
        DPRINTK("%s\n", __FUNCTION__ );

	drive->using_dma = 1;	
	ide_toggle_bounce(drive, 1);

	if (HWIF(drive)->ide_dma_host_on(drive))
		return 1;

	return 0;
}


/*				
 * this is supposed to enable the host side (PC) host controller
 * on the ep93xx it is already running, so nothing really happening here
 *
 */
static int
ep93xx_ide_dma_host_on(ide_drive_t *drive)
{
	DPRINTK("%s\n", __FUNCTION__ );

	return 0;
}

static int
ep93xx_ide_dma_host_off (ide_drive_t *drive)
{
	return 0;
}

/*****************************************************************************
 *
 *  ep93xx_rwproc()
 *
 *  Initializes the ep93xx IDE controller interface with the transfer type,
 *  transfer mode, and transfer direction.
 *
 ****************************************************************************/
static void
ep93xx_rwproc(ide_drive_t *drive, int action)
{
	int speed;


        DPRINTK("%s\n", __FUNCTION__ );

	/*
	 * Insure that neither device is selected. -wlg
	 */
	ep93xx_set_pio();


	DPRINTK("rwproc in udma mode (udma starts at 64): %d\n", drive->current_speed);

	/*
	 * Configure the IDE controller for the specified transfer mode.
	 */
	switch (drive->current_speed)
	{
		/*
		 * Configure for an MDMA operation.
		 */
		case XFER_MW_DMA_0:
		case XFER_MW_DMA_1:
		case XFER_MW_DMA_2:
		printk("ep93xx ide dma: BAAAAADDD ! someone tries to use MDMA: not supported !!\n");
		
		break;

		/*
		 * Configure for a UDMA operation.
		 */
		case XFER_UDMA_0:
		case XFER_UDMA_1:
		case XFER_UDMA_2:
		case XFER_UDMA_3:
		case XFER_UDMA_4:
		
		    speed = drive->current_speed;
		    speed -=64; //udma 0
		    if(speed >= 3)
			speed = 3; //max udma 3

		    DPRINTK("rwproc in udma %d, action is %d\n", speed, action);
		    outl( (speed << IDECfg_MODE_SHIFT) | IDECfg_UDMA | IDECfg_IDEEN, IDECFG );

		    //action == 0 for read, action == 1 for write

		    if(action)
			action=0x2; //IDEUDMAOp_RWOP
		    else
			action=0;
		        
		    outl( action , IDEUDMAOP );    
		    outl( action | IDEUDMAOp_UEN, IDEUDMAOP );
		
		break;

		default:
			break;
	}
}


/*****************************************************************************
 *
 * ep93xx_ide_dma_begin()
 *
 * This function initiates a dma transfer.
 *
 ****************************************************************************/
static void
ep93xx_ide_dma_start(ide_drive_t *drive)
{
	int rv;
	ide_hwif_t *hwif = HWIF(drive);
	struct request *rq = HWGROUP(drive)->rq;


	DPRINTK("%s\n", __FUNCTION__ );
	
	/*
	 * Configure the ep93xx ide controller for a dma operation.
	 */
	if (rq_data_dir(rq) == READ)
	    ep93xx_rwproc(drive, 0);
	else
	    ep93xx_rwproc(drive, 1);

	/*
	 * Start the dma transfer.
	 */
	rv=ep93xx_dma_start(hwif->hw.dma, 1, NULL);
	DPRINTK("	starting dma on handle 0x%08x, rv=%d\n", (int)  hwif->hw.dma, rv );
	DPRINTK(" 	DMAMM_0_CONTROL=0x%08x\n", inl(DMAMM_0_CONTROL) );
	//return rv;
}


/*****************************************************************************
 *
 * ep93xx_ide_callback()
 *
 * Registered with the ep93xx dma driver and called at the end of the dma
 * interrupt handler, this function should process the dma buffers.
 *
 ****************************************************************************/
static void
ep93xx_ide_callback(ep93xx_dma_int_t dma_int, ep93xx_dma_dev_t device,
                    unsigned int user_data)
{
	ide_drive_t *drive = (ide_drive_t *)user_data;
	ide_hwif_t *hwif = HWIF(drive);
	unsigned int temp;
	int rv;

	DPRINTK("ep93xx_ide_callback %d\n", dma_int);
	DPRINTK("	IDEUDMARdBufSts=0x%08x\n", inl(IDEUDMARFST) );

#ifdef EP93XX_IDE_DMA_DEBUG

    if( ep93xx_dma_is_done(hwif->hw.dma) == 1 )
    {
	printk("dma is done\n");
    }
    else
    {
	printk("dma is NOT DONE\n");
    }

    printk("DMAMM_0_SAR_CURRENT0  =0x%08x\n", inl(DMAMM_0_SAR_CURRENT0)   );
    printk("DMAMM_0_DAR_CURRENT0  =0x%08x\n", inl(DMAMM_0_DAR_CURRENT0)   );

#endif
	/*
	 * Retrieve from the dma interface as many used buffers as are
	 * available.
	 */
	while (1)
	{
	    rv = ep93xx_dma_remove_buffer(hwif->hw.dma, &temp);
	    if(rv<0)
		break;
	}
	DPRINTK("buffers removed\n");

    DPRINTK("	return from callback\n");
}

/*****************************************************************************
 *
 *  ep93xx_dma_timer_expiry()
 *
 *
 *	dma_timer_expiry	-	handle a DMA timeout
 *	@drive: Drive that timed out
 *
 *	An IDE DMA transfer timed out. In the event of an error we ask
 *	the driver to resolve the problem, if a DMA transfer is still
 *	in progress we continue to wait (arguably we need to add a
 *	secondary 'I dont care what the drive thinks' timeout here)
 *	Finally if we have an interrupt we let it complete the I/O.
 *	But only one time - we clear expiry and if it's still not
 *	completed after WAIT_CMD, we error and retry in PIO.
 *	This can occur if an interrupt is lost or due to hang or bugs.
 *
 *
 ****************************************************************************/
static int
ep93xx_idedma_timer_expiry(ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);
	u8 dev_stat	 = hwif->INB(IDE_ALTSTATUS_REG);
	u8 irq_stat	 = inl(IDECR) & IDECtrl_INTRQ;

	DPRINTK(WARNING "%s: dma_timer_expiry: dev status == 0x%02x,irq= %d\n",
		drive->name, dev_stat, irq_stat);

	/*
	 * Clear the expiry handler in case we decide to wait more,
	 * next time timer expires it is an error
	 */
	HWGROUP(drive)->expiry = NULL;

	/*
	 * If the interrupt is asserted, call the handler.
	 */
	if (irq_stat)
		HWGROUP(drive)->handler(drive);

	/*
	 * Check if the busy bit or the drq bit is set, indicating that
	 * a dma transfer is still active, or the IDE interrupt is asserted.
	 */
	if ( (dev_stat & 0x80) || (dev_stat & 0x08) || irq_stat)
		return WAIT_CMD;

	/*
	 * the device is not busy and the interrupt is not asserted, so check
	 * if there's an error.
	 */
	if (dev_stat & 0x01)	/* ERROR */
		return -1;

	return 0;	/* Unknown status -- reset the bus */
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_setup()
 *
 * This function sets up a dma read/write common operation.
 *
 ****************************************************************************/
static void ep93xx_ide_dma_exec_cmd(ide_drive_t *drive, u8 cmd)
{
	/* issue cmd to drive */
        ide_hwif_t *hwif = HWIF(drive);
        struct request *rq = hwif->hwgroup->rq;
        unsigned int dma_mode;

	DPRINTK("ep93xx_ide_dma_exec_cmd\n");

        if (rq_data_dir(rq) == READ)
                dma_mode = DMA_FROM_DEVICE;
        else
                dma_mode = DMA_TO_DEVICE;
	
	/*
	 * Enable PIO mode on the IDE interface.
	 */
	ep93xx_set_pio();
	
	/*
	 * Send the  command to the device.
	 */
	if(dma_mode == DMA_FROM_DEVICE){
		ide_execute_command(drive, cmd, /*&ep93xx_ide_dma_intr*/ide_dma_intr, 2*WAIT_CMD,
			    &ep93xx_idedma_timer_expiry);
	}
	else{
                ide_execute_command(drive, cmd, /*&ep93xx_ide_dma_intr*/ide_dma_intr, 4*WAIT_CMD,
                            &ep93xx_idedma_timer_expiry);
	}

}

/*****************************************************************************
 *
 * ep93xx_ide_dma_setup()
 *
 * This function sets up a dma read/write common operation.
 *
 ****************************************************************************/
static int ep93xx_ide_dma_setup(ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);
	struct request *rq = hwif->hwgroup->rq;
	unsigned int dma_mode;

	unsigned int flags;
	unsigned int *table	= hwif->dmatable_cpu;

	DPRINTK("ep93xx_ide_dma_setup\n");	
	
	if (rq_data_dir(rq) == READ)
		dma_mode = DMA_FROM_DEVICE;
	else
		dma_mode = DMA_TO_DEVICE;

	/*
	 * Check if we are already transferring on this dma channel.
	 */
	if (/*hwif->sg_dma_active ||*/ drive->waiting_for_dma) {
		DPRINTK("%s: dma_read: dma already active \n", drive->name);
		return 1;
	}


	/*
	 * Indicate that we're waiting for dma.
	 */
	drive->waiting_for_dma = 1;

	if(dma_mode == DMA_FROM_DEVICE){
	/*
	 * Configure DMA M2M channel flags for a source address hold, h/w
	 * initiated P2M transfer.
	 */
		flags = (SOURCE_HOLD | TRANSFER_MODE_HW_P2M);
	}
	else{
	/*
	 * Configure DMA M2M channel flags for a destination address
	 * hold, h/w initiated M2P transfer.
	 */
		flags = (DESTINATION_HOLD | TRANSFER_MODE_HW_M2P);
	}

	if (drive->current_speed & 0x20) 
	{
	    printk("ep93xx_ide_dma_read: been asked to to MDMA: refusing categorically!!!\n");
	} 
	else 
	{
		if(dma_mode == DMA_FROM_DEVICE){
			flags |= (WS_IDE_UDMA_READ << WAIT_STATES_SHIFT);
		       /*
		 	* UDMA data register address.
		 	*/
			hwif->dma_base = IDEUDMADATAIN - IO_BASE_VIRT + IO_BASE_PHYS;
		}
		else{
			flags |= (WS_IDE_UDMA_WRITE << WAIT_STATES_SHIFT);

			/*
		 	* UDMA data register address.
		 	*/
			hwif->dma_base = IDEUDMADATAOUT - IO_BASE_VIRT + IO_BASE_PHYS;
		}
	}

	/*
	 * Configure the dma interface for this IDE operation.
	 */
	if (ep93xx_dma_config(hwif->hw.dma, 0, flags, ep93xx_ide_callback,
			      (unsigned int)drive) != 0) {
		DPRINTK("%s: ep93xx_ide_dma_read: ERROR- dma config failed",
				drive->name);
		drive->waiting_for_dma = 0;
		/*
		 * Fail.
		 */
		return 1;
	}

	/*
	 * Build the table of dma-able buffers.
	 */
	if (!(g_prd_count=ep93xx_ide_build_dmatable(drive, rq)) ) 
	{
		DPRINTK("%s: ep93xx_ide_dma_read: ERROR- failed to build dma table",
			drive->name);
		drive->waiting_for_dma = 0;
		/*
		 * Fail, try PIO instead of DMA
		 */
		return 1;
	}

	DPRINTK("	%d dmas pending\n", g_prd_count);
	/*
	 * Indicate that the scatter gather is active.
	 */
	//hwif->sg_dma_active = 1;

	/*
	 * Prepare the dma interface with some buffers from the
	 * dma_table.
	 */
	do {
		DPRINTK("	add buf: handle=0x%08x, base=0x%08x, dest=0x%08x, len=0x%08x, g_prd_count=%d\n",
			     hwif->hw.dma,
			     hwif->dma_base,
			     table[0],
			     table[1],
			     g_prd_count);

	
		/*
		 * Add a buffer to the dma interface.
		 */
//		printk("rrd b=0x%08x, len=%d\n",  hwif->dmatable_cpu[0], (int) (hwif->dmatable_cpu[1] & 0x0fffffff) );
		if(dma_mode == DMA_FROM_DEVICE){
			if (ep93xx_dma_add_buffer(hwif->hw.dma, hwif->dma_base,
					  table[0],
					  table[1], 0,
					  g_prd_count) != 0){
				break;
			}
		}
		else{
			if (ep93xx_dma_add_buffer(hwif->hw.dma,
					  table[0],
					  hwif->dma_base,
					  table[1], 0,
					  g_pwr_count) != 0){
		    		break;
			}
			
		}
		
		
		table += 2;

		/*
		 * Decrement the count of dmatable entries
		 */
		g_prd_count--;
	} while (g_prd_count);

	return 0;
}


/*****************************************************************************
 *
 *  ep93xx_set_pio()
 *
 *  Configures the ep93xx controller for a PIO mode transfer.
 *
 ****************************************************************************/
static void
ep93xx_set_pio(void)
{
	DPRINTK("ep93xx_set_pio\n");

	/*
	 * Insure that neither device is selected -wlg
	 */
        outl(inl(IDECR) | (IDECtrl_CS0n | IDECtrl_CS1n), IDECR);
	/*
	 * Clear the MDMA and UDMA operation registers.
	 */
	outl(0, IDEMDMAOP);
	outl(0, IDEUDMAOP);

	/*
	 * Enable PIO mode of operation.
	 */
	outl(IDECfg_PIO | IDECfg_IDEEN | (4 << IDECfg_MODE_SHIFT) |
	     (1 << IDECfg_WST_SHIFT), IDECFG);
}


static int
ep93xx_ide_dma_end(ide_drive_t *drive)
{
	ide_hwif_t * hwif = HWIF(drive);
	int rv;
	int i;
	
	rv = 0;

	DPRINTK("%s\n", __FUNCTION__ );
	//mdelay(10);

	while(1)
	{
	/*
	 * See if there is any data left in UDMA FIFOs.  For a read, first wait
	 * until either the DMA is done or the FIFO is empty.  If there is any
	 * residual data in the FIFO, there was an error in the transfer.
	 */
	    i = inl(IDEUDMARFST);
	    if( ep93xx_dma_is_done(hwif->hw.dma) )
		break;
	    if ( (i & 15) == ((i >> 4) & 15) )
		break;
	    udelay(1);
	}
	udelay(10);

	/*
	 * Put the dma interface into pause mode.
	 */
	ep93xx_dma_pause(hwif->hw.dma, 1, 0);
	ep93xx_dma_flush(hwif->hw.dma);

	/*
	 * Enable PIO mode on the IDE interface.
	 */
	ep93xx_set_pio();

	/*
	 * Indicate there's no dma transfer currently in progress.
	 */
	//hwif->sg_dma_active = 0;
	drive->waiting_for_dma = 0;

	DPRINTK("IDEUDMARdBufSts=0x%08x\n", inl(IDEUDMARFST) );

	if ( (i & 15) != ((i >> 4) & 15) )
	{
	    printk("dma_end: udma fifo ptr mismatch !\n");
	    rv=-1;
	    goto end;
	}

	if( ep93xx_dma_is_done(hwif->hw.dma) != 1)
	{
	    printk("dma_end: dma not done !\n");
	    rv=-1;
	    goto end;
	}
//this doesn't really do anthing right now, but might be necessary in the future
	ep93xx_ide_destroy_dmatable(drive); 
//negative value if DMA engine failed	
end:
	return rv;
}


//from PCi PC land
/**
 *	__ide_dma_host_off_quietly	-	Generic DMA kill
 *	@drive: drive to control
 *
 *	Turn off the current DMA on this IDE controller. 
 */

static int ep93xx_ide_dma_off_quietly (ide_drive_t *drive)
{
        DPRINTK("%s\n", __FUNCTION__ );

	drive->using_dma = 0;
	ide_toggle_bounce(drive, 0);

	if (HWIF(drive)->ide_dma_host_off(drive))
		return 1;

	return 0;
}


/*****************************************************************************
 *
 * ep93xx_ide_dma_bad_timeout()
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_timeout(ide_drive_t *drive)
{
	DPRINTK("%s: ep93xx_ide_dma_timeout\n", drive->name);

	printk("	IDEUDMARdBufSts=0x%08x\n", inl(IDEUDMARFST) );
	printk("	DMAMM_0_SAR_CURRENT0  =0x%08x\n", inl(DMAMM_0_SAR_CURRENT0)   );
	printk("	DMAMM_0_DAR_CURRENT0  =0x%08x\n", inl(DMAMM_0_DAR_CURRENT0)   );

	if (HWIF(drive)->ide_dma_test_irq(drive))
		return 0;

	return HWIF(drive)->ide_dma_end(drive);
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_test_irq()
 *
 * This function checks if the IDE interrupt is asserted and returns a
 * 1 if it is, and 0 otherwise..
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_test_irq(ide_drive_t *drive)
{
	DPRINTK("%s: ep93xx_ide_dma_test_irq\n", drive->name);

	if (!drive->waiting_for_dma)
		printk(KERN_WARNING "%s: %s called while not waiting\n",
		       drive->name, __FUNCTION__);
	/*
	 * Return the value of the IDE interrupt bit.
	 */
	if( inl(IDECR) & IDECtrl_INTRQ )
	    return 1;
	    
	return 0;
}

/*
static int
ep93xx_ide_dma_verbose(ide_drive_t *drive)
{
        DPRINTK("%s\n", __FUNCTION__ );
	return 1;
}
*/

void ep93xx_dma_init(ide_hwif_t *hwif)
{
    int dma_handle;
    u32 uiTemp;
    unsigned long flags;
    
    DPRINTK("%s\n", __FUNCTION__ );

        local_irq_save(flags);
        
    	if( ep93xx_ide_allocate_dma_engine(hwif) )
	    return;


	/*
	 * Init the ep93xx dma handle to 0.  This field is used to hold a
	 * handle to the dma instance.
	 */
	hwif->hw.dma = 0; 

	/*
	 * Make sure the GPIO on IDE bits in the DEVCFG register are not set.
	 */
	uiTemp = inl(/*SYSCON_DEVCFG*/EP93XX_SYSCON_DEVICE_CONFIG) & ~(SYSCON_DEVCFG_EonIDE |
					SYSCON_DEVCFG_GonIDE |
					SYSCON_DEVCFG_HonIDE);
	SysconSetLocked( /*SYSCON_DEVCFG*/EP93XX_SYSCON_DEVICE_CONFIG, uiTemp );
	/*printk("inl(SYSCON_DEVCFG)=%x\n",inl(EP93XX_SYSCON_DEVICE_CONFIG));*/

	/*
	 * Insure that neither device is selected. -wlg
	 */
        outl(inl(IDECR) | (IDECtrl_CS0n | IDECtrl_CS1n), IDECR);

	/*
        * Clear the MDMA and UDMA operation registers.
        */
	outl(0, IDEMDMAOP);
	outl(0, IDEUDMAOP);

	/*
        * Reset the UDMA state machine.
        */
	outl(IDEUDMADebug_RWOE | IDEUDMADebug_RWPTR | IDEUDMADebug_RWDR | IDEUDMADebug_RROE | IDEUDMADebug_RRPTR | IDEUDMADebug_RRDR,
	     IDEUDMADEBUG);
	outl(0, IDEUDMADEBUG);

	/*
	 * Set up the IDE interface for PIO transfers, using the default PIO
	 * mode.
	 */
	outl(IDECfg_IDEEN | IDECfg_PIO | (4 << IDECfg_MODE_SHIFT) |
	     (1 << IDECfg_WST_SHIFT), IDECFG);

	/*
	 * Setup the ports.
	 */
	ide_init_hwif_ports(&hwif->hw, 0x800, 0x406, NULL);

	
            
        /*
	 * Get the interrupt.
	 */
	hwif->hw.irq = IRQ_EIDE;

	/*
	 * This is the dma channel number assigned to this IDE interface. Until
	 * dma is enabled for this interface, we set it to NO_DMA.
	 */
	hwif->hw.dma = NO_DMA;
 
 	/*
	 * Open an instance of the ep93xx dma interface.
	 */

	if ( ep93xx_dma_request(&dma_handle, hwif->name, DMA_IDE) != 0 ) 
	{
			/*
			 * Fail, so clean up.
			 */
			 
	    ep93xx_ide_release_dma_engine (hwif);
	    return;
	}

	/*
	 * Now that we've got a dma channel allocated, set up the rest
	 * of the dma specific stuff.
	 */
	DPRINTK("\n ide init- dma channel allocated: 0x%x, %s  %d \n", dma_handle, hwif->name, DMA_IDE);

	hwif->hw.dma = dma_handle;
	/*
	 * Enable dma support for atapi devices.
	 */

	hwif->atapi_dma			= 1;
	hwif->ultra_mask		= 0x0f;  
	hwif->mwdma_mask		= 0x00;
	hwif->swdma_mask		= 0x00;
	hwif->speedproc			= NULL;
	hwif->autodma			= 1;

	g_prd_count=0;
  
     	hwif->ide_dma_on 		= __ide_dma_on;
	hwif->ide_dma_lostirq 		= __ide_dma_lostirq;
	hwif->ide_dma_check 		= ep93xx_ide_dma_check;
	hwif->ide_dma_host_on 		= ep93xx_ide_dma_host_on;

	hwif->ide_dma_host_off 		= ep93xx_ide_dma_host_off;

	//hwif->ide_dma_begin 		= ep93xx_ide_dma_begin;
	//hwif->ide_dma_read 		= ep93xx_ide_dma_read;
	//hwif->ide_dma_write 		= ep93xx_ide_dma_write;
	hwif->dma_setup			= ep93xx_ide_dma_setup;
	hwif->dma_exec_cmd		= ep93xx_ide_dma_exec_cmd;
	hwif->dma_start			= ep93xx_ide_dma_start;
	
	hwif->ide_dma_end 		= ep93xx_ide_dma_end;
	hwif->ide_dma_off_quietly 	= ep93xx_ide_dma_off_quietly;
	hwif->ide_dma_timeout 		= ep93xx_ide_dma_timeout;
	hwif->ide_dma_test_irq 		= ep93xx_ide_dma_test_irq;
	//hwif->ide_dma_verbose 		= ep93xx_ide_dma_verbose;
	
        local_irq_restore(flags);
}

EXPORT_SYMBOL(ep93xx_dma_init);
