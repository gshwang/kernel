/*
 * linux/drivers/usb/gadget/ep93xx_udc.h
 * Cirruss EP93xx and ISP1581 on-chip high  speed USB device controller
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
 */

#ifndef __LINUX_USB_GADGET_EP931X_H
#define __LINUX_USB_GADGET_EP931X_H

#include <linux/types.h>

struct ep93xx_udc;

struct ep93xx_ep {
	struct usb_ep		                    ep;
	struct ep93xx_udc			            *dev;

	const struct usb_endpoint_descriptor	*desc;
	struct list_head			            queue;
	unsigned long				            pio_irqs;
	unsigned long				            dma_irqs;
	short					                dma; 
	unsigned short 						usSemStatus;
	unsigned short 						usEPStop;
	unsigned short				            fifo_size;
	u8					                    bEndpointAddress;
	u8					                    bmAttributes;

	unsigned				stopped : 1;
	unsigned				dma_fixup : 1;
	struct semaphore                wait_dio_cmd;						 

};

struct ep93xx_request {
	struct usb_request			req;
	struct list_head			queue;
};

enum ep0_state { 
	EP0_IDLE,
	EP0_IN_DATA_PHASE,
	EP0_OUT_DATA_PHASE,
	EP0_IN_STATUS_PHASE,
	EP0_OUT_STATUS_PHASE,
	EP0_END_XFER,
	EP0_STALL,
};

#define EP0_FIFO_SIZE	        ((unsigned)64)
/*#define BULK_FIFO_SIZE	((unsigned)64)*/
#define BULK_FIFO_SIZE          ((unsigned)512)

#define ISO_FIFO_SIZE	        ((unsigned)256)
#define INT_FIFO_SIZE	        ((unsigned)8)

struct udc_stats {
	struct ep0stats {
		unsigned long		ops;
		unsigned long		bytes;
	} read, write;
	unsigned long			irqs;
};


#define	EP93_UDC_NUM_ENDPOINTS	6/*setup ,control in/out,bulk in/out,int in*/

struct ep93xx_udc {
	struct usb_gadget			gadget;
	struct usb_gadget_driver	*driver;
	struct usb_ctrlrequest		*sControlOut;
	struct usb_request			*ep0req;	// For control responses
	struct ep93xx_request		*epBulkReq;	// For control responses

	enum ep0_state				ep0state;
	struct udc_stats			stats;
	unsigned				    got_irq : 1,
						        got_disc : 1,
						        has_cfr : 1,
						        req_pending : 1,
						        req_std : 1,
						        req_config : 1,
							removing_gadget : 1;

#define start_watchdog(dev) mod_timer(&dev->timer, jiffies + (HZ/200))
	struct timer_list			timer;

	struct device				*dev;
	struct ep93xx_udc_mach_info	*mach;
	u64					        dma_mask;
	struct ep93xx_ep			ep [EP93_UDC_NUM_ENDPOINTS];
};

/*-------------------------------------------------------------------------*/

static struct ep93xx_udc *the_controller;


/*-------------------------------------------------------------------------*/

/*
 * Debugging support vanishes in non-debug builds.  DBG_NORMAL should be
 * mostly silent during normal use/testing, with no timing side-effects.
 */
#define DBG_NORMAL	1	/* error paths, device state transitions */
#define DBG_VERBOSE	2	/* add some success path trace info */
#define DBG_NOISY	3	/* ... even more: request level */
#define DBG_VERY_NOISY	4	/* ... even more: packet level */


#define DBG(lvl, stuff...) do{if ((lvl) <= UDC_DEBUG) DMSG(stuff);}while(0)

#define WARN(stuff...) printk(KERN_WARNING "udc: " stuff)
#define INFO(stuff...) printk(KERN_INFO "udc: " stuff)

/*--------------------------------------------------------------------------------*/
/*
  * Defines for the USB specification and the Philips ISP1581 USB controller.
  *
  * Copyright (c) 2004 Cirrus Logic, Inc.
  */

/*
  *USB2.0  Slave Based on EDB931x
  *default Use the CS7
  * 
  *USB2.0 SLAVE Memory Space
  */

#define ep93IOBASE                /*IO_BASE_VIRT*/EP93XX_AHB_VIRT_BASE
#define USB_WINDOW_ADDR           0x70000000
#define USB_WINDOW_SIZE           0x1000

/*
  *USB2.0  Slave Based on EDB931x
  *Use the extern INT1
  */
//#define USBSlaveISRNumber    IRQ_EXT1


/*
  *USB2.0  Slave Based on EDB931x
  *We support USB Mass Storage Class,Bulk Only protocol
  */
#define CLASS_MASS_STORAGE 		0x08
#define SUBCLASS_SCSI   		0x06
#define PROTOCOL_BULK   		0x50


/*
  *USB2.0  Slave Based on EDB931x
  *We use ISP1581 as USB Slave Controller
  *We use ISP1581 EP1out as OUT Endpoint , EP2in as IN Endpoint
  */
#define  USBSlaveBulkOutEndPoint   	USB_ENDPOINT_ONE_OUT

#define  USBSlaveBulkInEndPoint     USB_ENDPOINT_TWO_IN



/*---------------------------------------------------------------------------------*/

/****************************************************************************/

/*
  * USB2_slave_hw.H - Defines for the USB specification and the Philips ISP1581 USB
  *         controller.
  *
  * Copyright (c) 2004 Cirrus Logic, Inc.
  */

/****************************************************************************/

/*
  * The following defines are specific the the Philips ISP1581 USB controller.
  */


/*
  * The offsets of the individual registers in the ISP1581 USB controller.
  */
#define HwUSBAddress                            0x00000000
#define HwUSBEndpointMaxPacketSize              0x00000004
#define HwUSBEndpointType                       0x00000008
#define HwUSBMode                               0x0000000c
#define HwUSBIntConfig                          0x00000010
#define HwUSBIntEnable                          0x00000014
#define HwUSBIntReason                          0x00000018
#define HwUSBEndpointBufferLength               0x0000001c
#define HwUSBEndpointData                       0x00000020
#define HwUSBEndpointShortPacket                0x00000024
#define HwUSBEndpointControl                    0x00000028
#define HwUSBEndpointIndex                      0x0000002c
#define HwUSBDMACommand                         0x00000030
#define HwUSBDMACount                           0x00000034
#define HwUSBDMAConfig                          0x00000038
#define HwUSBDMAHardware                        0x0000003c
#define HwUSBDMAIntReason                       0x00000050
#define HwUSBDMAIntEnable                       0x00000054
#define HwUSBDMAEndpoint                        0x00000058
#define HwUSBDMAStrobeTiming                    0x00000060
#define HwUSBChipID                             0x00000070
#define HwUSBFrameNumber                        0x00000074
#define HwUSBScratch                            0x00000078
#define HwUSBUnlock                             0x0000007c
#define HwUSBTest                               0x00000084

/*
  * Definitions of the bit fields in the Address register.
  */
#define USB_ADDRESS_DEVICE_ENABLE               0x00000080
#define USB_ADDRESS_DEVICE_ADDR_MASK            0x0000007f
#define USB_ADDRESS_DEVICE_ADDR_SHIFT           0

/*
  * Definitions of the bit fields in the EndpointMaxPacketSize register.
  */
#define USB_EPMAXPACKET_NTRANS_MASK             0x00001800
#define USB_EPMAXPACKET_NTRANS_ONE              0x00000000
#define USB_EPMAXPACKET_NTRANS_TWO              0x00000800
#define USB_EPMAXPACKET_NTRANS_THREE            0x00001000
#define USB_EPMAXPACKET_FIFO_SIZE_MASK          0x000007ff
#define USB_EPMAXPACKET_FIFO_SIZE_SHIFT         0

/*
  * Definitions of the bit fields in the EndpointType register.
  */
#define USB_EPTYPE_NO_EMPTY                     0x00000010
#define USB_EPTYPE_ENABLE                       0x00000008
#define USB_EPTYPE_DOUBLE_BUFFER                0x00000004
#define USB_EPTYPE_TYPE_MASK                    0x00000003
#define USB_EPTYPE_TYPE_CONTROL                 0x00000000
#define USB_EPTYPE_TYPE_ISOCHRONOUS             0x00000001
#define USB_EPTYPE_TYPE_BULK                    0x00000002
#define USB_EPTYPE_TYPE_INTERRUPT               0x00000003

/*
  * Definitions of the bit fields in the Mode register.
  */
#define USB_MODE_CLOCK_ON                       0x00000080
#define USB_MODE_SEND_RESUME                    0x00000040
#define USB_MODE_GO_SUSPEND                     0x00000020
#define USB_MODE_SOFT_RESET                     0x00000010
#define USB_MODE_INT_ENABLE                     0x00000008
#define USB_MODE_WAKE_UP_CS                     0x00000004
#define USB_MODE_SOFT_CONNECT                   0x00000001

/*
  * Definitions of the bit fields in the IntConfig register.
  */
#define USB_INTCONFIG_CDBGMOD_MASK              0x000000c0
#define USB_INTCONFIG_CDBGMOD_ALL               0x00000000
#define USB_INTCONFIG_CDBGMOD_ACK               0x00000040
#define USB_INTCONFIG_CDBGMOD_ACK_1NAK          0x00000080
#define USB_INTCONFIG_DDBGMODIN_MASK            0x00000030
#define USB_INTCONFIG_DDBGMODIN_ALL             0x00000000
#define USB_INTCONFIG_DDBGMODIN_ACK             0x00000010
#define USB_INTCONFIG_DDBGMODIN_ACK_1NAK        0x00000020
#define USB_INTCONFIG_DDBGMODOUT_MASK           0x0000000c
#define USB_INTCONFIG_DDBGMODOUT_ALL            0x00000000
#define USB_INTCONFIG_DDBGMODOUT_ACK            0x00000004
#define USB_INTCONFIG_DDBGMODOUT_ACK_1NAK       0x00000008
#define USB_INTCONFIG_INTLVL                    0x00000002
#define USB_INTCONFIG_INTPOL                    0x00000001

/*
  * Definitions of the bit fields in the IntEnable and IntReason registers.
  */
#define USB_INT_EP7_TX                          0x02000000
#define USB_INT_EP7_RX                          0x01000000
#define USB_INT_EP6_TX                          0x00800000
#define USB_INT_EP6_RX                          0x00400000
#define USB_INT_EP5_TX                          0x00200000
#define USB_INT_EP5_RX                          0x00100000
#define USB_INT_EP4_TX                          0x00080000
#define USB_INT_EP4_RX                          0x00040000
#define USB_INT_EP3_TX                          0x00020000
#define USB_INT_EP3_RX                          0x00010000
#define USB_INT_EP2_TX                          0x00008000
#define USB_INT_EP2_RX                          0x00004000
#define USB_INT_EP1_TX                          0x00002000
#define USB_INT_EP1_RX                          0x00001000
#define USB_INT_EP0_TX                          0x00000800
#define USB_INT_EP0_RX                          0x00000400
#define USB_INT_EP0_SETUP                       0x00000100
#define USB_INT_DMA                             0x00000040
#define USB_INT_HS_STATUS                       0x00000020
#define USB_INT_RESUME                          0x00000010
#define USB_INT_SUSPEND                         0x00000008
#define USB_INT_PSEUDO_SOF                      0x00000004
#define USB_INT_SOF                             0x00000002
#define USB_INT_BUS_RESET                       0x00000001

/*
  * Definitions of the bit fields in the EndpointBufferLength register.
  */
#define USB_EPBUFLEN_MASK                       0x0000ffff
#define USB_EPBUFLEN_SHIFT                      0

/*
  * Definitions of the bit fields in the EndpointData register.
  */
#define USB_EPDATA_MASK                         0x0000ffff
#define USB_EPDATA_SHIFT                        0

/*
  * Definitions of the bit fields in the EndpointShortPacket register.
  */

/* This register is reserved.*/

/*
  * Definitions of the bit fields in the EndpointControl register.
  */
#define USB_EPCONTROL_CLEAR                     0x00000010
#define USB_EPCONTROL_VALIDATE                  0x00000008
#define USB_EPCONTROL_STATUS_ACK                0x00000002
#define USB_EPCONTROL_STALL                     0x00000001

/*
  * Definitions of the bit fields in the EndpointIndex register.
  */
#define USB_ENDPOINT_CONTROL_OUT                0x00000000
#define USB_ENDPOINT_CONTROL_IN                 0x00000001
#define USB_ENDPOINT_ONE_OUT                    0x00000002
#define USB_ENDPOINT_ONE_IN                     0x00000003
#define USB_ENDPOINT_TWO_OUT                    0x00000004
#define USB_ENDPOINT_TWO_IN                     0x00000005
#define USB_ENDPOINT_THREE_OUT                  0x00000006
#define USB_ENDPOINT_THREE_IN                   0x00000007
#define USB_ENDPOINT_FOUR_OUT                   0x00000008
#define USB_ENDPOINT_FOUR_IN                    0x00000009
#define USB_ENDPOINT_FIVE_OUT                   0x0000000a
#define USB_ENDPOINT_FIVE_IN                    0x0000000b
#define USB_ENDPOINT_SIX_OUT                    0x0000000c
#define USB_ENDPOINT_SIX_IN                     0x0000000d
#define USB_ENDPOINT_SEVEN_OUT                  0x0000000e
#define USB_ENDPOINT_SEVEN_IN                   0x0000000f
#define USB_ENDPOINT_SETUP                      0x00000020

/*
  * Definitions of the bit fields in the DMACommand register.
  */
#define USB_DMACOMMAND_MASK                     0x000000ff
#define USB_DMACOMMAND_GDMA_READ                0x00000000
#define USB_DMACOMMAND_GDMA_WRITE               0x00000001
#define USB_DMACOMMAND_UDMA_READ                0x00000002
#define USB_DMACOMMAND_UDMA_WRITE               0x00000003
#define USB_DMACOMMAND_PIO_READ                 0x00000004
#define USB_DMACOMMAND_PIO_WRITE                0x00000005
#define USB_DMACOMMAND_MDMA_READ                0x00000006
#define USB_DMACOMMAND_MDMA_WRITE               0x00000007
#define USB_DMACOMMAND_READ_1F0                 0x0000000a
#define USB_DMACOMMAND_POLL_BSY                 0x0000000b
#define USB_DMACOMMAND_READ_TASK_FILE           0x0000000c
#define USB_DMACOMMAND_VALIDATE_BUFFER          0x0000000e
#define USB_DMACOMMAND_CLEAR_BUFFER             0x0000000f
#define USB_DMACOMMAND_RESTART                  0x00000010
#define USB_DMACOMMAND_RESET_DMA                0x00000011
#define USB_DMACOMMAND_MDMA_STOP                0x00000012

/*
  * Definitions of the bit fields in the DMACount register.
  */
#define USB_DMACOUNT_MASK                       0xffffffff
#define USB_DMACOUNT_SHIFT                      0

/*
  * Definitions of the bit fields in the DMAConfig register.
  */
#define USB_DMACONFIG_IGNORE_IORDY              0x00004000
#define USB_DMACONFIG_ATA_MODE                  0x00002000
#define USB_DMACONFIG_DMA_MODE_MASK             0x00001800
#define USB_DMACONFIG_DMA_MODE_0                0x00000000
#define USB_DMACONFIG_DMA_MODE_1                0x00000800
#define USB_DMACONFIG_DMA_MODE_2                0x00001000
#define USB_DMACONFIG_DMA_MODE_3                0x00001800
#define USB_DMACONFIG_PIO_MODE_MASK             0x00000700
#define USB_DMACONFIG_PIO_MODE_0                0x00000000
#define USB_DMACONFIG_PIO_MODE_1                0x00000100
#define USB_DMACONFIG_PIO_MODE_2                0x00000200
#define USB_DMACONFIG_PIO_MODE_3                0x00000300
#define USB_DMACONFIG_PIO_MODE_4                0x00000400
#define USB_DMACONFIG_DIS_XFER_CNT              0x00000080
#define USB_DMACONFIG_BURST_MASK                0x00000070
#define USB_DMACONFIG_BURST_ALL                 0x00000000
#define USB_DMACONFIG_BURST_1                   0x00000010
#define USB_DMACONFIG_BURST_2                   0x00000020
#define USB_DMACONFIG_BURST_4                   0x00000030
#define USB_DMACONFIG_BURST_8                   0x00000040
#define USB_DMACONFIG_BURST_12                  0x00000050
#define USB_DMACONFIG_BURST_16                  0x00000060
#define USB_DMACONFIG_BURST_32                  0x00000070
#define USB_DMACONFIG_MODE_MASK                 0x0000000c
#define USB_DMACONFIG_MODE_DIOR_DIOW            0x00000000
#define USB_DMACONFIG_MODE_DIOR_DACK            0x00000004
#define USB_DMACONFIG_MODE_DACK                 0x00000008
#define USB_DMACONFIG_WIDTH_MASK                0x00000001
#define USB_DMACONFIG_WIDTH_8                   0x00000000
#define USB_DMACONFIG_WIDTH_16                  0x00000001

/*
  * Definitions of the bit fields in the DMAHardware register.
  */
#define USB_DMAHW_ENDIAN_MASK                   0x000000c0
#define USB_DMAHW_ENDIAN_NORMAL                 0x00000000
#define USB_DMAHW_ENDIAN_SWAP                   0x00000040
#define USB_DMAHW_EOT_MASK                      0x00000020
#define USB_DMAHW_EOT_ACTIVE_LOW                0x00000000
#define USB_DMAHW_EOT_ACTIVE_HIGH               0x00000020
#define USB_DMAHW_MASTER                        0x00000010
#define USB_DMAHW_ACK_MASK                      0x00000008
#define USB_DMAHW_ACK_ACTIVE_LOW                0x00000000
#define USB_DMAHW_ACK_ACTIVE_HIGH               0x00000008
#define USB_DMAHW_DREQ_MASK                     0x00000004
#define USB_DMAHW_DREQ_ACTIVE_LOW               0x00000000
#define USB_DMAHW_DREQ_ACTIVE_HIGH              0x00000004
#define USB_DMAHW_WRITE_MASK                    0x00000002
#define USB_DMAHW_WRITE_ACTIVE_LOW              0x00000000
#define USB_DMAHW_WRITE_ACTIVE_HIGH             0x00000002
#define USB_DMAHW_READ_MASK                     0x00000001
#define USB_DMAHW_READ_ACTIVE_LOW               0x00000000
#define USB_DMAHW_READ_ACTIVE_HIGH              0x00000001

/*
  * Definitions of the bit fields in the DMAIntReason and DMAIntEnable
  * registers.
  */
#define USB_DMAINT_ODD                          0x00001000
#define USB_DMAINT_EXT_EOT                      0x00000800
#define USB_DMAINT_INT_EOT                      0x00000400
#define USB_DMAINT_INTRQ_PENDING                0x00000200
#define USB_DMAINT_XFER_OK                      0x00000100
#define USB_DMAINT_1F0_WF_E                     0x00000080
#define USB_DMAINT_1F0_WF_F                     0x00000040
#define USB_DMAINT_1F0_RF_E                     0x00000020
#define USB_DMAINT_1F0_RF_F                     0x00000010
#define USB_DMAINT_BSY_DONE                     0x00000008
#define USB_DMAINT_TF_RD_DONE                   0x00000004
#define USB_DMAINT_CMD_INTRQ_OK                 0x00000002

/*
  * Definitions of the bit fields in the DMAEndpoint register.
  */
#define USB_DMAEP_CONTROL_OUT                   0x00000000
#define USB_DMAEP_CONTROL_IN                    0x00000001
#define USB_DMAEP_ONE_OUT                       0x00000002
#define USB_DMAEP_ONE_IN                        0x00000003
#define USB_DMAEP_TWO_OUT                       0x00000004
#define USB_DMAEP_TWO_IN                        0x00000005
#define USB_DMAEP_THREE_OUT                     0x00000006
#define USB_DMAEP_THREE_IN                      0x00000007
#define USB_DMAEP_FOUR_OUT                      0x00000008
#define USB_DMAEP_FOUR_IN                       0x00000009
#define USB_DMAEP_FIVE_OUT                      0x0000000a
#define USB_DMAEP_FIVE_IN                       0x0000000b
#define USB_DMAEP_SIX_OUT                       0x0000000c
#define USB_DMAEP_SIX_IN                        0x0000000d
#define USB_DMAEP_SEVEN_OUT                     0x0000000e
#define USB_DMAEP_SEVEN_IN                      0x0000000f

/*
  * Definitions of the bit fields in the DMAStrobeTiming register.
  */
#define USB_DMASTROBE_COUNT_MASK                0x0000001f

/*
  * Definitions of the bit fields in the ChipID register.
  */
#define USB_CHIPID_ID_MASK                      0x00ffff00
#define USB_CHIPID_ID                           0x00158100
#define USB_CHIPID_VERSION_MASK                 0x000000ff
#define UBS_CHIPID_VERSION                      0x00000051

/*
  * Definitions of the bit fields in the FrameNumber register.
  */
#define USB_FRAMENUM_MICRONUM_MASK              0x00003800
#define USB_FRAMENUM_MICRONUM_SHIFT             11
#define USB_FRAMENUM_NUM_MASK                   0x000007ff
#define USB_FRAMENUM_NUM_SHIFT                  0

/*
  * Definitions of the bit fields in the Test register.
  */
#define USB_TEST_FORCEHS                        0x00000080
#define USB_TEST_FORCEFS                        0x00000010
#define USB_TEST_PRBS                           0x00000008
#define USB_TEST_KSTATE                         0x00000004
#define USB_TEST_JSTATE                         0x00000002
#define USB_TEST_SE0_NAK                        0x00000001

/*---------------------------------------------------------------------------------*/


/*
  * USB2_slave_dma_hw.H - Defines for the USB specification and the Philips ISP1581 USB
  *         controller.
  *
  * Copyright (c) 2004 Cirrus Logic, Inc.
  *
  */

/****************************************************************************/


/*
  *mmu map the adress 
  *0x8000_0000-------0xFF00_0000
  *(cirrus_linux1.4.1)
  *we need to get the DMA register startaddress
  */

/*
  *DMA Channel Base Address
  */
#define HwDMA_M2P_0                              0x00000000
#define HwDMA_M2P_1                              0x00000040
#define HwDMA_M2P_2                              0x00000080
#define HwDMA_M2P_3                              0x000000c0
#define HwDMA_M2M_0                              0x00000100
#define HwDMA_M2M_1                              0x00000140
#define HwDMA_M2P_5                              0x00000200
#define HwDMA_M2P_4                              0x00000240
#define HwDMA_M2P_7                              0x00000280
#define HwDMA_M2P_6                              0x000002c0
#define HwDMA_M2P_9                              0x00000300
#define HwDMA_M2P_8                              0x00000340
#define HwDMAChannelArb                        0x00000380
#define HwDMAGlobalInt                            0x000003c0

/*
  *DMA Channal REG
  */
#define HwDMA_M2M_Control                       0x00000000
#define HwDMA_M2M_Interrupt                     0x00000004
#define HwDMA_M2M_Status                        0x0000000c
#define HwDMA_M2M_BCR0                          0x00000010
#define HwDMA_M2M_BCR1                          0x00000014
#define HwDMA_M2M_SAR_Base0                     0x00000018
#define HwDMA_M2M_SAR_Base1                     0x0000001c
#define HwDMA_M2M_SAR_Current0                  0x00000024
#define HwDMA_M2M_SAR_Current1                  0x00000028
#define HwDMA_M2M_DAR_Base0                     0x0000002c
#define HwDMA_M2M_DAR_Base1                     0x00000030
#define HwDMA_M2M_DAR_Current0                  0x00000034
#define HwDMA_M2M_DAR_Current1                  0x0000003c

/*
  *DMA REG Value
  */
#define DMA_M2M_CONTROL_STALL_INT_EN            0x00000001
#define DMA_M2M_CONTROL_SCT                     0x00000002
#define DMA_M2M_CONTROL_DONE_INT_EN             0x00000004
#define DMA_M2M_CONTROL_ENABLE                  0x00000008
#define DMA_M2M_CONTROL_START                   0x00000010
#define DMA_M2M_CONTROL_BWC_MASK                0x000001e0
#define DMA_M2M_CONTROL_BWC_FULL                0x00000000
#define DMA_M2M_CONTROL_BWC_16                  0x00000020
#define DMA_M2M_CONTROL_BWC_32                  0x000000a0
#define DMA_M2M_CONTROL_BWC_64                  0x000000c0
#define DMA_M2M_CONTROL_BWC_128                 0x000000e0
#define DMA_M2M_CONTROL_BWC_256                 0x00000100
#define DMA_M2M_CONTROL_BWC_512                 0x00000120
#define DMA_M2M_CONTROL_BWC_1024                0x00000140
#define DMA_M2M_CONTROL_BWC_2048                0x00000160
#define DMA_M2M_CONTROL_BWC_4096                0x00000180
#define DMA_M2M_CONTROL_BWC_8192                0x000001a0
#define DMA_M2M_CONTROL_BWC_16384               0x000001c0
#define DMA_M2M_CONTROL_BWC_32768               0x000001e0
#define DMA_M2M_CONTROL_PW_MASK                 0x00000600
#define DMA_M2M_CONTROL_PW_BYTE                 0x00000000
#define DMA_M2M_CONTROL_PW_HALFWORD             0x00000200
#define DMA_M2M_CONTROL_PW_WORD                 0x00000400
#define DMA_M2M_CONTROL_DAH                     0x00000800
#define DMA_M2M_CONTROL_SAH                     0x00001000
#define DMA_M2M_CONTROL_TM_MASK                 0x00006000
#define DMA_M2M_CONTROL_TM_SOFTWARE             0x00000000
#define DMA_M2M_CONTROL_TM_HARDWARE_M2P         0x00002000
#define DMA_M2M_CONTROL_TM_HARDWARE_P2M         0x00004000
#define DMA_M2M_CONTROL_ETDP_MASK               0x00018000
#define DMA_M2M_CONTROL_ETDP_ACTIVE_LOW_EOT     0x00000000
#define DMA_M2M_CONTROL_ETDP_ACTIVE_HIGH_EOT    0x00008000
#define DMA_M2M_CONTROL_ETDP_ACTIVE_LOW_TC      0x00010000
#define DMA_M2M_CONTROL_ETDP_ACTIVE_HIGH_TC     0x00018000
#define DMA_M2M_CONTROL_DACK_ACTIVE_HIGH        0x00020000
#define DMA_M2M_CONTROL_DREQP_MASK              0x00180000
#define DMA_M2M_CONTROL_DREQP_LEVEL_LOW         0x00000000
#define DMA_M2M_CONTROL_DREQP_LEVEL_HIGH        0x00080000
#define DMA_M2M_CONTROL_DREQP_EDGE_LOW          0x00100000
#define DMA_M2M_CONTROL_DREQP_EDGE_HIGH         0x00180000
#define DMA_M2M_CONTROL_NFBINT_EN               0x00200000
#define DMA_M2M_CONTROL_RSS_MASK                0x00c00000
#define DMA_M2M_CONTROL_RSS_EXTERNAL_DREQ       0x00000000
#define DMA_M2M_CONTROL_RSS_SSPRX               0x00400000
#define DMA_M2M_CONTROL_RSS_SSPTX               0x00800000
#define DMA_M2M_CONTROL_RSS_IDE                 0x00c00000
#define DMA_M2M_CONTROL_NO_HDSK                 0x01000000
#define DMA_M2M_CONTROL_PWSC_MASK               0xfe000000
#define DMA_M2M_CONTROL_PWSC_SHIFT              25

#define DMA_M2M_INTERRUPT_STALL                 0x00000001
#define DMA_M2M_INTERRUPT_DONE                  0x00000002
#define DMA_M2M_INTERRUPT_NFB                   0x00000004

#define DMA_M2M_STATUS_STALL                    0x00000001
#define DMA_M2M_STATUS_CONTROL_STATE_MASK       0x0000000e
#define DMA_M2M_STATUS_CONTROL_IDLE             0x00000000
#define DMA_M2M_STATUS_CONTROL_STALL            0x00000002
#define DMA_M2M_STATUS_CONTROL_MEM_RD           0x00000004
#define DMA_M2M_STATUS_CONTROL_MEM_WR           0x00000006
#define DMA_M2M_STATUS_CONTROL_BWC_WAIT         0x00000008
#define DMA_M2M_STATUS_BUFFER_STATE_MASK        0x00000030
#define DMA_M2M_STATUS_BUFFER_NO                0x00000000
#define DMA_M2M_STATUS_BUFFER_ON                0x00000010
#define DMA_M2M_STATUS_BUFFER_NEXT              0x00000020
#define DMA_M2M_STATUS_DONE                     0x00000040
#define DMA_M2M_STATUS_TCS_MASK                 0x00000180
#define DMA_M2M_STATUS_TCS_NEITHER              0x00000000
#define DMA_M2M_STATUS_TCS_BUF0                 0x00000080
#define DMA_M2M_STATUS_TCS_BUF1                 0x00000100
#define DMA_M2M_STATUS_TCS_BOTH                 0x00000180
#define DMA_M2M_STATUS_EOTS_MASK                0x00000600
#define DMA_M2M_STATUS_EOTS_NEITHER             0x00000000
#define DMA_M2M_STATUS_EOTS_BUF0                0x00000200
#define DMA_M2M_STATUS_EOTS_BUF1                0x00000400
#define DMA_M2M_STATUS_EOTS_BOTH                0x00000600
#define DMA_M2M_STATUS_NFB                      0x00000800
#define DMA_M2M_STATUS_NB                       0x00001000
#define DMA_M2M_STATUS_DREQS                    0x00002000

#define DMA_M2M_BRC_MASK                        0x0000ffff
#define DMA_M2M_BRC_SHIFT                       0

#define DMA_M2M_SAR_MASK                        0xffffffff
#define DMA_M2M_SAR_SHIFT                       0

#define DMA_M2M_DAR_MASK                        0xffffffff
#define DMA_M2M_DAR_SHIFT                       0

#define DMA_M2M_SAR_CURRENT_MASK                0xffffffff
#define DMA_M2M_SAR_CURRENT_SHIFT               0

#define DMA_M2M_DAR_CURRENT_MASK                0xffffffff
#define DMA_M2M_DAR_CURRENT_SHIFT               0

#define DMA_INT_M2P_1                           0x00000001
#define DMA_INT_M2P_0                           0x00000002
#define DMA_INT_M2P_3                           0x00000004
#define DMA_INT_M2P_2                           0x00000008
#define DMA_INT_M2P_5                           0x00000010
#define DMA_INT_M2P_4                           0x00000020
#define DMA_INT_M2P_7                           0x00000040
#define DMA_INT_M2P_6                           0x00000080
#define DMA_INT_M2P_9                           0x00000100
#define DMA_INT_M2P_8                           0x00000200
#define DMA_INT_M2M_0                           0x00000400
#define DMA_INT_M2M_1                           0x00000800

#define DMA_ARB_CHARB                           0x00000001


/*---------------------------------------------------------------------------------*/

/*
  *
  * USB2_slave.H - Defines for the USB specification and the Philips ISP1581 USB
  *         controller.
  *
  * Copyright (c) 2004 Cirrus Logic, Inc.
  */

/****************************************************************************/

/****************************************************************************/

/*
  * The following defines are specific the USB standard version 2.0.
  */

/****************************************************************************/

/*
  * Definitions of the bit fields in the bmRequestType field of a setup packet.
  */
#define USB_RT_DEVICE_TO_HOST                   0x80
#define USB_RT_TYPE_MASK                        0x60
#define USB_RT_TYPE_STANDARD                    0x00
#define USB_RT_TYPE_CLASS                       0x20
#define USB_RT_TYPE_VENDOR                      0x40
#define USB_RT_RECIPIENT_MASK                   0x1F
#define USB_RT_RECIPIENT_DEVICE                 0x00
#define USB_RT_RECIPIENT_INTERFACE              0x01
#define USB_RT_RECIPIENT_ENDPOINT               0x02

/*
  * Definitions of the bit fields in the wIndex field of setup packets where the
  * wIndex field is used to specify a endpoint (i.e. Clear_Feature, Get_Status,
  * and Set_Feature).
  */
#define USB_ENDPOINT_DIRECTION_MASK             0x0080
#define USB_ENDPOINT_ADDRESS_MASK               0x000F

/*
  * Definitions of the features that can be specified in the wValue field of a
  * Clear_Feature or Set_Feature setup packet.
  */
#define USB_FEATURE_ENDPOINT_STALL              0x0000
#define USB_FEATURE_REMOTE_WAKEUP               0x0001
#define USB_FEATURE_POWER_D0                    0x0002
#define USB_FEATURE_POWER_D1                    0x0003
#define USB_FEATURE_POWER_D2                    0x0004
#define USB_FEATURE_POWER_D3                    0x0005

/*
  * Definitions of the wValue field for a Get_Descriptor setup packet.
  */
#define USB_DESCRIPTOR_TYPE_MASK                   0xFF00
#define USB_DESCRIPTOR_DEVICE                       0x0100
#define USB_DESCRIPTOR_CONFIGURATION               0x0200
#define USB_DESCRIPTOR_STRING                       0x0300
#define USB_DESCRIPTOR_INTERFACE                    0x0400
#define USB_DESCRIPTOR_ENDPOINT                     0x0500

#define USB_DESCRIPTOR_DEVICE_QUALIFIER        0x0600
#define USB_DESCRIPTOR_OTHER_SPEED_CONFIGURATION 0x0700
#define USB_DESCRIPTIO_Interface_Power_Descriptor  0x0800

#define USB_Manufacturer_String_Descriptor          0x0103
#define USB_Product_String_Descriptor               0x0203
#define USB_Config_String_Descriptor      0x0102

#define USB_DESCRIPTOR_INDEX_MASK                  0x00FF

/*
  * Definitions of the device status returned for a Get_Status setup packet.
  */
#define USB_DEVICE_STATUS_SELF_POWERED            0x01
#define USB_DEVICE_STATUS_REMOTE_WAKEUP         0x02

/*
  * Definitions of the endpoint status returned for a Get_Status setup packet.
  */
#define USB_ENDPOINT_STATUS_STALLED             0x01


/*
  * Definitions of the USB device Test mode
  */
#define  TEST_J                                   1
#define  TEST_K                                   2
#define  TEST_SE0_NAK                            3
#define  TEST_PACKET                             4
#define  TEST_FORCE_ENABLE                      5

/*
  * Definitions of the status of The USB Device
  */
#define FSM_DEV_POWERED  0
#define FSM_DEV_DEFAULT  1
#define FSM_DEV_ADDRESS  2
#define FSM_DEV_CONFIGURED 3



/*********************************************************************************/

/*
  *The USB Control process
  */

/*********************************************************************************/

/*
  *Setup control process status
  */
#define SETUPCOMMAND_DataPhase_OUTPID      1    /*1:setup control out  Set  command  DataPhase    OUT PID*/
#define SETUPCOMMAND_DataPhase_INPID         2  /*2:setup control In   Get  command  DataPhase    IN PID*/
#define SETUPCOMMAND_StatusPhase_INPID      3   /*3:setup control out  Set  command  StatusPhase  IN PID*/
#define SETUPCOMMAND_StatusPhase_OUTPID   4     /*4:setup control In   Get  command  StatusPhase  OUT PID*/
#define SETUPCOMMAND_STATUS_DEFAULT        0

/*
  *USB Speed
  */
#define HighSpeed  2
#define FullSpeed   1
#define LowSpeed   0

/*
  *default languale is 0409--- english
  */
#define DEF_LANG 0x0409


/****************************************************************************/

/*
  * This is the device descriptor for the  USB Mass Storage Device  See the USB
  * specification for the definition of this descriptor.
  */

/****************************************************************************/
static const unsigned char ucDeviceDescriptor[] =
    {
        0x12,                               /* bLength*/
        0x01,                               /*bDescriptorType*/
        0x00, 0x02,                         /*bcdUSB*/
        0x00,                               /*bDeviceClass*/
        0x00,                               /*bDeviceSubClass*/
        0x00,                               /*bDeviceProtocol*/
        0x40,                               /*bMaxPacketSize0*/
        0x29, 0x04,                         /*idVendor*/
        0x02, 0x07,                         /*idProduct*/
        0x02, 0x00,                         /* bcdDevice*/
        0x01,                               /*iManufacturer*/
        0x02,                               /*iProduct*/
        0x00,                               /*iSerial Number*/
        0x01                                /*bNumConfigurations*/
    };


/****************************************************************************/

/*
  * This is the configuration descriptor for the  USB Mass Storage Device Full Speed. 
  * See the USB specification for the definition of this descriptor.
  */

/****************************************************************************/
static const unsigned char ucConfigurationDescriptor[] =
    {
        /*
          * The configuration descriptor structure.
          */
        0x09,                               /* bLength*/
        0x02,                               /* bDescriptorType*/
        0x20, 0x00,                         /* wTotalLength*/
        0x01,                               /* bNumInterfaces*/
        0x01,                               /* bConfigurationValue*/
        0x03,                               /* iConfiguration*/
        0x80,                               /* bmAttributes*/
        0x01,                               /* MaxPower*/

        /*
          * The interface descriptor structure.
          */
        0x09,                               /* bLength*/
        0x04,                               /* bDescriptorType*/
        0x00,                               /*bInterfaceNumber*/
        0x00,                               /* bAlternateSetting*/
        0x02,                               /* bNumEndpoints*/
        0x08,                               /* bInterfaceClass       MASS STORAGE*/
        0x06,                               /* bInterfaceSubClass   SCSI*/
        0x50,                               /* bInterfaceProtocol     BULK-ONLY*/
        0x00,                               /* iInterface*/

        /*
          * The endpoint descriptor structure.
          */
        0x07,                               /* bLength*/
        0x05,                               /* bDescriptorType*/
        0x82,                               /* bEndpointAddress  IN---EP2IN*/
        0x02,                               /* bmAttributes*/
        0x40, 0x00,                         /* wMaxPacketSize*/
        0x00,                               /* bInterval*/

        /*
          * The endpoint descriptor structure.
          */
        0x07,                               /* bLength */
        0x05,                               /* bDescriptorType*/
        0x01,                               /* bEndpointAddress  OUT--EP1OUT*/
        0x02,                               /* bmAttributes*/
        0x40, 0x00,                         /* wMaxPacketSize*/
        0x00                                /* bInterval*/
    };

/****************************************************************************/

/*
  * This is the configuration descriptor for the  USB Mass Storage Device High Speed. 
  * See the USB specification for the definition of this descriptor.
  */

/****************************************************************************/
static const unsigned char ucConfigurationHighDescriptor[] =
    {
        /*
          * The configuration descriptor structure.
          */
        0x09,                               /* bLength*/
        0x02,                               /* bDescriptorType*/
        0x20, 0x00,                         /* wTotalLength*/
        0x01,                               /* bNumInterfaces*/
        0x01,                               /* bConfigurationValue*/
        0x03,                               /* iConfiguration*/
        0x80,                               /* bmAttributes*/
        0x01,                               /* MaxPower*/

        /*
          * The interface descriptor structure.
          */
        0x09,                               /* bLength*/
        0x04,                               /* bDescriptorType*/
        0x00,                               /* bInterfaceNumber*/
        0x00,                               /* bAlternateSetting*/
        0x02,                               /* bNumEndpoints*/
        0x08,                               /* bInterfaceClass       MASS STORAGE*/
        0x06,                               /* bInterfaceSubClass   SCSI*/
        0x50,                               /* bInterfaceProtocol     BULK-ONLY*/
        0x00,                               /* iInterface*/

        /*
          * The endpoint descriptor structure.
          */
        0x07,                               /* bLength*/
        0x05,                               /* bDescriptorType*/
        0x82,                               /* bEndpointAddress  IN---EP2IN*/
        0x02,                               /* bmAttributes*/
        0x00,0x02,                          /* wMaxPacketSize*/
        0x00,                               /* bInterval*/

        /*
          * The endpoint descriptor structure.
          */
        0x07,                               /* bLength*/
        0x05,                               /* bDescriptorType*/
        0x01,                               /* bEndpointAddress  OUT--EP1OUT*/
        0x02,                               /* bmAttributes*/
        0x00,0x02,                          /* wMaxPacketSize*/
        0x00                                /* bInterval*/
    };

/****************************************************************************/

/*
  * String descriptor 0 for the  USB Mass Storage Device.  This defines the
  * languages supported by the string descriptors.  See the USB specification
  * for the definition of this descriptor.
  */

/****************************************************************************/
static const unsigned char ucString0[] =
    {
        0x04,                               /* bLength*/
        0x03,                               /* bDescriptorType*/
        0x09, 0x04                          /* wLANGID[0] -> US English*/
    };

/****************************************************************************/

/*
  * String descriptor 1 for the  USB Mass Storage Device.  This defines the
  * manufacturer of the player.  See the USB specification for the definition
  * of this descriptor.
  */

/****************************************************************************/
static const unsigned char ucString1[] =
    {
        0x26,                               /* bLength*/
        0x03,                               /* bDescriptorType*/
        'C', 0x00,                          /* wString[]*/
        'i', 0x00,
        'r', 0x00,
        'r', 0x00,
        'u', 0x00,
        's', 0x00,
        ' ', 0x00,
        'L', 0x00,
        'o', 0x00,
        'g', 0x00,
        'i', 0x00,
        'c', 0x00,
        ',', 0x00,
        ' ', 0x00,
        'I', 0x00,
        'n', 0x00,
        'c', 0x00,
        '.', 0x00
    };

/****************************************************************************/
 
/*
  * String descriptor 2 for the USB Mass Storage Device.  This defines the
  * product description of the player.  See the USB specification for the
  * definition of this descriptor.
  */

/****************************************************************************/
static const unsigned char ucString2[] =
    {
        0x46,                               /* bLength*/
        0x03,                               /* bDescriptorType*/
        'C', 0x00,                          /* wString[]*/
        'i', 0x00,
        'r', 0x00,
        'r', 0x00,
        'u', 0x00,
        's', 0x00,
        ' ', 0x00,
        'L', 0x00,
        'o', 0x00,
        'g', 0x00,
        'i', 0x00,
        'c', 0x00,
        ' ', 0x00,
        'U', 0x00,
        'S', 0x00,
        'B', 0x00,
        'T', 0x00,
        'o', 0x00,
        'A', 0x00,
        'T', 0x00,
        'A', 0x00,
        ' ', 0x00,
        'H', 0x00,
        'a', 0x00,
        'r', 0x00,
        'd', 0x00,
        ' ', 0x00,
        ' ', 0x00,
        'D', 0x00,
        'i', 0x00,
        's', 0x00,
        'k', 0x00,
        ' ', 0x00,
        ' ', 0x00
    };

/****************************************************************************/

/*
  * String descriptor 3 for the USB Mass Storage Device.  This defines the
  * configuration description of the player.  See the USB specification for the
  * definition of this descriptor.
  */

/****************************************************************************/
static const unsigned char ucString3[] =
    {
        0x2c,                               /* bLength*/
        0x03,                               /* bDescriptorType*/
        'D', 0x00,                          /* wString[]*/
        'e', 0x00,
        'f', 0x00,
        'a', 0x00,
        'u', 0x00,
        'l', 0x00,
        't', 0x00,
        ' ', 0x00,
        'C', 0x00,
        'o', 0x00,
        'n', 0x00,
        'f', 0x00,
        'i', 0x00,
        'g', 0x00,
        'u', 0x00,
        'r', 0x00,
        'a', 0x00,
        't', 0x00,
        'i', 0x00,
        'o', 0x00,
        'n', 0x00
    };

/*-----------------------------------------------------------------------------------*/
//****************************************************************************
//
// USB2_slave_dma.H - Defines for the USB specification and the Philips ISP1581 USB
//         controller.
//
// Copyright (c) 2004 Cirrus Logic, Inc.
//
//****************************************************************************


//
//Ep931X DMA Channel base address
//M2M0
//M2M1
//

//#define USE_CHANNEL_ZERO
#define HwDMAM2M0BaseAddress				(ep93IOBASE+HwDMA_M2M_0)
#define HwDMAM2M1BaseAddress				(ep93IOBASE+HwDMA_M2M_1)

//
//Definitions of using DMA to read or write
//

#define USE_DMA_READ
#define USE_DMA_WRITE

//
//Definitions of the DMA host and slave pin
//
#define ACTIVE_HIGH

#ifdef ACTIVE_HIGH
#define DMA_PIN_CONFIG DMA_M2M_CONTROL_ETDP_ACTIVE_HIGH_TC | \
                       DMA_M2M_CONTROL_DACK_ACTIVE_HIGH | \
                       DMA_M2M_CONTROL_DREQP_LEVEL_HIGH
#define USB_PIN_CONFIG USB_DMAHW_EOT_ACTIVE_HIGH | \
                       USB_DMAHW_ACK_ACTIVE_HIGH | \
                       USB_DMAHW_DREQ_ACTIVE_HIGH
#else
#define DMA_PIN_CONFIG DMA_M2M_CONTROL_ETDP_ACTIVE_LOW_TC | \
                       DMA_M2M_CONTROL_DREQP_LEVEL_LOW
#define USB_PIN_CONFIG USB_DMAHW_EOT_ACTIVE_LOW | \
                       USB_DMAHW_ACK_ACTIVE_LOW | \
                       USB_DMAHW_DREQ_ACTIVE_LOW
#endif


/*---------------------------------------------------------------------------------------*/





/*---------------------------------------------------------------------------------*/

#endif /* __LINUX_USB_GADGET_EP931X_H */
