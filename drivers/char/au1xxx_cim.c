
/*
*  Alchemy Camera Interface (CIM) driver
*
* Copyright 2004 Advanced Micro Devices, Inc
*
*  This program is free software; you can redistribute  it and/or modify it
*  under  the terms of  the GNU General  Public License as published by the
*  Free Software Foundation;  either version 2 of the  License, or (at your
*  option) any later version.
*
*  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
*  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
*  NO  EVENT  SHALL   THE AUTHOR  BE        LIABLE FOR ANY   DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
*  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
*  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
*  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
*  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*  You should have received a copy of the  GNU General Public License along
*  with this program; if not, write  to the Free Software Foundation, Inc.,
*  675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/module.h>
//#include <linux/config.h>	//[FALinux ] 원본 내용
#include <linux/autoconf.h>	//[FALinux ] 수정한 내용. 2.6.21에서는 config.h대신 autoconf.h 파일을 사용 해야 한다.
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>

//[FALinux ] 원본 내용
#include <linux/interrupt.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/mach-au1x00/au1000.h>
#include <asm/mach-au1x00/au1xxx_dbdma.h>
#include <asm/irq.h>
#include <asm/mach-au1x00/au1xxx_cim.h>
#include <asm/mach-au1x00/au1xxx_psc.h>
#include <asm/mman.h>


#ifdef CONFIG_MIPS_PB1200
	#include <asm/mach-pb1x00/pb1200.h>
#endif

#ifdef CONFIG_MIPS_DB1200
	#include <asm/mach-db1x00/db1200.h>
#endif

#ifdef CONFIG_MIPS_EMAU1200
	#include <asm/mach-emau1200/emau1200.h>
#endif

/*
Camera Interface Driver will always work in DBDMA Mode. 
PIO Mode will result in OverFlow Error
*/

/*
* Global Variables
*/

#define CIM_NAME               "au1xxx_cim"
#define CIM_MAJOR              238
#define VERSION                "1.2"

#define MAX_DBDMA_CHANNEL       3  /* Number of DMA channel used by CIM interface*/
#define MAX_DEVICE_CMD         115 /*Max Command Send over SMbus to configure external camera*/
#define NUM_DBDMA_DESCRIPTORS   1  /* Number of descriptor used*/
#define MAX_FRAME_SIZE          (1280*960) 

//#define DEBUG    1
#ifdef DEBUG 
#define DPRINTK(fmt,args...) printk("%s: " fmt,__FUNCTION__, ## args) 
#else
#define DPRINTK(fmt, args...)
#endif

uint32  volatile  nInterruptDoneNumber = 0;
uint32 volatile ciminterruptcheck = 0;
int  prev_mode = 0;
int check_mode=0;
uint32 DBDMA_SourceID[]={DSCR_CMD0_CIM_RXA,DSCR_CMD0_CIM_RXB,DSCR_CMD0_CIM_RXC}; 
void *mem_buf; 

// [FALinux] pb1550_wm_codec_write() 이 함수를 현재 커널에서 찾을 수 없다.
extern int pb1550_wm_codec_write(u8 , u8 , u8 );


static AU1200_CIM * const au1200_cim = (AU1200_CIM*)CIM_BASE_ADDRESS;

typedef struct cim_cmos_camera_config
{
	uint32            frame_width; /* Frame Width (Pixel per Line)*/
	uint32            frame_height;/* Frame Height*/  
	unsigned  char    camera_name[32];/* Camera Name (Display/Debug Purpose)*/
	unsigned  char    camera_mode[32]; /* Camera Mode(Display/Debug Purpose)*/ 
	uint32            cmos_output_format; /*CMOS Camera output (Bayer, CCIR656*/
	uint32            camera_resformat;	/* Camera Mode(Display/Debug Purpose)*/
	uint32            au1200_dpsmode; /* Data Pattern Select ie: Mode on Camera Interface (BAYER, YUV, RAW)*/
	uint32            au1200_baymode; /* Mode within BAYER mode*/
	uint32            dbdma_channel;/* Number of DBDMA channels to be used */
	u8                device_addr; /*Camera Device address*/
	uint32            cmd_size;	/*Number of device sub register to be configured over SMBus*/ 
	u8                config_cmd[MAX_DEVICE_CMD][2]; /*2x2 array for sub device address and values*/
}CAMERA;


typedef struct cim_camera_runtime
{
	chan_tab_t        **ChannelArray[MAX_DBDMA_CHANNEL]; /* Pointer to DBDMA structure*/ 
	void              *memory[MAX_DBDMA_CHANNEL];        /*Number of DMA Channel*/
	uint32            nTransferSize[MAX_DBDMA_CHANNEL]; /*Transfer size for DMA descriptor*/
	CAMERA            *cmos_camera; /*Pointer to Camera Structure*/

}CAMERA_RUNTIME;


CAMERA_RUNTIME  cam_base;
CAMERA_RUNTIME*  pcam_base=&cam_base;

/* ***************************************************
Configurations for Different Mode of Different Cameras
****************************************************** */

CAMERA modes[]=
{
	{  /* Omnivision OV9640 Camera 1280x960 Mode (SXGA) in "Pass Thru Mode"
	     1.3 MP at 15 Fps*/

		/*  frame width=1280  */    1280,
		/*  frame heigth=960  */    960,
		/*  camera name  */         "omnivision",
		/*  camera mode */          "raw_SXGA",
		/* Cmos output format*/     CMOS_RAW,
		/* Resolution Format */     RAW_SXGA,
		/*   DPS mode */            CIM_CONFIG_RAW,
		/* Bayer Mode*/             CIM_CONFIG_BGGR,     
		/*   dbdma channel */       1,
		/*  Device Address*/       0x30,
		/* No of Sub Register*/     50,
		/* Array Initialization*/ {
			{0x12, 0x80},{0x12, 0x05},{0x11,0x80},{0x3b, 0x00},{0x33, 0x02},
			{0x37, 0x02},{0x38, 0x13},{0x39,0xf0},{0x6c, 0x40},{0x6d, 0x30},
			{0x6e, 0x4b},{0x6f, 0x60},{0x70,0x70},{0x71, 0x70},{0x74, 0x60},
			{0x75, 0x60},{0x76, 0x50},{0x77,0x48},{0x78, 0x3a},{0x79, 0x2e},
			{0x7a, 0x2a},{0x7b, 0x22},{0x7c,0x04},{0x7d, 0x07},{0x7e, 0x10},
			{0x7f, 0x28},{0x80, 0x36},{0x81,0x44},{0x82, 0x52},{0x83, 0x60},
			{0x84, 0x6c},{0x85, 0x78},{0x86,0x8c},{0x87, 0x9e},{0x88, 0xbb},
			{0x89, 0xd2},{0x8a, 0xe6},{0x0f,0x4f},{0x3c, 0x40},{0x14, 0xca},                                                     
			{0x42, 0x89},{0x24, 0x78},{0x25,0x68},{0x26, 0xd4},{0x27, 0x90},
			{0x2a, 0x00},{0x2b, 0x00},{0x3d,0x80},{0x41, 0x00},{0x60, 0x8d},

		}
 
	},  
	{  /* Omnivision OV9640 Camera 640x480 Mode (VGA) in "Pass Through Mode" */

		/*  frame width=640    */     640,
		/*  frame heigth=480   */     480,
		/*  camera name        */    "omnivision",
		/*  camera mode        */    "raw_VGA",
		/* Cmos output format*/     CMOS_RAW,
		/* Resolution Format   */    RAW_VGA,
		/*   DPS mode          */    CIM_CONFIG_RAW,
		/* Bayer Mode          */    CIM_CONFIG_BGGR,
		/*   dbdma channel */         1,    
		/*  Device Address     */    0x30,
		/* No of Sub Register  */    54,
		/* Array Initialization*/ {
			{0x12, 0x80},{0x12, 0x45},{0x11,0x81},{0x0c, 0x04},{0x0d, 0x40},{0x3b, 0x00},{0x33, 0x02},
			{0x37, 0x02},{0x38, 0x13},{0x39,0xf0},{0x6c, 0x40},{0x6d, 0x30},
			{0x6e, 0x4b},{0x6f, 0x60},{0x70,0x70},{0x71, 0x70},{0x72, 0x70},{0x73, 0x70},{0x74, 0x60},
			{0x75, 0x60},{0x76, 0x50},{0x77,0x48},{0x78, 0x3a},{0x79, 0x2e},
			{0x7a, 0x28},{0x7b, 0x22},{0x7c,0x04},{0x7d, 0x07},{0x7e, 0x2e},
			{0x7f, 0x28},{0x80, 0x36},{0x81,0x44},{0x82, 0x52},{0x83, 0x60},
			{0x84, 0x6c},{0x85, 0x78},{0x86,0x8c},{0x87, 0x9e},{0x88, 0xbb},
			{0x89, 0xd2},{0x8a, 0xe6},{0x0f,0x4f},{0x3c, 0x40},{0x14, 0xca},                                                     
			{0x42, 0x89},{0x24, 0x78},{0x25,0x68},{0x26, 0xd4},{0x27, 0x90},
			{0x2a, 0x00},{0x2b, 0x00},{0x3d,0x80},{0x41, 0x00},{0x60, 0x8d},

		}



	}, 
	{  /* Omnivision OV9640 Camera 352x288 Mode CIF "Pass Through Mode"*/

		/*  frame width=640    */     352,
		/*  frame heigth=480   */     288,
		/*  camera name        */    "omnivision",
		/*  camera mode        */    "raw_CIF",
		/* Cmos output format*/     CMOS_RAW,
		/* Resolution Format   */    RAW_CIF,
		/*   DPS mode          */    CIM_CONFIG_RAW,
		/* Bayer Mode          */    CIM_CONFIG_BGGR,     
		/*   dbdma channel     */    1,
		/*  Device Address     */    0x30,
		/* No of Sub Register  */    54,
		/* Array Initialization*/  {
			{0x12, 0x80},{0x12, 0x25},{0x11,0x80},{0x0c, 0x04},{0x0d, 0x40},{0x3b, 0x00},{0x33, 0x02},
			{0x37, 0x02},{0x38, 0x13},{0x39,0xf0},{0x6c, 0x40},{0x6d, 0x30},
			{0x6e, 0x4b},{0x6f, 0x60},{0x70,0x70},{0x71, 0x70},{0x72, 0x70},{0x73, 0x70},{0x74, 0x60},
			{0x75, 0x60},{0x76, 0x50},{0x77,0x48},{0x78, 0x3a},{0x79, 0x2e},
			{0x7a, 0x28},{0x7b, 0x22},{0x7c,0x04},{0x7d, 0x07},{0x7e, 0x10},
			{0x7f, 0x28},{0x80, 0x36},{0x81,0x44},{0x82, 0x52},{0x83, 0x60},
			{0x84, 0x6c},{0x85, 0x78},{0x86,0x8c},{0x87, 0x9e},{0x88, 0xbb},
			{0x89, 0xd2},{0x8a, 0xe6},{0x0f,0x4f},{0x3c, 0x40},{0x14, 0xca},                                                     
			{0x42, 0x89},{0x24, 0x78},{0x25,0x68},{0x26, 0xd4},{0x27, 0x90},
			{0x2a, 0x00},{0x2b, 0x00},{0x3d,0x80},{0x41, 0x00},{0x60, 0x8d},

		}

	},  
	{  /* Omnivision OV9640 Camera 320x240 Mode (QVGA) in "Pass Through Mode"*/

		/*  frame width=320    */     320,
		/*  frame heigth=240   */     240,
		/*  camera name        */    "omnivision",
		/*  camera mode        */    "raw_QVGA",
		/* Cmos output format*/     CMOS_RAW,
		/* Resolution Format   */    RAW_QVGA,
		/*   DPS mode          */    CIM_CONFIG_RAW,
		/* Bayer Mode          */    CIM_CONFIG_BGGR,     
		/*   dbdma channel     */    1,
		/*  Device Address     */    0x30,
		/* No of Sub Register  */    54,
		/* Array Initialization*/   {
			{0x12, 0x80},{0x12, 0x15},{0x11,0x83},{0x0c, 0x04},{0x0d, 0xc0},{0x3b, 0x00},{0x33, 0x02},
			{0x37, 0x02},{0x38, 0x13},{0x39,0xf0},{0x6c, 0x40},{0x6d, 0x30},
			{0x6e, 0x4b},{0x6f, 0x60},{0x70,0x70},{0x71, 0x70},{0x72, 0x70},{0x73, 0x70},{0x74, 0x60},
			{0x75, 0x60},{0x76, 0x50},{0x77,0x48},{0x78, 0x3a},{0x79, 0x2e},
			{0x7a, 0x28},{0x7b, 0x22},{0x7c,0x04},{0x7d, 0x07},{0x7e, 0x10},
			{0x7f, 0x28},{0x80, 0x36},{0x81,0x44},{0x82, 0x52},{0x83, 0x60},
			{0x84, 0x6c},{0x85, 0x78},{0x86,0x8c},{0x87, 0x9e},{0x88, 0xbb},
			{0x89, 0xd2},{0x8a, 0xe6},{0x0f,0x4f},{0x3c, 0x40},{0x14, 0xca},                                                     
			{0x42, 0x89},{0x24, 0x78},{0x25,0x68},{0x26, 0xd4},{0x27, 0x90},
			{0x2a, 0x00},{0x2b, 0x00},{0x3d,0x80},{0x41, 0x00},{0x60, 0x8d},

		}  

	},
	{  /* Omnivision OV9640 Camera 176x144 QCIF Mode "Pass Through Mode"*/

		/*  frame width=320    */     176,
		/*  frame heigth=240   */     144,
		/*  camera name        */    "omnivision",
		/*  camera mode        */    "raw_QCIF",
		/* Cmos output format*/      CMOS_RAW,
		/* Resolution Format   */    RAW_QCIF,
		/*   DPS mode          */    CIM_CONFIG_RAW,
		/* Bayer Mode          */    CIM_CONFIG_BGGR,     
		/*   dbdma channel     */    1,
		/*  Device Address     */    0x30,
		/* No of Sub Register  */    54,
		/* Array Initialization*/   {
			{0x12, 0x80},{0x12, 0x0D},{0x11,0x80},{0x0c, 0x04},{0x0d, 0xc0},{0x3b, 0x00},{0x33, 0x02},
			{0x37, 0x02},{0x38, 0x13},{0x39,0xf0},{0x6c, 0x40},{0x6d, 0x30},
			{0x6e, 0x4b},{0x6f, 0x60},{0x70,0x70},{0x71, 0x70},{0x72, 0x70},{0x73, 0x70},{0x74, 0x60},
			{0x75, 0x60},{0x76, 0x50},{0x77,0x48},{0x78, 0x3a},{0x79, 0x2e},
			{0x7a, 0x28},{0x7b, 0x22},{0x7c,0x04},{0x7d, 0x07},{0x7e, 0x10},
			{0x7f, 0x28},{0x80, 0x36},{0x81,0x44},{0x82, 0x52},{0x83, 0x60},
			{0x84, 0x6c},{0x85, 0x78},{0x86,0x8c},{0x87, 0x9e},{0x88, 0xbb},
			{0x89, 0xd2},{0x8a, 0xe6},{0x0f,0x6f},{0x3c, 0x60},{0x14, 0xca},                                                     
			{0x42, 0x89},{0x24, 0x78},{0x25,0x68},{0x26, 0xd4},{0x27, 0x90},
			{0x2a, 0x00},{0x2b, 0x00},{0x3d,0x80},{0x41, 0x00},{0x60, 0x8d},

		}  

	},
	 
	 /* Omnivision OV9640 BAYER*/
	 {  /* Omnivision OV9640 Camera 1280x960 Mode (SXGA) in BAYER Mode (Planar)*/

		/*  frame width=1280  */    1280,
		/*  frame heigth=960  */    960,
		/*  camera name  */         "omnivision",
		/*  camera mode */          "bayer_SXGA",
		/* Cmos output format*/     CMOS_RAW,
		/* Resolution Format */     BAYER_SXGA,
		/*   DPS mode */            CIM_CONFIG_BAYER,
		/* Bayer Mode*/             CIM_CONFIG_BGGR,     
		/*   dbdma channel */       3,
		/*  Device Address*/       0x30,
		/* No of Sub Register*/     50,
		/* Array Initialization*/ {
			{0x12, 0x80},{0x12, 0x05},{0x11,0x80},{0x3b, 0x00},{0x33, 0x02},
			{0x37, 0x02},{0x38, 0x13},{0x39,0xf0},{0x6c, 0x40},{0x6d, 0x30},
			{0x6e, 0x4b},{0x6f, 0x60},{0x70,0x70},{0x71, 0x70},{0x74, 0x60},
			{0x75, 0x60},{0x76, 0x50},{0x77,0x48},{0x78, 0x3a},{0x79, 0x2e},
			{0x7a, 0x2a},{0x7b, 0x22},{0x7c,0x04},{0x7d, 0x07},{0x7e, 0x10},
			{0x7f, 0x28},{0x80, 0x36},{0x81,0x44},{0x82, 0x52},{0x83, 0x60},
			{0x84, 0x6c},{0x85, 0x78},{0x86,0x8c},{0x87, 0x9e},{0x88, 0xbb},
			{0x89, 0xd2},{0x8a, 0xe6},{0x0f,0x4f},{0x3c, 0x40},{0x14, 0xca},                                                     
			{0x42, 0x89},{0x24, 0x78},{0x25,0x68},{0x26, 0xd4},{0x27, 0x90},
			{0x2a, 0x00},{0x2b, 0x00},{0x3d,0x80},{0x41, 0x00},{0x60, 0x8d},

		}

	}, /* 1280x960*/ 
	
	{  /* Omnivision OV9640 Camera 640x480 Mode (VGA) in BAYER Mode (Planar)*/

		/*  frame width=640    */     640,
		/*  frame heigth=480   */     480,
		/*  camera name        */    "omnivision",
		/*  camera mode        */    "bayer_VGA",
		/* Cmos output format*/      CMOS_RAW,
		/* Resolution Format   */    BAYER_VGA,
		/*   DPS mode          */    CIM_CONFIG_BAYER,
		/* Bayer Mode          */    CIM_CONFIG_BGGR,
		/*   dbdma channel */        3,
		/*  Device Address     */    0x30,
		/* No of Sub Register  */    54,
		/* Array Initialization*/ {
			{0x12, 0x80},{0x12, 0x45},{0x11,0x81},{0x0c, 0x04},{0x0d, 0x40},{0x3b, 0x00},{0x33, 0x02},
			{0x37, 0x02},{0x38, 0x13},{0x39,0xf0},{0x6c, 0x40},{0x6d, 0x30},
			{0x6e, 0x4b},{0x6f, 0x60},{0x70,0x70},{0x71, 0x70},{0x72, 0x70},{0x73, 0x70},{0x74, 0x60},
			{0x75, 0x60},{0x76, 0x50},{0x77,0x48},{0x78, 0x3a},{0x79, 0x2e},
			{0x7a, 0x28},{0x7b, 0x22},{0x7c,0x04},{0x7d, 0x07},{0x7e, 0x2e},
			{0x7f, 0x28},{0x80, 0x36},{0x81,0x44},{0x82, 0x52},{0x83, 0x60},
			{0x84, 0x6c},{0x85, 0x78},{0x86,0x8c},{0x87, 0x9e},{0x88, 0xbb},
			{0x89, 0xd2},{0x8a, 0xe6},{0x0f,0x4f},{0x3c, 0x40},{0x14, 0xca},                                                     
			{0x42, 0x89},{0x24, 0x78},{0x25,0x68},{0x26, 0xd4},{0x27, 0x90},
			{0x2a, 0x00},{0x2b, 0x00},{0x3d,0x80},{0x41, 0x00},{0x60, 0x8d},

		}

	}, 	
	{  /* Omnivision OV9640 Camera 352x288 CIF Mode in BAYER Mode (Planar)*/

		/*  frame width=640    */     352,
		/*  frame heigth=480   */     288,
		/*  camera name        */    "omnivision",
		/*  camera mode        */    "bayer_CIF",
		/* Cmos output format*/      CMOS_RAW,
		/* Resolution Format   */    BAYER_CIF,
		/*   DPS mode          */    CIM_CONFIG_BAYER,
		/* Bayer Mode          */    CIM_CONFIG_BGGR,     
		/*   dbdma channel     */    3,
		/*  Device Address     */    0x30,
		/* No of Sub Register  */    54,
		/* Array Initialization*/  {
			{0x12, 0x80},{0x12, 0x25},{0x11,0x80},{0x0c, 0x04},{0x0d, 0x40},{0x3b, 0x00},{0x33, 0x02},
			{0x37, 0x02},{0x38, 0x13},{0x39,0xf0},{0x6c, 0x40},{0x6d, 0x30},
			{0x6e, 0x4b},{0x6f, 0x60},{0x70,0x70},{0x71, 0x70},{0x72, 0x70},{0x73, 0x70},{0x74, 0x60},
			{0x75, 0x60},{0x76, 0x50},{0x77,0x48},{0x78, 0x3a},{0x79, 0x2e},
			{0x7a, 0x28},{0x7b, 0x22},{0x7c,0x04},{0x7d, 0x07},{0x7e, 0x10},
			{0x7f, 0x28},{0x80, 0x36},{0x81,0x44},{0x82, 0x52},{0x83, 0x60},
			{0x84, 0x6c},{0x85, 0x78},{0x86,0x8c},{0x87, 0x9e},{0x88, 0xbb},
			{0x89, 0xd2},{0x8a, 0xe6},{0x0f,0x4f},{0x3c, 0x40},{0x14, 0xca},                                                     
			{0x42, 0x89},{0x24, 0x78},{0x25,0x68},{0x26, 0xd4},{0x27, 0x90},
			{0x2a, 0x00},{0x2b, 0x00},{0x3d,0x80},{0x41, 0x00},{0x60, 0x8d},

		}

	}, 
	{  /* Omnivision OV9640 Camera 320x240 Mode (QVGA) in BAYER Mode (Planar)*/

		/*  frame width=320    */     320,
		/*  frame heigth=240   */     240,
		/*  camera name        */    "omnivision",
		/*  camera mode        */    "bayer_QVGA",
		/* Cmos output format*/      CMOS_RAW,
		/* Resolution Format   */    BAYER_QVGA,
		/*   DPS mode          */    CIM_CONFIG_BAYER,
		/* Bayer Mode          */    CIM_CONFIG_BGGR,     
		/*   dbdma channel     */    3,
		/*  Device Address     */    0x30,
		/* No of Sub Register  */    54,
		/* Array Initialization*/   {
			{0x12, 0x80},{0x12, 0x15},{0x11,0x83},{0x0c, 0x04},{0x0d, 0xc0},{0x3b, 0x00},{0x33, 0x02},
			{0x37, 0x02},{0x38, 0x13},{0x39,0xf0},{0x6c, 0x40},{0x6d, 0x30},
			{0x6e, 0x4b},{0x6f, 0x60},{0x70,0x70},{0x71, 0x70},{0x72, 0x70},{0x73, 0x70},{0x74, 0x60},
			{0x75, 0x60},{0x76, 0x50},{0x77,0x48},{0x78, 0x3a},{0x79, 0x2e},
			{0x7a, 0x28},{0x7b, 0x22},{0x7c,0x04},{0x7d, 0x07},{0x7e, 0x10},
			{0x7f, 0x28},{0x80, 0x36},{0x81,0x44},{0x82, 0x52},{0x83, 0x60},
			{0x84, 0x6c},{0x85, 0x78},{0x86,0x8c},{0x87, 0x9e},{0x88, 0xbb},
			{0x89, 0xd2},{0x8a, 0xe6},{0x0f,0x4f},{0x3c, 0x40},{0x14, 0xca},                                                     
			{0x42, 0x89},{0x24, 0x78},{0x25,0x68},{0x26, 0xd4},{0x27, 0x90},
			{0x2a, 0x00},{0x2b, 0x00},{0x3d,0x80},{0x41, 0x00},{0x60, 0x8d},

		}  

	}, 
	{  /* Omnivision OV9640 Camera 640x480 Mode (QCIF) in BAYER Mode (Planar)*/

		/*  frame width=320    */     176,
		/*  frame heigth=240   */     144,
		/*  camera name        */    "omnivision",
		/*  camera mode        */    "bayer_QCIF",
		/* Cmos output format*/      CMOS_RAW,
		/* Resolution Format   */    BAYER_QCIF,
		/*   DPS mode          */    CIM_CONFIG_BAYER,
		/* Bayer Mode          */    CIM_CONFIG_BGGR,     
		/*   dbdma channel     */    3,
		/*  Device Address     */    0x30,
		/* No of Sub Register  */    54,
		/* Array Initialization*/   {
			{0x12, 0x80},{0x12, 0x0D},{0x11,0x80},{0x0c, 0x04},{0x0d, 0xc0},{0x3b, 0x00},{0x33, 0x02},
			{0x37, 0x02},{0x38, 0x13},{0x39,0xf0},{0x6c, 0x40},{0x6d, 0x30},
			{0x6e, 0x4b},{0x6f, 0x60},{0x70,0x70},{0x71, 0x70},{0x72, 0x70},{0x73, 0x70},{0x74, 0x60},
			{0x75, 0x60},{0x76, 0x50},{0x77,0x48},{0x78, 0x3a},{0x79, 0x2e},
			{0x7a, 0x28},{0x7b, 0x22},{0x7c,0x04},{0x7d, 0x07},{0x7e, 0x10},
			{0x7f, 0x28},{0x80, 0x36},{0x81,0x44},{0x82, 0x52},{0x83, 0x60},
			{0x84, 0x6c},{0x85, 0x78},{0x86,0x8c},{0x87, 0x9e},{0x88, 0xbb},
			{0x89, 0xd2},{0x8a, 0xe6},{0x0f,0x6f},{0x3c, 0x60},{0x14, 0xca},                                                     
			{0x42, 0x89},{0x24, 0x78},{0x25,0x68},{0x26, 0xd4},{0x27, 0x90},
			{0x2a, 0x00},{0x2b, 0x00},{0x3d,0x80},{0x41, 0x00},{0x60, 0x8d},

		}  

	}, 
	{  /* Omnivision OV9640 Camera 1280x960 Mode (SXGA) in YCbCr Camera pass Thru Mode*/

		/*  frame width=320    */     1280,
		/*  frame heigth=240   */     960,
		/*  camera name        */    "omnivision",
		/*  camera mode        */    "YCbCr_SXGA",
		/* Cmos output format*/      CMOS_CCIR656,
		/* Resolution Format   */    YCbCr_SXGA_RAW,
		/*   DPS mode          */    CIM_CONFIG_RAW,
		/* Bayer Mode          */    CIM_CONFIG_BGGR,     
		/*   dbdma channel     */    1,
		/*  Device Address     */    0x30,
		/* No of Sub Register  */    115,
		/* Array Initialization*/   {
			{0x12, 0x80},{0x11, 0x80},{0x12,0x00},{0x13, 0xA8},{0x01, 0x80},
			{0x02, 0x80},{0x04, 0x40},{0x0C,0x04},{0x0D, 0xC0},{0x0E, 0x81},
			{0x0f, 0x4F},{0x14, 0x4A},{0x16,0x02},{0x1B, 0x01},{0x24, 0x70},
			{0x25, 0x68},{0x26, 0xD3},{0x27,0x90},{0x2A, 0x00},{0x2B, 0x00},
			{0x33, 0x28},{0x37, 0x02},{0x38,0x13},{0x39, 0xF0},{0x3A, 0x00},
			{0x3B, 0x01},{0x3C, 0x46},{0x3D,0x90},{0x3E, 0x02},{0x3F, 0xF2},
			{0x41, 0x02},{0x42, 0xC9},{0x43,0xF0},{0x44, 0x10},{0x45, 0x6C},
			{0x46, 0x6C},{0x47, 0x44},{0x48,0x44},{0x49, 0x03},{0x4F, 0x50},                                                     
			{0x50, 0x43},{0x51, 0x0D},{0x52,0x19},{0x53, 0x4C},{0x54, 0x65},
			{0x59, 0x49},{0x5A, 0x94},{0x5B,0x46},{0x5C, 0x84},{0x5D, 0x5C},
			{0x5E, 0x08},{0x5F, 0x00},{0x60,0x14},{0x61, 0xCE},{0x62, 0x70},
			{0x63, 0x00},{0x64, 0x04},{0x65,0x00},{0x66, 0x00},{0x69, 0x00},
			{0x6A, 0x3E},{0x6B, 0x3F},{0x6C,0x40},{0x6D, 0x30},{0x6E, 0x4B},
			{0x6F, 0x60},{0x70, 0x70},{0x71,0x70},{0x72, 0x70},{0x73, 0x70},
			{0x74, 0x70},{0x75, 0x60},{0x76,0x50},{0x77, 0x48},{0x78, 0x3A},
			{0x79, 0x2E},{0x7A, 0x28},{0x7B,0x22},{0x7C, 0x04},{0x7D, 0x07},
			{0x7E, 0x10},{0x7F, 0x28},{0x80,0x36},{0x81, 0x44},{0x82, 0x52},
			{0x83, 0x60},{0x84, 0x6C},{0x85,0x78},{0x86, 0x8C},{0x87, 0x9E},                                                     
			{0x88, 0xBB},{0x89, 0xD2},{0x8A,0xE6},{0x13, 0xAF},{0x13, 0x8D},
			{0x01, 0x80},{0x02, 0x80},{0x42,0xC9},{0x16, 0x02},{0x43, 0xF0},
			{0x44, 0x10},{0x45, 0x20},{0x46,0x20},{0x47, 0x20},{0x48, 0x20},
			{0x59, 0x17},{0x5A, 0x71},{0x5B,0x56},{0x5C, 0x74},{0x5D, 0x68},
			{0x5e, 0x10},{0x5f, 0x00},{0x60,0x14},{0x61, 0xCE},{0x13, 0x8F},
		}  

	},	
	{  /* Omnivision OV9640 Camera 640x480 Mode (SXGA) in YCbCr Camera pass Thru Mode*/

		/*  frame width=320    */     640,
		/*  frame heigth=240   */     480,
		/*  camera name        */    "omnivision",
		/*  camera mode        */    "YCbCr_VGA_raw",
		/* Cmos output format*/      CMOS_CCIR656,
		/* Resolution Format   */    YCbCr_VGA_RAW,
		/*   DPS mode          */    CIM_CONFIG_RAW,
		/* Bayer Mode          */    CIM_CONFIG_BGGR,     
		/*   dbdma channel     */    1,
		/*  Device Address     */    0x30,
		/* No of Sub Register  */    94,
		/* Array Initialization*/   {
			{0x12, 0x80},{0x11, 0x81},{0x12,0x40},{0x13, 0xA8},{0x01, 0x80},
			{0x02, 0x80},{0x04, 0x40},{0x0C,0x04},{0x0D, 0xC0},{0x0E, 0x81},
			{0x0f, 0x4F},{0x14, 0x4A},{0x16,0x02},{0x1B, 0x01},{0x24, 0x70},
			{0x25, 0x68},{0x26, 0xD3},{0x27,0x90},{0x2A, 0x00},{0x2B, 0x00},
			{0x33, 0x02},{0x37, 0x02},{0x38,0x13},{0x39, 0xF0},{0x3A, 0x00},
			{0x3B, 0x01},{0x3C, 0x46},{0x3D,0x90},{0x3E, 0x02},{0x3F, 0xF2},
			{0x41, 0x02},{0x42, 0xC9},{0x43,0xF0},{0x44, 0x10},{0x45, 0x6C},
			{0x46, 0x6C},{0x47, 0x44},{0x48,0x44},{0x49, 0x03},{0x4F, 0x50},                                                     
			{0x50, 0x43},{0x51, 0x0D},{0x52,0x19},{0x53, 0x4C},{0x54, 0x65},
			{0x59, 0x49},{0x5A, 0x94},{0x5B,0x46},{0x5C, 0x84},{0x5D, 0x5C},
			{0x5E, 0x08},{0x5F, 0x00},{0x60,0x14},{0x61, 0xCE},{0x62, 0x70},
			{0x63, 0x00},{0x64, 0x04},{0x65,0x00},{0x66, 0x00},{0x69, 0x00},
			{0x6A, 0x3E},{0x6B, 0x3F},{0x6C,0x40},{0x6D, 0x30},{0x6E, 0x4B},
			{0x6F, 0x60},{0x70, 0x70},{0x71,0x70},{0x72, 0x70},{0x73, 0x70},
			{0x74, 0x60},{0x75, 0x60},{0x76,0x50},{0x77, 0x48},{0x78, 0x3A},
			{0x79, 0x2E},{0x7A, 0x28},{0x7B,0x22},{0x7C, 0x04},{0x7D, 0x07},
			{0x7E, 0x10},{0x7F, 0x28},{0x80,0x36},{0x81, 0x44},{0x82, 0x52},
			{0x83, 0x60},{0x84, 0x6C},{0x85,0x78},{0x86, 0x8C},{0x87, 0x9E},                                                     
			{0x88, 0xBB},{0x89, 0xD2},{0x8A,0xE6},{0x13, 0xAF},
		}  

	},
	{  /* Omnivision OV9640 Camera 352x288 Mode (CIF) in YCbCr Camera pass Thru Mode*/

		/*  frame width=320    */     352,
		/*  frame heigth=240   */     288,
		/*  camera name        */    "omnivision",
		/*  camera mode        */    "YCbCr_CIF_raw",
		/* Cmos output format*/      CMOS_CCIR656,
		/* Resolution Format   */    YCbCr_CIF_RAW,
		/*   DPS mode          */    CIM_CONFIG_RAW,
		/* Bayer Mode          */    CIM_CONFIG_BGGR,     
		/*   dbdma channel     */    1,
		/*  Device Address     */    0x30,
		/* No of Sub Register  */    94,
		/* Array Initialization*/   {
			{0x12, 0x80},{0x11, 0x81},{0x12,0x20},{0x13, 0xA8},{0x01, 0x80},
			{0x02, 0x80},{0x04, 0x40},{0x0C,0x04},{0x0D, 0xC0},{0x0E, 0x81},
			{0x0f, 0x4F},{0x14, 0x4A},{0x16,0x02},{0x1B, 0x01},{0x24, 0x70},
			{0x25, 0x68},{0x26, 0xD3},{0x27,0x90},{0x2A, 0x00},{0x2B, 0x00},
			{0x33, 0x02},{0x37, 0x02},{0x38,0x13},{0x39, 0xF0},{0x3A, 0x00},
			{0x3B, 0x01},{0x3C, 0x46},{0x3D,0x90},{0x3E, 0x02},{0x3F, 0xF2},
			{0x41, 0x02},{0x42, 0xC9},{0x43,0xF0},{0x44, 0x10},{0x45, 0x6C},
			{0x46, 0x6C},{0x47, 0x44},{0x48,0x44},{0x49, 0x03},{0x4F, 0x50},                                                     
			{0x50, 0x43},{0x51, 0x0D},{0x52,0x19},{0x53, 0x4C},{0x54, 0x65},
			{0x59, 0x49},{0x5A, 0x94},{0x5B,0x46},{0x5C, 0x84},{0x5D, 0x5C},
			{0x5E, 0x08},{0x5F, 0x00},{0x60,0x14},{0x61, 0xCE},{0x62, 0x70},
			{0x63, 0x00},{0x64, 0x04},{0x65,0x00},{0x66, 0x00},{0x69, 0x00},
			{0x6A, 0x3E},{0x6B, 0x3F},{0x6C,0x40},{0x6D, 0x30},{0x6E, 0x4B},
			{0x6F, 0x60},{0x70, 0x70},{0x71,0x70},{0x72, 0x70},{0x73, 0x70},
			{0x74, 0x60},{0x75, 0x60},{0x76,0x50},{0x77, 0x48},{0x78, 0x3A},
			{0x79, 0x2E},{0x7A, 0x28},{0x7B,0x22},{0x7C, 0x04},{0x7D, 0x07},
			{0x7E, 0x10},{0x7F, 0x28},{0x80,0x36},{0x81, 0x44},{0x82, 0x52},
			{0x83, 0x60},{0x84, 0x6C},{0x85,0x78},{0x86, 0x8C},{0x87, 0x9E},                                                     
			{0x88, 0xBB},{0x89, 0xD2},{0x8A,0xE6},{0x13, 0xAF},
		}  

	},
	{  /* Omnivision OV9640 Camera 320x240 Mode QVGA in YCbCr Camera pass Thru Mode*/

		/*  frame width=320    */     320,
		/*  frame heigth=240   */     240,
		/*  camera name        */    "omnivision",
		/*  camera mode        */    "YCbCr_QVGA_raw",
		/* Cmos output format*/      CMOS_CCIR656,
		/* Resolution Format   */    YCbCr_QVGA_RAW,
		/*   DPS mode          */    CIM_CONFIG_RAW,
		/* Bayer Mode          */    CIM_CONFIG_BGGR,     
		/*   dbdma channel     */    1,
		/*  Device Address     */    0x30,
		/* No of Sub Register  */    94,
		/* Array Initialization*/   {
			{0x12, 0x80},{0x11, 0x81},{0x12,0x10},{0x13, 0xA8},{0x01, 0x80},
			{0x02, 0x80},{0x04, 0x40},{0x0C,0x04},{0x0D, 0xC0},{0x0E, 0x81},
			{0x0f, 0x4F},{0x14, 0x4A},{0x16,0x02},{0x1B, 0x01},{0x24, 0x70},
			{0x25, 0x68},{0x26, 0xD3},{0x27,0x90},{0x2A, 0x00},{0x2B, 0x00},
			{0x33, 0x02},{0x37, 0x02},{0x38,0x13},{0x39, 0xF0},{0x3A, 0x00},
			{0x3B, 0x01},{0x3C, 0x46},{0x3D,0x90},{0x3E, 0x02},{0x3F, 0xF2},
			{0x41, 0x02},{0x42, 0xC9},{0x43,0xF0},{0x44, 0x10},{0x45, 0x6C},
			{0x46, 0x6C},{0x47, 0x44},{0x48,0x44},{0x49, 0x03},{0x4F, 0x50},                                                     
			{0x50, 0x43},{0x51, 0x0D},{0x52,0x19},{0x53, 0x4C},{0x54, 0x65},
			{0x59, 0x49},{0x5A, 0x94},{0x5B,0x46},{0x5C, 0x84},{0x5D, 0x5C},
			{0x5E, 0x08},{0x5F, 0x00},{0x60,0x14},{0x61, 0xCE},{0x62, 0x70},
			{0x63, 0x00},{0x64, 0x04},{0x65,0x00},{0x66, 0x00},{0x69, 0x00},
			{0x6A, 0x3E},{0x6B, 0x3F},{0x6C,0x40},{0x6D, 0x30},{0x6E, 0x4B},
			{0x6F, 0x60},{0x70, 0x70},{0x71,0x70},{0x72, 0x70},{0x73, 0x70},
			{0x74, 0x60},{0x75, 0x60},{0x76,0x50},{0x77, 0x48},{0x78, 0x3A},
			{0x79, 0x2E},{0x7A, 0x28},{0x7B,0x22},{0x7C, 0x04},{0x7D, 0x07},
			{0x7E, 0x10},{0x7F, 0x28},{0x80,0x36},{0x81, 0x44},{0x82, 0x52},
			{0x83, 0x60},{0x84, 0x6C},{0x85,0x78},{0x86, 0x8C},{0x87, 0x9E},                                                     
			{0x88, 0xBB},{0x89, 0xD2},{0x8A,0xE6},{0x13, 0xAF},
		}  

	},
	{  /* Omnivision OV9640 Camera 176x144 Mode (QCIF) in YCbCr Camera pass Thru Mode*/

		/*  frame width=320    */     176,
		/*  frame heigth=240   */     144,
		/*  camera name        */    "omnivision",
		/*  camera mode        */    "YCbCr_QCIF_raw",
		/* Cmos output format*/      CMOS_CCIR656,
		/* Resolution Format   */    YCbCr_QCIF_RAW,
		/*   DPS mode          */    CIM_CONFIG_RAW,
		/* Bayer Mode          */    CIM_CONFIG_BGGR,     
		/*   dbdma channel     */    1,
		/*  Device Address     */    0x30,
		/* No of Sub Register  */    94,
		/* Array Initialization*/   {
			{0x12, 0x80},{0x11, 0x81},{0x12,0x08},{0x13, 0xA8},{0x01, 0x80},
			{0x02, 0x80},{0x04, 0x40},{0x0C,0x04},{0x0D, 0xC0},{0x0E, 0x81},
			{0x0f, 0x4F},{0x14, 0x4A},{0x16,0x02},{0x1B, 0x01},{0x24, 0x70},
			{0x25, 0x68},{0x26, 0xD3},{0x27,0x90},{0x2A, 0x00},{0x2B, 0x00},
			{0x33, 0x02},{0x37, 0x02},{0x38,0x13},{0x39, 0xF0},{0x3A, 0x00},
			{0x3B, 0x01},{0x3C, 0x46},{0x3D,0x90},{0x3E, 0x02},{0x3F, 0xF2},
			{0x41, 0x02},{0x42, 0xC9},{0x43,0xF0},{0x44, 0x10},{0x45, 0x6C},
			{0x46, 0x6C},{0x47, 0x44},{0x48,0x44},{0x49, 0x03},{0x4F, 0x50},                                                     
			{0x50, 0x43},{0x51, 0x0D},{0x52,0x19},{0x53, 0x4C},{0x54, 0x65},
			{0x59, 0x49},{0x5A, 0x94},{0x5B,0x46},{0x5C, 0x84},{0x5D, 0x5C},
			{0x5E, 0x08},{0x5F, 0x00},{0x60,0x14},{0x61, 0xCE},{0x62, 0x70},
			{0x63, 0x00},{0x64, 0x04},{0x65,0x00},{0x66, 0x00},{0x69, 0x00},
			{0x6A, 0x3E},{0x6B, 0x3F},{0x6C,0x40},{0x6D, 0x30},{0x6E, 0x4B},
			{0x6F, 0x60},{0x70, 0x70},{0x71,0x70},{0x72, 0x70},{0x73, 0x70},
			{0x74, 0x60},{0x75, 0x60},{0x76,0x50},{0x77, 0x48},{0x78, 0x3A},
			{0x79, 0x2E},{0x7A, 0x28},{0x7B,0x22},{0x7C, 0x04},{0x7D, 0x07},
			{0x7E, 0x10},{0x7F, 0x28},{0x80,0x36},{0x81, 0x44},{0x82, 0x52},
			{0x83, 0x60},{0x84, 0x6C},{0x85,0x78},{0x86, 0x8C},{0x87, 0x9E},                                                     
			{0x88, 0xBB},{0x89, 0xD2},{0x8A,0xE6},{0x13, 0xAF},
		}  

	},		 
   /* Add modes*/
};

CAMERA* OrigCimArryPtr=modes;

static int find_mode_index( uint32 res_format)
{
	int i;
	CAMERA_RUNTIME  *findex;
	
	findex=pcam_base;        
	findex->cmos_camera=OrigCimArryPtr;
	
	for ( i =0; i< (sizeof(modes)/sizeof(CAMERA));i++ )
	{
		if ( res_format==(findex->cmos_camera->camera_resformat) )
		{

			return i;
		}
		(findex->cmos_camera)++;
	}
	printk(" Au1xxx_CIM: ERROR: Camera Index Failed \n");
	return -1;
}

void Cim_DDMA_Done_Interrupt(int irq, void *param, struct pt_regs *regs)
{
	//Count Number of Interrupt  
	nInterruptDoneNumber++;
	DPRINTK(" DBDMA Interrupt Number **- %d -** \n", nInterruptDoneNumber);

}
	     
int Capture_Image (void)
{
	au1200_cim->capture=0;
	au1200_cim->capture=CIM_CAPTURE_CLR;
	au1200_cim->capture=CIM_CAPTURE_SCE;
	return 1;
}

void Camera_pwr_down(void)
{
	/* Power shut to Camera */
	//bcsr->board |= BCSR_BOARD_CAMPWR; 	//[FALinux ] 일단 컴파일 시키기 위해 주석 처리
	DPRINTK("Exit:Powered OFF Camera \n");
}

void Camera_pwr_up(void)
{
   	//bcsr->board &=~ BCSR_BOARD_CAMPWR; //[FALinux ] 일단 컴파일 시키기 위해 주석 처리
   	DPRINTK("Exit:Powered UP Camera \n");
}

void CIM_Cleanup( CAMERA_RUNTIME *cim_cleanup)
{
	CAMERA* cim_ptr;
	uint32 frame_size;
	int i; 
	cim_ptr=cim_cleanup->cmos_camera;
	frame_size=(cim_ptr->frame_width)*(cim_ptr->frame_height);

	DPRINTK(" ENTRY:Previous Index in Camera Array %d \n",prev_mode); 
	for ( i=0; i< cim_ptr->dbdma_channel;i++ )
	{
		if ( cim_cleanup->ChannelArray[i] )
		{
			DPRINTK("Releasing ChannelArray[%d]\n", i);
			au1xxx_dbdma_stop((u32)(cim_cleanup->ChannelArray[i]));
			au1xxx_dbdma_reset((u32)(cim_cleanup->ChannelArray[i]));
			au1xxx_dbdma_chan_free((u32)(cim_cleanup->ChannelArray[i]));
		}

		if ( (cim_cleanup->memory[i]) != NULL )
		{
			DPRINTK("Cleaning Memory[%d] \n", i);
			free_pages((unsigned long)cim_cleanup->memory[i],
					get_order(frame_size));
		}
	}
	 
}

int Camera_Config(CAMERA_RUNTIME* cim_config )
{
	uint32  nAckCount=0;
	int i, ErrorCheck;
	ErrorCheck=0;
	uint32 nCameraModeConfig=0;
	uint32   nClearSetInterrupt=0;
	CAMERA    *cim_config_ptr;

	cim_config_ptr=cim_config->cmos_camera;

	DPRINTK("Value of frame width is %d and frame height %d \n", 
	           cim_config_ptr->frame_width,cim_config_ptr->frame_height);
	DPRINTK( "Value of dbdma Channel %d \n", cim_config_ptr->dbdma_channel);
	DPRINTK( "DPS MODE is %d \n", cim_config_ptr->au1200_dpsmode);
	
	/* To get rid of hard coded number from Transfer Size*/
	/* Now transfer size will be calulated on the on the fly*/
	/*******************************************************************
		    In YCbCr 4:2:2 data size is twice the frame size
		     Y=Frame Size
		     Cb=Frame Size/2
		     Cr=Frame Size/2
		     Total size of Frame: Y+Cb+Cr effectively 2*FrameSize
		  *********************************************************************/
	if ((cim_config_ptr->au1200_dpsmode)==CIM_CONFIG_RAW)
	  {
		if (cim_config_ptr->cmos_output_format==CMOS_CCIR656)
		{
		  
			cim_config->nTransferSize[0]=2*(cim_config_ptr->frame_width) * ( cim_config_ptr->frame_height);
		  	DPRINTK("FIFO-A YCbCR Transfer Size in Raw mode %d \n", cim_config->nTransferSize[0]);
		}
		else
		{
	        cim_config->nTransferSize[0]=(cim_config_ptr->frame_width) * ( cim_config_ptr->frame_height);
			DPRINTK("FIFO-A Transfer Size in Raw mode %d \n", cim_config->nTransferSize[0]);
		}
	   		cim_config->memory[0]=mem_buf;
	 }
	else if ((cim_config_ptr->au1200_dpsmode)==CIM_CONFIG_BAYER)
	 {
			DPRINTK( "Bayer Mode(Planar) Memory Size Calculation\n");
	    /* FIFO A Hold Red Pixels which is Total Pixels/4*/
	    	cim_config->nTransferSize[0]=((cim_config_ptr->frame_width) * ( cim_config_ptr->frame_height))/4;
	    	cim_config->nTransferSize[1]=((cim_config_ptr->frame_width) * ( cim_config_ptr->frame_height))/2;   
	    	cim_config->nTransferSize[2]=((cim_config_ptr->frame_width) * ( cim_config_ptr->frame_height))/4;
	    	DPRINTK(" Transfer Size of FIFO-A %d FIFO-B %d & FIFO-C in Bayer Mode %d\n", cim_config->nTransferSize[0],
	           cim_config->nTransferSize[1],cim_config->nTransferSize[2]);
		     
	  		cim_config->memory[0]=mem_buf;
	  		cim_config->memory[1]=mem_buf+ cim_config->nTransferSize[0];
	  		cim_config->memory[2]=mem_buf+ cim_config->nTransferSize[0]
			      + cim_config->nTransferSize[1];
	  }
	  else
	  {
	       	DPRINTK( "CCIR656 (Planer) Mode Memory Size Calculation\n");
			cim_config->nTransferSize[0]=((cim_config_ptr->frame_width) * ( cim_config_ptr->frame_height));
			cim_config->nTransferSize[1]=((cim_config_ptr->frame_width) * ( cim_config_ptr->frame_height))/2;   
			cim_config->nTransferSize[2]=((cim_config_ptr->frame_width) * ( cim_config_ptr->frame_height))/2;
			DPRINTK(" Transfer Size of FIFO-A %d FIFO-B %d & FIFO-C in CCIR656 Mode %d\n", cim_config->nTransferSize[0],
			cim_config->nTransferSize[1],cim_config->nTransferSize[2]);
	  
	  		cim_config->memory[0]=mem_buf;
	  		cim_config->memory[1]=mem_buf+ cim_config->nTransferSize[0];
	  		cim_config->memory[2]=mem_buf+ cim_config->nTransferSize[0]
			      + cim_config->nTransferSize[1];
	  
	  }
	
 	for ( i=0; i< cim_config->cmos_camera->dbdma_channel;i++ )
	{
		/* Allocate Channel */
		cim_config->ChannelArray[i]=(chan_tab_t**)au1xxx_dbdma_chan_alloc(DBDMA_SourceID[i], DSCR_CMD0_ALWAYS,
					Cim_DDMA_Done_Interrupt, (void *)NULL); 
		
		if ( cim_config->ChannelArray[i] !=NULL )
		{ 
			au1xxx_dbdma_set_devwidth((u32)(cim_config->ChannelArray[i]), 32);
            if ( au1xxx_dbdma_ring_alloc((u32)(cim_config->ChannelArray[i]),  16) == 0 )
			{
				printk("Allocation failed for an CIM DDMA channel-A in Single Channel\n"); 
				ErrorCheck++;
				goto error_ch_alloc;
		  	}
	   	au1xxx_dbdma_start((u32)( cim_config->ChannelArray[i]));                              
		int j=0;
	   	for ( j=0; j <NUM_DBDMA_DESCRIPTORS; j++ )
			{ 
    
				if ( ! au1xxx_dbdma_put_dest((u32)( cim_config->ChannelArray[i]),  
								cim_config->memory[i],  cim_config->nTransferSize[i]) )
				{
					printk("DBDMA Error..Putting Descriptor on Buffer Ring Channel A in Single Channel \n");
				}
		  	} 
			  
		} 
		else
		{
			ErrorCheck++;
			goto error_ch_alloc;
		}

	}
	DPRINTK("CMOS Camera configuration \n");
	for ( i=0; i< cim_config_ptr->cmd_size; i++ )
	{
#if 0
		// [FALinux] pb1550_wm_codec_write() 이 함수를 현재 커널에서 찾을 수 없다.
		while ( ( pb1550_wm_codec_write(cim_config_ptr->device_addr , cim_config_ptr->config_cmd[i][0],cim_config_ptr->config_cmd[i][1] ) != 1)
				&& (nAckCount < 50) )
		{
			nAckCount++;
		}
#endif
		if(i==0){mdelay(1);}

	}	
	if(nAckCount==50)
	{
	 	printk("External CMOS Camera Not Present or not properly connected !!!! !\n");
	 	goto error_ch_alloc;
	}	else
	{
        DPRINTK("CMOS Camera configuration Sucessful \n");
	}

	au1200_cim->enable= CIM_ENABLE_EN;
	au1200_cim->capture=CIM_CAPTURE_CLR;
	
	if ( cim_config_ptr->au1200_dpsmode==1 )
	{
		nCameraModeConfig= CIM_CONFIG_DPS_N(cim_config_ptr->au1200_dpsmode) | 
		          							CIM_CONFIG_FS | 
											CIM_CONFIG_BAY_N(cim_config_ptr->au1200_baymode)|
											CIM_CONFIG_BYT |
											CIM_CONFIG_LEN( CIM_CONFIG_LEN_10BIT );
	}
	else if ( cim_config_ptr->au1200_dpsmode==0 )
	{
		nCameraModeConfig=CIM_CONFIG_DPS_N(cim_config_ptr->au1200_dpsmode) |
		 								   	CIM_CONFIG_FS | CIM_CONFIG_BYT |
						 					CIM_CONFIG_LEN( CIM_CONFIG_LEN_10BIT)|
											CIM_CONFIG_BAY_N(cim_config_ptr->au1200_baymode) |
											CIM_CONFIG_PM; 
	}
	else
	{
		/* Need to re check........*/
		nCameraModeConfig= CIM_CONFIG_DPS_N(cim_config_ptr->au1200_dpsmode) | 
											CIM_CONFIG_FS |
											CIM_CONFIG_BYT |
											CIM_CONFIG_LEN( CIM_CONFIG_LEN_10BIT)|
											CIM_CONFIG_FSEL_N(CIM_CONFIG_FIELD12);
	}

	au1200_cim->config=nCameraModeConfig;
	nClearSetInterrupt= CIM_INSTAT_CD | CIM_INSTAT_FD |
						CIM_INSTAT_UFA | CIM_INSTAT_OFA |
						CIM_INSTAT_UFB | CIM_INSTAT_OFB |
						CIM_INSTAT_UFB | CIM_INSTAT_OFC;

	au1200_cim->instat=nClearSetInterrupt;
	au1200_cim->inten=nClearSetInterrupt;
	DPRINTK("Camera Config register value %x \n",au1200_cim->config);
	for(i=0;i<6;i++){mdelay(1);}
	
	error_ch_alloc:
	if ( ErrorCheck )
	{
		CIM_Cleanup(cim_config);
		return -1;
	}
  return 0;
}

/* To get contigious Memroy location using GetFree pages*/

static unsigned long
Camera_mem_alloc(unsigned long size)
{
    /* __get_free_pages() fulfills a max request of 2MB */
    /* do multiple requests to obtain large contigous mem */
#define MAX_GFP 0x00200000

	unsigned long mem, amem, alloced = 0, allocsize;

    DPRINTK(" Allocating Contigous chunk of memory %d \n", size);
    size += 0x1000;
    allocsize = (size < MAX_GFP) ? size : MAX_GFP;

    /* Get first chunk */
    mem = (unsigned long )
        __get_free_pages(GFP_ATOMIC | GFP_DMA, get_order(allocsize));
    if (mem != 0) alloced = allocsize;

    /* Get remaining, contiguous chunks */
    while (alloced < size)
    {
        amem = (unsigned long )
            __get_free_pages(GFP_ATOMIC | GFP_DMA, get_order(allocsize));
        if (amem != 0)
            alloced += allocsize;

        /* check for contiguous mem alloced */
        if ((amem == 0) || (amem + allocsize) != mem)
            break;
        else
            mem = amem;
    }
    return mem;
}

void Cim_Interrupt_Handler(int irq, void *dev_id, struct pt_regs *regs)
{

	uint32 nStatus;

	disable_irq(AU1200_CAMERA_INT);
	nStatus=au1200_cim->instat;

	if ( nStatus &  CIM_INSTAT_CD )
	{
	//DPRINTK("\n Camera Driver: Capture Done Interrupt Recieved \n");
		au1200_cim->instat=CIM_INSTAT_CD;
	}
	else if ( nStatus &  CIM_INSTAT_FD )
	{
		 //DPRINTK("\n Camera Driver: Frame Done Interrupt Recieved \n");  
		au1200_cim->instat=CIM_INSTAT_FD;
	}
	else if ( nStatus &  CIM_INSTAT_UFA )
	{
		DPRINTK("\n Camera Driver: FIFO-A Underflow Interrupt Recieved \n");
		au1200_cim->instat=CIM_INSTAT_UFA; 
	   ciminterruptcheck = 1;
   	}
	else if ( nStatus &  CIM_INSTAT_OFA )
	{
		DPRINTK("\n Camera Driver: FIFO-A Overflow Interrupt Recieved \n"); 
		au1200_cim->instat=CIM_INSTAT_OFA;
		ciminterruptcheck = 1; 
	}
	else if ( nStatus &  CIM_INSTAT_UFB )
	{
		DPRINTK("\n Camera Driver: FIFO-B Underflow Interrupt Recieved \n"); 
		au1200_cim->instat=CIM_INSTAT_UFB;
		ciminterruptcheck = 1; 
	}
	else if ( nStatus &  CIM_INSTAT_OFB )
	{
		DPRINTK("\n Camera Driver: FIFO-B Overflow Interrupt Recieved \n");
		au1200_cim->instat=CIM_INSTAT_OFB;
		ciminterruptcheck = 1; 
	}
	else if ( nStatus &  CIM_INSTAT_UFC )
	{
		DPRINTK("\n Camera Driver: FIFO-C Underflow Interrupt Recieved \n");  
		au1200_cim->instat=CIM_INSTAT_UFC;
		ciminterruptcheck = 1; 
	}
	else if ( nStatus &  CIM_INSTAT_OFC )
	{
		DPRINTK("\n Camera Driver: FIFO-C Overflow Interrupt Recieved \n");  
		au1200_cim->instat=CIM_INSTAT_OFC;
		ciminterruptcheck = 1; 
	}
	else if ( nStatus &  CIM_INSTAT_ERR )
	{
		DPRINTK("\n Camera Driver: ERR (for 656 Mode) Interrupt Recieved \n"); 
		au1200_cim->instat=CIM_INSTAT_ERR;
	}
	
	enable_irq(AU1200_CAMERA_INT);
} 

static int
au1xxxcim_open(struct inode *inode, struct file *file)
{
	DPRINTK("FOPS Open Kernel Routine \n");
	return 0;
}

static int
au1xxxcim_ioctl(struct inode *inode, struct file *file,
				unsigned int cmd, unsigned long arg)
{
	nInterruptDoneNumber=0; 
	int mode_index;
	uint32 i;   
	CAMERA_RUNTIME *capture;
	CAMERA *capture_ptr;

	capture=pcam_base;
      
	switch (cmd)
	{
 
	case AU1XXXCIM_QUERY:{
                     		/*returing previous mode*/
					DPRINTK("QUERY Mode- Return Camera Index to Application is %d \n", prev_mode);
                 		 return prev_mode;
		      }
	case AU1XXXCIM_CONFIGURE: {
					DPRINTK(" CONFIGURE Mode\n");
					mode_index=find_mode_index(arg);
				    if (mode_index == -1){
					   printk(" Unsupported Mode. This mode is not supported in current driver \n");
						if(check_mode !=1)
					   	{
						  	capture->cmos_camera=OrigCimArryPtr+prev_mode; 
				         	capture_ptr=capture->cmos_camera;
						 	CIM_Cleanup(capture);
					  	 }
					  check_mode=1;
					  return  0;
					}
					else
					{
						if(check_mode !=1){
							capture->cmos_camera=OrigCimArryPtr+prev_mode; 
							capture_ptr=capture->cmos_camera;
			   				DPRINTK("CONFIGURE Mode: Calling CleanUP as Camera needs to be re-configured \n");
							CIM_Cleanup(capture); 
						 }
					}
					DPRINTK("CONFIGURE:Camera Array Index value is %d \n", mode_index);
			      	capture->cmos_camera=OrigCimArryPtr+mode_index;
					capture_ptr=capture->cmos_camera;
					DPRINTK(" CONFIGURE: Camera configured in  ** %s ** Mode \n", capture_ptr->camera_mode);
		    		au1200_cim->enable &= ~CIM_ENABLE_EN;
					Camera_Config(capture);
					Camera_pwr_down();
					mdelay(2);
					Camera_pwr_up();
					mdelay(7);
				   	prev_mode=mode_index;
					check_mode=0;
					printk(" Prev_mode %d mode_index %d \n", prev_mode, mode_index);
					return mode_index;
          	      }
case AU1XXXCIM_CAPTURE: {
                   
                    if(check_mode == 1){
                     printk(" Unsupported Mode. This mode is not supported in current driver \n");
					 return 0;
					 } 
					 
					capture->cmos_camera=OrigCimArryPtr+prev_mode; 
					capture_ptr=capture->cmos_camera;
					DPRINTK("CAPTURE: Camera Array Index # %d \n", prev_mode);
					DPRINTK("CAPTURE:Picture taken in **%s** Mode \n", capture_ptr->camera_mode);
					Capture_Image();
					DPRINTK("CAPTURE:Status Reg %x Capture Reg %x \n", au1200_cim->stat, au1200_cim->capture);
					DPRINTK("Waiting for %d DMA Interrupt \n",capture_ptr->dbdma_channel);
                	while ( (nInterruptDoneNumber != (capture_ptr->dbdma_channel)) && (!ciminterruptcheck) );

                 	if(ciminterruptcheck)
				 	{
				   		printk(" !! ERROR: DMA Transfer Error  \n");
				   		ciminterruptcheck = 0;
				   		return 0; 
				    }
								
                	DPRINTK("CAPTURE:Putting back descriptor back to ring\n");
					for (i=0; i< capture_ptr->dbdma_channel; i++)
					{
						if ( ! au1xxx_dbdma_put_dest((u32)( capture->ChannelArray[i]),  
							capture->memory[i],  capture->nTransferSize[i]) )
						{
							printk("DBDMA Error..Putting Descriptor on Buffer Ring Channel A in Single Channel \n");
					   	}
					}
					DPRINTK(" CAPTURE: Exiting Capture \n");  
				
					return 1;
                    break;        
		   	} 

	}
}

static int 
au1xxxcim_mmap(struct file* file, struct vm_area_struct *vma)
{
	unsigned int len;
	unsigned long start=0, off;

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT)) 
        {
		printk(" Error vma->vm_pgoff > !OUL PAGE_SHIFT \n");
		 return -EINVAL;
		}

	start = virt_to_phys(mem_buf) & PAGE_MASK;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + 2*MAX_FRAME_SIZE);
	off = vma->vm_pgoff << PAGE_SHIFT;
	
	if ((vma->vm_end - vma->vm_start + off) > len) 
	 {
		printk(" Error vma->vm_end-vma->vm_start\n");
		return -EINVAL;
	 }
	 
	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;
	pgprot_val(vma->vm_page_prot) &= ~_CACHE_MASK;
	pgprot_val(vma->vm_page_prot) |= PAGE_CACHABLE_DEFAULT;
	
	/* This is an IO map - tell maydump to skip this VMA */
	vma->vm_flags |= VM_IO;
	
	//[FALinux ] 원본 내용
	//if (io_remap_page_range(vma,vma->vm_start, off, vma->vm_end - vma->vm_start, vma->vm_page_prot)) 

	//[FALinux ] 수정한 내용. 2.6.21에서는 io_remap_page_range() 대신 io_remap_pfn_range() 을 사용 해야 한다.   
	if (io_remap_pfn_range(vma,vma->vm_start, off, vma->vm_end - vma->vm_start, vma->vm_page_prot)) 
	{
		return -EAGAIN;
	}
	
	return 0;


}

static int
au1xxxcim_release(struct inode *inode, struct file *file)
{
	DPRINTK("FOPS Release Function!\n");
	return 0;
}

static struct file_operations au1xxxcim_fops =
{

	owner:		THIS_MODULE,
	open:		au1xxxcim_open,
	ioctl:		au1xxxcim_ioctl,
	mmap:		au1xxxcim_mmap,
	release:	au1xxxcim_release,
	
};

int __init
au1xxxcim_init(void)
{
	int retval, error;
	unsigned long page;
	CAMERA_RUNTIME  *cam_init;
	CAMERA *cim_ptr; 

	cam_init=pcam_base;
	cam_init->cmos_camera=OrigCimArryPtr+prev_mode;
	cim_ptr=cam_init->cmos_camera;

	/*Allocating memory for MMAP*/ 
	mem_buf=(unsigned long*)Camera_mem_alloc(2*MAX_FRAME_SIZE);
	if(mem_buf==NULL){printk("MMAP unable to allocate memory \n");}	
	
	for (page = (unsigned long)mem_buf; page < PAGE_ALIGN((unsigned long)mem_buf + MAX_FRAME_SIZE);page += PAGE_SIZE) 
 	{
		SetPageReserved(virt_to_page(page));
 	}
    Camera_pwr_up();
	error=Camera_Config(cam_init);
	if ( error ==-1 )
	{
		DPRINTK("Camera Config Un-Sucessful: Calling Clean Up function \n");
		CIM_Cleanup(cam_init); 
	}
	
	Camera_pwr_down();
	mdelay(1);
	Camera_pwr_up();

	/* Register Device*/
	retval = register_chrdev(CIM_MAJOR, CIM_NAME, &au1xxxcim_fops);
	if ( retval < 0 )
	{
		printk("\n Could not register CIM device\n");
		return 0;
	}
   	printk("Au1XXX CIM driver registered Sucessfully v%s\n", VERSION);
   	if ( (retval=request_irq(AU1200_CAMERA_INT ,Cim_Interrupt_Handler, SA_SHIRQ | SA_INTERRUPT, CIM_NAME, au1200_cim )) )
	{
		printk("CIM: Could not get IRQ - 1");
		CIM_Cleanup(cam_init);  
		return retval;
	}
	
	return 0;

} 

void __exit
au1xxxcim_exit(void)
{
	int retval;
	CAMERA_RUNTIME *cam_exit;
	CAMERA *cam_exit_ptr; 

	cam_exit=pcam_base; 
	cam_exit->cmos_camera=OrigCimArryPtr+prev_mode; 
	cam_exit_ptr=cam_exit->cmos_camera; 

	/* Cleanup funtion will clean allocated memory, free DMA channels*/ 
	CIM_Cleanup(cam_exit);

	/*Unregister Device*/
 	retval = unregister_chrdev(CIM_MAJOR, CIM_NAME);
	if ( retval != -EINVAL )
	{
		printk("CIM driver unregistered Sucessfully \n");
	}
} 


module_init(au1xxxcim_init);
module_exit(au1xxxcim_exit);

MODULE_AUTHOR("AMD");
MODULE_DESCRIPTION("AMD CIM Interface Driver");
MODULE_LICENSE("GPL");

