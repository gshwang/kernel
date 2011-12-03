/*----------------------------------------------------------------------------
 * ep93xx_eth.c
 *  Ethernet Device Driver for Cirrus Logic EP93xx.
 *
 * Copyright (C) 2003 by Cirrus Logic www.cirrus.com
 * This software may be used and distributed according to the terms
 * of the GNU Public License.
 *
 *   This driver was written based on skeleton.c by Donald Becker and
 * smc9194.c by Erik Stahlman.
 *
 * Theory of Operation
 * Driver Configuration
 *  - Getting MAC address from system
 *     To setup identical MAC address for each target board, driver need
 *      to get a MAC address from system.  Normally, system has a Serial
 *      EEPROM or other media to store individual MAC address when
 *      manufacturing.
 *      The macro GET_MAC_ADDR is prepared to get the MAC address from
 *      system and one should supply a routine for this purpose.
 * Driver Initialization
 * DMA Operation
 * Cache Coherence
 *
 * History:
 * 07/19/01 0.1  Sungwook Kim  initial release
 * 10/16/01 0.2  Sungwook Kim  add workaround for ignorance of Tx request while sending frame
 *                             add some error stuations handling
 *
 * 03/25/03 Melody Lee Modified for EP93xx
 *--------------------------------------------------------------------------*/
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <linux/errno.h>
#include <linux/init.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <asm/arch/hardware.h>
#include <asm/arch/ssp.h>

#include "ep93xx_eth.h"

/*----------------------------------------------------------------------------
 * The name of the card.
 * It is used for messages and in the requests for io regions, irqs and ...
 * This device is not in a card but I used same name in skeleton.c file.
 *--------------------------------------------------------------------------*/
#define  cardname  "ep93xx-eth"

#ifdef CONFIG_FALINUX_EP9312

#define GET_MAC_ADDR(addr) { memcpy( addr, ezboot_mac, 6 ); ezboot_mac[5]++; }

unsigned char ezboot_mac[6];

static int __init ezboot_mac_setup(char *str)
{
	unsigned char   idx;
	char    str_mac[32], str_byte[3];
	char    *ptr;

	strcpy( str_mac, str );

	ptr = str_mac;
	for (idx=0; idx<6; idx++)
	{
		str_byte[0] =  *ptr++;
		str_byte[1] =  *ptr++;
		str_byte[2] =  0;
		ptr ++;

		ezboot_mac[idx] = simple_strtol( str_byte, NULL, 16 );
	}

	return 1;
}
__setup("mac=",  ezboot_mac_setup);

EXPORT_SYMBOL(ezboot_mac); 

#else

/*----------------------------------------------------------------------------
 * Macro to get MAC address from system
 *
 * Parameters:
 *  - pD  : a pointer to the net device information, struct net_device.
 *          one can get base address of device from pD->base_addr,
 *          device instance ID (0 for 1st and so on) from *((int*)pD->priv).
 *  - pMac: a pointer to a 6 bytes length buffer to return device MAC address.
 *          this buffer will be initialized with default_mac[] value before
 *          calling.
 * Return: none
 *--------------------------------------------------------------------------*/
#define GET_MAC_ADDR(pD, pMac)						\
	{								\
		unsigned int uiTemp;					\
		int SSP_Handle;						\
									\
		SSP_Handle = SSPDriver->Open( SERIAL_FLASH, 0 );	\
		if (SSP_Handle != -1) {					\
			SSPDriver->Read( SSP_Handle, 0x1000, &uiTemp );	\
			if (uiTemp == 0x43414d45) {			\
				SSPDriver->Read( SSP_Handle, 0x1004,	\
						 &uiTemp );		\
				pMac[0] = uiTemp & 255;			\
				pMac[1] = (uiTemp >> 8) & 255;		\
				pMac[2] = (uiTemp >> 16) & 255;		\
				pMac[3] = uiTemp >> 24;			\
				SSPDriver->Read( SSP_Handle, 0x1008,	\
						 &uiTemp );		\
				pMac[4] = uiTemp & 255;			\
				pMac[5] = (uiTemp >> 8) & 255;		\
			}						\
			SSPDriver->Close( SSP_Handle );			\
		}							\
	}

/****  default MAC address if GET_MAC_ADDR does not override  *************/
static const U8  default_mac[6] = {0x00, 0xba, 0xd0, 0x0b, 0xad, 0x00};

#endif

/*----------------------------------------------------------------------------
 * Some definitions belong to the operation of this driver.
 * You should understand how it affect to driver before any modification.
 *--------------------------------------------------------------------------*/

/****  Interrupt Sources in Use  *******************************************/
/*#define  Default_IntSrc  (IntEn_RxMIE|IntEn_RxSQIE|IntEn_TxLEIE|IntEn_TIE|IntEn_TxSQIE|IntEn_RxEOFIE|IntEn_RxEOBIE|IntEn_RxHDRIE)
*/
#define  Default_IntSrc  (IntEn_TxSQIE|IntEn_RxEOFIE|IntEn_RxEOBIE|IntEn_RxHDRIE)

/****  Length of Device Queue in number of entries
       (must be less than or equal to 255)  ********************************/
#define  LEN_QueRxDesc  64             /*length of Rx Descriptor Queue (4 or bigger) Must be power of 2.*/
#define  LEN_QueRxSts   LEN_QueRxDesc  /*length of Rx Status Queue*/
#define  LEN_QueTxDesc  8              /*length of Tx Descriptor Queue (4 or bigger) Must be power of 2.*/
#define  LEN_QueTxSts   LEN_QueTxDesc  /*length of Tx Status Queue*/

/****  Tx Queue fill-up level control  *************************************/
#define  LVL_TxStop    LEN_QueTxDesc - 2    /*level to ask the stack to stop Tx*/
#define  LVL_TxResume  2   /*level to ask the stack to resume Tx*/

/****  Rx Buffer length in bytes  ******************************************/
#define  LEN_RxBuf  (1518+2+16)  /*length of Rx buffer, must be 4-byte aligned*/
#define  LEN_TxBuf   LEN_RxBuf

/*----------------------------------------------------------------------------
 * MACRO for ease
 *--------------------------------------------------------------------------*/
#define  Align32(a)  (((unsigned int)(a)+3)&~0x03)  /*32bit address alignment*/
#define  IdxNext(idxCur,len)  (((idxCur)+1)%(len))  /*calc next array index number*/

/****  malloc/free routine for DMA buffer  **********************************/
 /*use non-cached DMA buffer*/
#define  MALLOC_DMA(size, pPhyAddr)  dma_alloc_writecombine(NULL, (size), (dma_addr_t*)(pPhyAddr), GFP_KERNEL | GFP_DMA)
//#define  FREE_DMA(size, vaddr, paddr)            dma_free_writecombine(NULL, (size), (vaddr), (paddr))

/*----------------------------------------------------------------------------
 * DEBUGGING LEVELS
 *
 * 0 for normal operation
 * 1 for slightly more details
 * >2 for various levels of increasingly useless information
 *    2 for interrupt tracking, status flags
 *    3 for packet dumps, etc.
 *--------------------------------------------------------------------------*/
//#define _DBG_3 
//#define _DBG_2
//#define _DBG_1
//#define _DBG

#ifdef _DBG_3 
#define PRINTK3( fmt, arg... )  printk( fmt, ##arg )
#else
#define PRINTK3( fmt, arg... )
#endif

#ifdef _DBG_2
#define PRINTK2( fmt, arg... )  printk( fmt, ##arg )
#else
#define PRINTK2( fmt, arg... )
#endif

#ifdef _DBG_1
#define PRINTK1( fmt, arg... )  printk( fmt, ##arg )
#else
#define PRINTK1( fmt, arg... )
#endif

#ifdef _DBG
#define PRINTK(x) printk x
#else
#define PRINTK(x)
#endif



#define  _PRTK_ENTRY      PRINTK2   /*to trace function entries*/
#define  _PRTK_SWERR      PRINTK    /*logical S/W error*/
#define  _PRTK_SYSFAIL    PRINTK    /*system service failure*/
#define  _PRTK_HWFAIL     PRINTK    /*H/W operation failure message*/
#define  _PRTK_WARN       PRINTK1   /*warning information*/
#define  _PRTK_INFO       PRINTK2   /*general information*/
#define  _PRTK_ENTRY_ISR  PRINTK3   /*to trace function entries belong to ISR*/
#define  _PRTK_WARN_ISR   PRINTK1   /*warning informations from ISR*/
#define  _PRTK_INFO_ISR   PRINTK3   /*general informations from ISR*/
#define  _PRTK_           PRINTK    /*for temporary print out*/

#if 0
# define  _PRTK_DUMP       PRINTK1   /*to dump large amount of debug info*/
#endif

/*----------------------------------------------------------------------------
 * Custom Data Structures
 *--------------------------------------------------------------------------*/

/****  the information about the buffer passed to device.
       there are matching bufferDescriptor informations
       for each Tx/Rx Descriptor Queue entry to trace
       the buffer within those queues.  ************************************/
typedef  struct bufferDescriptor  {
    void  *vaddr;                /*virtual address representing the buffer passed to device*/
    int(*pFreeRtn)(void *pBuf);  /*free routine*/
}  bufferDescriptor;

/****  device privite informations
       pointed by struct net_device::priv  *********************************/
typedef  struct ep93xxEth_info  {
    /****  static device informations  **********************************/
    struct {
        int                 id;            /*device instance ID (0 for 1st and so on)
                                             must be first element of this structure*/
        receiveDescriptor   *pQueRxDesc;   /*pointer to Rx Descriptor Queue*/
        receiveStatus       *pQueRxSts;    /*pointer to Rx Status Queue*/
        transmitDescriptor  *pQueTxDesc;   /*pointer to Tx Descriptor Queue*/
        transmitStatus      *pQueTxSts;    /*pointer to Tx Status Queue*/
        unsigned char       *pRxBuf;       /*base of Rx Buffer pool*/
        unsigned char       *pTxBuf;       /*base of Tx Buffer pool*/
        unsigned long       phyQueueBase;  /*physical address of device queues*/
        unsigned long       phyQueRxDesc,  /*physical address of Rx Descriptor Queue*/
                            phyQueRxSts,   /*physical address of Rx Status Queue*/
                            phyQueTxDesc,  /*physical address of Tx Descriptor Queue*/
                            phyQueTxSts,   /*physical address of Tx Status Queue*/
                            phyRxBuf,      /*physical address of Rx Buffer pool*/
                            phyTxBuf;      /*physical address of Tx Buffer pool*/
        bufferDescriptor    *pRxBufDesc,   /*info of Rx Buffers*/
                            *pTxBufDesc;   /*info of Tx Buffers*/
        int                 miiIdPhy;      /*MII Bus ID of Ethernet PHY*/
    }  s;
    /****  dynamic information, subject to clear when device open  ******/
    struct {
        struct net_device_stats  stats;  /*statistic data*/
        int  idxQueRxDesc,       /*next processing index of device queues*/
             idxQueRxSts,
             idxQueTxDescHead,
             idxQueTxDescTail,
             idxQueTxSts;
        int  txStopped;          /*flag for Tx condition*/
    }  d;
}  ep93xxEth_info;

/*----------------------------------------------------------------------------
 * Global Variables
 *--------------------------------------------------------------------------*/
static int  numOfInstance = 0;  /*total number of device instance, 0 means the 1st instance.*/

//static struct sk_buff gTxSkb;
//static char gTxBuff[LEN_TxBuf];

//static char gTxDataBuff[LEN_QueTxDesc][LEN_TxBuf];

// To know if PHY auto-negotiation has done?
static int gPhyAutoNegoDone=0;

/*============================================================================
 *
 * Internal Routines
 *
 *==========================================================================*/

/*****************************************************************************
* free_skb()
*****************************************************************************/
static int
free_skb(void *pSkb)
{
	dev_kfree_skb_irq((struct sk_buff*)pSkb);
	return 0;
}

/*****************************************************************************
* waitOnReg32()
*****************************************************************************/
static int
waitOnReg32(struct net_device *pD, int reg, unsigned long mask,
	    unsigned long expect, int tout)
{
	int  i;
	int  dt;

	for (i = 0; i < 10000; ) {
		dt = RegRd32(reg);
		dt = (dt ^ expect) & mask;
		if (dt == 0)
			break;
		if (tout)
			i++;
	}

	return dt;
}

#define  phy_wr(reg,dt)  _phy_write(pD, ((ep93xxEth_info*)pD->priv)->s.miiIdPhy, (reg), (dt))
#define  phy_rd(reg)     _phy_read(pD, ((ep93xxEth_info*)pD->priv)->s.miiIdPhy, (reg))
#define  phy_waitRdy()  waitOnReg32(pD, REG_MIISts,MIISts_Busy, ~MIISts_Busy, 1)

/*****************************************************************************
* _phy_write()
*****************************************************************************/
static void
_phy_write(struct net_device *pD, int idPhy, int reg, U16 dt)
{
	phy_waitRdy();
	RegWr32(REG_MIIData, dt);
	RegWr32(REG_MIICmd, MIICmd_OP_WR | ((idPhy & 0x1f) << 5) |
		((reg & 0x1f) << 0));
}

/*****************************************************************************
* _phy_read()
*****************************************************************************/
static U16
_phy_read(struct net_device *pD,int idPhy,int reg)
{
	U16  dt;

	phy_waitRdy();
	RegWr32(REG_MIICmd, MIICmd_OP_RD | ((idPhy & 0x1f) << 5) |
		((reg & 0x1f) << 0));
	phy_waitRdy();
	dt = (unsigned short)RegRd32(REG_MIIData);

	return dt;
}

#ifndef _PRTK_DUMP
#define _dbg_phy_dumpReg(pD)
#else
/*****************************************************************************
* _dbg_phy_dumpReg()
*****************************************************************************/
static void
_dbg_phy_dumpReg(struct net_device *pD)
{
	_PRTK_DUMP(("Dumping registers of Ethernet PHY\n"));
	_PRTK_DUMP((" pD:0x%p, Eth Base Address:0x%x\n", pD,
		    (unsigned int)pD->base_addr));

	_PRTK_DUMP((" 0-3:0x%04x 0x%04x  0x%04x 0x%04x\n",
		    phy_rd(0), phy_rd(1), phy_rd(2), phy_rd(3)));
	_PRTK_DUMP((" 4-6:0x%04x 0x%04x  0x%04x\n",
		    phy_rd(4), phy_rd(5), phy_rd(6)));
	_PRTK_DUMP((" 16-19:0x%04x 0x%04x  0x%04x  0x%04x\n",
		    phy_rd(16), phy_rd(17), phy_rd(18), phy_rd(19)));
	_PRTK_DUMP((" 20:0x%04x\n", phy_rd(20)));
}
#endif

/*****************************************************************************
* phy_autoNegotiation()
*****************************************************************************/
static int
phy_autoNegotiation(struct net_device *pD)
{
	U16 val;
	U16 oldVal;
	U16 count = 0;

	phy_wr(4, 0x01e1);  /*Set 802.3, 100M/10M Full/Half ability*/
	phy_wr(0, (1 << 12) | (1 << 9)); /* enable auto negotiation*/
	while (1) {
		val = phy_rd(1); /* read BM status Reg*/
		if (val & 0x0020) /* if Auto_Neg_complete?*/
			break;
		else {
			if (count >= 3)
				return -1;
			mdelay(1000);//delay 1 second.
			count++;
		}
	}

	//CS8952 PHY needs the delay.  Otherwise it won't send the 1st frame.
	mdelay(1000);//delay 1 second.

	val = phy_rd(5); /* read ANLPAR Reg*/
	if (val & 0x0140) { /* if 100M_FDX or 10M_FDX?*/
		oldVal = RegRd16(REG_TestCTL);
		/*Enable MAC's Full Duplex mode.*/
		RegWr16(REG_TestCTL, oldVal | TestCTL_MFDX);
	}

	gPhyAutoNegoDone = 1;

	return 0;
}

/*****************************************************************************
* phy_init()
*****************************************************************************/
static int
phy_init(struct net_device *pD)
{
	U32 oldVal;
	int status = -1;
	U16 val;

	oldVal = RegRd32(REG_SelfCTL);

	/*
	 * Set MDC clock to be divided by 8 and disable PreambleSuppress bit
	 */
	RegWr32(REG_SelfCTL, 0x4e00);

	/*
	 * read BM status Reg; Link Status Bit remains cleared until the Reg is
	 * read.
	 */
	val = phy_rd(1);

	/*
	 * read BMStaReg again to get the current link status
	 */
	val = phy_rd(1);
	if (val & 0x0004)
		status = phy_autoNegotiation(pD);

	RegWr32(REG_SelfCTL, oldVal);

	return status;
}

/*****************************************************************************
* phy_reset()
*****************************************************************************/
#if 0
static int
phy_reset(struct net_device *pD)
{
	int i;


	phy_wr(0, 1 << 15);

	for (i = 0; i < 1000; i++)
		if ((phy_rd(0) & (1 << 15)) == 0)
			break;

	if ((phy_rd(0) & (1 << 15)) != 0) {
		_PRTK_HWFAIL(("phy_reset(): PHY reset does not self-clear\n"));
		return -1;
	}

	phy_wr(19, 0x00);
	phy_wr(4, (1 << 8) | (1 << 7) | (1 << 6) | (1 << 5) | (0x01 << 0));
	phy_wr(0, (1 << 12) | (1 << 9));

	return 0;
}
#endif

#ifndef _PRTK_DUMP
# define _dbg_ep93xxEth_dumpQueues(pD)
#else
/*****************************************************************************
* _dbg_ep93xxEth_dumpQueues()
*****************************************************************************/
static void
_dbg_ep93xxEth_dumpQueues(struct net_device *pD)
{
	struct ep93xxEth_info *pP = pD->priv;
	int i;

	_PRTK_DUMP(("Dumping Descriptor/Status Queues\n"));
	_PRTK_DUMP((" pD:0x%p, Base Address:0x%x\n", pD,
		    (unsigned int)pD->base_addr));

	_PRTK_DUMP((" o Rx Status Queue: at 0x%p, %d entries\n",
		    pP->s.pQueRxSts, LEN_QueRxSts));
	for (i = 0; i < LEN_QueRxSts; i++)
		_PRTK_DUMP(("  - %2d: 0x%08x 0x%08x \n", i,
			    (unsigned int)pP->s.pQueRxSts[i].w.e0,
			    (unsigned int)pP->s.pQueRxSts[i].w.e1));

	_PRTK_DUMP((" o Rx Descriptor Queue: at 0x%p, %d entries\n",
		    pP->s.pQueRxDesc, LEN_QueRxDesc));
	for (i = 0; i < LEN_QueRxDesc; i++)
		_PRTK_DUMP(("  - %2d: 0x%08x 0x%08x \n", i,
			    (unsigned int)pP->s.pQueRxDesc[i].w.e0,
			    (unsigned int)pP->s.pQueRxDesc[i].w.e1));

	_PRTK_DUMP((" o Tx Status Queue: at 0x%p, %d entries\n",
		    pP->s.pQueTxSts, LEN_QueTxSts));
	for (i = 0; i < LEN_QueTxSts; i++)
		_PRTK_DUMP(("  - %2d: 0x%08x \n", i,
			    (unsigned int)pP->s.pQueTxSts[i].w.e0));

	_PRTK_DUMP((" o Tx Descriptor Queue: at 0x%p, %d entries\n",
		    pP->s.pQueTxDesc, LEN_QueTxDesc));
	for (i = 0; i < LEN_QueTxDesc; i++)
		_PRTK_DUMP(("  - %2d: 0x%08x 0x%08x \n", i,
			    (unsigned int)pP->s.pQueTxDesc[i].w.e0,
			    (unsigned int)pP->s.pQueTxDesc[i].w.e1));
}
#endif

/*****************************************************************************
* devQue_start()
*
  make descriptor queues active
  allocate queue entries if needed
  and set device registers up to make it operational
  assume device has been initialized
*
*****************************************************************************/
static int
devQue_start(struct net_device *pD)
{
	int err;
	struct ep93xxEth_info *pP = pD->priv;
	int i;
	void *pBuf;
	U32 phyA;

	RegWr32(REG_BMCtl, BMCtl_RxDis | BMCtl_TxDis | RegRd32(REG_BMCtl));
	err = waitOnReg32(pD, REG_BMSts, BMSts_TxAct, ~BMSts_TxAct, 1);
	err |= waitOnReg32(pD, REG_BMSts, BMSts_RxAct, ~BMSts_RxAct, 1);
	if (err)
		_PRTK_HWFAIL(("devQue_start(): BM does not stop\n"));

	memset(pP->s.pQueTxSts, 0, sizeof(pP->s.pQueTxSts[0]) * LEN_QueTxSts);
	pP->d.idxQueTxSts = 0;
	RegWr32(REG_TxSBA, pP->s.phyQueTxSts);
	RegWr32(REG_TxSCA, pP->s.phyQueTxSts);
	RegWr16(REG_TxSBL, sizeof(pP->s.pQueTxSts[0]) * LEN_QueTxSts);
	RegWr16(REG_TxSCL, sizeof(pP->s.pQueTxSts[0]) * LEN_QueTxSts);

	memset(pP->s.pQueTxDesc, 0,
	       sizeof(pP->s.pQueTxDesc[0]) * LEN_QueTxDesc);
	pP->d.idxQueTxDescHead = pP->d.idxQueTxDescTail = 0;
	RegWr32(REG_TxDBA, pP->s.phyQueTxDesc);
	RegWr32(REG_TxDCA, pP->s.phyQueTxDesc);
	RegWr16(REG_TxDBL, sizeof(pP->s.pQueTxDesc[0]) * LEN_QueTxDesc);
	RegWr16(REG_TxDCL, sizeof(pP->s.pQueTxDesc[0]) * LEN_QueTxDesc);

	memset(pP->s.pQueRxSts, 0, sizeof(pP->s.pQueRxSts[0]) * LEN_QueRxSts);
	pP->d.idxQueRxSts = 0;
	RegWr32(REG_RxSBA, pP->s.phyQueRxSts);
	RegWr32(REG_RxSCA, pP->s.phyQueRxSts);
	RegWr16(REG_RxSBL, sizeof(pP->s.pQueRxSts[0]) * LEN_QueRxSts);
	RegWr16(REG_RxSCL, sizeof(pP->s.pQueRxSts[0]) * LEN_QueRxSts);

	memset(pP->s.pQueRxDesc, 0,
	       sizeof(pP->s.pQueRxDesc[0]) * LEN_QueRxDesc);
	phyA = pP->s.phyRxBuf;
	for (i = 0; i < LEN_QueRxDesc; i++) {
		pP->s.pQueRxDesc[i].f.bi = i;
		pP->s.pQueRxDesc[i].f.ba = phyA;
		pP->s.pQueRxDesc[i].f.bl = LEN_RxBuf;
		phyA += (LEN_RxBuf + 3) & ~0x03;
	}
	pP->d.idxQueRxDesc = 0;
	RegWr32(REG_RxDBA, pP->s.phyQueRxDesc);
	RegWr32(REG_RxDCA, pP->s.phyQueRxDesc);
	RegWr16(REG_RxDBL, sizeof(pP->s.pQueRxDesc[0]) * LEN_QueRxDesc);
	RegWr16(REG_RxDCL, sizeof(pP->s.pQueRxDesc[0]) * LEN_QueRxDesc);

	pBuf = pP->s.pRxBuf;
	for (i = 0; i < LEN_QueRxDesc; i++) {
		pP->s.pRxBufDesc[i].vaddr = pBuf;
		pP->s.pRxBufDesc[i].pFreeRtn = 0;
		pBuf += (LEN_RxBuf + 3) & ~0x03;
	}

	memset(pP->s.pTxBufDesc, 0x0,
	       sizeof(*pP->s.pTxBufDesc) * LEN_QueTxDesc);
	pBuf = pP->s.pTxBuf;// = &gTxDataBuff[0][0];
	for (i = 0; i < LEN_QueTxDesc; i++) {
		pP->s.pTxBufDesc[i].vaddr =  pBuf + (i*LEN_TxBuf);//&gTxDataBuff[i][0];
		pP->s.pTxBufDesc[i].pFreeRtn = 0;
	}

	RegWr32(REG_BMCtl, BMCtl_TxEn | BMCtl_RxEn | RegRd32(REG_BMCtl));
	err = waitOnReg32(pD, REG_BMSts, BMSts_TxAct | BMSts_TxAct,
			  BMSts_TxAct | BMSts_TxAct, 1);
	if(err)
		_PRTK_HWFAIL(("devQue_start(): BM does not start\n"));

	RegWr32(REG_RxSEQ, LEN_QueRxSts);
	RegWr32(REG_RxDEQ, LEN_QueRxDesc);

	return 0;
}

/*****************************************************************************
* devQue_init()
  init device descriptor queues at system level
  device access is not recommended at this point
*
*****************************************************************************/
static int
devQue_init(struct net_device *pD)
{
	struct ep93xxEth_info *pP = pD->priv;
	void *pBuf;
	int size;

	if (sizeof(receiveDescriptor) != 8) {
		_PRTK_SWERR(("devQue_init(): size of receiveDescriptor is not 8 bytes!!!\n"));
		return -1;
	} else if (sizeof(receiveStatus) != 8) {
		_PRTK_SWERR(("devQue_init(): size of receiveStatus is not 8 bytes!!!\n"));
		return -1;
	} else if (sizeof(transmitDescriptor) != 8) {
		_PRTK_SWERR(("devQue_init(): size of transmitDescriptor is not 8 bytes!!!\n"));
		return -1;
	} else if (sizeof(transmitStatus) != 4) {
		_PRTK_SWERR(("devQue_init(): size of transmitStatus is not 4 bytes!!!\n"));
		return -1;
	}

	size = sizeof(receiveDescriptor) * (LEN_QueRxDesc + 1) +
		sizeof(receiveStatus) * (LEN_QueRxSts + 1) +
		sizeof(transmitDescriptor) * (LEN_QueTxDesc + 1) +
		sizeof(transmitStatus) * (LEN_QueTxSts + 1) +
		sizeof(unsigned long) * 4;

	pBuf = MALLOC_DMA(size, &pP->s.phyQueueBase);
	if(!pBuf)
		return -1;

	pP->s.pQueRxDesc = (void *)Align32(pBuf);
	pBuf = (char *)pBuf + sizeof(receiveDescriptor) * (LEN_QueRxDesc + 1);
	pP->s.pQueRxSts = (void *)Align32(pBuf);
	pBuf = (char *)pBuf + sizeof(receiveStatus) * (LEN_QueRxSts + 1);
	pP->s.pQueTxDesc = (void *)Align32(pBuf);
	pBuf = (char *)pBuf + sizeof(transmitDescriptor) * (LEN_QueTxDesc + 1);
	pP->s.pQueTxSts = (void *)Align32(pBuf);
	pBuf = (char *)pBuf + sizeof(transmitStatus) * (LEN_QueTxSts + 1);

	pP->s.phyQueRxDesc = Align32(pP->s.phyQueueBase);
	pP->s.phyQueRxSts = pP->s.phyQueRxDesc + ((U32)pP->s.pQueRxSts -
						  (U32)pP->s.pQueRxDesc);
	pP->s.phyQueTxDesc = pP->s.phyQueRxDesc + ((U32)pP->s.pQueTxDesc -
						   (U32)pP->s.pQueRxDesc);
	pP->s.phyQueTxSts = pP->s.phyQueRxDesc + ((U32)pP->s.pQueTxSts -
						  (U32)pP->s.pQueRxDesc);

	memset(pP->s.pQueRxDesc, 0, sizeof(receiveDescriptor) * LEN_QueRxDesc);
	memset(pP->s.pQueRxSts, 0, sizeof(receiveStatus) * LEN_QueRxSts);
	memset(pP->s.pQueTxDesc, 0,
	       sizeof(transmitDescriptor) * LEN_QueTxDesc);
	memset(pP->s.pQueTxSts, 0, sizeof(transmitStatus) * LEN_QueTxSts);

	pP->s.pRxBuf = MALLOC_DMA(((LEN_RxBuf + 3) & ~0x03) * LEN_QueRxDesc,
				  &pP->s.phyRxBuf);
	if (!pP->s.pRxBuf) {
		pP->s.pRxBuf = 0;
		_PRTK_SYSFAIL(("devQue_init(): fail to allocate memory for RxBuf\n"));
		return -1;
	}

        pP->s.pTxBuf = MALLOC_DMA(((LEN_TxBuf + 3) & ~0x03) * LEN_QueTxDesc,
                                  &pP->s.phyTxBuf);
        if (!pP->s.pTxBuf) {
                pP->s.pTxBuf = 0;
                _PRTK_SYSFAIL(("devQue_init(): fail to allocate memory for TxBuf\n"));
                return -1;
        }

	size = sizeof(bufferDescriptor) * (LEN_QueRxDesc + LEN_QueTxDesc);
	pBuf = kmalloc(size, GFP_KERNEL);
	if(!pBuf) {
		_PRTK_SYSFAIL(("devQue_initAll(): fail to allocate memory for buf desc\n"));
		return -1;
	}
	memset(pBuf, 0x0, size);
	pP->s.pRxBufDesc = pBuf;
	pP->s.pTxBufDesc = pBuf + sizeof(bufferDescriptor) * LEN_QueRxDesc;

	return 0;
}

#ifndef _PRTK_DUMP
# define _dbg_ep93xxeth_dumpReg(pD)
#else
/*****************************************************************************
* _dbg_ep93xxeth_dumpReg()
*****************************************************************************/
static void
_dbg_ep93xxeth_dumpReg(struct net_device *pD)
{
	struct ep93xxEth_info  *pP = pD->priv;

	_PRTK_DUMP(("Dumping registers of Ethernet Module Embedded within EP93xx\n"));
	_PRTK_DUMP((" pD:0x%p, Base Address:0x%x\n", pD,
		    (unsigned int)pD->base_addr));

	_PRTK_DUMP((" RxCTL:0x%08x  TxCTL:0x%08x  TestCTL:0x%08x\n",
		    (unsigned int)RegRd32(REG_RxCTL),
		    (unsigned int)RegRd32(REG_TxCTL),
		    (unsigned int)RegRd32(REG_TestCTL)));
	_PRTK_DUMP((" SelfCTL:0x%08x  IntEn:0x%08x  IntStsP:0x%08x\n",
		    (unsigned int)RegRd32(REG_SelfCTL),
		    (unsigned int)RegRd32(REG_IntEn),
		    (unsigned int)RegRd32(REG_IntStsP)));
	_PRTK_DUMP((" GT:0x%08x  FCT:0x%08x  FCF:0x%08x\n",
		    (unsigned int)RegRd32(REG_GT),
		    (unsigned int)RegRd32(REG_FCT),
		    (unsigned int)RegRd32(REG_FCF)));
	_PRTK_DUMP((" AFP:0x%08x\n", (unsigned int)RegRd32(REG_AFP)));
	_PRTK_DUMP((" TxCollCnt:0x%08x  RxMissCnt:0x%08x  RxRntCnt:0x%08x\n",
		    (unsigned int)RegRd32(REG_TxCollCnt),
		    (unsigned int)RegRd32(REG_RxMissCnt),
		    (unsigned int)RegRd32(REG_RxRntCnt)));
	_PRTK_DUMP((" BMCtl:0x%08x  BMSts:0x%08x\n",
		    (unsigned int)RegRd32(REG_BMCtl),
		    (unsigned int)RegRd32(REG_BMSts)));
	_PRTK_DUMP((" RBCA:0x%08x  TBCA:0x%08x\n",
		    (unsigned int)RegRd32(REG_RBCA),
		    (unsigned int)RegRd32(REG_TBCA)));
	_PRTK_DUMP((" RxDBA:0x%08x  RxDBL/CL:0x%08x  RxDCA:0x%08x\n",
		    (unsigned int)RegRd32(REG_RxDBA),
		    (unsigned int)RegRd32(REG_RxDBL),
		    (unsigned int)RegRd32(REG_RxDCA)));
	_PRTK_DUMP((" RxSBA:0x%08x  RxSBL/CL:0x%08x  RxSCA:0x%08x\n",
		    (unsigned int)RegRd32(REG_RxSBA),
		    (unsigned int)RegRd32(REG_RxSBL),
		    (unsigned int)RegRd32(REG_RxSCA)));
	_PRTK_DUMP((" RxDEQ:0x%08x  RxSEQ:0x%08x\n",
		    (unsigned int)RegRd32(REG_RxDEQ),
		    (unsigned int)RegRd32(REG_RxSEQ)));
	_PRTK_DUMP((" TxDBA:0x%08x  TxDBL/CL:0x%08x  TxDCA:0x%08x\n",
		    (unsigned int)RegRd32(REG_TxDBA),
		    (unsigned int)RegRd32(REG_TxDBL),
		    (unsigned int)RegRd32(REG_TxDCA)));
	_PRTK_DUMP((" TxSBA:0x%08x  TxSBL/CL:0x%08x  TxSCA:0x%08x\n",
		    (unsigned int)RegRd32(REG_TxSBA),
		    (unsigned int)RegRd32(REG_TxSBL),
		    (unsigned int)RegRd32(REG_TxSCA)));
	_PRTK_DUMP((" TxDEQ:0x%08x\n",(unsigned int)RegRd32(REG_TxDEQ)));
	_PRTK_DUMP((" RxBTH:0x%08x  TxBTH:0x%08x  RxSTH:0x%08x\n",
		    (unsigned int)RegRd32(REG_RxBTH),
		    (unsigned int)RegRd32(REG_TxBTH),
		    (unsigned int)RegRd32(REG_RxSTH)));
	_PRTK_DUMP((" TxSTH:0x%08x  RxDTH:0x%08x  TxDTH:0x%08x\n",
		    (unsigned int)RegRd32(REG_TxSTH),
		    (unsigned int)RegRd32(REG_RxDTH),
		    (unsigned int)RegRd32(REG_TxDTH)));
	_PRTK_DUMP((" MaxFL:0x%08x  RxHL:0x%08x\n",
		    (unsigned int)RegRd32(REG_MaxFL),
		    (unsigned int)RegRd32(REG_RxHL)));
	_PRTK_DUMP((" MACCFG0-3:0x%08x 0x%08x 0x%08x 0x%08x\n",
		    (unsigned int)RegRd32(REG_MACCFG0),
		    (unsigned int)RegRd32(REG_MACCFG1),
		    (unsigned int)RegRd32(REG_MACCFG2),
		    (unsigned int)RegRd32(REG_MACCFG3)));

	/*
	_PRTK_DUMP((" ---INT Controller Reg---\n"));
	_PRTK_DUMP((" RawIrqSts :0x%08x 0x%08x\n", _RegRd(U32,0x80800004),
		    _RegRd(U32,0x80800018)));
	_PRTK_DUMP((" IrqMask   :0x%08x 0x%08x\n", _RegRd(U32,0x80800008),
		    _RegRd(U32,0x8080001c)));
	_PRTK_DUMP((" MaskIrqSts:0x%08x 0x%08x\n", _RegRd(U32,0x80800000),
		    _RegRd(U32,0x80800014)));
	*/


	_PRTK_DUMP(("Dumping private data:\n"));
	_PRTK_DUMP((" d.txStopped:%d  d.idxQueTxSts:%d  d.idxQueTxDescHead:%d  d.idxQueTxDescTail:%d\n",
		    pP->d.txStopped, pP->d.idxQueTxSts, pP->d.idxQueTxDescHead,
		    pP->d.idxQueTxDescTail));
	_PRTK_DUMP((" d.idxQueRxDesc:%d  d.idxQueRxSts:%d\n",
		    pP->d.idxQueRxDesc, pP->d.idxQueRxSts));
}
#endif

#define CRC_PRIME 0xFFFFFFFF
#define CRC_POLYNOMIAL 0x04C11DB6
/*****************************************************************************
* calculate_hash_index()
*****************************************************************************/
static unsigned char
calculate_hash_index(char *pMulticastAddr)
{
	unsigned long CRC;
	unsigned char HashIndex;
	unsigned char AddrByte;
	unsigned char *pC;
	unsigned long HighBit;
	int Byte;
	int Bit;

	CRC = CRC_PRIME;
	pC = pMulticastAddr;

	for (Byte = 0; Byte < 6; Byte++) {
		AddrByte = *pC;
		pC++;

		for (Bit = 8; Bit > 0; Bit--)
		{
			HighBit = CRC >> 31;
			CRC <<= 1;

			if (HighBit ^ (AddrByte & 1))
			{
				CRC ^= CRC_POLYNOMIAL;
				CRC |= 1;
			}

			AddrByte >>= 1;
		}
	}

	for (Bit = 0, HashIndex = 0; Bit < 6; Bit++)
	{
		HashIndex <<= 1;
		HashIndex |= (unsigned char)(CRC & 1);
		CRC >>= 1;
	}

	return HashIndex;
}

/*****************************************************************************
* eth_setMulticastTbl()
*****************************************************************************/
static void
eth_setMulticastTbl(struct net_device *pD, U8 *pBuf)
{
	int i;
	unsigned char position;
	struct dev_mc_list *cur_addr;

	memset(pBuf, 0x00, 8);

	cur_addr = pD->mc_list;
	for (i = 0; i < pD->mc_count; i++, cur_addr = cur_addr->next) {
		if (!cur_addr)
			break;
		if (!(*cur_addr->dmi_addr & 1))
			continue;
		position = calculate_hash_index(cur_addr->dmi_addr);
		pBuf[position >> 3] |= 1 << (position & 0x07);
	}
}

/*****************************************************************************
* eth_indAddrWr()
*****************************************************************************/
static int
eth_indAddrWr(struct net_device *pD, int afp, char *pBuf)
{
	U32 rxctl;
	int i, len;

	afp &= 0x07;
	if (afp == 4 || afp == 5) {
		_PRTK_SWERR(("eth_indAddrWr(): invalid afp value\n"));
		return -1;
	}
	len = (afp == AFP_AFP_HASH) ? 8 : 6;

	rxctl = RegRd32(REG_RxCTL);
	RegWr32(REG_RxCTL, ~RxCTL_SRxON & rxctl);
	RegWr32(REG_AFP, afp);
	for (i = 0; i < len; i++)
		RegWr8(REG_IndAD + i, pBuf[i]);
	RegWr32(REG_RxCTL, rxctl);

	return 0;
}

/*****************************************************************************
* eth_indAddrRd()
*****************************************************************************/
#if 0
static int
eth_indAddrRd(struct net_device *pD, int afp, char *pBuf)
{
	int i, len;

	afp &= 0x07;
	if (afp == 4 || afp == 5) {
		_PRTK_SWERR(("eth_indAddrRd(): invalid afp value\n"));
		return -1;
	}

	RegWr32(REG_AFP, afp);
	len = (afp == AFP_AFP_HASH) ? 8 : 6;
	for (i = 0; i < len; i++)
		pBuf[i] = RegRd8(REG_IndAD + i);

	return 0;
}
#endif

/*****************************************************************************
* eth_rxCtl()
*****************************************************************************/
static int
eth_rxCtl(struct net_device *pD, int sw)
{
	/*
	 * Workaround for MAC lost 60-byte-long frames: must enable
	 * Runt_CRC_Accept bit
	 */
	RegWr32(REG_RxCTL,
		sw ? RegRd32(REG_RxCTL) | RxCTL_SRxON | RxCTL_RCRCA :
		RegRd32(REG_RxCTL) & ~RxCTL_SRxON);

	return 0;
}

/*****************************************************************************
* eth_chkTxLvl()
*****************************************************************************/
static void
eth_chkTxLvl(struct net_device *pD)
{
	struct ep93xxEth_info *pP = pD->priv;
	int idxQTxDescHd;
	int filled;

	idxQTxDescHd = pP->d.idxQueTxDescHead;

	filled = idxQTxDescHd - pP->d.idxQueTxDescTail;
	if (filled < 0)
		filled += LEN_QueTxDesc;

	if (pP->d.txStopped && filled <= (LVL_TxResume + 1)) {
		pP->d.txStopped = 0;
		pD->trans_start = jiffies;
		netif_wake_queue(pD);
		}
}

/*****************************************************************************
* eth_cleanUpTx()
*****************************************************************************/
static int
eth_cleanUpTx(struct net_device *pD)
{
	struct ep93xxEth_info *pP = pD->priv;
	transmitStatus *pQTxSts;
	int idxSts, bi;

	while (pP->s.pQueTxSts[pP->d.idxQueTxSts].f.txfp) {
		idxSts = pP->d.idxQueTxSts;

		pP->d.idxQueTxSts = IdxNext(pP->d.idxQueTxSts,LEN_QueTxSts);
		pQTxSts = &pP->s.pQueTxSts[idxSts];
		if (!pQTxSts->f.txfp) {
			_PRTK_HWFAIL(("eth_cleanUpTx(): QueTxSts[%d]:x%08x is empty\n",
				      idxSts, (int)pQTxSts->w.e0));
			return -1;
	}

		pQTxSts->f.txfp = 0;

		bi = pQTxSts->f.bi;
#if 0
		if (pP->d.idxQueTxDescTail != bi) {
			_PRTK_HWFAIL(("eth_cleanUpTx(): unmatching QTxSts[%d].BI:%d idxQTxDTail:%d\n",
				      idxSts,bi, pP->d.idxQueTxDescTail));
		}
#endif

		if (pP->s.pTxBufDesc[bi].pFreeRtn) {
			(*pP->s.pTxBufDesc[bi].pFreeRtn)(pP->s.pTxBufDesc[bi].vaddr);
			pP->s.pTxBufDesc[bi].pFreeRtn = 0;
		}

		if (pQTxSts->f.txwe) {
			pP->d.stats.tx_packets++;
		} else {
			pP->d.stats.tx_errors++;
			if (pQTxSts->f.lcrs)
				pP->d.stats.tx_carrier_errors++;
			if(pQTxSts->f.txu)
				pP->d.stats.tx_fifo_errors++;
			if(pQTxSts->f.ecoll)
				pP->d.stats.collisions++;
		}

		pP->d.idxQueTxDescTail = IdxNext(pP->d.idxQueTxDescTail,
						 LEN_QueTxDesc);
	}

	return 0;
}

/*****************************************************************************
* eth_restartTx()
*****************************************************************************/
static int
eth_restartTx(struct net_device *pD)
{
	struct ep93xxEth_info *pP = pD->priv;
	int i;

	RegWr32(REG_GIntMsk, RegRd32(REG_GIntMsk) & ~GIntMsk_IntEn);

	RegWr32(REG_TxCTL, RegRd32(REG_TxCTL) & ~TxCTL_STxON);
	RegWr32(REG_BMCtl, RegRd32(REG_BMCtl) | BMCtl_TxDis);

	RegWr32(REG_BMCtl, BMCtl_TxChR | RegRd32(REG_BMCtl));

	for (i = 0; i < LEN_QueTxDesc; i++) {
		if (pP->s.pTxBufDesc[i].pFreeRtn) {
			pP->s.pTxBufDesc[i].pFreeRtn(pP->s.pTxBufDesc[i].vaddr);
			pP->s.pTxBufDesc[i].pFreeRtn = 0;
	}
		pP->d.stats.tx_dropped++;
	}

	memset(pP->s.pQueTxSts, 0, sizeof(pP->s.pQueTxSts[0]) * LEN_QueTxSts);

	pP->d.txStopped = 0;
	pP->d.idxQueTxSts = pP->d.idxQueTxDescHead = pP->d.idxQueTxDescTail =
		0;

	waitOnReg32(pD, REG_BMSts, BMCtl_TxChR, ~BMCtl_TxChR, 1);
	RegWr32(REG_TxSBA, pP->s.phyQueTxSts);
	RegWr32(REG_TxSCA, pP->s.phyQueTxSts);
	RegWr16(REG_TxSBL, sizeof(pP->s.pQueTxSts[0]) * LEN_QueTxSts);
	RegWr16(REG_TxSCL, sizeof(pP->s.pQueTxSts[0]) * LEN_QueTxSts);
	RegWr32(REG_TxDBA, pP->s.phyQueTxDesc);
	RegWr32(REG_TxDCA, pP->s.phyQueTxDesc);
	RegWr16(REG_TxDBL, sizeof(pP->s.pQueTxDesc[0]) * LEN_QueTxDesc);
	RegWr16(REG_TxDCL, sizeof(pP->s.pQueTxDesc[0]) * LEN_QueTxDesc);

	RegWr32(REG_TxCTL, RegRd32(REG_TxCTL) | TxCTL_STxON);
	RegWr32(REG_BMCtl, RegRd32(REG_BMCtl) | BMCtl_TxEn);

	RegWr32(REG_GIntMsk, RegRd32(REG_GIntMsk) | GIntMsk_IntEn);

	return 0;
}

/*****************************************************************************
* eth_reset()
*****************************************************************************/
static int
eth_reset(struct net_device *pD)
{
	int err;

	RegWr8(REG_SelfCTL, SelfCTL_RESET);
	err = waitOnReg32(pD, REG_SelfCTL, SelfCTL_RESET, ~SelfCTL_RESET, 1);
	if (err)
		_PRTK_WARN(("eth_reset(): Soft Reset does not self-clear\n"));

	//phy_reset(pD);

	return 0;
}

/*****************************************************************************
 . Function: eth_shutDown()
 . Purpose:  closes down the Ethernet module
 . Make sure to:
 .	1. disable all interrupt mask
 .	2. disable Rx
 .	3. disable Tx
 .
 . TODO:
 .   (1) maybe utilize power down mode.
 .	Why not yet?  Because while the chip will go into power down mode,
 .	the manual says that it will wake up in response to any I/O requests
 .	in the register space.   Empirical results do not show this working.
*
*****************************************************************************/
static int
eth_shutDown(struct net_device *pD)
{
	eth_reset(pD);

	return 0;
}

/*****************************************************************************
*  eth_enable()

  Purpose:
        Turn on device interrupt for interrupt driven operation.
        Also turn on Rx but no Tx.
*
*****************************************************************************/
static int
eth_enable(struct net_device *pD)
{
	RegWr32(REG_IntEn, Default_IntSrc);
	RegWr32(REG_GIntMsk, GIntMsk_IntEn);
	eth_rxCtl(pD, 1);

	return 0;
}

/*****************************************************************************
*  eth_init()

  Purpose:
        Reset and initialize the device.
        Device should be initialized enough to function in polling mode.
        Tx and Rx must be disabled and no INT generation.
*
*****************************************************************************/
static int
eth_init(struct net_device *pD)
{
	int status;

	eth_reset(pD);

	gPhyAutoNegoDone = 0;
	status = phy_init(pD);
	if (status != 0)
		printk(KERN_WARNING "%s: No network cable detected!\n", pD->name);

	RegWr32(REG_SelfCTL, 0x0f00);
	RegWr32(REG_GIntMsk, 0x00);
	RegWr32(REG_RxCTL, RxCTL_BA | RxCTL_IA0);
	RegWr32(REG_TxCTL, 0x00);
	RegWr32(REG_GT, 0x00);
	RegWr32(REG_BMCtl, 0x00);
	RegWr32(REG_RxBTH, (0x80 << 16) | (0x40 << 0));
	RegWr32(REG_TxBTH, (0x80 << 16) | (0x40 << 0));
	RegWr32(REG_RxSTH, (4 << 16) | (2 << 0));
	RegWr32(REG_TxSTH, (4 << 16) | (2 << 0));
	RegWr32(REG_RxDTH, (4 << 16) | (2 << 0));
	RegWr32(REG_TxDTH, (4 << 16) | (2 << 0));
	RegWr32(REG_MaxFL, ((1518 + 1) << 16) | (944 << 0));

	RegRd32(REG_TxCollCnt);
	RegRd32(REG_RxMissCnt);
	RegRd32(REG_RxRntCnt);

	RegRd32(REG_IntStsC);

	RegWr32(REG_TxCTL, TxCTL_STxON | RegRd32(REG_TxCTL));

	eth_indAddrWr(pD, AFP_AFP_IA0, &pD->dev_addr[0]);

	devQue_start(pD);

	return 0;
}

/*****************************************************************************
* eth_isrRx()
*
*  Interrupt Service Routines
*
*****************************************************************************/
static int
eth_isrRx(struct net_device *pD)
{
	ep93xxEth_info *pP = pD->priv;
	receiveStatus *pQRxSts;
	int idxQRxStsHead;
	int idxSts;
	int cntStsProcessed, cntDescProcessed;
	char *pDest;
	struct sk_buff *pSkb;
	int len;
	UINT dt;

	dt = RegRd32(REG_RxSCA);
	idxQRxStsHead = (dt - pP->s.phyQueRxSts) / sizeof(pP->s.pQueRxSts[0]);
	if (!(idxQRxStsHead >= 0 && idxQRxStsHead < LEN_QueRxSts)) {
		_PRTK_HWFAIL(("eth_isrRx(): invalid REG_RxSCA:0x%x idx:%d (phyQueRxSts:0x%x Len:%x)\n",
			      dt,idxQRxStsHead, (int)pP->s.phyQueRxSts,
			      LEN_QueRxSts));
		return -1;
	}

	cntStsProcessed = cntDescProcessed = 0;
	while (idxQRxStsHead != pP->d.idxQueRxSts) {
		idxSts = pP->d.idxQueRxSts;
		pP->d.idxQueRxSts = IdxNext(pP->d.idxQueRxSts, LEN_QueRxSts);
		pQRxSts = &pP->s.pQueRxSts[idxSts];
		if (!pQRxSts->f.rfp) {
			_PRTK_HWFAIL(("eth_isrRx(): QueRxSts[%d] is empty; Hd:%d\n",
				      idxSts,idxQRxStsHead));
			return -1;
		}
		pQRxSts->f.rfp = 0;

		if(pQRxSts->f.eob) {
			if(pQRxSts->f.bi == pP->d.idxQueRxDesc) {
				pP->d.idxQueRxDesc =
					IdxNext(pP->d.idxQueRxDesc,
						LEN_QueRxDesc);
				cntDescProcessed++;
				if (pQRxSts->f.eof && pQRxSts->f.rwe) {
					len = pQRxSts->f.fl;
					pSkb = dev_alloc_skb(len + 5);
					if (pSkb != NULL) {
						skb_reserve(pSkb, 2);
						pSkb->dev = pD;
						pDest = skb_put(pSkb, len);

						memcpy(pDest,
						       pP->s.pRxBufDesc[pQRxSts->f.bi].vaddr,
						       len);
						pSkb->protocol =
							eth_type_trans(pSkb,
								       pD);
						netif_rx(pSkb);
						pP->d.stats.rx_packets++;
						if(pQRxSts->f.am == 3)
							pP->d.stats.multicast++;
					} else
						_PRTK_SYSFAIL(("eth_isrRx(): Low Memory, Rx dropped\n"));
						pP->d.stats.rx_dropped++;
				} else {
					pP->d.stats.rx_errors++;
					if (pQRxSts->f.oe)
						pP->d.stats.rx_fifo_errors++;
					if (pQRxSts->f.fe)
						pP->d.stats.rx_frame_errors++;
					if (pQRxSts->f.runt ||
					    pQRxSts->f.edata)
						pP->d.stats.rx_length_errors++;
					if (pQRxSts->f.crce)
						pP->d.stats.rx_crc_errors++;
				}
			} else
				_PRTK_HWFAIL(("eth_isrRx(): unmatching QueRxSts[%d].BI:0x%x; idxQueRxDesc:0x%x\n",
					      idxSts, pQRxSts->f.bi,
					      pP->d.idxQueRxDesc));
		}

		cntStsProcessed++;
	}

	RegWr32(REG_RxSEQ, cntStsProcessed);
	RegWr32(REG_RxDEQ, cntDescProcessed);

	return 0;
}

/*****************************************************************************
* eth_isrTx()
*****************************************************************************/
static int
eth_isrTx(struct net_device *pD)
{
	eth_cleanUpTx(pD);
	eth_chkTxLvl(pD);
	return 0;
}

/*****************************************************************************
* ep93xxEth_isr()
*****************************************************************************/
static irqreturn_t
ep93xxEth_isr(int irq,void *pDev,struct pt_regs *pRegs)
{
	struct net_device *pD = pDev;
	int lpCnt;
	U32 intS;

	lpCnt = 0;
	do {
		intS = RegRd32(REG_IntStsC);

		if (!intS)
			break;
		if (intS & IntSts_RxSQ)
			eth_isrRx(pD);
		if (intS & IntSts_TxSQ)
			eth_isrTx(pD);
	} while (lpCnt++ < 64);

	if (lpCnt)
		return IRQ_HANDLED;
	else
		return IRQ_NONE;
}

/*=========================================================
 *  Exposed Driver Routines to the Outside World
 *=======================================================*/

/*****************************************************************************
* ep93xxEth_getStats()
*****************************************************************************/
static struct net_device_stats *
ep93xxEth_getStats(struct net_device *pD)
{
	return &((struct ep93xxEth_info *)pD->priv)->d.stats;
}

/*****************************************************************************
* ep93xxEth_setMulticastList()
*****************************************************************************/
static void
ep93xxEth_setMulticastList(struct net_device *pD)
{
	U8 tblMulti[8 + 1];

	if (pD->flags & IFF_PROMISC)
		RegWr32(REG_RxCTL, RxCTL_PA | RegRd32(REG_RxCTL));
	else if(pD->flags & IFF_ALLMULTI) {
		RegWr32(REG_RxCTL, RxCTL_MA |
			(~RxCTL_PA & RegRd32(REG_RxCTL)));
		eth_indAddrWr(pD, AFP_AFP_HASH,
			      "\xff\xff\xff\xff\xff\xff\xff\xff");
	} else if(pD->mc_count) {
		RegWr32(REG_RxCTL, RxCTL_MA |
			(~RxCTL_PA & RegRd32(REG_RxCTL)));
		eth_setMulticastTbl(pD, &tblMulti[0]);
		eth_indAddrWr(pD, AFP_AFP_HASH, &tblMulti[0]);
	} else
		RegWr32(REG_RxCTL,
			~(RxCTL_PA | RxCTL_MA) & RegRd32(REG_RxCTL));
}

/*****************************************************************************
* ep93xxEth_txTimeout()
*****************************************************************************/
static void
ep93xxEth_txTimeout(struct net_device *pD)
{
	int status;

	_PRTK_WARN(("ep93xxEth_txTimeout(): transmit timed out\n"));

	if (gPhyAutoNegoDone == 0) {
		status = phy_init(pD);
		if (status != 0)
		{
			printk(KERN_WARNING "%s: No network cable detected!\n",
			       pD->name);
			return;
		}
	}

	eth_restartTx(pD);

	pD->trans_start = jiffies;
	netif_wake_queue(pD);
}

/*****************************************************************************
* ep93xxEth_hardStartXmit()
*****************************************************************************/
static int
ep93xxEth_hardStartXmit(struct sk_buff *pSkb, struct net_device *pD)
{
	struct ep93xxEth_info *pP = pD->priv;
	transmitDescriptor *pQTxDesc;
	int idxQTxDescHd;
	int filled;
	int status;

	if (gPhyAutoNegoDone == 0) {
		status = phy_init(pD);
		if (status != 0)
		{
			return 1;
		}
	}

	idxQTxDescHd = pP->d.idxQueTxDescHead;
	pQTxDesc = &pP->s.pQueTxDesc[idxQTxDescHd];

	filled = idxQTxDescHd - pP->d.idxQueTxDescTail;
	if (filled < 0)
		filled += LEN_QueTxDesc;
	filled += 1;

	if(filled >= LVL_TxStop) {
		netif_stop_queue(pD);
		pP->d.txStopped = 1;
		if(filled > LVL_TxStop) {
			_PRTK_SYSFAIL(("ep93xxEth_hardStartXmit(): a Tx Request while stop\n"));
			return 1;
		}
	}

	if (pSkb->len < 60) {
		pQTxDesc->f.bl = 60;
		memset(pP->s.pTxBufDesc[idxQTxDescHd].vaddr, 0, 60);
	} else
		pQTxDesc->f.bl = pSkb->len;
	pQTxDesc->f.ba = pP->s.phyTxBuf+(idxQTxDescHd * LEN_TxBuf);//virt_to_bus(pP->s.pTxBufDesc[idxQTxDescHd].vaddr);
	pQTxDesc->f.bi = idxQTxDescHd;
	pQTxDesc->f.af = 0;
	pQTxDesc->f.eof = 1;

	memcpy(pP->s.pTxBufDesc[idxQTxDescHd].vaddr, pSkb->data, pSkb->len);
	pP->s.pTxBufDesc[idxQTxDescHd].pFreeRtn = 0;

	free_skb(pSkb);

	pP->d.idxQueTxDescHead = IdxNext(pP->d.idxQueTxDescHead,
					 LEN_QueTxDesc);
	RegWr32(REG_TxDEQ, 1);

	return 0;
}

/*****************************************************************************
 . ep93xxEth_close()
 .
 . this makes the board clean up everything that it can
 . and not talk to the outside world.   Caused by
 . an 'ifconfig ethX down'
 *
*****************************************************************************/
static int
ep93xxEth_close(struct net_device *pD)
{
	netif_stop_queue(pD);
	eth_shutDown(pD);

	/*MOD_DEC_USE_COUNT;*/

	return 0;
}

/*******************************************************
 * ep93xxEth_open()
 *
 * Open and Initialize the board
 *
 * Set up everything, reset the card, etc ..
 *
 ******************************************************/
static int
ep93xxEth_open(struct net_device *pD)
{
	int status;
	struct ep93xxEth_info  *pP = pD->priv;

	memset(&pP->d, 0, sizeof(pP->d));

	/*MOD_INC_USE_COUNT;*/

	status = eth_init(pD);
	if (status != 0 )
		return -EAGAIN;

	eth_enable(pD);

#if 0
	_dbg_phy_dumpReg(pD);
	_dbg_ep93xxeth_dumpReg(pD);
	_dbg_ep93xxEth_dumpQueues(pD);
#endif

	netif_start_queue(pD);

	return 0;
}

/*****************************************************************************
 .
 . ep93xxEth_probe( struct net_device * dev )
 .   This is the first routine called to probe device existance
 .   and initialize the driver if the device found.
 .
 .   Input parameters:
 .	dev->base_addr == 0, try to find all possible locations
 .	dev->base_addr == 1, return failure code
 .	dev->base_addr == 2, always allocate space,  and return success
 .	dev->base_addr == <anything else>   this is the address to check
 .
 .   Output:
 .	0 --> there is a device
 .	anything else, error

*****************************************************************************/
int __init
ep93xxEth_probe(struct net_device *pD) {
	struct ep93xxEth_info *pP;
	int err;
	int i;

	if (pD->priv == 0) {
		pD->priv = kmalloc(sizeof(struct ep93xxEth_info), GFP_KERNEL);
		if(pD->priv == 0)
			return -ENOMEM;
	}
	memset(pD->priv, 0x00, sizeof(struct ep93xxEth_info));

	pP = pD->priv;
	pP->s.id = numOfInstance;
	pP->s.miiIdPhy = 1;

#ifdef CONFIG_FALINUX_EP9312
	GET_MAC_ADDR(pD->dev_addr);
#else
	for (i = 0; i < 6; i++)
		pD->dev_addr[i] = default_mac[i];
	GET_MAC_ADDR(pD, pD->dev_addr);
#endif

	err = (int)request_irq(pD->irq, &ep93xxEth_isr, 0, cardname, pD);
	if(err) {
		kfree(pD->priv);
		return -EAGAIN;
	}

	pD->open               = &ep93xxEth_open;
	pD->stop               = &ep93xxEth_close;
	pD->hard_start_xmit    = &ep93xxEth_hardStartXmit;
	pD->tx_timeout         = &ep93xxEth_txTimeout;
	pD->watchdog_timeo     = HZ * 5;
	pD->get_stats          = &ep93xxEth_getStats;
	pD->set_multicast_list = &ep93xxEth_setMulticastList;

	ether_setup(pD);

	devQue_init(pD);
	eth_reset(pD);

	numOfInstance++;

	err = register_netdev(pD);
	if (err) {
		free_irq(pD->irq, pD);
		kfree(pP);
		return err;
	}

	return 0;
}

static int
ep93xxEth_drv_probe(struct platform_device *pdev)
{
	/*struct platform_device *pdev = to_platform_device(dev);*/
	struct net_device *ndev;
	struct resource *res;
	int ret;

	PRINTK("ep93xxEth_drv_probe init\n");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res){
		printk("platform_get_resource fail\n");
		return -ENODEV;
	}

	if (!request_mem_region(res->start, 0x10000, cardname)){
		printk("request_mem_region fail\n");
		return -EBUSY;
	}

	ndev = alloc_etherdev(sizeof(struct ep93xxEth_info));
	if (!ndev) {
		release_mem_region(res->start, 0x10000);
		printk("alloc_etherdev fail\n");
		return -ENOMEM;
	}

	/*SET_MODULE_OWNER(ndev);*/
	/*SET_NETDEV_DEV(ndev, dev);*/
	SET_NETDEV_DEV(ndev, &pdev->dev);

	/*ndev->base_addr = (unsinged int )HwRegToVirt(res->start);*/
	ndev->base_addr = (unsigned long)ioremap(res->start, 0x10000);	
	if (!ndev->base_addr) {		
		printk("ioremap failed\n");		
		release_mem_region(res->start, 0x10000);
		return -ENOMEM;	
	}
	ndev->irq = platform_get_irq(pdev, 0);

	/*dev_set_drvdata(dev, ndev);*/
	dev_set_drvdata(&pdev->dev, ndev);

	ret = ep93xxEth_probe(ndev);
	if (ret != 0) {
		free_netdev(ndev);
		release_mem_region(res->start, 0x10000);
		printk("ep93xxEth_probe fail\n");
		return ret;
	}

	return 0;
}

static int
ep93xxEth_drv_remove(struct platform_device *pdev)
{
	/*struct platform_device *pdev = to_platform_device(dev);*/
	struct net_device *ndev = dev_get_drvdata(&pdev->dev);
	struct resource *res;

	dev_set_drvdata(&pdev->dev, NULL);

	unregister_netdev(ndev);

	free_irq(ndev->irq, ndev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, 0x10000);

	free_netdev(ndev);

	return 0;
}

static struct platform_driver /*ep93xxEth_driver*/ep93xx_eth_driver = {
	/*.name		= cardname,*/
	/*.bus		= &platform_bus_type,*/
	.probe		= ep93xxEth_drv_probe,
	.remove		= __devexit_p(ep93xxEth_drv_remove),
	.driver		= {
		.name	= cardname,
		.owner	= THIS_MODULE,
	},
};

static int __init ep93xxEth_init(void)
{
	printk("ep93xx Ethernet support initialized\n");
	return platform_driver_register(&ep93xx_eth_driver/*&ep93xxEth_driver*/);
}

static void __exit ep93xxEth_cleanup(void)
{
	platform_driver_unregister(&ep93xx_eth_driver/*&ep93xxEth_driver*/);
}

module_init(ep93xxEth_init);
module_exit(ep93xxEth_cleanup);

MODULE_AUTHOR("Cirrus Logic");
MODULE_DESCRIPTION("EP93xx ethernet driver");
MODULE_LICENSE("GPL");
