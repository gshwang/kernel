/*
 * BRIEF MODULE DESCRIPTION
 *	Au1xxx processor specific IRQ tables
 *
 * Copyright 2004 Embedded Edge, LLC
 *	dan@embeddededge.com
 *
 *  This program is free software; you can redistribute	 it and/or modify it
 *  under  the terms of	 the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the	License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED	  ``AS	IS'' AND   ANY	EXPRESS OR IMPLIED
 *  WARRANTIES,	  INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO	EVENT  SHALL   THE AUTHOR  BE	 LIABLE FOR ANY	  DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED	  TO, PROCUREMENT OF  SUBSTITUTE GOODS	OR SERVICES; LOSS OF
 *  USE, DATA,	OR PROFITS; OR	BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN	 CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/bitops.h>

#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/system.h>
#include <asm/mach-au1x00/au1000.h>

/* The IC0 interrupt table.  This is processor, rather than
 * board dependent, so no reason to keep this info in the board
 * dependent files.
 *
 * Careful if you change match 2 request!
 * The interrupt handler is called directly from the low level dispatch code.
 */
au1xxx_irq_map_t __initdata au1xxx_ic0_map[] = {

#if defined(CONFIG_SOC_AU1000)
	{ AU1000_UART0_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_UART1_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_UART2_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_UART3_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_SSI0_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_SSI1_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+1, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+2, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+3, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+4, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+5, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+6, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+7, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_TOY_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_TOY_MATCH0_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_TOY_MATCH1_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_TOY_MATCH2_INT, INTC_INT_RISE_EDGE, 1 },
	{ AU1000_RTC_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_RTC_MATCH0_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_RTC_MATCH1_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_RTC_MATCH2_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_IRDA_TX_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_IRDA_RX_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_USB_DEV_REQ_INT, INTC_INT_HIGH_LEVEL, 0 },
	{ AU1000_USB_DEV_SUS_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_USB_HOST_INT, INTC_INT_LOW_LEVEL, 0 },
	{ AU1000_ACSYNC_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_MAC0_DMA_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_MAC1_DMA_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_AC97C_INT, INTC_INT_RISE_EDGE, 0 },

#elif defined(CONFIG_SOC_AU1500)

	{ AU1500_UART0_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_PCI_INTA, INTC_INT_LOW_LEVEL, 0 },
	{ AU1000_PCI_INTB, INTC_INT_LOW_LEVEL, 0 },
	{ AU1500_UART3_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_PCI_INTC, INTC_INT_LOW_LEVEL, 0 },
	{ AU1000_PCI_INTD, INTC_INT_LOW_LEVEL, 0 },
	{ AU1000_DMA_INT_BASE, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+1, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+2, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+3, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+4, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+5, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+6, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+7, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_TOY_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_TOY_MATCH0_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_TOY_MATCH1_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_TOY_MATCH2_INT, INTC_INT_RISE_EDGE, 1 },
	{ AU1000_RTC_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_RTC_MATCH0_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_RTC_MATCH1_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_RTC_MATCH2_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_USB_DEV_REQ_INT, INTC_INT_HIGH_LEVEL, 0 },
	{ AU1000_USB_DEV_SUS_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_USB_HOST_INT, INTC_INT_LOW_LEVEL, 0 },
	{ AU1000_ACSYNC_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1500_MAC0_DMA_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1500_MAC1_DMA_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_AC97C_INT, INTC_INT_RISE_EDGE, 0 },

#elif defined(CONFIG_SOC_AU1100)

	{ AU1100_UART0_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1100_UART1_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1100_SD_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1100_UART3_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_SSI0_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_SSI1_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+1, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+2, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+3, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+4, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+5, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+6, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_DMA_INT_BASE+7, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_TOY_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_TOY_MATCH0_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_TOY_MATCH1_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_TOY_MATCH2_INT, INTC_INT_RISE_EDGE, 1 },
	{ AU1000_RTC_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_RTC_MATCH0_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_RTC_MATCH1_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_RTC_MATCH2_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_IRDA_TX_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_IRDA_RX_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_USB_DEV_REQ_INT, INTC_INT_HIGH_LEVEL, 0 },
	{ AU1000_USB_DEV_SUS_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_USB_HOST_INT, INTC_INT_LOW_LEVEL, 0 },
	{ AU1000_ACSYNC_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1100_MAC0_DMA_INT, INTC_INT_HIGH_LEVEL, 0},
	/*{ AU1000_GPIO215_208_INT, INTC_INT_HIGH_LEVEL, 0},*/
	{ AU1100_LCD_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_AC97C_INT, INTC_INT_RISE_EDGE, 0 },

#elif defined(CONFIG_SOC_AU1550)

	{ AU1550_UART0_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_PCI_INTA, INTC_INT_LOW_LEVEL, 0 },
	{ AU1550_PCI_INTB, INTC_INT_LOW_LEVEL, 0 },
	{ AU1550_DDMA_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_CRYPTO_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_PCI_INTC, INTC_INT_LOW_LEVEL, 0 },
	{ AU1550_PCI_INTD, INTC_INT_LOW_LEVEL, 0 },
	{ AU1550_PCI_RST_INT, INTC_INT_LOW_LEVEL, 0 },
	{ AU1550_UART1_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_UART3_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_PSC0_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_PSC1_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_PSC2_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_PSC3_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_TOY_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_TOY_MATCH0_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_TOY_MATCH1_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_TOY_MATCH2_INT, INTC_INT_RISE_EDGE, 1 },
	{ AU1000_RTC_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_RTC_MATCH0_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_RTC_MATCH1_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_RTC_MATCH2_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1550_NAND_INT, INTC_INT_RISE_EDGE, 0},
	{ AU1550_USB_DEV_REQ_INT, INTC_INT_HIGH_LEVEL, 0 },
	{ AU1550_USB_DEV_SUS_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1550_USB_HOST_INT, INTC_INT_LOW_LEVEL, 0 },
	{ AU1550_MAC0_DMA_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1550_MAC1_DMA_INT, INTC_INT_HIGH_LEVEL, 0},

#elif defined(CONFIG_SOC_AU1200)

	{ AU1200_UART0_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1200_SWT_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1200_SD_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1200_DDMA_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1200_MAE_BE_INT, INTC_INT_HIGH_LEVEL, 0 },
	{ AU1200_UART1_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1200_MAE_FE_INT, INTC_INT_HIGH_LEVEL, 0 },
	{ AU1200_PSC0_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1200_PSC1_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1200_AES_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1200_CAMERA_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1000_TOY_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_TOY_MATCH0_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_TOY_MATCH1_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_TOY_MATCH2_INT, INTC_INT_RISE_EDGE, 1 },
	{ AU1000_RTC_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_RTC_MATCH0_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_RTC_MATCH1_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1000_RTC_MATCH2_INT, INTC_INT_RISE_EDGE, 0 },
	{ AU1200_NAND_INT, INTC_INT_RISE_EDGE, 0},
	{ AU1200_USB_INT, INTC_INT_HIGH_LEVEL, 0 },
	{ AU1200_LCD_INT, INTC_INT_HIGH_LEVEL, 0},
	{ AU1200_MAE_BOTH_INT, INTC_INT_HIGH_LEVEL, 0},
    { AU1000_GPIO_2,  INTC_INT_FALL_EDGE, 0 }, 			// [FALINUX] AX88796B  - INTC_INT_LOW_LEVEL, INTC_INT_RISE_EDGE
    { AU1000_GPIO_10,  INTC_INT_HIGH_LEVEL, 0 }, 		// [FALINUX] WM9712-Touch
    { AU1000_GPIO_12,  INTC_INT_HIGH_LEVEL, 0 }, 		// [FALINUX] MK712 - INTC_INT_LOW_LEVEL, INTC_INT_RISE_EDGE
#else
#error "Error: Unknown Alchemy SOC"
#endif

};

int __initdata au1xxx_ic0_nr_irqs = ARRAY_SIZE(au1xxx_ic0_map);

