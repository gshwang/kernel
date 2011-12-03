/*
 * linux/drivers/usb/gadget/ep93xx_udc.c
 * Cirrus EP93xx and ISP1581 on-chip high speed USB device controllers
 *
 * Based on PXA-UDC driver and previous EP93XX driver.
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
 */
#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
/*#include <linux/device.h>*/
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <asm/byteorder.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <asm/unaligned.h>

#include <asm/arch/hardware.h>


#include <linux/usb_ch9.h>
#include <linux/usb_gadget.h>

#include "ep93xx_udc.h"


/*-----------------------------------------------------------------------------------*/
//#define gadget_debug 1
#ifdef gadget_debug
#define DEBUG( fmt, arg... )  printk( fmt, ##arg )
#else
#define DEBUG( fmt, arg... )
#endif

/*------------------------------------------------------------------------------------*/

/*
 * This structure defines the setup packet received from the host via the
 * control out endpoint.
 */

typedef struct
{
    unsigned char bmRequestType;
    unsigned char bRequest;
    unsigned short wValue;
    unsigned short wIndex;
    unsigned short wLength;
}
ControlTransfer;

/*
 * Function prototypes for the standard device request handling routines.
 */

static void USBGetStatus(struct ep93xx_udc *dev);
static void USBClearFeature(struct ep93xx_udc *dev);
static void USBSetFeature(struct ep93xx_udc *dev);
static void USBSetAddress(struct ep93xx_udc *dev);
static void USBGetDescriptor(struct ep93xx_udc *dev);
static void USBGetConfiguration(struct ep93xx_udc *dev);
static void USBSetConfiguration(struct ep93xx_udc *dev);
static void USBGetInterface(struct ep93xx_udc *dev);
static void USBSetInterface(struct ep93xx_udc *dev);
static void USBReserved(struct ep93xx_udc *dev);

/*
 * An array of pointers to the USB standard device request handler Functions.
 */
static void (* const USBStandardDeviceRequest[])(struct ep93xx_udc *dev) =
{
    USBGetStatus,
    USBClearFeature,
    USBReserved,
    USBSetFeature,
    USBReserved,
    USBSetAddress,
    USBGetDescriptor,
    USBReserved,
    USBGetConfiguration,
    USBSetConfiguration,
    USBGetInterface,
    USBSetInterface,
    USBReserved
};

/*
 * The following structure contains the persistent state of the USB interface.
 */
static struct
{
    /*
     * Flags which describe the current state of the USB connection.
     */
    unsigned char ucFlags;

    /*
     * Indicates if a vendor specific setup packet has been received on the
     * control out endpoint and is waiting to be processed.
     */
    unsigned char ucHaveVendorMessage;

    /*
     * The number of bytes to be sent to the control endpoint.
     */
    unsigned long usControlInCount;

    /*
     * The buffer of data that is being sent to the control endpoint.
     */
    const unsigned char *pucControlIn;

    /*
     * The buffer of data that is being received from the control endpoint.
     */
    ControlTransfer sControlOut;

    /*
     * The number of bytes to be sent to the bulk endpoint.
     */
    unsigned long usBulkInCount;

    /*
     * The number of bytes still to be read from the bulk endpoint.
     */
    unsigned long usBulkOutCount;

    /*
     * The buffer of data that is being sent to the bulk endpoint.
     */
    const unsigned char *pucBulkIn;

    /*
     * The buffer of data that is being received from the bulk endpoint.
     */
    unsigned char *pucBulkOut;

    /*
     * Flags which describe the current state of the USB Setup Control Out or IN.
     */

    /*1:setup control out   Set  command DataPhase    OUT PID*/
    /*2:setup control In    Get  command DataPhase    IN PID*/
    /*3:setup control out   Set  command StatusPhase  IN PID*/
    /*4:setup control In    Get  command StatusPhase  OUT PID*/
    unsigned char ucSetupFlags;

    /*
     *Flags whch describe the current speed of the USB process
     */
    unsigned char ucUSBSpeed;

    /*
     * The Length of Bulk Endpoint MAXPacket .
     */
    unsigned long  ulBulkMAXLen;

    /*
     * The Max Length of write  Endpoint Packet .
     */
    unsigned long  ulWriteEndpointMAXLen;

}
sUSB;

/* For control responses*/
struct usb_request                      UDC_ep0req;        

/*
 *Set the USB Chip Base Address
 */
volatile unsigned long *pulUSB ;
volatile unsigned short *pulUSB16;
volatile unsigned short *pulUSBDataPort;

/*
 *set the DMA buffer base address
 */
unsigned long DMAVaddress;
unsigned short * DMAPHYaddress;

/*
 *Set the Ep931x M2M Chanel Base Address
 */
#ifdef USE_CHANNEL_ZERO
volatile unsigned long *pulDMA = (unsigned long *)(ep93IOBASE+HwDMA_M2M_0);
#else
volatile unsigned long *pulDMA = (unsigned long *)(ep93IOBASE+HwDMA_M2M_1);
#endif

/*----------------------------------------------------------------------------------*/
/*
 *
 */
static void
DMAOddRead(unsigned long ulEndpoint, void *pvBuffer, unsigned long ulCount)
{
    unsigned long ulTemp,i;
    /*
     *Setting the EP931x DMA control REG
     */
    pulDMA[HwDMA_M2M_Control >> 2] = (DMA_M2M_CONTROL_DONE_INT_EN |
                                      DMA_M2M_CONTROL_STALL_INT_EN|
                                      DMA_M2M_CONTROL_NFBINT_EN|
                                      DMA_M2M_CONTROL_BWC_FULL |
                                      DMA_M2M_CONTROL_PW_BYTE |
                                      DMA_M2M_CONTROL_SAH |
                                      DMA_M2M_CONTROL_TM_HARDWARE_P2M |
                                      DMA_PIN_CONFIG |
                                      DMA_M2M_CONTROL_RSS_EXTERNAL_DREQ |
                                      (5<< DMA_M2M_CONTROL_PWSC_SHIFT));
    ulTemp = pulDMA[HwDMA_M2M_Control >> 2];

    pulDMA[HwDMA_M2M_SAR_Base0 >> 2] = 0x30000000;/*0x70000020;*/

    pulDMA[HwDMA_M2M_DAR_Base0 >> 2] = DMAVaddress;

    pulDMA[HwDMA_M2M_BCR0 >> 2] = ulCount;

    /*
     *enable Ep931x dma
     */
    pulDMA[HwDMA_M2M_Control >> 2] |= DMA_M2M_CONTROL_ENABLE;
    ulTemp = pulDMA[HwDMA_M2M_Control >> 2];

    /*
     *clear int
     */
    ulTemp=pulUSB[HwUSBDMAIntReason >> 2] ;
    pulUSB[HwUSBDMAIntReason >> 2] =ulTemp ;

    /*
     *Set 1581 DMA Endpoint
     */
    pulUSB[HwUSBEndpointIndex >> 2] = USB_ENDPOINT_SIX_OUT;

    pulUSB[HwUSBDMAEndpoint >> 2] = ulEndpoint;

    /*
     *Setting the DMA configure
     */
    pulUSB16[HwUSBDMAConfig >> 1] = (USB_DMACONFIG_BURST_1|
                                     /*  USB_DMACONFIG_BURST_ALL |*/
                                     USB_DMACONFIG_MODE_DACK |
                                     USB_DMACONFIG_WIDTH_8);


    /*
     *set 1581 dma count
     */
    pulUSB16[HwUSBDMACount>>1] = ulCount & 0xffff;

    pulUSB16[(HwUSBDMACount+2)>>1] = ulCount >> 16;

    /*
     *set 1581 dma command
     */
    pulUSB16[HwUSBDMACommand>>1] = USB_DMACOMMAND_GDMA_WRITE;


    /*while(!(pulUSB[HwUSBIntReason >> 2] & USB_INT_DMA))
    while(!(pulDMA[HwDMA_M2M_Interrupt >> 2] & DMA_M2M_INTERRUPT_DONE))*/
    while(!(pulDMA[HwDMA_M2M_Status >> 2] & DMA_M2M_STATUS_DONE))
    /*while(pulDMA[HwDMA_M2M_BCR0 >> 2] != 0)*/
    {

        /*       if(pulUSB[HwUSBIntReason >> 2] & USB_INT_BUS_RESET)
                {
                    return;
                }
        */
    }


    /*
     *clear Setting the EP931x DMA control REG
     */
    pulUSB[HwUSBDMAEndpoint >> 2] = USB_ENDPOINT_SEVEN_OUT;
    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;


    pulDMA[HwDMA_M2M_Interrupt >> 2] |= pulDMA[HwDMA_M2M_Interrupt >> 2];

    pulDMA[HwDMA_M2M_Control >> 2] &= ~DMA_M2M_CONTROL_ENABLE;
    ulTemp = pulDMA[HwDMA_M2M_Control >> 2];

    /*
     *clear Setting the 1581 DMA control REG
     */
    pulUSB[HwUSBIntReason >> 2] = USB_INT_DMA;

    ulTemp=pulUSB[HwUSBDMAIntReason >> 2] ;
    pulUSB[HwUSBDMAIntReason >> 2] =ulTemp ;

    for (i=0;i<ulCount;i++) {
        *(unsigned char *)(pvBuffer+i) = *( (unsigned char *)DMAPHYaddress +i );
    }

    return;


}

/*
 *DMARead
 */
static void
DMARead(unsigned long ulEndpoint, void *pvBuffer, unsigned long ulCount)
{
    unsigned long ulTemp,i;
    /*
     *Setting the EP931x DMA control REG
     */
    pulDMA[HwDMA_M2M_Control >> 2] = (DMA_M2M_CONTROL_DONE_INT_EN |
                                      DMA_M2M_CONTROL_STALL_INT_EN|
                                      DMA_M2M_CONTROL_NFBINT_EN|
                                      DMA_M2M_CONTROL_BWC_FULL |
                                      DMA_M2M_CONTROL_PW_HALFWORD |
                                      DMA_M2M_CONTROL_SAH |
                                      DMA_M2M_CONTROL_TM_HARDWARE_P2M |
                                      DMA_PIN_CONFIG |
                                      DMA_M2M_CONTROL_RSS_EXTERNAL_DREQ |
                                      (5<< DMA_M2M_CONTROL_PWSC_SHIFT));
    ulTemp = pulDMA[HwDMA_M2M_Control >> 2];

    pulDMA[HwDMA_M2M_SAR_Base0 >> 2] = 0x30000000;;/*0x70000020;*/

    pulDMA[HwDMA_M2M_DAR_Base0 >> 2] = DMAVaddress;

    pulDMA[HwDMA_M2M_BCR0 >> 2] = ulCount;

    /*
     *enable Ep931x dma
     */
    pulDMA[HwDMA_M2M_Control >> 2] |= DMA_M2M_CONTROL_ENABLE;
    ulTemp = pulDMA[HwDMA_M2M_Control >> 2];

    /*
     *clear int
     */
    ulTemp=pulUSB[HwUSBDMAIntReason >> 2] ;
    pulUSB[HwUSBDMAIntReason >> 2] =ulTemp ;

    /*
     *Set 1581 DMA Endpoint
     */
    pulUSB[HwUSBEndpointIndex >> 2] = USB_ENDPOINT_SIX_OUT;

    pulUSB[HwUSBDMAEndpoint >> 2] = ulEndpoint;

    /*
     *
     */
    pulUSB16[HwUSBDMAConfig >> 1] = (USB_DMACONFIG_BURST_1|
                                     //  USB_DMACONFIG_BURST_ALL |
                                     USB_DMACONFIG_MODE_DACK |
                                     USB_DMACONFIG_WIDTH_16);


    /*
     *set 1581 dma count
     */
    pulUSB16[HwUSBDMACount>>1] = ulCount & 0xffff;

    pulUSB16[(HwUSBDMACount+2)>>1] = ulCount >> 16;

    /*
     *set 1581 dma command
     */
    pulUSB16[HwUSBDMACommand>>1] = USB_DMACOMMAND_GDMA_WRITE;


    /*
    while(!(pulUSB[HwUSBIntReason >> 2] & USB_INT_DMA))
    while(!(pulDMA[HwDMA_M2M_Interrupt >> 2] & DMA_M2M_INTERRUPT_DONE))
    */
    while(!(pulDMA[HwDMA_M2M_Status >> 2] & DMA_M2M_STATUS_DONE))
    /*while(pulDMA[HwDMA_M2M_BCR0 >> 2] != 0)*/
    {

        /*
               if(pulUSB[HwUSBIntReason >> 2] & USB_INT_BUS_RESET)
                {
                    return;
                }
        */
    }


    /*
     *clear Setting the EP931x DMA control REG
     */
    pulUSB[HwUSBDMAEndpoint >> 2] = USB_ENDPOINT_SEVEN_OUT;
    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;


    pulDMA[HwDMA_M2M_Interrupt >> 2] |= pulDMA[HwDMA_M2M_Interrupt >> 2];

    pulDMA[HwDMA_M2M_Control >> 2] &= ~DMA_M2M_CONTROL_ENABLE;
    ulTemp = pulDMA[HwDMA_M2M_Control >> 2];

    /*
     *clear Setting the 1581 DMA control REG
     */
    pulUSB[HwUSBIntReason >> 2] = USB_INT_DMA;

    ulTemp=pulUSB[HwUSBDMAIntReason >> 2] ;
    pulUSB[HwUSBDMAIntReason >> 2] =ulTemp ;

    for (i=0;i<ulCount;i++) {
        *(unsigned char *)(pvBuffer+i) = *( (unsigned char *)DMAPHYaddress +i );
    }

    return;
}

/*
 *DMAWrite 
 */

static void
DMAWrite(unsigned long ulEndpoint,  void *pvBuffer, unsigned long ulCount)
{
    unsigned long ulTemp,i;
    /*
     *Setting the EP931x DMA control REG
     */
    pulDMA[HwDMA_M2M_Control >> 2] = (DMA_M2M_CONTROL_DONE_INT_EN |
                                      DMA_M2M_CONTROL_BWC_FULL |
                                      DMA_M2M_CONTROL_PW_HALFWORD |
                                      DMA_M2M_CONTROL_DAH |
                                      DMA_M2M_CONTROL_TM_HARDWARE_M2P |
                                      DMA_PIN_CONFIG |
                                      DMA_M2M_CONTROL_RSS_EXTERNAL_DREQ |
                                      (5 << DMA_M2M_CONTROL_PWSC_SHIFT));
    ulTemp = pulDMA[HwDMA_M2M_Control >> 2];
    /*
     *Setting EP931XDMA Address and count
     */
    for (i=0;i<ulCount;i++) {
        *( (unsigned char *)DMAPHYaddress +i ) = *(unsigned char *)(pvBuffer+i);
    }

    pulDMA[HwDMA_M2M_SAR_Base0 >> 2] = DMAVaddress;

    pulDMA[HwDMA_M2M_DAR_Base0 >> 2] =   0x70000020;

    pulDMA[HwDMA_M2M_BCR0 >> 2] = ulCount;

    /*
     *enable Ep931x dma
     */
    pulDMA[HwDMA_M2M_Control >> 2] |= DMA_M2M_CONTROL_ENABLE;
    ulTemp = pulDMA[HwDMA_M2M_Control >> 2];
    /*
     *clear int
     */

    /*
     *ep93xx intrruption
     */
    ulTemp=pulDMA[HwDMA_M2M_Interrupt >> 2] ;
    pulDMA[HwDMA_M2M_Interrupt >> 2]=ulTemp;

    /*
     *Resetting 1581 DMA
     */
    pulUSB16[HwUSBDMAConfig >> 1] = (USB_DMACONFIG_BURST_1|
                                     USB_DMACONFIG_MODE_DACK |         /*10   both ACK low  8bit  */
                                     USB_DMACONFIG_WIDTH_16);          /*GDMA 16bit     low  8bit  */

    /*
     *Set 1581 DMA Endpoint
     */
    pulUSB[HwUSBEndpointIndex >> 2] = USB_ENDPOINT_SIX_OUT;
    pulUSB[HwUSBDMAEndpoint >> 2] = ulEndpoint;

    /*
     *Set 1581 DMA count ,and send command
     */
    pulUSB16[HwUSBDMACount>>1] = ulCount & 0xffff;
    pulUSB16[(HwUSBDMACount+2)>>1] = ulCount >> 16;

    pulUSB16[HwUSBDMACommand>>1] = USB_DMACOMMAND_GDMA_READ;


    while(!(pulDMA[HwDMA_M2M_Status >> 2] & DMA_M2M_STATUS_DONE)) {

        /*       if(pulUSB[HwUSBIntReason >> 2] & USB_INT_BUS_RESET)
                  {
                     return;
                  }
         */
    }




    pulUSB[HwUSBDMAEndpoint >> 2] = USB_ENDPOINT_SEVEN_OUT;
    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;

    pulDMA[HwDMA_M2M_Interrupt >> 2] |= DMA_M2M_INTERRUPT_DONE;

    pulDMA[HwDMA_M2M_Control >> 2] &= ~DMA_M2M_CONTROL_ENABLE;

    /*
     *clear Setting the EP931x DMA control REG
     */
    pulDMA[HwDMA_M2M_Control >> 2] = 0;

    ulTemp = pulDMA[HwDMA_M2M_Interrupt >> 2];
    pulDMA[HwDMA_M2M_Interrupt >> 2] = ulTemp;

    pulDMA[HwDMA_M2M_Status >> 2] = 2000;

    ulTemp=pulUSB[HwUSBDMAIntReason >> 2] ;
    pulUSB[HwUSBDMAIntReason >> 2] =ulTemp ;


}

/*
 *DMAOddWrite 
 */

static void
DMAOddWrite(unsigned long ulEndpoint,  void *pvBuffer, unsigned long ulCount)
{
    unsigned long ulTemp,i;
    /*
     *Setting the EP931x DMA control REG
     */
    pulDMA[HwDMA_M2M_Control >> 2] = (DMA_M2M_CONTROL_DONE_INT_EN |
                                      DMA_M2M_CONTROL_BWC_FULL |
                                      DMA_M2M_CONTROL_PW_BYTE |
                                      DMA_M2M_CONTROL_DAH |
                                      DMA_M2M_CONTROL_TM_HARDWARE_M2P |
                                      DMA_PIN_CONFIG |
                                      DMA_M2M_CONTROL_RSS_EXTERNAL_DREQ |
                                      (5 << DMA_M2M_CONTROL_PWSC_SHIFT));
    ulTemp = pulDMA[HwDMA_M2M_Control >> 2];
    /*
     *Setting EP931XDMA Address and count
     */
    for (i=0;i<ulCount;i++) {
        *( (unsigned char *)DMAPHYaddress +i ) = *(unsigned char *)(pvBuffer+i);
    }

    pulDMA[HwDMA_M2M_SAR_Base0 >> 2] = DMAVaddress;

    pulDMA[HwDMA_M2M_DAR_Base0 >> 2] =   0x70000020;

    pulDMA[HwDMA_M2M_BCR0 >> 2] = ulCount;

    /*
     *enable Ep931x dma
     */
    pulDMA[HwDMA_M2M_Control >> 2] |= DMA_M2M_CONTROL_ENABLE;
    ulTemp = pulDMA[HwDMA_M2M_Control >> 2];
    /*
     *clear int
     */

    /*
     *ep93xx intrruption
     */
    ulTemp=pulDMA[HwDMA_M2M_Interrupt >> 2] ;
    pulDMA[HwDMA_M2M_Interrupt >> 2]=ulTemp;

    /*
     *Resetting 1581 DMA
     */
    pulUSB16[HwUSBDMAConfig >> 1] = (USB_DMACONFIG_BURST_1|
                                      USB_DMACONFIG_MODE_DACK |         /*10   both ACK low  8bit  */
                                      USB_DMACONFIG_WIDTH_8);             /*GDMA 16bit     low  8bit  */

    /*
     *Set 1581 DMA Endpoint
     */
    pulUSB[HwUSBEndpointIndex >> 2] = USB_ENDPOINT_SIX_OUT;
    pulUSB[HwUSBDMAEndpoint >> 2] = ulEndpoint;
    /*
     *Set 1581 DMA count ,and send command
     */
    pulUSB16[HwUSBDMACount>>1] = ulCount & 0xffff;
    pulUSB16[(HwUSBDMACount+2)>>1] = ulCount >> 16;

    pulUSB16[HwUSBDMACommand>>1] = USB_DMACOMMAND_GDMA_READ;


    while(!(pulDMA[HwDMA_M2M_Status >> 2] & DMA_M2M_STATUS_DONE)) {

        /*       if(pulUSB[HwUSBIntReason >> 2] & USB_INT_BUS_RESET)
                  {
                     return;
                  }
         */
    }




    pulUSB[HwUSBDMAEndpoint >> 2] = USB_ENDPOINT_SEVEN_OUT;
    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;

    pulDMA[HwDMA_M2M_Interrupt >> 2] |= DMA_M2M_INTERRUPT_DONE;

    pulDMA[HwDMA_M2M_Control >> 2] &= ~DMA_M2M_CONTROL_ENABLE;

    /*
     *clear Setting the EP931x DMA control REG
     */
    pulDMA[HwDMA_M2M_Control >> 2] = 0;

    ulTemp = pulDMA[HwDMA_M2M_Interrupt >> 2];
    pulDMA[HwDMA_M2M_Interrupt >> 2] = ulTemp;

    pulDMA[HwDMA_M2M_Status >> 2] = 2000;

    ulTemp=pulUSB[HwUSBDMAIntReason >> 2] ;
    pulUSB[HwUSBDMAIntReason >> 2] =ulTemp ;


}

/*
 * USBReadEndpoint reads data from the specified endpoint.
 */
unsigned long
USBReadEndpoint(unsigned long ulEndpoint, unsigned char **ppucData,
                unsigned long *pusLength)
{
    unsigned long ulIdx, ulLength,ulData;
    volatile unsigned short *pulUSB_data16;

    pulUSB_data16 = pulUSBDataPort;

    /*
     * Select the appropriate endpoint.
     */
    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;

    /*
     * Read the length of the data buffer.
     */
    ulLength = pulUSB[HwUSBEndpointBufferLength >> 2] ;//& USB_EPBUFLEN_MASK;

    /*
     * Is there buffer space to fill with this data or should we throw the
     * data away?
     */
    if(*pusLength) {
        /*
         * Read the data into the receive buffer.
         */
	for(ulIdx = 0; (ulIdx < ulLength) && (ulIdx < *pusLength); ) {

            ulData =  *pulUSB_data16;

            *(*ppucData)++ = ulData & 0xff;
            ulIdx++;
            if((ulIdx < ulLength) && (ulIdx < *pusLength)) {
                *(*ppucData)++ = ulData >> 8;
                ulIdx++;
            }
        }

        /*
         * Decrement the count of bytes to read.
         */
        *pusLength -= ulIdx;

    } else {
        /*
         * Send the clear buffer command so that the endpoint can receive
         * another packet.
         */
        pulUSB[HwUSBEndpointControl >> 2] |= USB_EPCONTROL_CLEAR;
        printk("USBReadEndpoint clear\n");
    }

    /*
     * Return the size of the packet received.
     */
    return(ulLength);

}

/*
 *USBReadBulkEndpoint read data in USB bulk endpoint
 */
unsigned long
USBReadPPBulkEndpoint(unsigned long ulEndpoint, unsigned char *ppucData,
                      unsigned long pusLength)
{
    unsigned long ulIdx, ulLength,ulData;
    volatile unsigned short *pulUSB_data16;
   

    pulUSB_data16 = pulUSBDataPort;

    /*
     * Select the appropriate endpoint.
     */
    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;

    /*
     * Read the length of the data buffer.
     */
    ulLength = pulUSB[HwUSBEndpointBufferLength >> 2] ;//& USB_EPBUFLEN_MASK;

    /*
     *Is there buffer space to fill with this data or should we throw the
     *data away?
     */
    if(pusLength) {


        if(ulLength %2 ==0) {

            DMARead(ulEndpoint, ppucData, ulLength);

            ppucData += ulLength;
        } else {
            /*
             * Read the data into the receive buffer.
             */
		for(ulIdx = 0; (ulIdx < ulLength) && (ulIdx < pusLength); ) {


                	ulData =  *pulUSB_data16;


                	*ppucData = ulData & 0xff;
                	ppucData++;
                	ulIdx++;
                	if((ulIdx < ulLength) && (ulIdx < pusLength)) {
                    		*ppucData = ulData >> 8;
                    		ppucData++ ;
                    		ulIdx++;
                	}

            	}
        }

        /*
         * Decrement the count of bytes to read.
         */
        pusLength -= ulIdx;
    } else {
        	/*
         	* Send the clear buffer command so that the endpoint can receive
         	* another packet.
         	*/
		pulUSB[HwUSBEndpointControl >> 2] |= USB_EPCONTROL_CLEAR;
        	printk("USBReadEndpoint clear\n");
    }

    /*
     *Return the size of the packet received.
     */
    return(ulLength);
}

/*
 *USBReadBulkEndpoint read data in USB bulk endpoint
 */
unsigned long
USBReadBulkEndpoint(unsigned long ulEndpoint, unsigned char *ppucData,
                    unsigned long pusLength)
{
    unsigned long ulIdx, ulLength;
    volatile unsigned short *pulUSB_data16;
#ifdef   USE_DMA_READ
#else
    unsigned long	ulData;
#endif

    pulUSB_data16 = pulUSBDataPort;

    /*
     * Select the appropriate endpoint.
     */
    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;

    /*
     * Read the length of the data buffer.
     */
    ulLength = pulUSB[HwUSBEndpointBufferLength >> 2] ;//& USB_EPBUFLEN_MASK;

    /*
     *Is there buffer space to fill with this data or should we throw the
     *data away?
     */
    if(pusLength) {

#ifdef   USE_DMA_READ
        if(ulLength %2 ==0) {
            DMARead(ulEndpoint, ppucData, ulLength);
            ppucData += ulLength;
        } else if(ulLength %2 ==1) {
            DMAOddRead(ulEndpoint, ppucData, ulLength);
            ppucData += ulLength;

      }

#else
            /*
             * Read the data into the receive buffer.
             */
            for(ulIdx = 0; (ulIdx < ulLength) && (ulIdx < pusLength); ) {

                ulData =  *pulUSB_data16;

                *ppucData = ulData & 0xff;
                ppucData++;
                ulIdx++;

                if((ulIdx < ulLength) && (ulIdx < pusLength)) {
                    *ppucData = ulData >> 8;
                    ppucData++ ;
                    ulIdx++;
                }

            }
        
#endif
        /*
         * Decrement the count of bytes to read.
         */
        pusLength -= ulIdx;
    } else {
        /*
         * Send the clear buffer command so that the endpoint can receive
         * another packet.
          */
        pulUSB[HwUSBEndpointControl >> 2] |= USB_EPCONTROL_CLEAR;
        printk("USBReadEndpoint clear\n");
    }

    /*
     *Return the size of the packet received.
     */
    return(ulLength);
}



/*
 * USBWriteEndpoint writes data to the specified endpoint.
 */
void
USBWriteEndpoint(unsigned long ulEndpoint, const unsigned char **ppucData,
                 unsigned long *pusLength)
{
    unsigned long ulIdx, ulLength, ulData;
    volatile unsigned short *pulUSB_data16;


    pulUSB_data16 = pulUSBDataPort;

    ulLength = (*pusLength > sUSB.ulWriteEndpointMAXLen) ? sUSB.ulWriteEndpointMAXLen: *pusLength;

    /*
     * Select the appropriate endpoint.
     */
    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;

    /*
     * Write the packet length.
     */
    pulUSB[HwUSBEndpointBufferLength >> 2] = ulLength;

    /*
     * Write the data into the transmit buffer.
     */
    for(ulIdx = 0; ulIdx < ulLength; ) {
        ulData = *(*ppucData)++;
        ulIdx++;
        if(ulIdx < ulLength) {

            ulData |= ((unsigned long)*(*ppucData)++) << 8;
            ulIdx++;
        }


        *pulUSB_data16=ulData;

    }

    /*
     * Decrement the count of bytes to write.
     */
    *pusLength -= ulLength;

}


/*
  * USBReadEndpoint reads data from the specified endpoint.
  */
unsigned long
UDC_USBReadEndpoint(unsigned long ulEndpoint, unsigned char *ppucData,
                unsigned long pusLength)
{
    unsigned long ulIdx, ulLength,ulData;
    volatile unsigned short *pulUSB_data16;

    pulUSB_data16 = pulUSBDataPort;

    /*
     * Select the appropriate endpoint.
     */
    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;

    /*
     * Read the length of the data buffer.
     */
    ulLength = pulUSB[HwUSBEndpointBufferLength >> 2] ;//& USB_EPBUFLEN_MASK;

    /*
     * Is there buffer space to fill with this data or should we throw the
     * data away?
     */
    if(pusLength) {
        /*
         * Read the data into the receive buffer.
         */
        for(ulIdx = 0; (ulIdx < ulLength) && (ulIdx < pusLength); ) {

            ulData =  *pulUSB_data16;

            *(ppucData++) = ulData & 0xff;
            ulIdx++;
            if((ulIdx < ulLength) && (ulIdx < pusLength)) {
                *(ppucData++) = ulData >> 8;
                ulIdx++;
            }
        }

        /*
         * Decrement the count of bytes to read.
         */
        pusLength -= ulIdx;

    } else {
        /*
         * Send the clear buffer command so that the endpoint can receive
         * another packet.
         */
        pulUSB[HwUSBEndpointControl >> 2] |= USB_EPCONTROL_CLEAR;
        printk("USBReadEndpoint clear\n");
    }

    /*
     * Return the size of the packet received.
     */
    return(ulLength);

}



/*
 * USBWriteEndpoint writes data to the specified endpoint.
 */
static unsigned long 
UDC_USBWriteEndpoint(unsigned long ulEndpoint, unsigned char *ppucData,
                 unsigned long pusLength,unsigned long max)
{
    unsigned long ulIdx, ulLength, ulData;
    volatile unsigned short *pulUSB_data16;

/*	printk("usb w len=%d\n",pusLength);*/
    pulUSB_data16 = pulUSBDataPort;

    ulLength = (pusLength > max) ? max: pusLength;

    /*
     * Select the appropriate endpoint.
     */
    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;

    /*
     * Write the packet length.
     */
    pulUSB[HwUSBEndpointBufferLength >> 2] = ulLength;

    /*
     * Write the data into the transmit buffer.
     */
    for(ulIdx = 0; ulIdx < ulLength; ) {
        ulData = (*(ppucData++));
        ulIdx++;
        if(ulIdx < ulLength) {

            ulData |= ((unsigned long)(*(ppucData++))) << 8;
            ulIdx++;
        }


        *pulUSB_data16=ulData;

    }

    /*
     * Decrement the count of bytes to write.
     */
    return(ulLength);

}

/*
 *USBDMAWriteEndpoint writes data to the specified endpoint.
 */
static unsigned long
UDC_USBDMAWriteEndpoint(unsigned long ulEndpoint, const unsigned char *ppucData,
                    unsigned long pusLength,unsigned long max)
{
    unsigned long ulLength;
    

    ulLength = (pusLength > max) ? max: pusLength;

    /*
     * Select the appropriate endpoint.
     */
    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;

    /*
     * Write the packet length.
     */
    pulUSB[HwUSBEndpointBufferLength >> 2] = ulLength;

    /*
     * Write the data into the transmit buffer.
     */

    if(ulLength %2 ==0)
        DMAWrite(ulEndpoint, (void *)ppucData, ulLength);
    else
        DMAOddWrite(ulEndpoint, (void *)ppucData, ulLength);


    /* *ppucData += ulLength;*/

    /*
     * Decrement the count of bytes to write.
     */
    /* *pusLength -= ulLength;*/

	return ulLength;
}


/*
 *USBReadBulkEndpoint read data in USB bulk endpoint
 */
unsigned long
UDC_USBReadBulkEndpoint(unsigned long ulEndpoint, unsigned char *ppucData,
                    unsigned long pusLength)
{
    unsigned long ulIdx, ulLength;
    volatile unsigned short *pulUSB_data16;
#ifdef   USE_DMA_READ
#else
    unsigned long	ulData;
#endif
    
   

    pulUSB_data16 = pulUSBDataPort;

    /*
     * Select the appropriate endpoint.
     */
    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;

    /*
     * Read the length of the data buffer.
     */
    ulLength = pulUSB[HwUSBEndpointBufferLength >> 2] ;//& USB_EPBUFLEN_MASK;
    
    /*printk("UDC_USBReadBulkEndpoint=%x",ulLength);*/
    
    /*
     *Is there buffer space to fill with this data or should we throw the
     *data away?
     */
    if(pusLength) {

#ifdef   USE_DMA_READ

        if(ulLength %2 ==0) {
            DMARead(ulEndpoint, (void *)ppucData, ulLength);
            ppucData += ulLength;
        } else if(ulLength %2 ==1) {
            DMAOddRead(ulEndpoint, (void *)ppucData, ulLength);
            ppucData += ulLength;

        }

#else 
            /*
             * Read the data into the receive buffer.
             */
            for(ulIdx = 0; (ulIdx < ulLength) && (ulIdx < pusLength); ) {

                ulData =  *pulUSB_data16;

                *ppucData = ulData & 0xff;
                ppucData++;
                ulIdx++;

                if((ulIdx < ulLength) && (ulIdx < pusLength)) {
                    *ppucData = ulData >> 8;
                    ppucData++ ;
                    ulIdx++;
                }

            }
        

#endif
        /*
         * Decrement the count of bytes to read.
         */
        pusLength -= ulIdx;
    } else {
        /*
         * Send the clear buffer command so that the endpoint can receive
         * another packet.
         */
        pulUSB[HwUSBEndpointControl >> 2] |= USB_EPCONTROL_CLEAR;
        printk("USBReadEndpoint clear\n");
    }

    /*
     *Return the size of the packet received.
     */
    return(ulLength);
}






/*
 *USBDMAWriteEndpoint writes data to the specified endpoint.
 */
void
USBDMAWriteEndpoint(unsigned long ulEndpoint, const unsigned char **ppucData,
                    unsigned long *pusLength)
{
    unsigned long ulLength;
    

    ulLength = (*pusLength > sUSB.ulWriteEndpointMAXLen) ? sUSB.ulWriteEndpointMAXLen: *pusLength;

    /*
     * Select the appropriate endpoint.
     */
    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;

    /*
     * Write the packet length.
     */
    pulUSB[HwUSBEndpointBufferLength >> 2] = ulLength;

    /*
     * Write the data into the transmit buffer.
     */

    if(ulLength %2 ==0)
        DMAWrite(ulEndpoint, (void *)*ppucData, ulLength);
    else
        DMAOddWrite(ulEndpoint, (void *)*ppucData, ulLength);


    *ppucData += ulLength;

    /*
     * Decrement the count of bytes to write.
     */
    *pusLength -= ulLength;


}

/*----------------------------------------------------------------------------------*/

/*
 * USBEnableEndpoint 
 * Enable or Disable the Endpoint FIFO
 */
static void
USBEnableEndpoint(unsigned long ulEndpoint,unsigned char bEnable)
{

    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;

    /*
     * Set the mode of  the endpoint .
     */
    if(bEnable ==1) {
        pulUSB[HwUSBEndpointType >> 2] &= ~USB_EPTYPE_ENABLE;
        pulUSB[HwUSBEndpointType >> 2] |= USB_EPTYPE_ENABLE;
    } else {
        pulUSB[HwUSBEndpointType >> 2] &= ~USB_EPTYPE_ENABLE;
    }

    return;

}

/*
 * USBStallEndpoint stalls or un-stalls the specified endpoint.
 */
void
USBStallEndpoint(unsigned long ulEndpoint, unsigned long bStall)
{
    /*
     * Select the appropriate endpoint.
     */
    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;
    /*
     * Stall or Unstall the appropriate endpoint.
     */
    if(bStall) {
        pulUSB[HwUSBEndpointControl >> 2] |= USB_EPCONTROL_STALL;
    } else {
        pulUSB[HwUSBEndpointControl >> 2] &= ~USB_EPCONTROL_STALL;
        pulUSB[HwUSBEndpointType >> 2] &= ~USB_EPTYPE_ENABLE;
        pulUSB[HwUSBEndpointType >> 2] |= USB_EPTYPE_ENABLE;
    }
}


/*
 * USBSendControl transmits a block of data back to the host via the control
 * endpoint.
 */
unsigned long
USBSendControl(const unsigned char *pucData, unsigned long ulLength)
{

    /*
     * If a block is already being transmitted, then return a failure.
     */
    if(sUSB.usControlInCount) {
        return(0);
    }

    /*
     * Prepare to transmit this block back to the host.
     */
    sUSB.pucControlIn = pucData;
    sUSB.usControlInCount = ulLength;

    /*
     * Send the first packet of this block back to the host.
     */
    USBWriteEndpoint(USB_ENDPOINT_CONTROL_IN, &sUSB.pucControlIn,
                     &sUSB.usControlInCount);

    /*
     * Success.
     */
    return(1);
}


/*
 *USBSendBulk transmits a block of data back to the host via the bulk  endpoint.
 */
unsigned long
USBSendBulk(const unsigned char *pucData, unsigned long ulLength)
{
    /*
     * If a block is already being transmitted, then return a failure.
     */
    if(sUSB.usBulkInCount) {
        return(0);
    }

    /*
     * Prepare to transmit this block back to the host.
     */
    sUSB.pucBulkIn = pucData;
    sUSB.usBulkInCount = ulLength;

    /*
     * Send the first packet of this block back to the host.
     */
#ifdef   USE_DMA_WRITE

    USBDMAWriteEndpoint(USB_ENDPOINT_TWO_IN, &sUSB.pucBulkIn,
                        &sUSB.usBulkInCount);
#else

    USBWriteEndpoint(USB_ENDPOINT_TWO_IN, &sUSB.pucBulkIn,
                     &sUSB.usBulkInCount);
#endif

    /*
     * Success.
     */
    ulLength=ulLength-sUSB.usBulkInCount;
    sUSB.usBulkInCount=0;

    return(ulLength-sUSB.usBulkInCount);

}

/*
 * USBReceiveBulk reads a block of data from the host via the bulk endpoint.
 */
#ifdef gadget_debug
static unsigned long
USBReceiveBulk(unsigned char *pucData, unsigned long ulLength)
{
    /*
     * Prepare to read data from the host into this buffer.
     */
    sUSB.pucBulkOut = pucData;
    sUSB.usBulkOutCount = ulLength;

    /*
     * Success.
     */
    return(1);
}

#endif

/*----------------------------------------------------------------------------*/

/*
 * USBGetEndpointFifoLen 
 * Get the length of the EndpointFifo of the endpoint.
 */
unsigned long
USBGetEndpointFifoLen(unsigned long ulEndpoint)
{
    unsigned long ulLength;

    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;

    /*
     * Send the Validate buffer command so that the endpoint can send
     * the packet.
     */
    ulLength = pulUSB[HwUSBEndpointBufferLength >> 2] & USB_EPBUFLEN_MASK;

    return(ulLength);
}



/*
 *
 * USBClearEndpointBuffer 
 * send the clear buffer command  so that the endpoint can receive another packet.
 *
 */
static void
USBClearEndpointBuffer(unsigned long ulEndpoint)
{
    /*
     * Select the appropriate endpoint.
     */
    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;

   /*
    *Send the clear buffer command so that the endpoint can receive
    *another packet.
    */
    pulUSB[HwUSBEndpointControl >> 2] |= USB_EPCONTROL_CLEAR;

    return;
}


/*
 *
 * USBGetEndpointStatus 
 * Get the EndpointStatus of the endpoint.
 */
unsigned long 
USBGetEndpointStatus(unsigned long ulEndpoint)
{
    unsigned long ulEndpointStatus;
    /*
     * Select the appropriate endpoint.
     */
    pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;

   /*
    * Send the Validate buffer command so that the endpoint can send
    * the packet.
    */
    ulEndpointStatus = pulUSB[HwUSBEndpointControl >> 2] ;

    return(ulEndpointStatus);
}


/*----------------------------------------------------------------------------------*/
/*
 *USBSlavePreInit
 */
void
USBSlavePreInit(unsigned long pulUSBStartAddress)
{
      
    /*
     *Init The USB default Speed
     */
    sUSB.ucUSBSpeed = FullSpeed;

    /*
     * Indicate that the SoftConnect pull-up is active and that we are not
     * configured.
     */
    sUSB.ucFlags = 2;
    sUSB.ucSetupFlags = 0;
    sUSB.usBulkInCount = 0;
    sUSB.ulBulkMAXLen = 64;
    sUSB.ulWriteEndpointMAXLen = 64;


    pulUSB  = (unsigned long *)pulUSBStartAddress;
    pulUSB16  = (unsigned short *)pulUSBStartAddress;
    pulUSBDataPort = (unsigned short *)(pulUSBStartAddress+HwUSBEndpointData);

}

/*
 *USB_DMA_init allocate the DMA uncache buffer,and clear 0
 */
void
USB_DMA_init(void)
{
    int i;
/*
    DMAPHYaddress = consistent_alloc(GFP_KERNEL|GFP_DMA,4096,&DMAVaddress);
    for(i=0;i<(4096/4);i++) {
        *((unsigned long *)(DMAPHYaddress)+i) =0;

    }
*/
	DMAPHYaddress = (unsigned short *)dma_alloc_coherent(NULL,4096,(void *)&DMAVaddress, 0);

	for(i=0;i<(4096/4);i++) {
        *((unsigned long *)(DMAPHYaddress)+i) =0;

    }
	
}


/*
 *USBDMAinit initialize the DMA setting of EP93xx and ISp1581
 */
void USBDMAinit(void)
{

    unsigned long ulTemp;

    pulUSB[HwUSBDMAEndpoint >> 2] = USB_DMAEP_SEVEN_OUT;/*111(7) 0(RX)     */

    /*
     *init Ep931x DMA
     */
    pulDMA[HwDMA_M2M_Control >> 2] = (DMA_M2M_CONTROL_DONE_INT_EN |
                                      DMA_M2M_CONTROL_BWC_FULL |
                                      DMA_M2M_CONTROL_PW_HALFWORD |
                                      DMA_M2M_CONTROL_SAH |
                                      DMA_M2M_CONTROL_TM_HARDWARE_P2M |
                                      DMA_PIN_CONFIG |
                                      DMA_M2M_CONTROL_RSS_EXTERNAL_DREQ |
                                      (5 << DMA_M2M_CONTROL_PWSC_SHIFT));

    ulTemp = pulDMA[HwDMA_M2M_Control >> 2];
    mdelay(10);
    /*printk("pulDMA[HwDMA_M2M_Control >> 2] =%x\n",ulTemp);*/


    /*
     *init 1581 DMA
     */
    pulUSB16[HwUSBDMAConfig >> 1] = (USB_DMACONFIG_BURST_1|
                                     /* (USB_DMACONFIG_BURST_ALL |*/
                                     USB_DMACONFIG_MODE_DACK | 
                                     USB_DMACONFIG_WIDTH_16);  
    ulTemp= pulUSB16[HwUSBDMAConfig >>1];
    mdelay(10);
    /*printk(" pulUSB[HwUSBDMAConfig >> 2]=%x\n",ulTemp);*/

    /*   0(14bit) not ignor the IOREADY;
            0(13bit) not ATA mod;
         00(12 11 bit) udma/mdma mode0;
         000(10 9 8bit)PIO time; 
         0(7bit)  not disable DMA count;
         000(6 5 4)burst[2:0] 
    high 8bit  */
    pulUSB16[HwUSBDMAHardware >> 1] = (USB_DMAHW_ENDIAN_NORMAL |
                                       USB_PIN_CONFIG |
                                       USB_DMAHW_WRITE_ACTIVE_LOW |
                                       USB_DMAHW_READ_ACTIVE_LOW);
    ulTemp=pulUSB16[HwUSBDMAHardware >> 1];
    mdelay(10);
    /*printk(" pulUSB[HwUSBDMAHardware >> 2]=%x\n",ulTemp);*/

    /*
    00(7 6 bit) 16bit little endian
    1(5 bit) EOT    high
    0(4 bit) GDMA slave mode
    1(3 bit) DACK    high
    1(2 bit) DREQ    high
    0(1 bit) DIOW low
    0(0 bit) DIOR low
    */

    /*
     *1581 DMA enable int
     */
    pulUSB16[HwUSBDMAIntEnable >> 1]  = 0x0D00;

}


/*
 * USBEnable configures the ISP1581 device.
 */
void
USBChipEnable(void)
{
    /*
     * Configure the ISP1581 and enable the SoftConnect pull-up.       
     */
  
    pulUSB[HwUSBMode >> 2] = 0;
    pulUSB[HwUSBMode >> 2] = USB_MODE_INT_ENABLE | USB_MODE_SOFT_CONNECT;
    mdelay(1);
    
    /*
     * Configure the interrupt Setting line.      
     * EDB931x INT intput High Level valdation      
     */
    pulUSB[HwUSBIntConfig >> 2] = (USB_INTCONFIG_CDBGMOD_ACK |
                                   USB_INTCONFIG_DDBGMODIN_ACK |
                                   USB_INTCONFIG_DDBGMODOUT_ACK|0x01);
    mdelay(1);
    /*0x55=|0101|0101|*/
    /*
     * Enable the interrupts source for the bulk endpoints.      
     */
    pulUSB[HwUSBIntEnable >> 2] = ( USB_INT_EP2_TX | USB_INT_EP2_RX |USB_INT_HS_STATUS|
                                    USB_INT_EP1_RX|USB_INT_EP1_TX|USB_INT_EP0_TX |
                                    USB_INT_EP0_SETUP |USB_INT_EP0_RX|USB_INT_DMA);
    mdelay(1);
    /*
     * Configure the  bulk endpoint.      
     * Select EP2 is Bulk OUT EndPoint, EP1 is BULK IN EndPoint      
     * Set the MaxPacketSize      
     * Set The EndPoint Type      
     * Enable The EndPoint      
     */
    pulUSB[HwUSBEndpointIndex >> 2] = USB_ENDPOINT_ONE_OUT;
    pulUSB[HwUSBEndpointMaxPacketSize >> 2] = 1024;
    pulUSB[HwUSBEndpointIndex >> 2] = USB_ENDPOINT_ONE_IN;
    pulUSB[HwUSBEndpointMaxPacketSize >> 2] = 0;
    pulUSB[HwUSBEndpointIndex >> 2] = USB_ENDPOINT_TWO_IN;
    pulUSB[HwUSBEndpointMaxPacketSize >> 2] = 1024;
    pulUSB[HwUSBEndpointIndex >> 2] = USB_ENDPOINT_TWO_OUT;
    pulUSB[HwUSBEndpointMaxPacketSize >> 2] = 0;

    pulUSB[HwUSBEndpointIndex >> 2]  = USB_ENDPOINT_ONE_OUT;
    pulUSB[HwUSBEndpointType >> 2]  = (USB_EPTYPE_NO_EMPTY |
                                       USB_EPTYPE_DOUBLE_BUFFER |USB_EPTYPE_ENABLE |
                                       USB_EPTYPE_TYPE_BULK);
    pulUSB[HwUSBEndpointIndex >> 2]  = USB_ENDPOINT_ONE_IN;
    pulUSB[HwUSBEndpointType >> 2]   = 0;
    pulUSB[HwUSBEndpointIndex >> 2]  = USB_ENDPOINT_TWO_IN;
    pulUSB[HwUSBEndpointType >> 2]   = (USB_EPTYPE_NO_EMPTY |
                                        USB_EPTYPE_DOUBLE_BUFFER |USB_EPTYPE_ENABLE |
                                        USB_EPTYPE_TYPE_BULK);
    pulUSB[HwUSBEndpointIndex >> 2]  = USB_ENDPOINT_TWO_OUT;
    pulUSB[HwUSBEndpointType >> 2]  = 0;
    /*
     *Clear The Bufferof The Bulk Endpoint      
     */
    pulUSB[HwUSBEndpointIndex >> 2] = USB_ENDPOINT_ONE_OUT;
    pulUSB[HwUSBEndpointControl >> 2] |= USB_EPCONTROL_CLEAR;
    pulUSB[HwUSBEndpointIndex >> 2] = USB_ENDPOINT_TWO_IN;
    pulUSB[HwUSBEndpointControl >> 2] |= USB_EPCONTROL_CLEAR;

    /*
     * Enable the device and reset the device address.      
     */
    pulUSB[HwUSBAddress >>2] = USB_ADDRESS_DEVICE_ENABLE;
    mdelay(10);

    /*
     *Configure USB Chip DMA part
     */
    USBDMAinit();


}

/*
 * USBDisable de-configures the ISP1581 device.
 */
void
USBDisable(void)
{
    /*
     * Disable the interrupts for the bulk endpoints.
     */
    pulUSB[HwUSBIntEnable >> 2] = 0;

    /*
     * Disable the SoftConnect pull-up.
     */
    pulUSB[HwUSBMode >> 2] = 0;

    /*
     * Indicate that the SoftConnect pull-up is no longer active.
     */
    sUSB.ucFlags &= ~2;
}

/*----------------------------------------------------------------------------------*/

/*
 * USBWakeUpCs :WakeUp the ISP1581 device.
 */
void
USBWakeUpCs(void)
{

    pulUSB[HwUSBMode >> 2] = USB_MODE_WAKE_UP_CS;

}

/*
 * USBSoftReSet : SoftRest  the ISP1581 device.
 */
void
USBSoftReSet(void)
{

    pulUSB[HwUSBMode >> 2] = USB_MODE_SOFT_RESET;

}

/*
 * USBSuspendDevice : Set The ISP1581 USB Device to go suspend mode , and set the CLOCK off.
 */
void
USBSuspendDevice(void)
{

    pulUSB[HwUSBMode >> 2] = USB_MODE_GO_SUSPEND ;

}

/*
 * USBResumDevice : Resume the ISP1581 device.
 */
void
USBResumDevice(void)
{

    pulUSB[HwUSBMode >> 2] = USB_MODE_SEND_RESUME |USB_MODE_CLOCK_ON;

}

/*
 * USBSoftConnect :SoftConnect the ISP1581 device,
 *
 * Enables the D+ (or potentially D-) pullup.  The host will start
 * enumerating this gadget when the pullup is active and a VBUS session
 * is active (the link is powered).  This pullup is always enabled unless
 * usb_gadget_disconnect() has been used to disable it.
 */
void
USBSoftConnect(void)
{

    pulUSB[HwUSBMode >> 2] = pulUSB[HwUSBMode >> 2]|USB_MODE_SOFT_CONNECT;

}


/* USBSoftDisConnect - Soft Disconnect the ISP1581 device,
 *
 * Disables the D+ (or potentially D-) pullup, which the host may see
 * as a disconnect (when a VBUS session is active).  Not all systems
 * support software pullup controls.
 *
 * This routine may be used during the gadget driver bind() call to prevent
 * the peripheral from ever being visible to the USB host, unless later
 * usb_gadget_connect() is called.  For example, user mode components may
 * need to be activated before the system can talk to hosts.
 *
 * Returns zero on success, else negative errno.
 */
void
USBSoftDisConnect(void)
{

    pulUSB[HwUSBMode >> 2] = pulUSB[HwUSBMode >> 2]&(~USB_MODE_SOFT_CONNECT);

}


/*
 *USBGet_frame_number  Get the Current Frame Number that the ISP1582 got
 */
static unsigned short
USBGet_frame_number(void)
{
	unsigned short usGet_Frame_Number=0;
	
	usGet_Frame_Number = pulUSB16[HwUSBFrameNumber >> 1] ;

	return usGet_Frame_Number;
}

/*-----------------------------------------------------------------------------------*/

/* *handles USB Starndar device requests command. */

/*
 * USBGetStatus implements the USB Get_Status device request.
 */
static void USBGetStatus(struct ep93xx_udc *dev)
{
    unsigned char ucStatus[2];
    unsigned long ulEndpoint;

	printk("USBGetStatus\n");
    /*
     *Set the USB control process status
     */
    sUSB.ucSetupFlags = SETUPCOMMAND_DataPhase_INPID;
	
	dev->ep0state=EP0_IN_DATA_PHASE;

    /*
     * Determine how to handle this request based on the recipient.
     */
    /*switch(sUSB.sControlOut.bmRequestType & USB_RT_RECIPIENT_MASK) {*/
	switch(dev->sControlOut->bRequestType & USB_RT_RECIPIENT_MASK) {
        /*
         * If the recipient is a device, return the state of the device's
         * remote wakeup and self powered states.
         */
        case USB_RT_RECIPIENT_DEVICE: {
            /*
             * The player is self powered and does not support remote wakeup.
             */
            ucStatus[0] = USB_DEVICE_STATUS_SELF_POWERED;
            ucStatus[1] = 0;

            /*
             * Send our response back to the host.
             */
            USBSendControl(ucStatus, 2);

            /*
             * We're done handling this request.
             */
            break;
        }

        /*
         * If the recipient is a device interface, return a value of
         * 0x00 as required by the USB spec.
         */
        case USB_RT_RECIPIENT_INTERFACE: {
            /*
             * The USB spec. requires a GetStatus request for an interface
             * return a pair of zero bytes.
             */
            ucStatus[0] = 0;
            ucStatus[1] = 0;

            /*
             * Send our response back to the host.
             */
            USBSendControl(ucStatus, 2);

            /*
             * We're done handling this request.
             */
            break;
        }

        /*
         * If the recipient is an endpoint, determine whether it is stalled or
         * not and return that information to the host.
         */
        case USB_RT_RECIPIENT_ENDPOINT: {
            /*
             * Find out which endpoint is the recipient of the request.
             */
            /*ulEndpoint = sUSB.sControlOut.wIndex & USB_ENDPOINT_ADDRESS_MASK;*/
			ulEndpoint = dev->sControlOut->wIndex & USB_ENDPOINT_ADDRESS_MASK;


            /*
             * Determine whether the IN or the OUT endpoint is being addressed
             * in the device request.
             */
            ulEndpoint *= 2;
            /*if(sUSB.sControlOut.wIndex & USB_ENDPOINT_DIRECTION_MASK) {*/
			if(dev->sControlOut->wIndex & USB_ENDPOINT_DIRECTION_MASK) {
                ulEndpoint++;
            }

            /*
             * Make sure that the specified endpoint is valid.
             */
            if((ulEndpoint != USB_ENDPOINT_CONTROL_OUT) &&
               (ulEndpoint != USB_ENDPOINT_CONTROL_IN) &&
               (ulEndpoint != USB_ENDPOINT_ONE_OUT) &&
               (ulEndpoint != USB_ENDPOINT_TWO_IN)) {
                /*
                 * An invalid endpoint was specified, so stall both control
                 * endpoints.
                 */
                USBStallEndpoint(USB_ENDPOINT_CONTROL_OUT, 1);
                USBStallEndpoint(USB_ENDPOINT_CONTROL_IN, 1);

                /*
                 * There is nothing further to do.
                 */
                return;
            }

            /*
             * Read the endpoint status.
             */
            pulUSB[HwUSBEndpointIndex >> 2] = ulEndpoint;
            ulEndpoint = pulUSB[HwUSBEndpointControl >> 2];

            /*
             * Send the endpoint's status to the host.
             */
            if(ulEndpoint & USB_EPCONTROL_STALL) {
                ucStatus[0] = USB_ENDPOINT_STATUS_STALLED;
            } else {
                ucStatus[0] = 0;
            }
            ucStatus[1] = 0;

            /*
             * Send our response back to the host.
             */
            USBSendControl(ucStatus, 2);

            /*
             * We're done handling this request.
             */
            break;
        }

        /*
         * If an invalid request is received, stall the control endpoint.
         */
    default: {
            /*
             * Stall the both control endpoints.
             */
            USBStallEndpoint(USB_ENDPOINT_CONTROL_OUT, 1);
            USBStallEndpoint(USB_ENDPOINT_CONTROL_IN, 1);

            /*
              * We're done handling this request.
              */
            break;
        }

    }

}

/*
 * USBClearFeature implements the USB Clear_Feature device request.
 */
/*static void USBClearFeature(void)*/
static void USBClearFeature(struct ep93xx_udc *dev)
{
    unsigned long ulEndpoint;
	
	printk("USBClearFeature:type=%x,value=%x,index=%x",
		dev->sControlOut->bRequestType,dev->sControlOut->wValue,
		dev->sControlOut->wIndex );
    /*
     *Set the USB control process status
     */
    sUSB.ucSetupFlags = SETUPCOMMAND_StatusPhase_INPID;

	dev->ep0state=EP0_IN_STATUS_PHASE;

    /*
     * The only feature we support is stall on an endpoint.
     */
    /*if(((sUSB.sControlOut.bmRequestType & USB_RT_RECIPIENT_MASK) ==*/
    if(((dev->sControlOut->bRequestType & USB_RT_RECIPIENT_MASK) ==
        USB_RT_RECIPIENT_ENDPOINT) &&
       (dev->sControlOut->wValue == USB_FEATURE_ENDPOINT_STALL)) {
        /*
         * Compute the endpoint number.
         */
        ulEndpoint = (dev->sControlOut->wIndex & USB_ENDPOINT_ADDRESS_MASK) * 2;
        if(dev->sControlOut->wIndex & USB_ENDPOINT_DIRECTION_MASK) {
            ulEndpoint++;
        }

        /*
         * Make sure that the specified endpoint is valid.
         */
        if((ulEndpoint != USB_ENDPOINT_CONTROL_OUT) &&
           (ulEndpoint != USB_ENDPOINT_CONTROL_IN) &&
           (ulEndpoint != USB_ENDPOINT_ONE_OUT) &&
           (ulEndpoint != USB_ENDPOINT_TWO_IN)) {
            /*
             * An invalid endpoint was specified, so stall both control
             * endpoints.
             */
            USBStallEndpoint(USB_ENDPOINT_CONTROL_OUT, 1);
            USBStallEndpoint(USB_ENDPOINT_CONTROL_IN, 1);

            /*
             * There is nothing further to do.
             */
            return;
        }

        /*
         * Clear the stall condition on the specified endpoint.
         */
        USBStallEndpoint(ulEndpoint, 0);

        /*
         * Respond to the host with an empty packet.
         */
        pulUSB[HwUSBEndpointControl >> 2] |= USB_EPCONTROL_STATUS_ACK;

    } else {
        /*
         * An unknown feature was specified, so stall both control endpoints.
         */
        USBStallEndpoint(USB_ENDPOINT_CONTROL_OUT, 1);
        USBStallEndpoint(USB_ENDPOINT_CONTROL_IN, 1);
    }

}

/*
 * USBSetFeature implements the USB Set_Feature device request.
 */
/*static void USBSetFeature(void)*/
static void USBSetFeature(struct ep93xx_udc *dev)
{
    unsigned long ulEndpoint;
	
	printk("USBSetFeature\n");
    /*
     *Set the USB control process status
     */
    sUSB.ucSetupFlags = SETUPCOMMAND_StatusPhase_INPID;

	dev->ep0state=EP0_IN_STATUS_PHASE;

    /*
     * The only feature we support is stall on an endpoint.
     */
    if(((dev->sControlOut->bRequestType & USB_RT_RECIPIENT_MASK) ==
        USB_RT_RECIPIENT_ENDPOINT) &&
       (dev->sControlOut->wValue == USB_FEATURE_ENDPOINT_STALL)) {
        /*
         * Compute the endpoint number.
         */
        ulEndpoint = (dev->sControlOut->wIndex & USB_ENDPOINT_ADDRESS_MASK) * 2;
        if(dev->sControlOut->wIndex & USB_ENDPOINT_DIRECTION_MASK) {
            ulEndpoint++;
        }

        /*
         * Make sure that the specified endpoint is valid.
         */
        if((ulEndpoint != USB_ENDPOINT_CONTROL_OUT) &&
           (ulEndpoint != USB_ENDPOINT_CONTROL_IN) &&
           (ulEndpoint != USB_ENDPOINT_ONE_OUT) &&
           (ulEndpoint != USB_ENDPOINT_TWO_IN)) {
            /*
             * An invalid endpoint was specified, so stall both control
             * endpoints.
             */
            USBStallEndpoint(USB_ENDPOINT_CONTROL_OUT, 1);
            USBStallEndpoint(USB_ENDPOINT_CONTROL_IN, 1);

            /*
             * There is nothing further to do.
             */
            return;
        }

        /*
         * Set the stall condition on the specified endpoint.
         */
        USBStallEndpoint(ulEndpoint, 1);
    } else {
        /*
         * An unknown feature was specified, so stall both control endpoints.
         */
        USBStallEndpoint(USB_ENDPOINT_CONTROL_OUT, 1);
        USBStallEndpoint(USB_ENDPOINT_CONTROL_IN, 1);
    }

}

/*
 * USBSetAddress implements the USB Set_Address device request.
 */
/*static void USBSetAddress(void)*/
static void USBSetAddress(struct ep93xx_udc *dev)
{
    /*DEBUG("USBSetAddress\n");*/
    /*
     * Configure our UBS controller for the USB address assigned by the host.
     */
    pulUSB[HwUSBAddress >> 2] = (USB_ADDRESS_DEVICE_ENABLE |
                                 dev->sControlOut->wValue);

    /*
     *Set the USB control process status
     */
    sUSB.ucSetupFlags = SETUPCOMMAND_StatusPhase_INPID;

	dev->ep0state=EP0_IN_STATUS_PHASE;
	dev->ep0req->length=0;
	dev->ep0req->actual=0;
    /*
     * Select the appropriate endpoint.
     */
    pulUSB[HwUSBEndpointIndex >> 2] = USB_ENDPOINT_CONTROL_OUT;


    /*
     * Respond to the host with an empty packet.
     */
    pulUSB[HwUSBEndpointControl >> 2] = USB_EPCONTROL_STATUS_ACK|0x01;

}

/*
 * USBGetDescriptor implements the USB Get_Descriptor device request.
 */
/*static void USBGetDescriptor(void)*/
static void USBGetDescriptor(struct ep93xx_udc *dev)
{
    const unsigned char *pucDescriptor = 0;
    unsigned long ulLength = 0;

    printk("USBGetDescriptor\n");
    /*
     * Determine how to handle this request based on the requested descriptor.
     */
    switch(dev->sControlOut->wValue & USB_DESCRIPTOR_TYPE_MASK) {
        /*
         * The device descriptor was requested.
         */
    case USB_DESCRIPTOR_DEVICE: {
            /*
             * Prepare to return the device descriptor.
             */
            pucDescriptor = ucDeviceDescriptor;
            ulLength = sizeof(ucDeviceDescriptor);

            /*
             * We're done handling this request.
             */
            break;
        }

        /*
         * The configuration descriptor was requested.
         */
    case USB_DESCRIPTOR_CONFIGURATION: {
            /*
             * Prepare to return the configuration descriptor.
             */
            /*if(sUSB.ucUSBSpeed == FullSpeed) {*/
			if(dev->gadget.speed == USB_SPEED_FULL){
                pucDescriptor = ucConfigurationDescriptor;
                ulLength = sizeof(ucConfigurationDescriptor);
            /*} else if(sUSB.ucUSBSpeed == HighSpeed) {*/
			} else if(dev->gadget.speed== USB_SPEED_HIGH) {
                pucDescriptor = ucConfigurationHighDescriptor;
                ulLength = sizeof(ucConfigurationHighDescriptor);
            }

            /*
             * We're done handling this request.
             */
            break;
        }

        /*
         * A string descriptor was requested.
         */
    case USB_DESCRIPTOR_STRING: {
            /*
             * Prepare to return the requested string.
             */
            switch(dev->sControlOut->wValue & USB_DESCRIPTOR_INDEX_MASK) {
                /*
                 * String index 0 is the language ID string.
                 */
            case 0x00: {
                    pucDescriptor = ucString0;
                    ulLength = sizeof(ucString0);
                    break;
                }

                /*
                  * String index 1 is the manufacturer name.
                  */
            case 0x01: {
                    pucDescriptor = ucString1;
                    ulLength = sizeof(ucString1);
                    break;
                }

                /*
                  * String index 2 is the product name.
                  */
            case 0x02: {
                    pucDescriptor = ucString2;
                    ulLength = sizeof(ucString2);
                    break;
                }

                /*
                  * String index 3 is the configuration name.
                  */
            case 0x03: {
                    pucDescriptor = ucString3;
                    ulLength = sizeof(ucString3);
                    break;
                }
            }

            /*
              * We're done handling this request.
              */
            break;
        }

        /*
          * An invalid request is received, so stall both control endpoints.
          */
    default: {
            /*
              * Stall both of the control endpoints.
              */
            USBStallEndpoint(USB_ENDPOINT_CONTROL_OUT, 1);
            USBStallEndpoint(USB_ENDPOINT_CONTROL_IN, 1);

            /*
              * We're done handling this request.
              */
            return;
        }
    }

     /*
      * If the requested length is less than the length of the descriptor to be
      * returned, then simply return the requested portion of the descriptor.
      */
    if(dev->sControlOut->wLength < ulLength) {
        ulLength = dev->sControlOut->wLength;
    }

     /*
      *Set the USB control process status
      */
    sUSB.ucSetupFlags = SETUPCOMMAND_DataPhase_INPID;

	dev->ep0state=EP0_IN_DATA_PHASE;
     /*
      * Send the descriptor back to the host.
      */
    USBSendControl(pucDescriptor, ulLength);

}

/*
 * USBGetConfiguration implements the USB Get_Configuration device request.
 */
/*static void USBGetConfiguration(void)*/

static void USBGetConfiguration(struct ep93xx_udc *dev)
{
    unsigned char ucConfiguration = sUSB.ucFlags & 1;

    printk("USBGetConfiguration\n");
    /*
     *Set the USB control process status
     */
    sUSB.ucSetupFlags = SETUPCOMMAND_DataPhase_INPID;

	dev->ep0state=EP0_IN_DATA_PHASE;
    /*
     * Send the current configuration value back to the host.
     */
    USBSendControl(&ucConfiguration, 1);
}

/*
 * USBSetConfiguration implements the USB Set_Configuration device request.
 */
/*static void USBSetConfiguration(void)*/
static void USBSetConfiguration(struct ep93xx_udc *dev)
{

    /*DEBUG("USBSetConfiguration :%d\n",dev->sControlOut->wValue);*/
    /*
     *Set the USB control process status
     */
    sUSB.ucSetupFlags = SETUPCOMMAND_StatusPhase_INPID;


	dev->ep0state=EP0_IN_STATUS_PHASE;
    /*
     * If the requested configuration is zero, then go into the unconfigured
     * state.
     */
    if(dev->sControlOut->wValue == 0) {
         /*
          * Clear the global configuration flag.
          */
        sUSB.ucFlags &= ~1;

         /*
          * Disable the generic endpoints.
          */
        pulUSB[HwUSBEndpointType >> 2] &= ~USB_EPTYPE_ENABLE;
        pulUSB[HwUSBEndpointType >> 2] &= ~USB_EPTYPE_ENABLE;

         /*
          * Respond to the host with an empty packet.
          */
        pulUSB[HwUSBEndpointControl >> 2] |= USB_EPCONTROL_STATUS_ACK;
    }

     /*
      * If the requested configuration is one, then go into the configured
      * state.
      */
    else if(dev->sControlOut->wValue == 1) {
         /*
          * Set the global configuration flag.
          */
        sUSB.ucFlags |= 1;

         /*
          * Disable the generic endpoints.
          */
        pulUSB[HwUSBEndpointType >> 2] &= ~USB_EPTYPE_ENABLE;
        pulUSB[HwUSBEndpointType >> 2] &= ~USB_EPTYPE_ENABLE;

         /*
          * Enable the generic contorl endpoints.
          */
        USBStallEndpoint(USB_ENDPOINT_CONTROL_OUT,0);
        USBStallEndpoint(USB_ENDPOINT_CONTROL_IN,0);

         /*
          * Respond to the host with an empty packet.
          */
        pulUSB[HwUSBEndpointControl >> 2] |= USB_EPCONTROL_STATUS_ACK;


    }

     /*
      * If the requested configuration is anything else, then stall both of the
      * control endpoints.
      */
    else {
        USBStallEndpoint(USB_ENDPOINT_CONTROL_OUT, 1);
        USBStallEndpoint(USB_ENDPOINT_CONTROL_IN, 1);
    }


}

/*
 * USBGetInterface implements the USB Get_Interface device request.
 */
/*static void USBGetInterface(void)*/
static void USBGetInterface(struct ep93xx_udc *dev)
{
    unsigned char ucInterface = 0;

	printk("USBGetInterface\n");
     /*
      *Set the USB control process status
      */
    sUSB.ucSetupFlags = SETUPCOMMAND_DataPhase_INPID;

	dev->ep0state=EP0_IN_DATA_PHASE;
     /*
      * We only support a single interface, so the current interface is always
      * the first one.  Send our response back to the host.
      */
    USBSendControl(&ucInterface, 1);
}

/*
 * USBSetInterface implements the USB Set_Interface device request.
 */
/*static void USBSetInterface(void)*/
static void USBSetInterface(struct ep93xx_udc *dev)
{
	printk("USBSetInterface\n");
    /*
     *Set the USB control process status
     */
    sUSB.ucSetupFlags = SETUPCOMMAND_StatusPhase_INPID;


	dev->ep0state=EP0_IN_STATUS_PHASE;
     /*
      * We only support a single interface.
      */
    if((dev->sControlOut->wValue == 0) && (dev->sControlOut->wIndex == 0)) {
         /*
          * The first interface was requested, so do nothing.
          */
        return;
    } else {
         /*
          * A non-existent interface was requested, so stall both control
          * endpoints.
          */
        USBStallEndpoint(USB_ENDPOINT_CONTROL_OUT, 1);
        USBStallEndpoint(USB_ENDPOINT_CONTROL_IN, 1);
    }
}

/*
 *vendor specific device or class specific device Command
 */

/*
 * USBReserved handles device requests which are not supported by this USB
 * device implementation.
 */
/*static void USBReserved(void)*/
static void USBReserved(struct ep93xx_udc *dev)
{
    printk("This USB Command is not support by Our USB Device,So Stall the control pipe\n");
     /*
      * Stall both control endpoints.
      */
    USBStallEndpoint(USB_ENDPOINT_CONTROL_OUT, 1);
    USBStallEndpoint(USB_ENDPOINT_CONTROL_IN, 1);
}



/*------------------------------------------------------------------------------------*/
/*
 * USBISR is the interrupt handler routine for the USB.
 */

void USBISR (int irq,void * parmer,struct pt_regs *r)
{
    unsigned long ulIntStatus;
    unsigned long usLength;
    unsigned char *pucChar;
    

     /*
      * Read the ISP1581 interrupt register.
      */
    ulIntStatus = pulUSB[HwUSBIntReason >> 2];

     /*
      * Clear the interrupt reason register.
      */
    pulUSB[HwUSBIntReason >> 2] =ulIntStatus;


     /*
      * Handle a hs status change.
      */
    if(ulIntStatus & USB_INT_HS_STATUS)
    {
         /*
          * Reconfigure the ISP1581.
          */
        sUSB.ucUSBSpeed = HighSpeed;
        sUSB.ulBulkMAXLen = 512;
        USBChipEnable();

         /*
          * We're done handling the interrupts.
          */
    }

     /*
      * Handle a bus reset.
      */
    if(ulIntStatus & USB_INT_BUS_RESET)
    {
         /*
          * Reconfigure the ISP1581.
          */
        USBChipEnable();

         /*
          *We're done handling the interrupts.
          */

    }

    /*
      *Handle an RX interrupt on the bulk out endpoint.
      */
    if(ulIntStatus & USB_INT_EP1_RX)
    {
        /*
          *Read the data packet.
          */
    }

    /*
      * Handle an interrupt on the control in endpoint.
      */
    if(ulIntStatus & USB_INT_EP0_TX)
    {


        if(sUSB.usControlInCount !=0) {
             /*
              * Send the next packet of data to the host.
              */
            USBWriteEndpoint(USB_ENDPOINT_CONTROL_IN, &sUSB.pucControlIn,
                             &sUSB.usControlInCount);
        } else {
            if(sUSB.ucSetupFlags==3) {
                sUSB.ucSetupFlags=0;
            } else if(sUSB.ucSetupFlags==2) {
                sUSB.ucSetupFlags=4;

                pulUSB[HwUSBEndpointIndex >> 2] = USB_ENDPOINT_CONTROL_IN;

                pulUSB[HwUSBEndpointControl >> 2] |= USB_EPCONTROL_STATUS_ACK;

            } else {
                sUSB.ucSetupFlags=0;

            }

        }

    }

     /*
      *Handle an interrupt from a setup packet.
      */
    if(ulIntStatus & USB_INT_EP0_SETUP)
    {
         /*
          * Read the packet.
          */
        pucChar = (unsigned char *)&sUSB.sControlOut;
        usLength = sizeof(ControlTransfer);
        if(USBReadEndpoint(USB_ENDPOINT_SETUP, &pucChar,
                           &usLength) != sizeof(ControlTransfer)) {
             /*
              * The size of the setup packet is incorrect, so stall both of the
              * control endpoints.
              */
            USBStallEndpoint(USB_ENDPOINT_CONTROL_OUT, 1);
            USBStallEndpoint(USB_ENDPOINT_CONTROL_IN, 1);
            printk("EP0--setup   stall\n");
        } else {
             /*
              * Process the command in the setup packet.
              */
            if(((sUSB.sControlOut.bmRequestType & USB_RT_TYPE_MASK) ==
                USB_RT_TYPE_STANDARD) && (sUSB.sControlOut.bRequest < 16)) {
                 /*
                  *This is a standard request, so call the appropriate
                  *routine.
                  */
               /* (*USBStandardDeviceRequest[sUSB.sControlOut.bRequest])();*/
            } else if((sUSB.sControlOut.bmRequestType & USB_RT_TYPE_MASK) ==
                      USB_RT_TYPE_VENDOR) {
                 /*
                  *This is a vendor specific request.
                  */
                  /*USBVendor();*/
            } else if((sUSB.sControlOut.bmRequestType & USB_RT_TYPE_MASK) ==
                      USB_RT_TYPE_CLASS) {
                 /*
                  * This is a class specific request.
                  */
                 /*    USBClass(parmer);   */
            } else {
                 /*
                  * All other requests are treated as reserved requests.
                  */
                /*USBReserved(dev);*/
            }
        }


    }


     /*
      * Handle an TX interrupt on the bulk in endpoint.
      */
    if(ulIntStatus & USB_INT_EP2_TX)
    {
    /*    P1581ep2_tx(parmer);     */

    }


}


/*----------------------------------------------------------------------------------*/
#define	DRIVER_VERSION	"17-10-2006"
#define	DRIVER_DESC	"EP93xx USB Device Controller driver"

static const char driver_name [] = "ep93xx_udc";/*udc= usb device control*/
static const char ep0name [] = "ep0";

#ifdef DISABLE_TEST_MODE
/* (mode == 0) == no undocumented chip tweaks
 * (mode & 1)  == double buffer bulk IN
 * (mode & 2)  == double buffer bulk OUT
 * ... so mode = 3 (or 7, 15, etc) does it for both
 */
static ushort fifo_mode = 0;
module_param(fifo_mode, ushort, 0);
MODULE_PARM_DESC (fifo_mode, "ep93xx udc fifo mode");
#endif

static short IRQ_USB_SLAVE      =0;
/*---------------------------------------------------------------------------------------*/
static unsigned long 
UDC_SendControl(struct usb_request *req)
{
	unsigned char 	*buf;
	unsigned		length, count;
	/*struct ep93xx_request	*_req;
	struct ep93xx_udc	*dev = the_controller;
	struct ep93xx_ep	*ep = &(dev->ep[1]);

	_req = container_of(req, struct ep93xx_request, req);*/

	if((req==NULL)||(req->length==0)||(req->buf==NULL)){
		DEBUG("UDC_SendControl error\n");
		return 0;
	}
		
	req->actual=0;

	buf = req->buf + req->actual;

	length = req->length - req->actual;
	/*
     	 * Send the first packet of this block back to the host.
     	 */
    	count	= UDC_USBWriteEndpoint(USB_ENDPOINT_CONTROL_IN, buf,
                     		length,EP0_FIFO_SIZE);

	/* how big will this packet be? */

	req->actual += count;
	
	//list_add_tail(&(_req->queue),&(ep->queue));

    return count;

}

static unsigned long 
UDC_SendBulk(struct ep93xx_ep *ep,struct usb_request *req)
{
	unsigned char 	*buf;
	unsigned long	length, count;
	unsigned long 	ulEndpoint=0;

	
    ulEndpoint = ((ep->bEndpointAddress&0x80)>>7)|((ep->bEndpointAddress&0x7F)<<1);



	buf = req->buf + req->actual;

	length = req->length - req->actual;
	/*
	* Send the first packet of this block back to the host.
     	*/

#ifdef   USE_DMA_WRITE

    count	= UDC_USBDMAWriteEndpoint(ulEndpoint, buf,length,/*BULK_FIFO_SIZE512*/sUSB.ulBulkMAXLen);
#else	
    count	= UDC_USBWriteEndpoint(ulEndpoint, buf,
                     		length,/*BULK_FIFO_SIZE*/sUSB.ulBulkMAXLen);
#endif
	/* how big will this packet be? */
	req->actual += count;
	return count;


}

static unsigned long 
UDC_ReceiveBulk(struct ep93xx_ep *ep,struct usb_request *req)
{
	unsigned char 	*buf;
	unsigned long	length, count;
	unsigned long 	ulEndpoint=0;
	
	ulEndpoint = ((ep->bEndpointAddress&0x80)>>7)|((ep->bEndpointAddress&0x7F)<<1);


	buf = req->buf + req->actual;

	length = req->length - req->actual;
	/*
     	* Send the first packet of this block back to the host.
     	*/
    	count	= UDC_USBReadBulkEndpoint(ulEndpoint, buf,length);

	/* how big will this packet be? */

	req->actual += count;

	return count;
}

/*
 *	done - retire a request; caller blocked irqs
 */
static void done(struct ep93xx_ep *ep, struct ep93xx_request *req, int status)
{
	unsigned		stopped = ep->stopped;

	list_del_init(&req->queue);

	if (likely (req->req.status == -EINPROGRESS))
		req->req.status = status;
	else
		status = req->req.status;

	if (status && status != -ESHUTDOWN)
		printk("complete %s req %p stat %d len %u/%u\n",
			ep->ep.name, &req->req, status,
			req->req.actual, req->req.length);

	/* don't modify queue heads during completion callback */
	ep->stopped = 1;
	req->req.complete(&ep->ep, &req->req);
	ep->stopped = stopped;

	
}

/*
 * 	nuke - dequeue ALL requests
 */
static void nuke(struct ep93xx_ep *ep, int status)
{
	struct ep93xx_request  *req;
	
	while (!list_empty(&ep->queue)) {
		req = list_entry(ep->queue.next,
				struct ep93xx_request,
				queue);
		done(ep, req, status);
	}
	
	if (ep->desc)
		USBEnableEndpoint(ep->bEndpointAddress, 0);
	else
		printk("ep->desc is NULL\n");
}


/*
 * read_fifo -  unload packet(s) from the fifo we use for usb OUT
 * transfers and put them into the request.  caller should have made
 * sure there's at least one packet ready.
 *
 * returns true if the request completed because of short packet or the
 * request buffer having filled (and maybe overran till end-of-packet).
 */
static int
read_fifo (struct ep93xx_ep *ep, struct ep93xx_request *req)
{
	unsigned long is_short,count;

	while(req->req.length!=req->req.actual){

		ep->usSemStatus=1;
        down(&ep->wait_dio_cmd);
        ep->usSemStatus=0;
        if(ep->usEPStop==1){
	        ep->usEPStop=0;
            return 1 ;
        }

		DEBUG("bulk RX have data\n");
		
		disable_irq(IRQ_USB_SLAVE);
		count = UDC_ReceiveBulk(ep,&(req->req));
		enable_irq(IRQ_USB_SLAVE);

		is_short = (count < ep->ep.maxpacket);
		/* completion */
		if (is_short || req->req.actual == req->req.length) {
			done (ep, req, 0);
			if (list_empty(&ep->queue))
				DEBUG("irq_disable---read_fifo\n") ;
			return 1;
		}
	}
		/* finished that packet.  the next one may be waiting... */
	return 0;
}

/*
 * write to an IN endpoint fifo, as many packets as possible.
 * irqs will use this to write the rest later.
 * caller guarantees at least one packet buffer is ready (or a zlp).
 */
static int
write_fifo (struct ep93xx_ep *ep, struct ep93xx_request *req)
{
	unsigned	count;
	int		is_last=0;

	up(&ep->wait_dio_cmd);
    	
	while(is_last==0){

	/*down(&ep->wait_dio_cmd);*/
	ep->usSemStatus=1;
	down(&ep->wait_dio_cmd);
	ep->usSemStatus=0;
        if(ep->usEPStop==1){
	  	  ep->usEPStop=0;
                  return 1 ;
    	}
	
	disable_irq(IRQ_USB_SLAVE);
	/*local_irq_save(flags);*/
	count = UDC_SendBulk(ep,&(req->req));
	enable_irq(IRQ_USB_SLAVE);
	/* local_irq_restore(flags);*/
	if(req->req.length==req->req.actual)
        is_last = 1;

	}
		/* requests complete when all IN data is in the FIFO */
		done (ep, req, 0);
		is_last=0;
		DEBUG("write done\n");
		return 1;

		/*
		 TODO experiment: how robust can fifo mode tweaking be?
		 double buffering is off in the default fifo mode, which
		 prevents TFS from being set here.
        	*/

	return 0;
}


/*
 *ep93xx_ep_set_halt - sets the endpoint halt feature.
 *The ISP1581 reg HwUSBEndpointControl, the bit STALL can not to halt the ISO endpoint
 */
static int ep93xx_ep_set_halt(struct usb_ep *_ep, int value)
{
	struct ep93xx_ep	*ep;
	unsigned long		flags;
	unsigned long 		ulEndpoint;

	DEBUG(" ep93xx_ep_set_halt \n");

	ep = container_of(_ep, struct ep93xx_ep, ep);

	if (unlikely (!_ep || (!ep->desc) || (ep->bmAttributes == USB_ENDPOINT_XFER_ISOC))){
		printk("%s, bad ep\n", __FUNCTION__);
		return -EINVAL;
	}
	
	local_irq_save(flags);
	
	ulEndpoint	=	ep->bEndpointAddress;
	USBStallEndpoint(ulEndpoint, value);
	
 	local_irq_restore(flags);

	printk("%s halt\n", _ep->name);
	return 0;
}


/* for the ep93xx, these can just wrap kmalloc/kfree.  gadget drivers
 * must still pass correctly initialized endpoints, since other controller
 * drivers may care about how it's currently set up (dma issues etc).
 */

/*
 * 	ep93xx_ep_alloc_request - allocate a request data structure
 */
static struct usb_request *
ep93xx_ep_alloc_request (struct usb_ep *_ep, gfp_t gfp_flags)
{
	struct ep93xx_request   *req;
        struct ep93xx_udc       *dev;
        struct ep93xx_ep        *ep;

	DEBUG(" ep93xx_ep_alloc_request \n");


	ep = container_of (_ep, struct ep93xx_ep, ep);
	
	dev=ep->dev;
        
	if(dev->req_config==1){
            enable_irq(IRQ_USB_SLAVE);
            dev->req_config=0;
        }
	
	req = kmalloc (sizeof *req, gfp_flags);
	if (!req)
		return 0;

	memset (req, 0, sizeof *req);
	INIT_LIST_HEAD (&req->queue);
	return &req->req;
}


/*
 * 	ep93xx_ep_free_request - deallocate a request data structure
 */
static void
ep93xx_ep_free_request (struct usb_ep *_ep, struct usb_request *_req)
{
	struct ep93xx_request	*req;

	DEBUG(" ep93xx_ep_free_request \n");

	req = container_of (_req, struct ep93xx_request, req);
	if (!_req) {
		printk("%s, bad rep\n", __FUNCTION__);
		
	}
	
	WARN_ON (!list_empty (&req->queue));
	kfree(req);
}

static void *
ep93xx_ep_alloc_buffer(struct usb_ep *_ep, unsigned bytes,
	dma_addr_t *dma, gfp_t gfp_flags)
{
	char			*retval;

	DEBUG(" ep93xx_ep_alloc_buffer \n");
	
	retval = kmalloc (bytes, gfp_flags & ~(__GFP_DMA|__GFP_HIGHMEM));
	if (retval)
		*dma = /*virt_to_bus*/virt_to_phys(retval);
	return retval;
}

static void
ep93xx_ep_free_buffer(struct usb_ep *_ep, void *buf, dma_addr_t dma,
		unsigned bytes)
{

	DEBUG(" ep93xx_ep_free_buffer \n");
	
	kfree (buf);
}

static int ep93xx_ep_fifo_status(struct usb_ep *_ep)
{
	struct ep93xx_ep        *ep;
	unsigned long 			ulEndpoint;
	unsigned long 			ulEndpointStatus;

	DEBUG(" ep93xx_ep_fifo_status \n");
	
	ep = container_of(_ep, struct ep93xx_ep, ep);
	if (!_ep) {
		printk("%s, bad ep\n", __FUNCTION__);
		return -ENODEV;
	}


	ulEndpoint			=	((ep->bEndpointAddress&0x80)>>7)|((ep->bEndpointAddress&0x7F)<<1);
	DEBUG(" ep93xx_ep_fifo_status ep=%x\n",ulEndpoint);
	ulEndpointStatus	=	USBGetEndpointStatus(ulEndpoint);

	if (ep->dev->gadget.speed == USB_SPEED_UNKNOWN
			|| ((ulEndpointStatus&0x1) == 0))
		return 0;
	else
		return USBGetEndpointFifoLen(ulEndpoint);;
}


static void ep93xx_ep_fifo_flush(struct usb_ep *_ep)
{
	struct ep93xx_ep        *ep;
	unsigned long 			ulEndpoint;

	DEBUG(" ep93xx_ep_fifo_flush \n");
	
	ep = container_of(_ep, struct ep93xx_ep, ep);
	if (!_ep) {
		printk("%s, bad ep\n", __FUNCTION__);
		
	}
	
    	ulEndpoint  = ((ep->bEndpointAddress&0x80)>>7)|((ep->bEndpointAddress&0x7F)<<1);
	DEBUG(" ep93xx_ep_fifo_flush ep=%x\n",ulEndpoint);

	USBClearEndpointBuffer(ulEndpoint);
}



/*
 * endpoint enable/disable
 *
 * we need to verify the descriptors used to enable endpoints.  since pxa2xx
 * endpoint configurations are fixed, and are pretty much always enabled,
 * there's not a lot to manage here.
 *
 * because pxa2xx can't selectively initialize bulk (or interrupt) endpoints,
 * (resetting endpoint halt and toggle), SET_INTERFACE is unusable except
 * for a single interface (with only the default altsetting) and for gadget
 * drivers that don't halt endpoints (not reset by set_interface).  that also
 * means that if you use ISO, you must violate the USB spec rule that all
 * iso endpoints must be in non-default altsettings.
 */
static int ep93xx_ep_enable (struct usb_ep *_ep,
		const struct usb_endpoint_descriptor *desc)
{
	struct ep93xx_ep        *ep;
	struct ep93xx_udc       *dev;
	unsigned long ulEndpoint;

	ep = container_of (_ep, struct ep93xx_ep, ep);
	if (!_ep || !desc || ep->desc 
			|| ep->bEndpointAddress != desc->bEndpointAddress
			|| ep->fifo_size < le16_to_cpu
						(desc->wMaxPacketSize)) {


		printk("%s, bad ep or descriptor\n", __FUNCTION__);
		return -EINVAL;
	}

	/* xfer types must match, except that interrupt ~= bulk */
	if (ep->bmAttributes != desc->bmAttributes
			&& ep->bmAttributes != USB_ENDPOINT_XFER_BULK
			&& desc->bmAttributes != USB_ENDPOINT_XFER_INT) {
		printk("%s, %s type mismatch\n", __FUNCTION__, _ep->name);
		return -EINVAL;
	}

	/* hardware _could_ do smaller, but driver doesn't */
	if ((desc->bmAttributes == USB_ENDPOINT_XFER_BULK
				&& le16_to_cpu (desc->wMaxPacketSize)
						!= /*BULK_FIFO_SIZE*/sUSB.ulBulkMAXLen)
			|| !desc->wMaxPacketSize) {
		printk("%s, bad %s maxpacket\n", __FUNCTION__, _ep->name);
		return -ERANGE;
	}

	dev = ep->dev;
	if (!dev->driver || dev->gadget.speed == USB_SPEED_UNKNOWN) {
		printk("%s, bogus device state\n", __FUNCTION__);
		return -ESHUTDOWN;
	}

	ep->desc = desc;
	ep->dma = -1;
	ep->stopped = 0;
	ep->pio_irqs = ep->dma_irqs = 0;
	ep->ep.maxpacket = le16_to_cpu (desc->wMaxPacketSize);

	ep93xx_ep_fifo_flush (_ep);

    	ulEndpoint   =   ((ep->bEndpointAddress&0x80)>>7)|((ep->bEndpointAddress&0x7F)<<1);
	DEBUG(" ep93xx_ep_enable ep=%x\n",ulEndpoint);
	
	/* ... reset halt state too, if we could ... */
	USBEnableEndpoint(ep->bEndpointAddress, 1);

	

	DEBUG( "enabled %s\n", _ep->name);
	return 0;
}

static int ep93xx_ep_disable (struct usb_ep *_ep)
{
	struct ep93xx_ep	*ep;

	ep = container_of (_ep, struct ep93xx_ep, ep);
	if (!_ep || !ep->desc) {
		DEBUG("%s, %s not enabled\n", __FUNCTION__,
			_ep ? ep->ep.name : NULL);
		return -EINVAL;
	}
	nuke (ep, -ESHUTDOWN);

	/* flush fifo (mostly for IN buffers) */
	ep93xx_ep_fifo_flush (_ep);

	ep->desc = 0;
	ep->stopped = 1;

	DEBUG("%s disabled\n", _ep->name);
	return 0;
}

/* dequeue JUST ONE request */
static int ep93xx_ep_dequeue(struct usb_ep *_ep, struct usb_request *_req)
{
	struct ep93xx_ep	*ep;
	struct ep93xx_request	*req;
	unsigned long		flags;
	
	DEBUG(" ep93xx_ep_dequeue \n");

	ep = container_of(_ep, struct ep93xx_ep, ep);
	if (!_ep || ep->ep.name == ep0name)
		return -EINVAL;

	local_irq_save(flags);

	/* make sure it's actually queued on this endpoint */
	list_for_each_entry (req, &ep->queue, queue) {
		if (&req->req == _req)
			break;
	}
	if (&req->req != _req) {
		local_irq_restore(flags);
		return -EINVAL;
	}

	done(ep, req, -ECONNRESET);

	local_irq_restore(flags);
	return 0;
}

static int
ep93xx_ep_queue(struct usb_ep *_ep, struct usb_request *_req, gfp_t gfp_flags)
{
	struct ep93xx_request	*req;
	struct ep93xx_ep	*ep;
	struct ep93xx_udc	*dev;
	unsigned long		flags;
	unsigned long 		count=0;

	
	if(the_controller->removing_gadget == 1) /*BoLiu 07/19/07*/
                return -ESHUTDOWN;
	
	req = container_of(_req, struct ep93xx_request, req);
	if (unlikely (!_req || !_req->complete || !_req->buf
			|| !list_empty(&req->queue))) {
		printk("%s, bad params\n", __FUNCTION__);
		return -EINVAL;
	}

	ep = container_of(_ep, struct ep93xx_ep, ep);
	if (unlikely (!_ep || (!ep->desc && ep->ep.name != ep0name))) {
		printk("%s, bad ep\n", __FUNCTION__);
		return -EINVAL;
	}

	dev = ep->dev;
	if (unlikely (!dev->driver
			|| dev->gadget.speed == USB_SPEED_UNKNOWN)) {
		printk("%s, bogus device state\n", __FUNCTION__);
		return -ESHUTDOWN;
	}

	if(dev->removing_gadget == 1)
                return -ESHUTDOWN;


	dev->ep0req		=	_req;
	dev->epBulkReq	=	req;

	/* iso is always one packet per request, that's the only way
	 * we can report per-packet status.  that also helps with dma.
	 */
	if (unlikely (ep->bmAttributes == USB_ENDPOINT_XFER_ISOC
			&& req->req.length > le16_to_cpu
						(ep->desc->wMaxPacketSize)))
		return -EMSGSIZE;


	DEBUG("%s queue req %p, len %d buf %p,ep_Attr=%x,ep->address=%x\n",
	     _ep->name, _req, _req->length, _req->buf,ep->bmAttributes,ep->bEndpointAddress);

	local_irq_save(flags);

	_req->status = -EINPROGRESS;
	_req->actual = 0;
	
        
	/* kickstart this i/o queue? */
	if((ep->bmAttributes == USB_ENDPOINT_XFER_BULK)&&
			((ep->bEndpointAddress & USB_DIR_IN) != 0)){
	    local_irq_restore(flags);
		if(write_fifo(ep,req)==1)
			req=0;
		count=_req->actual;
		local_irq_save(flags);
	}
	else if((ep->bmAttributes == USB_ENDPOINT_XFER_BULK)&&
			((ep->bEndpointAddress & USB_DIR_IN) == 0)){
		local_irq_restore(flags);
		if(read_fifo(ep,req)==1)
			req=0;
		count=_req->actual;
		local_irq_save(flags);
	}
	else{
		/*local_irq_save(flags);*/
		count = UDC_SendControl(_req);
		/*local_irq_restore(flags);*/

	}

	/* pio or dma irq handler advances the queue. */

	local_irq_restore(flags);

	return 0;
}
/*------------------------------------------------------------------------------------*/
static struct usb_ep_ops ep93xx_ep_ops = {
	.enable			= ep93xx_ep_enable,
	.disable		= ep93xx_ep_disable,

	.alloc_request	= ep93xx_ep_alloc_request,
	.free_request	= ep93xx_ep_free_request,

	.alloc_buffer	= ep93xx_ep_alloc_buffer,
	.free_buffer	= ep93xx_ep_free_buffer,

	.queue		    = ep93xx_ep_queue,
	.dequeue	    = ep93xx_ep_dequeue,

	.set_halt		= ep93xx_ep_set_halt,
	.fifo_status	= ep93xx_ep_fifo_status,
	.fifo_flush		= ep93xx_ep_fifo_flush,

};

static inline void ep0_idle (struct ep93xx_udc *dev)
{
	dev->ep0state = EP0_IDLE;

}


/*----------------------------------------------------------------------------*/

/* 
 * 	device-scoped parts of the api to the usb controller hardware
 * 
 */

static int ep93xx_udc_get_frame(struct usb_gadget *_gadget)
{
	DEBUG(" ep93xx_udc_get_frame number\n");
	
	return USBGet_frame_number();
}

static int ep93xx_udc_wakeup(struct usb_gadget *_gadget)
{
	/* host may not have enabled remote wakeup */
	DEBUG(" ep93xx_udc_wakeup\n");
	
	USBWakeUpCs();

	return 0;
}

static int ep93xx_pullup (struct usb_gadget * sDevice, int is_on)
{

	DEBUG(" ep93xx_pullup\n");
	
	if(is_on==1)
		USBSoftConnect();
	else if(is_on==0)
		USBSoftDisConnect();
	else
		return -EOPNOTSUPP;

	return 0;


}

static int ep93xx_set_selfpowered(struct usb_gadget *_gadget, int value)
{

	return 0;
}

static const struct usb_gadget_ops ep93xx_udc_ops = {
	.get_frame	 	= 	ep93xx_udc_get_frame,
	.wakeup		 	= 	ep93xx_udc_wakeup,
	/* current versions must always be self-powered*/
	.set_selfpowered=	ep93xx_set_selfpowered,
	.pullup		 =	ep93xx_pullup,
	
};


/*-------------------------------------------------------------------------*/

/*
 * 	udc_disable - disable USB device controller
 */
static void udc_disable(struct ep93xx_udc *dev)
{


	USBDisable();
	ep0_idle (dev);
	
	dev->gadget.speed 	= USB_SPEED_UNKNOWN;
	dev->ep0req->length	=0;
	dev->ep0req->buf	=NULL;
	dev->ep0req->context=NULL;
	dev->ep0req->zero	=1;

}


/*
 * 	udc_reinit - initialize software state
 */
static void udc_reinit(struct ep93xx_udc *dev)
{
	u32	i;

	/* device/ep0 records init */
	INIT_LIST_HEAD (&dev->gadget.ep_list);
	INIT_LIST_HEAD (&dev->gadget.ep0->ep_list);
	dev->ep0state = EP0_IDLE;

	/* basic endpoint records init */
	for (i = 0; i < EP93_UDC_NUM_ENDPOINTS; i++) {
		struct ep93xx_ep *ep = &dev->ep[i];

		if (i != 0)
			list_add_tail (&ep->ep.ep_list, &dev->gadget.ep_list);

		ep->desc = 0;
		ep->stopped = 0;
		INIT_LIST_HEAD (&ep->queue);
		ep->pio_irqs = ep->dma_irqs = 0;
	}

	/* the rest was statically initialized, and is read-only */
}


/*
 * until it's enabled, this UDC should be completely invisible
 * to any USB host.
 */
static void udc_enable (struct ep93xx_udc *dev)
{
	USBChipEnable();
	ep0_idle(dev);
	dev->gadget.speed = USB_SPEED_UNKNOWN;
	/*USB_SPEED_FULL*/
	dev->stats.irqs = 0;

	dev->ep0req->length	=0;
	dev->ep0req->buf	=NULL;
	dev->ep0req->context=NULL;
	dev->ep0req->zero	=1;

}



/*-------------------------------------------------------------------------*/

#ifdef CONFIG_USB_GADGET_DEBUG_FILES

static const char proc_node_name [] = "driver/udc";

static int
udc_proc_read(char *page, char **start, off_t off, int count,
		int *eof, void *_dev)
{
	/*char    *buf = page;*/
	unsigned    size = count;
	unsigned long	flags;


	if (off != 0)
		return 0;

	local_irq_save(flags);

	local_irq_restore(flags);
	*eof = 1;
	return count - size;
}

#define create_proc_files() \
	create_proc_read_entry(proc_node_name, 0, NULL, udc_proc_read, dev)
#define remove_proc_files() \
	remove_proc_entry(proc_node_name, NULL)

#else	/* !CONFIG_USB_GADGET_DEBUG_FILES */
#define create_proc_files() do {} while (0)
#define remove_proc_files() do {} while (0)

#endif	/* CONFIG_USB_GADGET_DEBUG_FILES */

/* "function" sysfs attribute */
static ssize_t
show_function (struct device *_dev, struct device_attribute *attr, char *buf)
{
	struct ep93xx_udc	*dev = dev_get_drvdata (_dev);

	if (!dev->driver
			|| !dev->driver->function
			|| strlen (dev->driver->function) > PAGE_SIZE)
		return 0;
	return scnprintf (buf, PAGE_SIZE, "%s\n", dev->driver->function);
}
static DEVICE_ATTR (function, S_IRUGO, show_function, NULL);

/*-------------------------------------------------------------------------*/


/*
 * when a driver is successfully registered, it will receive
 * control requests including set_configuration(), which enables
 * non-control requests.  then usb traffic follows until a
 * disconnect is reported.  then a host may connect again, or
 * the driver might get unbound.
 */
int usb_gadget_register_driver(struct usb_gadget_driver *driver)
{
	struct ep93xx_udc	*dev = the_controller;
	int			retval;

	if (!driver
			|| ((driver->speed != USB_SPEED_FULL)&&(driver->speed != USB_SPEED_HIGH))
			|| !driver->bind
			|| !driver->unbind
			|| !driver->disconnect
			|| !driver->setup)
		return -EINVAL;

	if (!dev)
		return -ENODEV;
	if (dev->driver)
		return -EBUSY;

	/* first hook up the driver ... */
	dev->driver = driver;
	dev->removing_gadget = 0;
	dev->gadget.dev.driver = &driver->driver;

	device_add (&dev->gadget.dev);
	retval = driver->bind(&dev->gadget);
	if (retval) {
		printk("bind to driver %s --> error %d\n",
				driver->driver.name, retval);
		device_del (&dev->gadget.dev);

		dev->driver = 0;
		dev->gadget.dev.driver = 0;
		return retval;
	}

	device_create_file(dev->dev, &dev_attr_function);

	/* ... then enable host detection and ep0; and we're ready
	 * for set_configuration as well as eventual disconnect.
	 * NOTE:  this shouldn't power up until later.
	 */
	printk("registered gadget driver '%s'\n", driver->driver.name);
	udc_enable(dev);
	
	return 0;
}
EXPORT_SYMBOL(usb_gadget_register_driver);

static void
stop_activity(struct ep93xx_udc *dev, struct usb_gadget_driver *driver)
{
	int i;

	/* don't disconnect drivers more than once */
	if (dev->gadget.speed == USB_SPEED_UNKNOWN)
		driver = 0;
	dev->gadget.speed = USB_SPEED_UNKNOWN;

	/* prevent new request submissions, kill any outstanding requests  */
	for (i = 0; i < EP93_UDC_NUM_ENDPOINTS; i++) {
		struct ep93xx_ep *ep = &dev->ep[i];

		ep->stopped = 1;
		/*nuke(ep, -ESHUTDOWN);*/
	}

	/* report disconnect; the driver is already quiesced */
	
	if (driver)
		driver->disconnect(&dev->gadget);

	/* re-init driver-visible data structures */
	udc_reinit(dev);
}

int usb_gadget_unregister_driver(struct usb_gadget_driver *driver)
{
	struct ep93xx_udc	*dev = the_controller;

	if (!dev)
		return -ENODEV;
	if (!driver || driver != dev->driver)
		return -EINVAL;
	dev->removing_gadget = 1;

	local_irq_disable();
	dev->ep0req->complete(&dev->ep[4].ep,dev->ep0req); /*BoLiu 07/19/07*/
	udc_disable(dev);
	stop_activity(dev, driver);
	local_irq_enable();
	
    if(dev->ep[4].usSemStatus==1){
        dev->ep[4].usEPStop=1;
        up(&(dev->ep[4].wait_dio_cmd));
    }
    if(dev->ep[3].usSemStatus==1){
        dev->ep[3].usEPStop=1;
        up(&(dev->ep[3].wait_dio_cmd));
    }

	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(250));        
	driver->unbind(&dev->gadget);
	del_timer_sync(&dev->timer);

	dev->driver = 0;
	device_del (&dev->gadget.dev);
	device_remove_file(dev->dev, &dev_attr_function);

	printk("unregistered gadget driver '%s'\n", driver->driver.name);

	return 0;
}
EXPORT_SYMBOL(usb_gadget_unregister_driver);


/*-------------------------------------------------------------------------*/

/*
 *	ep93xx_udc_irq - interrupt handler
 *
 */
static irqreturn_t
ep93xx_udc_irq(int irq, void *_dev, struct pt_regs *r)
{
	struct ep93xx_udc	*dev = _dev;
	int			handled;
	
	unsigned long ulIntStatus;
    unsigned long usLength;
    unsigned char *pucChar;

	unsigned char 	*ucWriteBuf;
	unsigned long	ulWriteLength, ulWriteCount;

	int rc=0;

	struct usb_ctrlrequest sControlOut;

	dev->stats.irqs++;
	handled = 0;
	
 	/*
     	* Read the ISP1581 interrupt register.
     	*/
    	ulIntStatus = pulUSB[HwUSBIntReason >> 2];

    	/*
     	* Clear the interrupt reason register.
     	*/
    	pulUSB[HwUSBIntReason >> 2] =ulIntStatus;

	

	/* SUSpend Interrupt Request */
		
	/* RESume Interrupt Request */
		
	/*
     * Handle a hs status change.
     */
	if(ulIntStatus & USB_INT_HS_STATUS)
	{
    	/*
      	 * Reconfigure the ISP1581.
      	 */
        	printk("The USB bus is high speed\n");

    		sUSB.ucUSBSpeed = HighSpeed;
    		sUSB.ulBulkMAXLen = 512;


		dev->gadget.speed = USB_SPEED_HIGH;
		memset(&dev->stats, 0, sizeof dev->stats);
    		USBChipEnable();
		handled = 1;
    	/*
      	 * We're done handling the interrupts.
      	 */
	}


	/* ReSeT Interrupt Request - USB reset */

	/*
  	 * Handle a bus reset.
  	 */
	if(ulIntStatus & USB_INT_BUS_RESET)
	{
		sUSB.ulBulkMAXLen =64;
                sUSB.ucUSBSpeed = FullSpeed;

	    	if(dev->ep[4].usSemStatus==1){
            		dev->ep[4].usEPStop=1;
			dev->ep[4].usSemStatus=0;
            		up(&(dev->ep[4].wait_dio_cmd));
	            	/*printk("read fifo unbind\n");*/
        	}
        	
		if(dev->ep[3].usSemStatus==1){
            		dev->ep[3].usEPStop=1;
			dev->ep[3].usSemStatus=0;
            		up(&(dev->ep[3].wait_dio_cmd));
            		/*printk("read fifo unbind\n");*/
        	}

		printk("\nThe USB dev is connected to USB host \n");
    	/*
      	 * Reconfigure the ISP1581.
      	 */
    		USBChipEnable();

		dev->gadget.speed = USB_SPEED_FULL;
		memset(&dev->stats, 0, sizeof dev->stats);
		handled = 1;
		/*
      	 *We're done handling the interrupts.
      	 */

	}


    /*
     *Handle an RX interrupt on the bulk out endpoint.
     */
    if(ulIntStatus & USB_INT_EP1_RX)
    {
        /*
         *Read the data packet.
         */
        DEBUG("INT bulk Rx \n");
		up(&(dev->ep[4].wait_dio_cmd));
    }


 	/*
     * Handle an interrupt on the control in endpoint.
     */
    if(ulIntStatus & USB_INT_EP0_TX)
    {

		DEBUG("INT EP0 Tx \n");
	

		if(dev->ep0req==NULL){
			printk("EP0 Tx parmer error\n");
			return 0;
		}
		
		ucWriteBuf = dev->ep0req->buf + dev->ep0req->actual;

		ulWriteLength = dev->ep0req->length - dev->ep0req->actual;

		if(ulWriteLength!=0) {
            /*
             * Send the next packet of data to the host.
             */
   		    ulWriteCount	= UDC_USBWriteEndpoint(USB_ENDPOINT_CONTROL_IN, ucWriteBuf,
                     			ulWriteLength,EP0_FIFO_SIZE);
			dev->ep0req->actual += ulWriteCount;
        } 
		else {
            if(dev->ep0state==EP0_IN_STATUS_PHASE) {
            
				dev->ep0state=EP0_IDLE;
				
            } else if(dev->ep0state==EP0_IN_DATA_PHASE) {
                
				dev->ep0state=EP0_OUT_STATUS_PHASE;

                pulUSB[HwUSBEndpointIndex >> 2] = USB_ENDPOINT_CONTROL_IN;

                pulUSB[HwUSBEndpointControl >> 2] |= USB_EPCONTROL_STATUS_ACK;

            } else {
                
				dev->ep0state=EP0_IDLE;

            }

        }
    }



 	/*
     *Handle an interrupt from a setup packet.
     */
    if(ulIntStatus & USB_INT_EP0_SETUP)
    {
		dev->ep[0].pio_irqs++;
		DEBUG("INT SEtup\n");
		/*
         * Read the packet.
         */
        pucChar = (unsigned char *)&sControlOut;
        usLength = sizeof(struct usb_ctrlrequest);
        if(USBReadEndpoint(USB_ENDPOINT_SETUP, &pucChar,
                           &usLength) != sizeof(struct usb_ctrlrequest)) {
            /*
             * The size of the setup packet is incorrect, so stall both of the
             * control endpoints.
             */
            USBStallEndpoint(USB_ENDPOINT_CONTROL_OUT, 1);
            USBStallEndpoint(USB_ENDPOINT_CONTROL_IN, 1);
            printk("EP0--setup   stall\n");
        } else {
        	dev->sControlOut=&sControlOut;
           
            /* Process the command in the setup packet.*/
          
   
            
			if (dev->sControlOut->bRequestType & USB_DIR_IN)
				dev->ep0state = EP0_IN_DATA_PHASE;
			else
				dev->ep0state = EP0_IN_STATUS_PHASE;
			
            if(dev->sControlOut->bRequest==USB_REQ_SET_ADDRESS)
				USBSetAddress(dev);
            else if(dev->sControlOut->bRequest==USB_REQ_GET_STATUS)
				USBGetStatus(dev);
		    else if(dev->sControlOut->bRequest==USB_REQ_CLEAR_FEATURE)
				USBClearFeature(dev);
		    else if(dev->sControlOut->bRequest==USB_REQ_SET_FEATURE)
				USBSetFeature(dev);
   			else if(dev->sControlOut->bRequest==USB_REQ_SET_DESCRIPTOR)
				USBReserved(dev);
			else{
		 		rc=dev->driver->setup(&dev->gadget,dev->sControlOut);
				if(rc==(256+999)){
					if(dev->sControlOut->bRequest==USB_REQ_SET_CONFIGURATION){
						/*
						   give the fsg main thread timne to process Handle_porcess()
						   do_set_config and do_set_interface
						 */
						disable_irq(IRQ_USB_SLAVE);
						dev->req_config=1;
						USBSetConfiguration(dev);
						/*enable_irq(IRQ_USB_SLAVE);*/
					}
            		else if(dev->sControlOut->bRequest==USB_REQ_SET_INTERFACE)
						USBSetInterface(dev);
				}
			}
			DEBUG("EP0--setup   finished\n");
        }


    }

    /*
     * Handle an TX interrupt on the bulk in endpoint.
     */
    if(ulIntStatus & USB_INT_EP2_TX)
    {
        if(dev->epBulkReq->req.actual!=dev->epBulkReq->req.length){
			DEBUG("int bulk Tx\n");
			up(&(dev->ep[3].wait_dio_cmd));
        }
	/*	else	DEBUG("INT bulk Tx no work \n");*/

    }


		

	/* we could also ask for 1 msec SOF (SIR) interrupts */

	
	
	DEBUG(" USB Slave int\n");
	return IRQ_HANDLED;
}


static void nop_release (struct device *dev)
{
	printk("%s %s\n", __FUNCTION__, dev->bus_id);
}

/*
 * this uses load-time allocation and initialization (instead of
 * doing it at run-time) to save code, eliminate fault paths, and
 * be more obviously correct.
 */
static struct ep93xx_udc memory = {
	.gadget = {
		.ops		= &ep93xx_udc_ops,
		.ep0		= &memory.ep[0].ep,
		.name		= driver_name,
		.dev = {
			.bus_id		= "gadget",
			.release	= nop_release,
		},
	},

	/* control setup endpoint */
	.ep[0] = {
		.ep = {
			.name			= ep0name,
			.ops			= &ep93xx_ep_ops,
			.maxpacket		= EP0_FIFO_SIZE,
		},
		.dev				= &memory,
		.bEndpointAddress		=USB_ENDPOINT_SETUP,
	},
	
	/* control in endpoint */
	.ep[1] = {
		.ep = {
			.name		= "ep0in-control",
			.ops		= &ep93xx_ep_ops,
			.maxpacket	= EP0_FIFO_SIZE,
		},
		.dev		= &memory,
		.bEndpointAddress = USB_ENDPOINT_CONTROL_IN,
		

	},

	/* control out endpoint */
	
	.ep[2] = {
		.ep = {
			.name		= "ep0out-control",
			.ops		= &ep93xx_ep_ops,
			.maxpacket	= EP0_FIFO_SIZE,
		},
		.dev		= &memory,
		.bEndpointAddress = USB_ENDPOINT_CONTROL_OUT,
		

	},
	/* bulk group of endpoints */
	.ep[3] = {
		.ep = {
			.name		= "ep2in-bulk",
			.ops		= &ep93xx_ep_ops,
			.maxpacket	= BULK_FIFO_SIZE,
		},
		.dev		= &memory,
		.fifo_size	= BULK_FIFO_SIZE,
		.bEndpointAddress = 0x80|2,/*USB_ENDPOINT_TWO_IN,*/
		.bmAttributes	= USB_ENDPOINT_XFER_BULK,

	},

	.ep[4] = {
		.ep = {
			.name		= "ep1out-bulk",
			.ops		= &ep93xx_ep_ops,
			.maxpacket	= BULK_FIFO_SIZE,
		},
		.dev		= &memory,
		.fifo_size	= BULK_FIFO_SIZE,
		.bEndpointAddress = 0x00|1,/*USB_ENDPOINT_ONE_OUT,*/
		.bmAttributes	= USB_ENDPOINT_XFER_BULK,

	},
	
	.ep[5] = {
		.ep = {
			.name		= "ep3in-int",
			.ops		= &ep93xx_ep_ops,
			.maxpacket	= INT_FIFO_SIZE,
		},
		.dev		= &memory,
		.fifo_size	= INT_FIFO_SIZE,
		.bEndpointAddress =  0x80|3,/*USB_ENDPOINT_THREE_IN,*/
		.bmAttributes	= USB_ENDPOINT_XFER_INT,

	},	


};





/*-------------------------------------------------------------------------*/

unsigned long  *pulUSBStartAdress_32,ulUSBPhyBaseAddress,ulSMCBase;
#ifdef gadget_debug
    	static short major			=3;
    	static short minor			=0;
#endif

	static short cs   			=7;

#if (defined(CONFIG_MACH_EDB9315A) || defined(CONFIG_MACH_EDB9307A) || defined(CONFIG_MACH_EDB9302A))
       	static short irq                        =0;
#else
	static short irq			=1;
#endif


/*
 *SelectCS select cs of the system  EDP931x
 *0 sucess
 *1 faile
 *   ulUSBPhyBaseAddress : the Physical Base Address of the USB Slave board
 *   ulSMCBase       : the SMC      Base Address of the USB Slave board
 */

unsigned long
SelectCS(short sCS)
{

    if(sCS==0) {
        ulUSBPhyBaseAddress = 0x00000000;
        ulSMCBase = 0x00;
    } else if(sCS==1) {
        ulUSBPhyBaseAddress = 0x10000000;
        ulSMCBase = 0x04;
    } else if(sCS==2) {
        ulUSBPhyBaseAddress = 0x20000000;
        ulSMCBase = 0x08;
    } else if(sCS==3) {
        ulUSBPhyBaseAddress = 0x30000000;
        ulSMCBase = 0x0C;
    } else if(sCS==6) {
        ulUSBPhyBaseAddress = 0x60000000;
        ulSMCBase = 0x18;
    } else if(sCS==7) {
        ulUSBPhyBaseAddress = 0x70000000;
        ulSMCBase = 0x1C;
    } else {
        ulUSBPhyBaseAddress = 0;
        ulSMCBase = 0;
        return 1;
    }

    return(0);
}

/*
 *initDmaDevice init the EP931x system register
 */
void initDmaDevice(void)
{
    unsigned long uiPWRCNT,uiDEVCFG;

    uiPWRCNT = inl(EP93XX_SYSCON_CLOCK_CONTROL/*SYSCON_PWRCNT*/);
    uiPWRCNT |= SYSCON_PWRCNT_DMA_M2MCH1;
    outl( uiPWRCNT, EP93XX_SYSCON_CLOCK_CONTROL/*SYSCON_PWRCNT*/ );

    uiPWRCNT = inl(EP93XX_SYSCON_CLOCK_CONTROL/*SYSCON_PWRCNT*/);
    
    uiDEVCFG = inl(/*SYSCON_DEVCFG*/EP93XX_SYSCON_DEVICE_CONFIG);
    uiDEVCFG |=SYSCON_DEVCFG_D1onG;
    SysconSetLocked( /*SYSCON_DEVCFG*/EP93XX_SYSCON_DEVICE_CONFIG, uiDEVCFG );

    uiDEVCFG = inl(/*SYSCON_DEVCFG*/EP93XX_SYSCON_DEVICE_CONFIG);
   
}

/*
 * GetUSBVirAdress  get the USB Slave Board vir adress.
 */
unsigned long
GetUSBVirAdress(void)
{
    unsigned long  map_usb_Phy_To_Vir,USB_PHY_ADDR;

    USB_PHY_ADDR = ulUSBPhyBaseAddress;
    map_usb_Phy_To_Vir = (unsigned long)ioremap(USB_PHY_ADDR, USB_WINDOW_SIZE);

    if (!map_usb_Phy_To_Vir) {
        printk("failed to USV Slave driver ioremap\n");
        return -EIO;
    }
    return(map_usb_Phy_To_Vir);
}

/*
 *SelectCS select irq of the system  EDP931x
 *return the system irq number
 *return o show fail
 */

short
SelectIRQ(short usIrq)
{
    short  ulUSBSlaveISRNumber;

    if(usIrq==0)
        ulUSBSlaveISRNumber = (short)IRQ_EP93XX_EXT0;
    else if(usIrq==1)
        ulUSBSlaveISRNumber = (short)IRQ_EP93XX_EXT1;
    else if(usIrq==2)
        ulUSBSlaveISRNumber = (short)IRQ_EP93XX_EXT2;
    else
        ulUSBSlaveISRNumber = 0;

    return(ulUSBSlaveISRNumber);
}

/*
 * TestUSBChipID  test the USB Slave Board chip ID
 *  return 1 is right ,0 is error
 */

static int
TestUSBChipID(unsigned long ul_map_usb_Phy_To_Vir)
{

    unsigned long ulChipID1,ulUSBSlaveChipIDVersion;

    pulUSBStartAdress_32 =( unsigned long *)ul_map_usb_Phy_To_Vir;
    ulChipID1 = pulUSBStartAdress_32[HwUSBChipID>>2];
    printk(" USB Device Controller Chip ID :%x",(unsigned short)ulChipID1);
    if ( (ulChipID1 & USB_CHIPID_ID_MASK) == USB_CHIPID_ID) {
        ulUSBSlaveChipIDVersion = (ulChipID1&USB_CHIPID_VERSION_MASK);
        printk(" Version :%x\n",(unsigned short)ulUSBSlaveChipIDVersion);
        return (1);
    }

    printk(" test chipID is wrong\n Are you sure the USB2 Slave Daughter Board Link on EDB931X?\n");
    return (0);

}


/*
 * USBEnable configures the ISP1581 device and the EDB931x HW.
 * Return 1 sucess,0 error.
 */

unsigned char
USBEnable(void)
{
    volatile unsigned long *pulSMCValue;
    unsigned long pulUSBStartAdress;
    /*
     *  SMC7 Base address 0x8008_001c
     *  mmu map the adress  0x8000_0000 to 0xFF00_0000(Based on cirrus_linux1.41)
     *  USB Slave Board link to EDB931x SMC ,and use CS7
     */
    pulSMCValue =  (unsigned long *)(/*IO_BASE_VIRT*/EP93XX_AHB_VIRT_BASE+0x00080000+ulSMCBase);

    /*
     *Configure the EDB931X SMC
     *USB Slave Chip be linked on  the SMC(cs7) 0x8008001c
     *Configure the USB chip select for 16-bit access.
     */
    *pulSMCValue=0x1000ffe0;

    /*
     *Get the virual address of USB Slave Board
     */
    pulUSBStartAdress=GetUSBVirAdress();

    /*
     *Test the USB Slave Board Chip ID,So That just the Board is or not linked to EDB931x
     */
    if(TestUSBChipID(pulUSBStartAdress)==0) {
        return 0;
    }

    /*
     *Configure the USB Slave Status
     */
    USBSlavePreInit(pulUSBStartAdress);

    /*
     *Configure the USB DMA
     */
    USB_DMA_init();
    initDmaDevice();


    /*
     *Configure the USB Slave chip HW
     */
    USBChipEnable();



    return(1);

}

/*
 * Init The EDB931x USB2.0  Slave Daughter Board Module
 * Return 0 sucess
 */

static int init_edb931xusb(void)
{
    /*short  sUSBSlaveISRNumber;*/
    DEBUG(KERN_ALERT " USB Slave board will use dev_major: %hd\n", major);
    DEBUG(KERN_ALERT " USB Slave board will use dev_minor: %hd\n", minor);
    DEBUG(KERN_ALERT " USB Slave board will use cs: %hd\n", cs);
    DEBUG(KERN_ALERT " USB Slave board will use extern INT: %hd\n", irq);

    if(SelectCS(cs) !=0) {
        printk("failed to select cs of USV Slave driver\n");
        return -EIO;
    }
    if(USBEnable() == 0) {
        return -EIO;
    }

    IRQ_USB_SLAVE = SelectIRQ(irq);
    if (IRQ_USB_SLAVE==0) {
        printk("failed to select irq of USV Slave driver:%d\n",IRQ_USB_SLAVE);
        return -EIO;
    }

	
    DEBUG(" The USB Slave Module init sucess\n");
    return 0;

}


/*
 * Clear The EDB931x USB2.0  Slave Daughter Board Module
 */
static void cleanup_edb931x_usb(void)
{
    unsigned long ulmap_usb_Phy_To_Vir;
    /*short  sUSBSlaveISRNumber;*/
    /*
     *clear the ioremap
     */
    ulmap_usb_Phy_To_Vir = (unsigned long)pulUSBStartAdress_32 ;
    iounmap((void *)ulmap_usb_Phy_To_Vir);

    DEBUG("The USB Slave Module uninstall sucess\n");
}

static void ep93xx_udc_watchdog(unsigned long data)
{

        struct ep93xx_udc       *dev = (struct ep93xx_udc*)data;
        struct ep93xx_ep        *ep;
        struct ep93xx_request   *req;
        int i;
	
	if(dev->removing_gadget == 1)
        {
        	for(i = 0; i < EP93_UDC_NUM_ENDPOINTS; i++)
	        {
        	        ep = &dev->ep[i];
		
                	list_for_each_entry (req, &ep->queue, queue) {
					printk("%s:remove queued req\n",__FUNCTION__);
                        	        req->req.complete(&ep->ep,&req->req);
	                }

        	}
        }
}


/*-----------------------------------------------------------------------------------*/

/*
 * 	probe - binds to the platform device
 */

static int __init ep93xx_udc_probe(struct platform_device *pdev)
{
	struct ep93xx_udc *dev = &memory;
	int retval;
	


	if(init_edb931xusb()!=0)
		return -EBUSY;

	/* other non-static parts of init*/ 
	dev->dev = &pdev->dev;
	dev->mach = pdev->dev.platform_data;
	
	
	/*init the udc EP0requestion*/
	dev->ep0req		=	&UDC_ep0req;
	
	/*timer*/
	init_timer(&dev->timer);
	dev->timer.function = (void*) ep93xx_udc_watchdog;
	dev->timer.data = (unsigned long) dev;


	device_initialize(&dev->gadget.dev);
	dev->gadget.dev.parent = &pdev->dev;
	dev->gadget.dev.dma_mask = pdev->dev.dma_mask;

	dev->gadget.is_dualspeed= 1;

	/*sem*/
	sema_init(&(dev->ep[3].wait_dio_cmd), 0);
	sema_init(&(dev->ep[4].wait_dio_cmd), 0);
	dev->ep[4].usSemStatus=0;
    	dev->ep[3].usSemStatus=0;
    	dev->ep[4].usEPStop=0;
    	dev->ep[3].usEPStop=0;


	the_controller = dev;
	platform_set_drvdata(pdev, dev);

	udc_disable(dev);
	udc_reinit(dev);

	retval = request_irq(IRQ_USB_SLAVE, ep93xx_udc_irq,
			SA_INTERRUPT, driver_name, dev);
	if (retval != 0) {
		printk(KERN_ERR "%s: can't get irq %i, err %d\n",
			driver_name, IRQ_USB_SLAVE, retval);
		return -EBUSY;
	}
	
	dev->got_irq = 1;
	dev->req_config=0;

	create_proc_files();

	return 0;
}


static void ep93xx_udc_shutdown(struct platform_device *dev)
{
	/* force disconnect on reboot */
	ep93xx_pullup(platform_get_drvdata(dev), 0);
}
static int __exit ep93xx_udc_remove(struct platform_device *pdev)
{
	struct ep93xx_udc *dev = platform_get_drvdata(pdev);

	
	cleanup_edb931x_usb();
	udc_disable(dev);
	remove_proc_files();
	usb_gadget_unregister_driver(dev->driver);

	if (dev->got_irq) {
		free_irq(IRQ_USB_SLAVE, dev);
		dev->got_irq = 0;
	}
		
	platform_set_drvdata(pdev, NULL);
	the_controller = NULL;
	return 0;
}

static struct platform_driver udc_driver = {
        /*.name           = "ep93xx-udc",*/
        /*.bus            = &platform_bus_type,*/
        .probe          = ep93xx_udc_probe,
        .remove         = __exit_p(ep93xx_udc_remove),
        .shutdown	=ep93xx_udc_shutdown,
 	.driver		= {
		.name	= "ep93xx-udc",
		.owner	= THIS_MODULE,
	},                                                                                                                            
        // FIXME power management support
        // .suspend = ... disable UDC
        // .resume = ... re-enable UDC
};


/*-----------------------------------------------------------------------------------*/
static int __init ep931x_usb2_init(void)
{
	printk(KERN_INFO "%s: version %s\n", driver_name, DRIVER_VERSION);
	return platform_driver_register(&udc_driver);

}
module_init(ep931x_usb2_init);

static void __exit ep931x_usb2_exit(void)
{
	platform_driver_unregister(&udc_driver);

}
module_exit(ep931x_usb2_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Shrek");
MODULE_LICENSE("GPL");

