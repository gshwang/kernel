 /*	Defines for using the Camera Interfaces on the
  *      Alchemy Au1100 mips processor.
  *
  *  This program is free software; you can redistribute  it and/or modify it
  *  under  the terms of  the GNU General  Public License as published by the
  *  Free Software Foundation;  either version 2 of the  License, or (at your
  *  option) any later version.
  *
  *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
  *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
  *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
  *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
  *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
  *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
  *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
  *   ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
  *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *  You should have received a copy of the  GNU General Public License along
  *  with this program; if not, write  to the Free Software Foundation, Inc.,
  *  675 Mass Ave, Cambridge, MA 02139, USA.
  *
  */
#ifndef AUXXX_CIM_H
#define AUXXX_CIM_H

#include <linux/ioctl.h>

typedef unsigned long uint32;

#define AU1XXXCIM_IOC_MAGIC 'C'

#define RAW_SXGA              _IOW(AU1XXXCIM_IOC_MAGIC,1, int *)
#define RAW_VGA               _IOW(AU1XXXCIM_IOC_MAGIC,2, int *)
#define RAW_CIF               _IOW(AU1XXXCIM_IOC_MAGIC,3, int *)
#define RAW_QVGA              _IOW(AU1XXXCIM_IOC_MAGIC,4, int *)
#define RAW_QCIF              _IOW(AU1XXXCIM_IOC_MAGIC,5, int *)
#define RAW_QQVGA             _IOW(AU1XXXCIM_IOC_MAGIC,6, int *)
#define RAW_QQCIF             _IOW(AU1XXXCIM_IOC_MAGIC,7, int *)
#define BAYER_SXGA            _IOW(AU1XXXCIM_IOC_MAGIC,8, int *)
#define BAYER_VGA             _IOW(AU1XXXCIM_IOC_MAGIC,9, int *)
#define BAYER_CIF             _IOW(AU1XXXCIM_IOC_MAGIC,10, int *)
#define BAYER_QVGA            _IOW(AU1XXXCIM_IOC_MAGIC,11, int *)
#define BAYER_QCIF            _IOW(AU1XXXCIM_IOC_MAGIC,12, int *)
#define BAYER_QQVG            _IOW(AU1XXXCIM_IOC_MAGIC,13, int *)
#define BAYER_QQCI            _IOW(AU1XXXCIM_IOC_MAGIC,14, int *)
#define YCbCr_SXGA_RAW        _IOW(AU1XXXCIM_IOC_MAGIC,18, int *)
#define YCbCr_VGA_RAW         _IOW(AU1XXXCIM_IOC_MAGIC,19, int *)
#define YCbCr_CIF_RAW         _IOW(AU1XXXCIM_IOC_MAGIC,20, int *)
#define YCbCr_QVGA_RAW        _IOW(AU1XXXCIM_IOC_MAGIC,21, int *)
#define YCbCr_QCIF_RAW        _IOW(AU1XXXCIM_IOC_MAGIC,22, int *)
#define YCbCr_SXGA            _IOW(AU1XXXCIM_IOC_MAGIC,23, int *)
#define YCbCr_VGA             _IOW(AU1XXXCIM_IOC_MAGIC,24, int *)
#define YCbCr_CIF             _IOW(AU1XXXCIM_IOC_MAGIC,25, int *)
#define YCbCr_QVGA            _IOW(AU1XXXCIM_IOC_MAGIC,26, int *)
#define YCbCr_QCIF            _IOW(AU1XXXCIM_IOC_MAGIC,27, int *)
#define AU1XXXCIM_CAPTURE     _IOW(AU1XXXCIM_IOC_MAGIC, 100, int *)
#define AU1XXXCIM_QUERY       _IOW(AU1XXXCIM_IOC_MAGIC, 50, int *)
#define AU1XXXCIM_CONFIGURE   _IOW(AU1XXXCIM_IOC_MAGIC, 75, int *)


#define  CMOS_RAW               0
#define  CMOS_CCIR656           1




#define CIM_BASE_ADDRESS        0xB4004000



#define CIM_FIFOA       	0xB4004020
#define CIM_FIFOB       	0xB4004040
#define CIM_FIFOC       	0xB4004060



typedef volatile struct
{
        unsigned long         enable;                /* 00 */
        unsigned long         config;                /* 04 */
        unsigned long         reserve0;              /* 08 */
	unsigned long         reserve1;              /* 0C */
	unsigned long         capture;               /* 10 */
	unsigned long         stat;                  /* 14 */
        unsigned long         inten;                 /* 18 */
	unsigned long         instat;                /* 1C */
	unsigned long         fifoa;                 /* 20 */
	unsigned long         reserve2;              /* 24 */
        unsigned long         reserve3;              /* 28 */
        unsigned long         reserve4;              /* 2C */
        unsigned long         reserve5;              /* 30 */
        unsigned long         reserve6;              /* 34 */
        unsigned long         reserve7;              /* 38 */
        unsigned long         reserve8;              /* 3C */
        unsigned long         fifob;                 /* 40 */
	unsigned long         reserve9;              /* 44 */
        unsigned long         reserve10;              /* 48 */
        unsigned long         reserve11;              /* 4C */
        unsigned long         reserve12;              /* 50 */
        unsigned long         reserve13;              /* 54 */
        unsigned long         reserve14;              /* 58 */
        unsigned long         reserve15;              /* 5C */
	unsigned long         fifoc;                  /* 60 */
}AU1200_CIM;


#define CIM_ENABLE              0x00000000
#define CIM_CONFIG       	0x00000004
#define CIM_CAPTURE	        0x00000010
#define CIM_STAT                0x00000014
#define CIM_INTEN       	0x00000018
#define CIM_INSTAT      	0x0000001C




#define   CIM_ENABLE_EN		        (1<<0)       /* enable/disable/reset the block*/

/* CIM Configuration Register */

#define   CIM_CONFIG_PM 		(1<<0)	 
#define   CIM_CONFIG_CLK    	        (1<<1)   /* Rising Edge of the Clock */ 	 
#define   CIM_CONFIG_LS			(1<<2)	 /* Line Sync Active Low */
#define   CIM_CONFIG_FS			(1<<3)	 /* Frame Sync is Active Low */

#define   CIM_CONFIG_RAW                 0  /* RAW MODE */
#define   CIM_CONFIG_BAYER               1  /*Bayer Mode*/
#define   CIM_CONFIG_656                 2  /* 656 YCbCr Mode*/

#define   CIM_CONFIG_DPS_N(n)           (((n) & 0x03)<<6)


#define   CIM_CONFIG_RGGB                 0
#define   CIM_CONFIG_GRBG                 1
#define   CIM_CONFIG_BGGR                 2
#define   CIM_CONFIG_GBRG                 3

#define   CIM_CONFIG_BAY_N(n)           (((n) & 0x03)<<8)

#define   CIM_CONFIG_LEN_8BIT              0
#define   CIM_CONFIG_LEN_9BIT              1
#define   CIM_CONFIG_LEN_10BIT             2 


#define   CIM_CONFIG_LEN(n)		(((n) & 0x0f)<<10)
#define   CIM_CONFIG_BYT		(1<<14)
#define   CIM_CONFIG_SF			(1<<15)

#define   CIM_CONFIG_FIELD1              0  /* Capture from Field 1*/
#define   CIM_CONFIG_FIELD2              1  /*Capture from Field 2*/
#define   CIM_CONFIG_FIELD12             2  /*Capture from Either Field*/

#define   CIM_CONFIG_FSEL_N(n)	        (((n) & 0x03)<<16)

#define   CIM_CONFIG_SI			(1<<18)

/* CIM Capture Control Register */

#define CIM_CAPTURE_VCE			(1<<0)
#define CIM_CAPTURE_SCE			(1<<1)
#define CIM_CAPTURE_CLR			(1<<2)

/* CIM Status Register */

#define CIM_STATUS_VC			(1<<0)
#define CIM_STATUS_SC			(1<<1)
#define CIM_STATUS_AF			(1<<2)
#define CIM_STATUS_AE			(1<<3)
#define CIM_STATUS_AR			(1<<4)
#define CIM_STATUS_BF			(1<<5)
#define CIM_STATUS_BE			(1<<6)
#define CIM_STATUS_BR			(1<<7)
#define CIM_STATUS_CF			(1<<8)
#define CIM_STATUS_CE			(1<<9)
#define CIM_STATUS_CR			(1<<10)


/* Interrupt  Rgister */
#define CIM_INTEN_CD			(1<<0)
#define CIM_INTEN_FD			(1<<1)
#define CIM_INTEN_UFA			(1<<2)
#define CIM_INTEN_OFA			(1<<3)
#define CIM_INTEN_UFB			(1<<4)
#define CIM_INTEN_OFB			(1<<5)
#define CIM_INTEN_UFC			(1<<6)
#define CIM_INTEN_OFC			(1<<7)
#define CIM_INTEN_ERR			(1<<8)


/* Interrupt Status Rgister */

#define CIM_INSTAT_CD			(1<<0)
#define CIM_INSTAT_FD			(1<<1)
#define CIM_INSTAT_UFA			(1<<2)
#define CIM_INSTAT_OFA			(1<<3)
#define CIM_INSTAT_UFB			(1<<4)
#define CIM_INSTAT_OFB			(1<<5)
#define CIM_INSTAT_UFC			(1<<6)
#define CIM_INSTAT_OFC			(1<<7)
#define CIM_INSTAT_ERR			(1<<8)

#endif

  



