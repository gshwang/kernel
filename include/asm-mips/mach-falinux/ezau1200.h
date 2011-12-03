/*
 * AMD Alchemy EZ-AU1200 Referrence Board  -- www.falinux.com
 * Board Registers defines.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 * ########################################################################
 *
 *
 */
#ifndef __ASM_EZAU1200_H
#define __ASM_EZAU1200_H

#include <linux/types.h>
#include <au1xxx_gpio.h>

// nand
#define NAND_CS 				1

// GPIO
#define	GPNR_MMC_CD				5	
#define	GPNR_MMC_WP				4

// AC97
#define DBDMA_AC97_TX_CHAN		DSCR_CMD0_PSC0_TX
#define DBDMA_AC97_RX_CHAN		DSCR_CMD0_PSC0_RX

//#define DBDMA_I2S_TX_CHAN		DSCR_CMD0_PSC1_TX
//#define DBDMA_I2S_RX_CHAN		DSCR_CMD0_PSC1_RX

// SPI and SMB are muxed on the EM-AU1200 board
#define SPI_PSC_BASE        	PSC1_BASE_ADDR
#define SMBUS_PSC_BASE      	PSC1_BASE_ADDR

// AC97 and I2S are muxed on the EM-AU1200 board
#define AC97_PSC_BASE       	PSC0_BASE_ADDR
#define I2S_PSC_BASE			PSC0_BASE_ADDR

// static memory bus 
#define	FALINUX_AU1200_PHYS_CS0		0x1E000000
#define	FALINUX_AU1200_PHYS_CS1		0x1C000000
#define	FALINUX_AU1200_PHYS_CS2		0x1A000000
#define	FALINUX_AU1200_PHYS_CS3		0x18000000

#define	FALINUX_AU1200_VIRT_CS0		(FALINUX_AU1200_PHYS_CS0 + 0xA0000000 )
#define	FALINUX_AU1200_VIRT_CS1		(FALINUX_AU1200_PHYS_CS1 + 0xA0000000 )
#define	FALINUX_AU1200_VIRT_CS2		(FALINUX_AU1200_PHYS_CS2 + 0xA0000000 )
#define	FALINUX_AU1200_VIRT_CS3		(FALINUX_AU1200_PHYS_CS3 + 0xA0000000 )


// MMC card_detect, write_protect
#define	MMC_CD_STATUS()			au1xxx_gpio_read(GPNR_MMC_CD)
#define	MMC_WP_STATUS()			au1xxx_gpio_read(GPNR_MMC_WP)


#if 0
#define AU1XXX_ATA_PHYS_ADDR	(0x18800000)
#define AU1XXX_ATA_PHYS_LEN		(0x100)
#define AU1XXX_ATA_REG_OFFSET	(5)
#define AU1XXX_ATA_INT			DB1200_IDE_INT
#define AU1XXX_ATA_DDMA_REQ		DSCR_CMD0_DMA_REQ1;
#define AU1XXX_ATA_RQSIZE		128
#endif



#if 1

// This is defined in au1000.h with bogus value
#undef AU1X00_EXTERNAL_INT

#define BCSR_KSEG1_ADDR		0xB9800000

typedef volatile struct
{
	/*00*/	u16 whoami;
		u16 reserved0;
	/*04*/	u16 status;
		u16 reserved1;
	/*08*/	u16 switches;
		u16 reserved2;
	/*0C*/	u16 resets;
		u16 reserved3;

	/*10*/	u16 pcmcia;
		u16 reserved4;
	/*14*/	u16 board;
		u16 reserved5;
	/*18*/	u16 disk_leds;
		u16 reserved6;
	/*1C*/	u16 system;
		u16 reserved7;

	/*20*/	u16 intclr;
		u16 reserved8;
	/*24*/	u16 intset;
		u16 reserved9;
	/*28*/	u16 intclr_mask;
		u16 reserved10;
	/*2C*/	u16 intset_mask;
		u16 reserved11;

	/*30*/	u16 sig_status;
		u16 reserved12;
	/*34*/	u16 int_status;
		u16 reserved13;
	/*38*/	u16 reserved14;
		u16 reserved15;
	/*3C*/	u16 reserved16;
		u16 reserved17;

} BCSR;

static BCSR * const bcsr = (BCSR *)BCSR_KSEG1_ADDR;

/*
 * Register bit definitions for the BCSRs
 */
#define BCSR_WHOAMI_DCID	0x000F
#define BCSR_WHOAMI_CPLD	0x00F0
#define BCSR_WHOAMI_BOARD	0x0F00

#define BCSR_STATUS_PCMCIA0VS	0x0003
#define BCSR_STATUS_PCMCIA1VS	0x000C
#define BCSR_STATUS_SWAPBOOT	0x0040
#define BCSR_STATUS_FLASHBUSY	0x0100
#define BCSR_STATUS_IDECBLID	0x0200
#define BCSR_STATUS_SD0WP		0x0400
#define BCSR_STATUS_U0RXD		0x1000
#define BCSR_STATUS_U1RXD		0x2000

#define BCSR_SWITCHES_OCTAL	0x00FF
#define BCSR_SWITCHES_DIP_1	0x0080
#define BCSR_SWITCHES_DIP_2	0x0040
#define BCSR_SWITCHES_DIP_3	0x0020
#define BCSR_SWITCHES_DIP_4	0x0010
#define BCSR_SWITCHES_DIP_5	0x0008
#define BCSR_SWITCHES_DIP_6	0x0004
#define BCSR_SWITCHES_DIP_7	0x0002
#define BCSR_SWITCHES_DIP_8	0x0001
#define BCSR_SWITCHES_ROTARY	0x0F00

#define BCSR_RESETS_ETH		0x0001
#define BCSR_RESETS_CAMERA	0x0002
#define BCSR_RESETS_DC		0x0004
#define BCSR_RESETS_IDE		0x0008
#define BCSR_RESETS_TV		0x0010
/* not resets but in the same register */
#define BCSR_RESETS_PWMR1mUX 0x0800
#define BCSR_RESETS_PCS0MUX	0x1000
#define BCSR_RESETS_PCS1MUX	0x2000
#define BCSR_RESETS_SPISEL	0x4000

#define BCSR_PCMCIA_PC0VPP	0x0003
#define BCSR_PCMCIA_PC0VCC	0x000C
#define BCSR_PCMCIA_PC0DRVEN	0x0010
#define BCSR_PCMCIA_PC0RST	0x0080
#define BCSR_PCMCIA_PC1VPP	0x0300
#define BCSR_PCMCIA_PC1VCC	0x0C00
#define BCSR_PCMCIA_PC1DRVEN	0x1000
#define BCSR_PCMCIA_PC1RST	0x8000

#define BCSR_BOARD_LCDVEE	0x0001
#define BCSR_BOARD_LCDVDD	0x0002
#define BCSR_BOARD_LCDBL	0x0004
#define BCSR_BOARD_CAMSNAP	0x0010
#define BCSR_BOARD_CAMPWR	0x0020
#define BCSR_BOARD_SD0PWR	0x0040

#define BCSR_LEDS_DECIMALS	0x0003
#define BCSR_LEDS_LED0		0x0100
#define BCSR_LEDS_LED1		0x0200
#define BCSR_LEDS_LED2		0x0400
#define BCSR_LEDS_LED3		0x0800

#define BCSR_SYSTEM_POWEROFF	0x4000
#define BCSR_SYSTEM_RESET	0x8000

/* Bit positions for the different interrupt sources */
#define BCSR_INT_IDE		0x0001
#define BCSR_INT_ETH		0x0002
#define BCSR_INT_PC0		0x0004
#define BCSR_INT_PC0STSCHG	0x0008
#define BCSR_INT_PC1		0x0010
#define BCSR_INT_PC1STSCHG	0x0020
#define BCSR_INT_DC			0x0040
#define BCSR_INT_FLASHBUSY	0x0080
#define BCSR_INT_PC0INSERT	0x0100
#define BCSR_INT_PC0EJECT	0x0200
#define BCSR_INT_PC1INSERT	0x0400
#define BCSR_INT_PC1EJECT	0x0800
#define BCSR_INT_SD0INSERT	0x1000
#define BCSR_INT_SD0EJECT	0x2000



/*
 *	External Interrupts for Pb1200 as of 8/6/2004.
 *   Bit positions in the CPLD registers can be calculated by taking
 *   the interrupt define and subtracting the DB1200_INT_BEGIN value.
 *    *example: IDE bis pos is  = 64 - 64
                ETH bit pos is  = 65 - 64
 */
#define DB1200_INT_BEGIN		(AU1000_LAST_INTC1_INT + 1)
#define DB1200_IDE_INT			(DB1200_INT_BEGIN + 0)
#define DB1200_ETH_INT			(DB1200_INT_BEGIN + 1)
#define DB1200_PC0_INT			(DB1200_INT_BEGIN + 2)
#define DB1200_PC0_STSCHG_INT	(DB1200_INT_BEGIN + 3)
#define DB1200_PC1_INT			(DB1200_INT_BEGIN + 4)
#define DB1200_PC1_STSCHG_INT	(DB1200_INT_BEGIN + 5)
#define DB1200_DC_INT			(DB1200_INT_BEGIN + 6)
#define DB1200_FLASHBUSY_INT	(DB1200_INT_BEGIN + 7)
#define DB1200_PC0_INSERT_INT	(DB1200_INT_BEGIN + 8)
#define DB1200_PC0_EJECT_INT	(DB1200_INT_BEGIN + 9)
#define DB1200_PC1_INSERT_INT	(DB1200_INT_BEGIN + 10)
#define DB1200_PC1_EJECT_INT	(DB1200_INT_BEGIN + 11)
#define DB1200_SD0_INSERT_INT	(DB1200_INT_BEGIN + 12)
#define DB1200_SD0_EJECT_INT	(DB1200_INT_BEGIN + 13)

#define DB1200_INT_END			(DB1200_INT_BEGIN + 15)


/*
// For drivers/pcmcia/au1000_db1x00.c 
// PCMCIA Db1x00 specific defines 
#define PCMCIA_MAX_SOCK 1
#define PCMCIA_NUM_SOCKS (PCMCIA_MAX_SOCK+1)

// VPP/VCC 
#define SET_VCC_VPP(VCC, VPP, SLOT)\
	((((VCC)<<2) | ((VPP)<<0)) << ((SLOT)*8))

#define BOARD_PC0_INT 	DB1200_PC0_INT
#define BOARD_PC1_INT 	DB1200_PC1_INT
#define BOARD_CARD_INSERTED(SOCKET) bcsr->sig_status & (1<<(8+(2*SOCKET)))
*/



#endif


#endif /* __ASM_DB1200_H */



/*


#define NAND_PHYS_ADDR   0x1C000000
#define AU1XXX_SMC91111_PHYS_ADDR	(0x1A000300)	// nCS2
#define AU1XXX_SMC91111_IRQ		AU1000_GPIO_5	// GPIO-05

#define AU1XXX_AX88796B_PHYS_ADDR	(0x1A000000)    // nCS2
#define AU1XXX_AX88796B_IRQ		AU1000_GPIO_2   // GPIO-02

#define AU1XXX_MK712_PHYS_ADDR	(0x18000000)    // nCS3
//#define AU1XXX_MK712_PHYS_ADDR	(0x19FFF300)    // nCS3
#define AU1XXX_MK712_IRQ		AU1000_GPIO_12   // GPIO-12

*/

