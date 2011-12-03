/*****************************************************************************
 *  linux/include/asm-arm/arch-ep93xx/ide.h
 *
 *  IDE definitions for the EP93XX architecture
 *
 *
 *  Copyright (c) 2003 Cirrus Logic, Inc., All rights reserved.
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
#ifndef ASM_ARCH_IDE_H
#define ASM_ARCH_IDE_H
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/scatterlist.h>

/*
 *  Maximum number of IDE interfaces for this architecture is 1.
 */
#undef  MAX_HWIFS
#define MAX_HWIFS	1 

/*
 *  Default PIO mode used for setting up DMA commands
 */
#define DEFAULT_PIO_MODE	    4

/*
 *  ATA Command Register addresses.
 */

#define DATAREGISTER            0x00
#define ERRORREGISTER           0x01
#define FEATURESREGISTER        0x01
#define SECTORCOUNTREGISTER     0x02
#define SECTORNUMBERREGISTER    0x03
#define CYLINDERLOWREGISTER     0x04
#define CYLINDERHIGHREGISTER    0x05
#define DEVICEHEADREGISTER      0x06
#define COMMANDREGISTER         0x07
#define STATUSREGISTER          0x07

/*
 *  ATA Control Register addresses.
 */
#define DEVICECONTROLREGISTER   0x06
#define ALTERNATESTATUSREGISTER 0x06

/*
 *  ATA Register Bit Masks
 */
#define ATASRST  		        0x04
#define ATAnIEN  		        0x02
#define ATADEV   		        0x10
#define ATAABRT  		        0x04
#define ATABSY   		        0x80
#define ATADRDY  		        0x40
#define ATADRQ   		        0x08
#define ATAERR   		        0x01
#define ATADEVFAULT		        0x20
#define	ATAWRITEFAULT	        0x20
#define ATASERVICE 		        0x10
#define ATACORRECTED	        0x04
#define ATAINDEX		        0x02

/*****************************************************************************
 *
 * Set up a hw structure for a specified data port, control port and IRQ.
 * This should follow whatever the default interface uses.
 *
 ****************************************************************************/
static __inline__ void
old_ide_init_hwif_ports(hw_regs_t *hw, int data_port, int ctrl_port, int *irq)
{
       unsigned long reg; 
	int i;
    
	outl(0, IDEMDMAOP);
	outl(0, IDEUDMAOP);
	outl(IDECfg_IDEEN | IDECfg_PIO | (4 << IDECfg_MODE_SHIFT) |
	     (1 << IDECfg_WST_SHIFT), IDECFG);
    
	memset(hw, 0, sizeof(*hw));
                                                                                                                             
        reg = (unsigned long)data_port;
        for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
                hw->io_ports[i] = reg;
                reg += 1;
        }
                                                                                                                             
        hw->io_ports[IDE_CONTROL_OFFSET] = (unsigned long)ctrl_port;
                                                                                                                             
        if(irq){
                *irq = 0;
	}
  
}

extern void ep93xx_ide_init(struct hwif_s * hwif);

/*****************************************************************************
 *
 * This registers the standard ports for this architecture with the IDE
 * driver.
 *
 ****************************************************************************/
static __inline__ void
old_ide_init_default_hwifs(void)
{
    hw_regs_t hw;

    struct hwif_s *hwif;
    unsigned long value;

    /*
     *  Make sure the GPIO on IDE bits in the DEVCFG register are not set.
     */
    value = inl(EP93XX_SYSCON_DEVICE_CONFIG) & ~(SYSCON_DEVCFG_EonIDE |
                                  SYSCON_DEVCFG_GonIDE |
                                SYSCON_DEVCFG_HonIDE);
	
    SysconSetLocked( EP93XX_SYSCON_DEVICE_CONFIG, value );
    
    /*
     *  Initialize the IDE interface
     */
    old_ide_init_hwif_ports(&hw, 0x800, 0x406, NULL);
        
   	
    /*
     *  Get the interrupt.
     */
    hw.irq = IRQ_EIDE;
    
    /*
     * This is the dma channel number assigned to this IDE interface. Until
     * dma is enabled for this interface, we set it to NO_DMA.
     */
  hw.dma = NO_DMA;    
    
    /*
     *  Register the IDE interface, an ide_hwif_t pointer is passed in,
     *  which will get filled in with the hwif pointer for this interface.
     */
  ide_register_hw(&hw, &hwif);
    
    /*
     *  Set up a pointer to the ep93xx ideproc function.
     */
    ep93xx_ide_init(hwif);
        
    printk("Cirrus Logic EP93XX IDE initialization.\n");
    
    
}



#endif /* ASM_ARCH_IDE_H */
