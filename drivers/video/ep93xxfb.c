#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/reboot.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "ep93xxfb.h"
#include "cx25871.h"
#include <asm/hardware.h>
#include <linux/platform_device.h>

#include "console/fbcon.h"
//extern int pls_proba;

#if defined(CONFIG_MACH_EDB9312) || defined(CONFIG_MACH_EDB9315) || defined(CONFIG_MACH_EDB9307) || defined(CONFIG_MACH_EDB9301) || defined(CONFIG_MACH_EDB9302)
#define CONFIG_EP93XX_SDCS3
#else
#define CONFIG_EP93XX_SDCS0
#endif

//#define DEBUG 
#ifdef DEBUG
#define DPRINTK( fmt, arg... )  printk( fmt, ##arg )
#else
#define DPRINTK( fmt, arg... )
#endif

#define FBDEV_NAME "ep93xxfb"

#define ep93xxfb_outl(value, reg)              \
{                                              \
    outl(RASTER_SWLOCK_VALUE, RASTER_SWLOCK);  \
    outl(value, reg);                          \
}

#if defined(CONFIG_FB_EP93XX_CRT)
#define DEFAULT_MODE	1
#define DEFAULT_OUT     CRT_OUT
#elif defined(CONFIG_FB_EP93XX_LCD)
#define DEFAULT_MODE 	0
#define DEFAULT_OUT     LCD_OUT
#elif defined(CONFIG_FB_EP93XX_PAL)
#define DEFAULT_MODE 	12
#define DEFAULT_OUT     TV_OUT
#elif defined(CONFIG_FB_EP93XX_NTSC)
#define DEFAULT_MODE 	0
#define DEFAULT_OUT     TV_OUT
#else
#define DEFAULT_MODE 	0
#define DEFAULT_OUT     LCD_OUT
#endif

#if defined(CONFIG_FB_EP93XX_8BPP)
#define DEFAULT_BPP  8
#elif defined(CONFIG_FB_EP93XX_16BPP)
#define DEFAULT_BPP 	16
#else
#define DEFAULT_BPP 	16
#endif

static DECLARE_WAIT_QUEUE_HEAD(ep93xxfb_wait_in);

static unsigned int pseudo_palette[256];
static unsigned long *cursor_data = NULL;
static struct ep93xxfb_info epinfo;

static int vout	 = 1;
static int vmode = 0;
static int depth = 16;

// [FALINUX]-----------------------------------------------------------------------------
#define FILP_AREA_MAX        4

typedef struct
{
    int count;
    int mmap_size;
    unsigned long offsets[FILP_AREA_MAX];
} flip_info_t;

typedef struct
{
    unsigned long offset;
    int           wait;
} flip_set_visual_addr_t;


#define FB_IOCTL_FLIP_GET_INFO        _IOR('G', 0x01, flip_info_t )
#define FB_IOCTL_FLIP_SET_ADDR        _IOW('G', 0x02, flip_set_visual_addr_t )

static flip_info_t flip_info = { 2, MAX_FBMEM_SIZE, { 0, (MAX_FBMEM_SIZE)/2 } } ;
// [FALINUX]-----------------------------------------------------------------------------


static int ep93xxfb_setcol(struct fb_info *info, int bpp);

struct cx25871_vmodes cxmods[CXMODES_COUNT]=
{
    { "CX25871-NTSC", 0x10, 640, 400, 936, 525, 259, 76, 29454552 },    // 0
    { "CX25871-NTSC", 0x00, 640, 480, 784, 600, 126, 75, 28195793 },    // 1
    { "CX25871-NTSC", 0x40, 640, 480, 770, 645, 113, 100, 29769241 },   // 2
    { "CX25871-NTSC", 0x30, 720, 400, 1053, 525, 291, 76, 33136345 },   // 3
    { "CX25871-NTSC", 0x54, 720, 480, 880, 525, 140, 36, 27692310 },    // 4
    { "CX25871-NTSC", 0x02, 800, 600, 880, 735,  66, 86, 38769241 },    // 5
    { "CX25871-NTSC", 0x22, 800, 600, 1176, 750, 329, 94, 52867138 },   // 6
    { "CX25871-NTSC", 0x42, 800, 600, 1170, 805, 323, 125, 56454552 },  // 7
    { "CX25871-NTSC", 0x50, 800, 600, 1170, 770, 323, 105, 54000000 },  // 8
    { "CX25871-NTSC", 0x12, 1024, 768, 1176, 975, 133, 130,68727276 },  // 9
    { "CX25871-NTSC", 0x32, 1024, 768, 1170, 945, 127, 115, 66272724 }, // 10
    { "CX25871-NTSC", 0x52, 1024, 768, 1170, 1015, 127, 150, 71181793 },// 11
	             	                        
    { "CX25871-PAL",  0x11, 640, 400, 1160, 500, 363, 64, 28999992 },   // 12
    { "CX25871-PAL",  0x01, 640, 480, 944, 625, 266, 90, 29500008 },    // 13
    { "CX25871-PAL",  0x21, 640, 480, 950, 600, 271,76 , 28500011 },    // 14
    { "CX25871-PAL",  0x41, 640, 480, 950, 650, 271, 104, 30875015 },   // 15
    { "CX25871-PAL-M", 0x56, 640, 480, 784, 600, 126, 75, 28195793 },   // 16
    { "CX25871-PAL-Nc",  0x57, 640, 480, 944, 625, 266, 90, 29500008 }, // 17 
    { "CX25871-PAL",  0x31, 720, 400, 1305, 500, 411, 64, 32625000 },   // 18
    { "CX25871-PAL",  0x03, 800, 600, 960, 750, 140, 95, 36000000 },    // 19 
    { "CX25871-PAL",  0x23, 800, 600, 950, 775, 131, 109, 36812508 },   // 20
    { "CX25871-PAL",  0x43, 800, 600, 950, 800, 131, 122, 37999992 },   // 21
    { "CX25871-PAL",  0x13, 1024, 768, 1400, 975, 329, 131, 68249989 }, // 22
    { "CX25871-PAL",  0x53, 1024, 768, 1410, 1000, 337, 147, 70499989 },// 23
};

static int cx25871_reboot(struct notifier_block *this, unsigned long event, void *x)
{
    switch (event)
    {
	case SYS_HALT:
	case SYS_POWER_OFF:
	case SYS_RESTART:
	        write_reg(0xba, 0x80);
	default:
	    return NOTIFY_DONE;
    }
}

static struct notifier_block cx25871_notifier = 
{
    cx25871_reboot,
    NULL,
    5
};
											    													
int write_reg(unsigned char ucRegAddr, unsigned char ucRegValue)
{
    unsigned long uiVal, uiDDR;
    unsigned char ucData, ucIdx, ucBit;
    unsigned long ulTimeout;

    uiVal = inl(GPIO_PGDR);
    uiDDR = inl(GPIO_PGDDR);
    uiVal |= (GPIOG_EEDAT | GPIOG_EECLK);
    outl( uiVal, GPIO_PGDR );
    udelay( EE_DELAY_USEC );
    uiDDR |= (GPIOG_EEDAT | GPIOG_EECLK);
    outl( uiDDR, GPIO_PGDDR );
    udelay( EE_DELAY_USEC );
    uiVal &= ~GPIOG_EEDAT;
    outl( uiVal, GPIO_PGDR );
    udelay( EE_DELAY_USEC );
    uiVal &= ~GPIOG_EECLK;
    outl( uiVal, GPIO_PGDR );
    udelay( EE_DELAY_USEC );

    for (ucIdx = 0; ucIdx < 3; ucIdx++) {
	if (ucIdx == 0)
	    ucData = (unsigned char)CX25871_DEV_ADDRESS;
	else if (ucIdx == 1)
	    ucData = ucRegAddr;
	else
	    ucData = ucRegValue;

        for (ucBit = 0; ucBit < 8; ucBit++) {
	    if (ucData & 0x80)
		uiVal |= GPIOG_EEDAT;
	    else
		uiVal &= ~GPIOG_EEDAT;
	    
	    outl( uiVal, GPIO_PGDR );
	    udelay( EE_DELAY_USEC );
	    outl( (uiVal | GPIOG_EECLK), GPIO_PGDR );
	    udelay( EE_DELAY_USEC );
	    outl( uiVal, GPIO_PGDR );
	    udelay( EE_DELAY_USEC );
	    ucData <<= 1;
	}

	uiDDR &= ~GPIOG_EEDAT;
	outl( uiDDR, GPIO_PGDDR );
	udelay( EE_DELAY_USEC );
	outl( (uiVal | GPIOG_EECLK), GPIO_PGDR );
	udelay( EE_DELAY_USEC );
	ulTimeout = 0;
	while ( inl(GPIO_PGDR) & GPIOG_EEDAT ) {
	    udelay( EE_DELAY_USEC );
	    ulTimeout++;
	    if (ulTimeout > EE_READ_TIMEOUT )
		return 1;
	}

	outl( uiVal, GPIO_PGDR );
	udelay( EE_DELAY_USEC );
	uiDDR |= GPIOG_EEDAT;
	outl( uiDDR, GPIO_PGDDR );
	udelay( EE_DELAY_USEC );
    }

    uiVal &= ~GPIOG_EEDAT;
    outl( uiVal, GPIO_PGDR );
    udelay( EE_DELAY_USEC );
    uiVal |= GPIOG_EECLK;
    outl( uiVal, GPIO_PGDR );
    udelay( EE_DELAY_USEC );
    uiVal |= GPIOG_EEDAT;
    outl( uiVal, GPIO_PGDR );
    udelay( EE_DELAY_USEC );
    return 0;
}

void cx25871_on( unsigned char value )
{
    write_reg(0x30,0x00);
    write_reg(0xBA, 0x20 );
}

void cx25871_off( unsigned char value )
{
    write_reg(0xBA, 0x30);
    write_reg(0xC4, 0x00);
    write_reg(0x30, 0x02);
}

void cx25871_config( unsigned char value )
{
    write_reg(0xBA, CX25871_REGxBA_SRESET);
    mdelay(1);
    write_reg(0xB8, value);
    write_reg(0xBA, CX25871_REGxBA_SLAVER | CX25871_REGxBA_DACOFF);
    write_reg(0xC6,(CX25871_REGxC6_INMODE_MASK & 0x3));
    write_reg(0xC4, CX25871_REGxC4_EN_OUT);
    write_reg(0xBA, CX25871_REGxBA_SLAVER );
}
/*
 * { LCD_NAME, HRes, HFrontPorch, HSyncWidth, HBackPorch, VRes, VFrontPorch, VBackPorch, VSyncWidth }
 */
static struct ep93xxfb_videomodes ep93xxfb_vmods[] = {
	{
		"Philips-LB064V02-640x480x60",
		640, 24, 96,  40, 480, 10, 2, 33, 60,
		CLK_INTERNAL, EDGE_FALLING, POL_LOW, POL_LOW, POL_LOW,
	},
	{ // 1
		"Philips-LB104S5C1-800x600x60",
		800, 30, 12,  30, 600,  3, 1,  2, 60,
		CLK_INTERNAL, EDGE_FALLING, POL_LOW, POL_LOW, POL_LOW,
	},
	{ // 2
		"Philips-LB150X06A4-1024x768x60",
		1024, 24, 8,  40, 768,  3, 2, 29, 60,
		CLK_INTERNAL, EDGE_FALLING, POL_LOW, POL_LOW, POL_LOW,
	},
	{ // 3
		"CRT-640x480-60",
		640, 24, 96, 160, 480, 11, 2, 32, 60,
		CLK_INTERNAL, EDGE_RISING, POL_LOW, POL_LOW, POL_LOW,
	},
	{ // 4
		"CRT-640x480-72",
		640, 40, 40, 144, 480, 8, 3, 30, 72,
		CLK_INTERNAL, EDGE_RISING, POL_LOW, POL_LOW, POL_LOW,
	},
	{ // 5
		"CRT-640x480-75",
		640, 16, 76, 120, 480, 1, 3, 16, 75,
		CLK_INTERNAL, EDGE_RISING, POL_LOW, POL_LOW, POL_LOW,
	},
	{ // 6
		"CRT-640x480-85",
		640, 56, 56, 80, 480, 1, 3, 25, 85,
		CLK_INTERNAL, EDGE_RISING, POL_LOW, POL_LOW, POL_LOW,
	},
	{ // 7
		"CTR-640x480-100",
		640, 32, 96, 96, 480, 8, 6, 36, 100,
		CLK_INTERNAL, EDGE_RISING, POL_LOW, POL_LOW, POL_LOW,
	},
	{ // 8
		"CRT-800x600-56",
		800, 24, 72, 128, 600, 1, 2, 22, 56,
		CLK_INTERNAL, EDGE_RISING, POL_LOW, POL_LOW, POL_LOW,
	},
	{ // 9
		"CRT-800x600-60",
		800, 40, 128, 88, 600, 1, 4, 23, 60,
		CLK_INTERNAL, EDGE_RISING, POL_LOW, POL_HIGH, POL_HIGH,
	},
	{ // 10
		"CRT-800x600-72",
		800, 56, 120, 64, 600, 37, 6, 23, 72,
		CLK_INTERNAL, EDGE_RISING, POL_LOW, POL_LOW, POL_LOW,
	},
	{ // 11
		"CRT-800x600-85",
		800, 64, 64, 160, 600, 16, 5, 36, 85,
		CLK_INTERNAL, EDGE_RISING, POL_LOW, POL_LOW, POL_LOW,
	},
	{ // 12
		"CRT-800x600-100",
		800, 64, 64, 160, 600, 4, 6, 30, 100,
		CLK_INTERNAL, EDGE_RISING, POL_LOW, POL_LOW, POL_LOW,
	},
	{ // 13
		"CRT-1024x768-60",
		1024, 8, 144, 168, 768, 3, 6, 29, 60,
		CLK_INTERNAL, EDGE_RISING, POL_LOW, POL_LOW, POL_LOW,
	},
	{ // 14 
		"CRT-1024x768-70",   
		1024, 24, 136, 144, 768, 3, 6, 29, 70,
		CLK_INTERNAL, EDGE_RISING, POL_LOW, POL_LOW, POL_LOW,
	},
	{ // 15
		"CRT-1024x768-75",
		1024, 16, 96, 176, 768, 1, 3, 28, 75,
		CLK_INTERNAL, EDGE_RISING, POL_LOW, POL_HIGH, POL_HIGH,
	},
	{ // 16
		"CRT-1024x768-80",
		1024, 113, 96, 90, 768, 40, 6, 29, 80,
		0x0000E202, EDGE_RISING, POL_LOW, POL_LOW, POL_LOW,
	},
	{ // 17
		"CRT-1024x768-85",
		1024, 48, 96, 208, 768, 1, 3, 36, 85,
		CLK_INTERNAL, EDGE_RISING, POL_LOW, POL_HIGH, POL_HIGH,
	},
	{ // 18
		"CRT-1280x720-60",
		1280, 48, 112, 248, 720, 1, 3, 38, 60,
		CLK_INTERNAL, EDGE_RISING, POL_LOW, POL_HIGH, POL_HIGH,
	}
};

static void philips_lb064v02_on(unsigned char value)
{
    DPRINTK("philips_lb064v02_on \n");
    outl(inl(GPIO_PADDR) | 2, GPIO_PADDR);
    outl(inl(GPIO_PADR) | 2, GPIO_PADR);
}
    
static void philips_lb064v02_off(unsigned char value)
{
    DPRINTK("philips_lb064v02_off \n");
    outl(inl(GPIO_PADR) & ~2, GPIO_PADR);
}
	    
static void falinux_on(unsigned char value)
{
	DPRINTK("falinux lcd backlight_on \n");
    outl(inl(GPIO_PBDDR) |  0x10, GPIO_PBDDR);
    outl(inl(GPIO_PBDR)  & ~0x10, GPIO_PBDR);
}
    
static void falinux_off(unsigned char value)
{
    DPRINTK("falinux lcd backlight_off \n");
    outl(inl(GPIO_PBDR)  |  0x10, GPIO_PBDR);
}

static irqreturn_t ep93xxfb_irq_handler(int i, void *blah)
{
    outl(0x00000000, BLOCKCTRL);
    wake_up(&ep93xxfb_wait_in);
    return IRQ_HANDLED;
}

static void ep93xxfb_wait(void)
{
    DECLARE_WAITQUEUE(wait, current);
    add_wait_queue(&ep93xxfb_wait_in, &wait);
    set_current_state(TASK_UNINTERRUPTIBLE);
	    
    while (inl(BLOCKCTRL) & 0x00000001){
	if(/*(pls_proba==1)&&*/(!in_atomic()))
	    schedule();
    }			

    remove_wait_queue(&ep93xxfb_wait_in, &wait);
    set_current_state(TASK_RUNNING);
				
}

void ep93xxfb_fillrect(struct fb_info *p, const struct fb_fillrect *fill)
{
    unsigned long blkdestwidth,tmp;

    if (!fill->width || !fill->height || 
	(fill->dx >= p->var.xres) || 
	(fill->dy >= p->var.yres) ||
        ((fill->dx + fill->width - 1) >= p->var.xres) ||
        ((fill->dy + fill->height - 1) >= p->var.yres))
        return;

    tmp =  (( fill->dx + fill->width ) * epinfo.bpp );
    blkdestwidth = tmp / 32;
    if(blkdestwidth > 0 && (tmp % 32 == 0))
	blkdestwidth--;
    blkdestwidth = blkdestwidth - (fill->dx * epinfo.bpp) / 32;
		       
    outl(fill->color, BLOCKMASK);
    outl( ((fill->dx * epinfo.bpp) & 0x1F) |
	  ((((fill->dx + fill->width - 1) * epinfo.bpp ) & 0x1F) << 16),
	  DESTPIXELSTRT);
    outl( ((epinfo.xres * epinfo.bpp) / 32), DESTLINELENGTH);
    outl( blkdestwidth, BLKDESTWIDTH );
    outl(fill->height - 1, BLKDESTHEIGHT);
    outl((epinfo.fb_phys + (fill->dy * epinfo.xres * epinfo.bpp ) / 8 +
	 (fill->dx * epinfo.bpp ) / 8 )
	 , BLKDSTSTRT);
    outl( epinfo.pixformat | 0x0000000B, BLOCKCTRL);
    ep93xxfb_wait();
											         
}

void ep93xxfb_copyarea(struct fb_info *p, const struct fb_copyarea *area) 
{
    unsigned long startsx,stopsx,startdx,stopdx,startsy,startdy;
    unsigned long blksrcwidth,blkdestwidth,tmp;
    unsigned long val = 0;

    if( !area->width || !area->width || 
		(area->sx >= p->var.xres) || (area->sy >= p->var.yres) ||
        (area->dx >= p->var.xres) || (area->dy >= p->var.yres) ||
        ((area->dx + area->width  - 1) >= p->var.xres) ||
        ((area->dy + area->height - 1) >= p->var.yres))
	return;
    
    if(area->sx == area->dx && area->sy == area->dy)
	return;

    if ((area->dy == area->sy) && (area->dx > area->sx) &&
		(area->dx < (area->sx + area->width - 1))) {
		startdx   = area->dx + area->width - 1;
        stopdx	  = area->dx;
        startsx   = area->sx + area->width - 1;
        stopsx	  = area->sx;
        val 	 |= 0x000000A0;
    }
    else {
		startdx   = area->dx;
        stopdx 	  = area->dx + area->width - 1;
        startsx   = area->sx;
        stopsx 	  = area->sx + area->width - 1;
    }

    if (area->dy <= area->sy) {
        startdy   = area->dy;
        startsy	  = area->sy;
    }
    else {
		startdy   = area->dy + area->height -1;
		startsy   = area->sy + area->height -1;
		val 	 |= 0x00000140;
    }
    
    tmp =  (( area->sx + area->width ) * epinfo.bpp );
    blksrcwidth = tmp / 32;
    if(blksrcwidth > 0 && (tmp % 32 == 0))
	blksrcwidth--;
    blksrcwidth = blksrcwidth - (area->sx * epinfo.bpp) / 32;				           

    tmp =  (( area->dx + area->width ) * epinfo.bpp );
    blkdestwidth = tmp / 32;
    if(blkdestwidth > 0 && (tmp % 32 == 0))
        blkdestwidth--;
    blkdestwidth = blkdestwidth - (area->dx * epinfo.bpp) / 32;
			    
    outl( 0x00000000 , BLOCKCTRL);

    /*** src ***/
    outl((((startsx * epinfo.bpp) & 0x1F) |
         (((stopsx * epinfo.bpp ) & 0x1F) << 16))
	 , SRCPIXELSTRT);	
    outl((epinfo.fb_phys + (startsy * epinfo.xres * epinfo.bpp ) / 8 +
         (startsx * epinfo.bpp ) / 8 )
	 , BLKSRCSTRT);
    outl(((epinfo.xres * epinfo.bpp) / 32), SRCLINELENGTH);
    outl( blksrcwidth, BLKSRCWIDTH );
	    
    /*** dest ***/
    outl((((startdx * epinfo.bpp) & 0x1F) | 
	 (((stopdx * epinfo.bpp ) & 0x1F) << 16))
	 , DESTPIXELSTRT);
    outl((epinfo.fb_phys + (startdy * epinfo.xres * epinfo.bpp ) / 8 +
         (startdx * epinfo.bpp ) / 8 )
	 , BLKDSTSTRT);
    outl( ((epinfo.xres * epinfo.bpp) / 32), DESTLINELENGTH);
    outl( blkdestwidth, BLKDESTWIDTH);
    outl( area->height - 1 , BLKDESTHEIGHT);
    outl( epinfo.pixformat | val | 0x00000003, BLOCKCTRL);
    ep93xxfb_wait();
}
    
void ep93xxfb_imageblit(struct fb_info *p, const struct fb_image *image) 
{
//    unsigned long blkdestwidth,tmp;
//    void * pucBlitBuf;
    cfb_imageblit( p , image );
    return;
/*    
    if ((image->dx >= p->var.xres) ||
        (image->dy >= p->var.yres) ||
        ((image->dx + image->width - 1) >= p->var.xres) ||
        ((image->dy + image->height - 1) >= p->var.yres))
        return;
    if (epinfo.bpp != image->depth )
	return;	    

    tmp =  (( image->dx + image->width ) * epinfo.bpp );
    blkdestwidth = tmp / 32;
    if(blkdestwidth > 0 && (tmp % 32 == 0))
	blkdestwidth--;
    blkdestwidth = blkdestwidth - (image->dx * epinfo.bpp) / 32;

    pucBlitBuf = kmalloc(1024*8,GFP_KERNEL);    
    copy_from_user(pucBlitBuf, image->data, 5000);

    outl( 0x00000000 , BLOCKCTRL);
    
    outl( 0x00000000, SRCPIXELSTRT);
    outl( virt_to_phys(pucBlitBuf), BLKSRCSTRT);
    outl( (image->width * epinfo.bpp) / 32 , SRCLINELENGTH);
    outl(((image->width - 1) * epinfo.bpp) / 32, BLKSRCWIDTH );
    						      
    outl(((image->dx * epinfo.bpp) & 0x1F) |
         ((((image->dx + image->width - 1) * epinfo.bpp ) & 0x1F) << 16)
         , DESTPIXELSTRT);
    outl((epinfo.fb_phys + (image->dy * epinfo.xres * epinfo.bpp ) / 8 +
         (image->dx * epinfo.bpp ) / 8 )
         , BLKDSTSTRT);
    outl( ((epinfo.xres * epinfo.bpp) / 32), DESTLINELENGTH );
    outl( blkdestwidth, BLKDESTWIDTH );
    outl( image->height - 1 , BLKDESTHEIGHT);
    outl(image->fg_color, BLOCKMASK);
    outl(image->bg_color, BACKGROUND);
    outl( epinfo.pixformat | 0x00000003, BLOCKCTRL );
    ep93xxfb_wait();
*/
}


static unsigned long isqrt(unsigned long a)
{
    unsigned long rem = 0;
    unsigned long root = 0;
    int i;
			
    for (i = 0; i < 16; i++) {
        root <<= 1;
        rem = ((rem << 2) + (a >> 30));
        a <<= 2;
        root++;
        if (root <= rem) {
            rem -= root;
            root++;
        }
	else
            root--;
    }
    return root >> 1;
}
																											
int ep93xxfb_line(struct fb_info *info,  struct ep93xx_line *line)
{
    unsigned long value = 0;
    long x, y, dx, dy, count, xinc, yinc, xval, yval, incr;
    
    if ((line->x1 > info->var.xres) ||
        (line->x2 > info->var.xres) ||
        (line->y1 > info->var.yres) ||
        (line->y2 > info->var.yres))
	return -EFAULT;	
    x = line->x1;
    y = line->y1;
    dx = line->x2 - line->x1;
    dy = line->y2 - line->y1;

    if ( !dx || !dy )
    	return -EFAULT;

    if ( dx < 0 && dy < 0 ) {
	x = line->x2;
	y = line->y2;
	dx *= -1;
	dy *= -1;
    }
    else if ( dx < 0 && dy > 0 ){
        dx *= -1;
	value = 0x000000A0;
    }
    else if( dy < 0 && dx > 0 ){
	dy *= -1;
        value = 0x00000140;
    }
	
    if (line->flags & LINE_PRECISE) {
	count = isqrt(((dy * dy) + (dx * dx)) * 4096);
	xinc = (4095 * 64 * dx) / count;
	yinc = (4095 * 64 * dy) / count;
	xval = 2048;
	yval = 2048;
	count = 0;
	while (dx || dy) {
	    incr = 0;
	    xval -= xinc;
	    if (xval <= 0) {
		xval += 4096;
		dx--;
		incr = 1;
	    }
	    yval -= yinc;
	    if (yval <= 0) {
		yval += 4096;
		dy--;
		incr = 1;
	    }
	    count += incr;
	}
    }
    else {
        if ( dx == dy ) {
            xinc = 4095;
            yinc = 4095;
            count = dx;
			    
        }
	else if ( dx < dy ) {
            xinc = ( dx * 4095 ) / dy;
            yinc = 4095;
	    count = dy;
				    
	}
	else {
            xinc = 4095;
            yinc = ( dy * 4095 ) / dx;
            count = dx;
        }
    }
                                                                                                                         
    outl(0x08000800, LINEINIT);
    if (line->flags & LINE_PATTERN)
        outl(line->pattern, LINEPATTRN);
    else
        outl(0x000fffff, LINEPATTRN);
    outl(epinfo.fb_phys + ((y * epinfo.xres * epinfo.bpp) / 8) +
         ((x * epinfo.bpp ) / 8 ), BLKDSTSTRT);

    outl(((x * epinfo.bpp) & 0x1F) |
         ((((x + dx - 1) * epinfo.bpp) & 0x1F ) << 16),
	  DESTPIXELSTRT);
    outl((epinfo.xres * epinfo.bpp) / 32, DESTLINELENGTH);
    outl(line->fgcolor, BLOCKMASK);
    outl(line->bgcolor, BACKGROUND);
    outl((yinc << 16) | xinc, LINEINC);
    outl( count & 0xFFF, BLKDESTWIDTH);
    outl( 0  , BLKDESTHEIGHT);
    value |= (line->flags & LINE_BACKGROUND) ? 0x00004000 : 0;
    outl(epinfo.pixformat | value | 0x00000013, BLOCKCTRL);
    ep93xxfb_wait();

    return 0;                                                                                                                    
}

int ioctl_cursor=0;

int ep93xxfb_cursor(struct fb_info *info, struct ep93xx_cursor *cursor)
{
    unsigned long x,y,save;

    if((cursor->width ==0)  || (cursor->height ==0) )
    {
		struct fb_cursor *fbcon_cursor =(struct fb_cursor *)cursor;
		struct fbcon_ops *ops = (struct fbcon_ops *)info->fbcon_par;
		unsigned int scan_align = info->pixmap.scan_align - 1;
		unsigned int buf_align = info->pixmap.buf_align - 1;
		unsigned int i, size, dsize, s_pitch, d_pitch;
		struct fb_image *image;
		u8 *src, *dst;
    	
		if(ioctl_cursor==1 ){
			DPRINTK("softcursor error return\n");
			return 0;
		}
    	
		if (info->state != FBINFO_STATE_RUNNING)
		        return 0;
		
		s_pitch = (fbcon_cursor->image.width + 7) >> 3;
		dsize = s_pitch * fbcon_cursor->image.height;
		
		if (dsize + sizeof(struct fb_image) != ops->cursor_size) {
		        if (ops->cursor_src != NULL)
		                kfree(ops->cursor_src);
		        ops->cursor_size = dsize + sizeof(struct fb_image);
		
		        ops->cursor_src = kmalloc(ops->cursor_size, GFP_ATOMIC);
		        if (!ops->cursor_src) {
		                ops->cursor_size = 0;
		                return -ENOMEM;
		        }
		}
		src = ops->cursor_src + sizeof(struct fb_image);
		image = (struct fb_image *)ops->cursor_src;
		*image = fbcon_cursor->image;
		d_pitch = (s_pitch + scan_align) & ~scan_align;
		
		size = d_pitch * image->height + buf_align;
		size &= ~buf_align;
		dst = fb_get_buffer_offset(info, &info->pixmap, size);
		
		if (fbcon_cursor->enable) {
		        switch (fbcon_cursor->rop) {
		        case ROP_XOR:
		                for (i = 0; i < dsize; i++)
		                        src[i] = image->data[i] ^ fbcon_cursor->mask[i];
		                break;
		        case ROP_COPY:
		        default:
		                for (i = 0; i < dsize; i++)
		                        src[i] = image->data[i] & fbcon_cursor->mask[i];
		                break;
		        }
		} else
		        memcpy(src, image->data, dsize);
		
		fb_pad_aligned_buffer(dst, d_pitch, src, s_pitch, image->height);
		image->data = dst;
		info->fbops->fb_imageblit(info, image);
		return 0;
	
    }
    else{
		ioctl_cursor = 1;
    	
    	/*if (cursor->width > 16 || cursor->height > 16){
		DPRINTK("%s width %d or heright %d error\n",__FUNCTION__,cursor->width,cursor->height);
		return -ENXIO;
    	}*/
    
    	if (cursor->flags & CURSOR_OFF)
        	outl(inl(CURSORXYLOC) & ~0x00008000, CURSORXYLOC);
	
    	if (cursor->flags & CURSOR_SETSHAPE) {
        	copy_from_user(cursor_data, cursor->data,
	    		cursor->width * cursor->height / 4);
		save = inl(CURSORXYLOC);
		outl(save & ~0x00008000, CURSORXYLOC);
	
        	outl(virt_to_phys(cursor_data), CURSOR_ADR_START);
        	outl(virt_to_phys(cursor_data), CURSOR_ADR_RESET);
		outl(((cursor->width - 1) & 0x30) << 4 | ((cursor->height - 1) << 2) |
	     		((cursor->width - 1) >> 4), CURSORSIZE);
		outl(save, CURSORXYLOC);
	
    	}
    
    	if (cursor->flags & CURSOR_SETCOLOR) {
		outl(cursor->color1, CURSORCOLOR1);
		outl(cursor->color2, CURSORCOLOR2);
		outl(cursor->blinkcolor1, CURSORBLINK1);
		outl(cursor->blinkcolor2, CURSORBLINK2);
    	}
						
    	if (cursor->flags & CURSOR_BLINK) {
        	if (cursor->blinkrate)
            		outl(0x00000100 | cursor->blinkrate, CURSORBLINK);
        	else
	    		outl(0x0000000FF, CURSORBLINK);
    	}
																						
    	if (cursor->flags & CURSOR_MOVE) {
       		x = (inl(HACTIVESTRTSTOP) & 0x000007ff) - cursor->dx - 2;
        	y = (inl(VACTIVESTRTSTOP) & 0x000007ff) - cursor->dy;
        	outl((inl(CURSORXYLOC) & 0x8000) | (y << 16) | x, CURSORXYLOC);
    	}

    	if(cursor->flags & CURSOR_ON)
		outl(inl(CURSORXYLOC) | 0x00008000, CURSORXYLOC);
    	
	return 0;
    }
}
			
static inline void 
ep93xxfb_palette_write(u_int regno, u_int red, u_int green, u_int blue, u_int trans)
{
    unsigned int cont, i, pal;
    DPRINTK("ep93xxfb_palette_write - enter\n");	   
    pal = ((red & 0xFF00) << 8) | (green & 0xFF00) | ((blue & 0xFF00) >> 8);
    pseudo_palette[regno] = pal;
    outl( pal, ( COLOR_LUT + ( regno << 2 )));
    cont = inl( LUTCONT );

    if (( cont & LUTCONT_STAT && cont & LUTCONT_RAM1 ) ||
        ( !( cont & LUTCONT_STAT ) && !( cont&LUTCONT_RAM1 ))) {
	for ( i = 0; i < 256; i++ ) {
	    outl( pseudo_palette[i], ( COLOR_LUT + ( i << 2 )) );
	}
        outl( cont ^ LUTCONT_RAM1, LUTCONT );
    }
}
 
int ep93xxfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
        
#define CNVT_TOHW(val,width) ((((val)<<(width))+0x7FFF-(val))>>16)

    switch ( info->fix.visual )
    {
	case FB_VISUAL_PSEUDOCOLOR:
	    ep93xxfb_palette_write(regno, red, green, blue, transp);
	    break;
	case FB_VISUAL_TRUECOLOR:
	    if (regno >= 16)
	        return 1;
            red = CNVT_TOHW(red, info->var.red.length);
            green = CNVT_TOHW(green, info->var.green.length);
            blue = CNVT_TOHW(blue, info->var.blue.length);
            transp = CNVT_TOHW(transp, info->var.transp.length);
            ((u32 *)(info->pseudo_palette))[regno] =
                    (red << info->var.red.offset) |
                    (green << info->var.green.offset) |
                    (blue << info->var.blue.offset) |
                    (transp << info->var.transp.offset);
      	    break;
        case FB_VISUAL_DIRECTCOLOR:
            red = CNVT_TOHW(red, 8);
	    green = CNVT_TOHW(green, 8);
	    blue = CNVT_TOHW(blue, 8);
	    transp = CNVT_TOHW(transp, 8);
	    break;
    }
#undef CNVT_TOHW
    return 0;
}

static int ep93xx_pan_display(struct fb_var_screeninfo *var,
			 struct fb_info *info)
{
    DPRINTK("ep93xx_pan_display - enter\n");
	    
    if (var->yoffset < 0 
        || var->yoffset + var->yres > info->var.yres_virtual
        || var->xoffset)
	    return -EINVAL;

    outl(epinfo.fb_phys + info->fix.line_length * var->yoffset
	 , VIDSCRNPAGE);

    info->var.xoffset = var->xoffset;
    info->var.yoffset = var->yoffset;

    DPRINTK("ep93xx_pan_display - exit\n");
    return 0;
}

static int 
ep93xxfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
    struct fb_var_screeninfo tmp_var;
    unsigned long pclk;	
    DPRINTK("ep93xxfb_check_var - enter\n");

    if( vout != 0)
        return -EINVAL;
    
    memcpy (&tmp_var, var, sizeof (tmp_var));

    if( (tmp_var.vmode & FB_VMODE_MASK) != FB_VMODE_NONINTERLACED ) {
	printk("  ep93xxfb_check_var - unsupported video mode\n");
	return -EINVAL;
    } 
    
    if( ((tmp_var.xres * tmp_var.yres * tmp_var.bits_per_pixel) / 8) > 
	  MAX_FBMEM_SIZE ) {
	printk("  ep93xxfb_check_var - memory error \n");
	return -ENOMEM;
    }
 
    if( ((tmp_var.xres_virtual * tmp_var.yres_virtual * tmp_var.bits_per_pixel) / 8) >
          MAX_FBMEM_SIZE ) {
	printk("  ep93xxfb_check_var - memory error \n");
        return -ENOMEM;
    }

    pclk = 1000 * (1000000000 / tmp_var.pixclock);
    
    if( pclk > ep93xx_get_max_video_clk() ) {
        printk("  ep93xxfb_check_var - pixel clock error %lu\n",pclk);
	return -EINVAL;
    }

    if (var->xres_virtual != var->xres)
	var->xres_virtual = var->xres;
    if (var->yres_virtual < var->yres)
        var->yres_virtual = var->yres;
				
    if (var->xoffset < 0)
        var->xoffset = 0;
    if (var->yoffset < 0)
        var->yoffset = 0;

    switch (tmp_var.bits_per_pixel) {
	case 8:
	    break;									  
        case 16:
	    break;
        case 24:
            break;
        case 32:
	    break;
	default:
	    return -EINVAL;    
    }

    DPRINTK("ep93xxfb_check_var - exit\n");
    return 0;
}
	    
	    
static int ep93xxfb_set_par(struct fb_info *info)
{
    struct fb_var_screeninfo tmp_var;
    unsigned long attribs;	

    DPRINTK("ep93xxfb_set_par - enter\n");

    if( ep93xxfb_check_var(&info->var,info) < 0 )
        return -EINVAL;
 
    if( !ep93xxfb_setcol(info,info->var.bits_per_pixel) )
	return -EINVAL;
   

	info->fix.line_length = (info->var.xres * info->var.bits_per_pixel) / 8;

    ep93xxfb_blank( 1 , info );
        
    memcpy(&tmp_var,&info->var,sizeof(tmp_var));
	
    epinfo.xres		= tmp_var.xres;
    epinfo.xsync	= tmp_var.hsync_len;
    epinfo.xfp		= tmp_var.right_margin;
    epinfo.xbp	  	= tmp_var.left_margin;
    epinfo.xtotal	= epinfo.xres + epinfo.xsync + epinfo.xfp + epinfo.xbp;
    
    epinfo.yres		= tmp_var.yres;
    epinfo.ysync	= tmp_var.vsync_len;
    epinfo.yfp		= tmp_var.lower_margin;
    epinfo.ybp		= tmp_var.upper_margin;
    epinfo.ytotal	= epinfo.yres + epinfo.ysync + epinfo.yfp + epinfo.ybp;
    
    epinfo.pixclock	= tmp_var.pixclock ;	
    epinfo.refresh	= 1000 * (1000000000 / tmp_var.pixclock) ;
    
    if( epinfo.refresh > ep93xx_get_max_video_clk())
		epinfo.refresh	= ep93xx_get_max_video_clk();      
    epinfo.bpp		= tmp_var.bits_per_pixel;

    ep93xxfb_setclk();
    ep93xxfb_timing_signal_generation();
	
    // set video memory parameters 
    outl(epinfo.fb_phys, VIDSCRNPAGE);
    outl(epinfo.yres , SCRNLINES);
    outl(((epinfo.xres * epinfo.bpp) / 32) - 1, LINELENGTH);
    outl((epinfo.xres * epinfo.bpp)  / 32, 		VLINESTEP );

    // set pixel mode 
    ep93xxfb_pixelmod(epinfo.bpp);
	
    attribs = 0;
#ifdef CONFIG_EP93XX_SDCS0
    attribs |= 0 << VIDEOATTRIBS_SDSEL_SHIFT;
#endif
#ifdef CONFIG_EP93XX_SDCS1
    attribs |= 1 << VIDEOATTRIBS_SDSEL_SHIFT;
#endif
#ifdef CONFIG_EP93XX_SDCS2
    attribs |= 2 << VIDEOATTRIBS_SDSEL_SHIFT;
#endif
#ifdef CONFIG_EP93XX_SDCS3
    attribs |= 3 << VIDEOATTRIBS_SDSEL_SHIFT;
#endif
		    
    attribs |= VIDEOATTRIBS_INVCLK;
    if( tmp_var.sync & FB_SYNC_HOR_HIGH_ACT )
        attribs |= VIDEOATTRIBS_HSPOL;
    if( tmp_var.sync & FB_SYNC_VERT_HIGH_ACT )
        attribs |= VIDEOATTRIBS_VCPOL;

    ep93xxfb_outl(attribs, VIDEOATTRIBS);
    ep93xxfb_blank( 0 , info );

    DPRINTK("ep93xxfb_set_par - exit\n");

    return 0;
}																					       


static int ep93xxfb_blank(int blank_mode,struct fb_info *info)
{
    unsigned long attribs;
    DPRINTK("ep93xxfb_blank - enter\n");	
    attribs = inl(VIDEOATTRIBS);

#ifdef CONFIG_EP93XX_SDCS0
    attribs |= 0 << VIDEOATTRIBS_SDSEL_SHIFT;
#endif
#ifdef CONFIG_EP93XX_SDCS1
    attribs |= 1 << VIDEOATTRIBS_SDSEL_SHIFT;
#endif
#ifdef CONFIG_EP93XX_SDCS2
    attribs |= 2 << VIDEOATTRIBS_SDSEL_SHIFT;
#endif
#ifdef CONFIG_EP93XX_SDCS3
    attribs |= 3 << VIDEOATTRIBS_SDSEL_SHIFT;
#endif
        					
    if (blank_mode) {
		if (epinfo.off)
            (epinfo.off)( 0 );
        
		ep93xxfb_outl(attribs & ~(VIDEOATTRIBS_DATAEN | VIDEOATTRIBS_SYNCEN | VIDEOATTRIBS_PCLKEN | VIDEOATTRIBS_EN), VIDEOATTRIBS);
    }
    else {
		if (epinfo.clk_src == CLK_INTERNAL)
	    	attribs |=  VIDEOATTRIBS_PCLKEN;
		else
	    	attribs &= ~VIDEOATTRIBS_PCLKEN;
			
		ep93xxfb_outl(attribs | VIDEOATTRIBS_DATAEN | VIDEOATTRIBS_SYNCEN | VIDEOATTRIBS_EN, VIDEOATTRIBS);

		if (epinfo.configure)
			(epinfo.configure)( epinfo.automods );
		if (epinfo.on)
	       (epinfo.on)( 0 );
    }
    return 0;
}

static int ep93xxfb_mmap(struct fb_info *info,struct vm_area_struct *vma)
{
    unsigned long off, start, len;
    
    DPRINTK("ep93xxfb_mmap - enter\n");	       
    	       
    off = vma->vm_pgoff << PAGE_SHIFT;
    start = info->fix.smem_start;
    len = PAGE_ALIGN(start & ~PAGE_MASK) + info->fix.smem_len;
    start &= PAGE_MASK;
    if ((vma->vm_end - vma->vm_start + off) > len)
	return -EINVAL;
			      			      
    off += start;
    vma->vm_pgoff = off >> PAGE_SHIFT;
  
    vma->vm_flags |= VM_IO;
    vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

    if (io_remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT, 
	vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
	DPRINTK("ep93xxfb_mmap error\n");
	return -EAGAIN;
    }
				     
    DPRINTK("ep93xxfb_mmap - exit\n");
    return 0;
}

static unsigned long ep93xx_get_pll_frequency(unsigned long pll)
{
    unsigned long fb1, fb2, ipd, ps, freq;

    if (pll == 1)
	pll = inl(SYSCON_CLKSET1);
    else if (pll == 2)
	pll = inl(SYSCON_CLKSET2);
    else
	return 0;

    ps	= ( pll & SYSCON_CLKSET1_PLL1_PS_MASK)	   >> SYSCON_CLKSET1_PLL1_PS_SHIFT;
    fb1	= ((pll & SYSCON_CLKSET1_PLL1_X1FBD1_MASK) >> SYSCON_CLKSET1_PLL1_X1FBD1_SHIFT);
    fb2	= ((pll & SYSCON_CLKSET1_PLL1_X2FBD2_MASK) >> SYSCON_CLKSET1_PLL1_X2FBD2_SHIFT);
    ipd	= ((pll & SYSCON_CLKSET1_PLL1_X2IPD_MASK)  >> SYSCON_CLKSET1_PLL1_X2IPD_SHIFT);

    freq = (((0x00e10000 * (fb1+1)) / (ipd+1)) * (fb2+1)) >> ps;
    return freq;
}

static int ep93xx_get_max_video_clk()
{
    unsigned long f,freq = 0;
    
    freq = 14745600 / 4;
    f = ep93xx_get_pll_frequency(1) / 4;
    if ( f > freq )
	freq = f;
    f = ep93xx_get_pll_frequency(2) / 4;
    if ( f > freq )
	freq = f;
    
    return freq;
}

static int ep93xx_set_video_div(unsigned long freq)
{
    unsigned long pdiv = 0, div = 0, psel = 0, esel = 0;
    unsigned long err, f, i, j, k;

    err = -1;

    for (i = 0; i < 3; i++) {
	if (i == 0)
	    f = 14745600 * 2;
	else if (i == 1)
	    f = ep93xx_get_pll_frequency(1) * 2;
	else
	    f = ep93xx_get_pll_frequency(2) * 2;
	
	for (j = 4; j <= 6; j++) {
	    k = f / (freq * j);
	    if (k < 2)
		continue;

	    if (abs(((f / (j * k))) - freq ) < err ) {
		pdiv = j - 3;
		div = k;
		psel = (i == 2) ? 1 : 0;
		esel = (i == 0) ? 0 : 1;
		err = (f / (j * k)) - freq;
	    }
	}
    }

    if (err == -1)
	return -1;
    /*
    outl(0xaa, SYSCON_SWLOCK);
    outl(SYSCON_VIDDIV_VENA | (esel ? SYSCON_VIDDIV_ESEL : 0) |
         (psel ? SYSCON_VIDDIV_PSEL : 0) |
         (pdiv << SYSCON_VIDDIV_PDIV_SHIFT) |
         (div << SYSCON_VIDDIV_VDIV_SHIFT), SYSCON_VIDDIV);
	*/
    SysconSetLocked(SYSCON_VIDDIV,SYSCON_VIDDIV_VENA | (esel ? SYSCON_VIDDIV_ESEL : 0) |
                       (psel ? SYSCON_VIDDIV_PSEL : 0) |
                       (pdiv << SYSCON_VIDDIV_PDIV_SHIFT) |
                       (div << SYSCON_VIDDIV_VDIV_SHIFT)
                  );

    return freq + err;
}

static void ep93xxfb_pixelmod(int bpp)
{
    unsigned long tmpdata = 0;

    DPRINTK("ep93xxfb_pixelmod %dbpp -enter\n",bpp);
    switch(bpp) {
	case 8:
            tmpdata = PIXELMODE_P_8BPP | 
		    		  PIXELMODE_S_1PPCMAPPED |
                      PIXELMODE_C_LUT;
	    	epinfo.pixformat = PIXEL_FORMAT_8;
	    	break;
	case 16:
	    	tmpdata = PIXELMODE_P_16BPP | 
		    		  PIXELMODE_S_1PPCMAPPED | 
		      		  PIXELMODE_C_565;
            epinfo.pixformat = PIXEL_FORMAT_16;
		    break;
	case 24:
            tmpdata = PIXELMODE_P_24BPP | 
	              	  PIXELMODE_S_1PPC | 
		      		  PIXELMODE_C_888;
            epinfo.pixformat = PIXEL_FORMAT_24;
		    break;
	case 32:
            tmpdata = PIXELMODE_P_32BPP |
		    		  PIXELMODE_S_1PPC |
				      PIXELMODE_C_888;
            epinfo.pixformat = PIXEL_FORMAT_32;
	    	break;
	default:
	    	break;
    }
    outl(tmpdata,PIXELMODE);
}

static void ep93xxfb_timing_signal_generation(void)
{
    unsigned long vlinestotal,vsyncstart,vsyncstop,
		  vactivestart,vactivestop,
		  vblankstart,vblankstop,
		  vclkstart,vclkstop;
    
    unsigned long hclkstotal,hsyncstart,hsyncstop,
                  hactivestart,hactivestop,
		  hblankstart,hblankstop,
		  hclkstart,hclkstop;
							 
    DPRINTK("ep93xxfb_timing_signal_generation - enter\n");

    vlinestotal = epinfo.ytotal - 1; 
    vsyncstart = vlinestotal;
    vsyncstop = vlinestotal - epinfo.ysync;
    vblankstart = vlinestotal - epinfo.ysync - epinfo.ybp;
    vblankstop = epinfo.yfp - 1;
    vactivestart = vblankstart;
    vactivestop = vblankstop;
    vclkstart = vlinestotal;
    vclkstop = vlinestotal + 1;
    
    hclkstotal = epinfo.xtotal - 1;
    hsyncstart = hclkstotal;
    hsyncstop = hclkstotal - epinfo.xsync;
    hblankstart = hclkstotal - epinfo.xsync - epinfo.xbp;
    hblankstop = epinfo.xfp - 1;
    hactivestart = hblankstart;
    hactivestop = hblankstop;
    hclkstart = hclkstotal ;
    hclkstop = hclkstotal ;

    ep93xxfb_outl(0, VIDEOATTRIBS);

    ep93xxfb_outl( vlinestotal , VLINESTOTAL );
    ep93xxfb_outl( vsyncstart + (vsyncstop << 16), VSYNCSTRTSTOP );
    ep93xxfb_outl( vactivestart + (vactivestop << 16), VACTIVESTRTSTOP );
    ep93xxfb_outl( vblankstart + (vblankstop << 16), VBLANKSTRTSTOP );
    ep93xxfb_outl( vclkstart + (vclkstop << 16), VCLKSTRTSTOP );

    ep93xxfb_outl( hclkstotal , HCLKSTOTAL );
    ep93xxfb_outl( hsyncstart + (hsyncstop << 16), HSYNCSTRTSTOP );
    ep93xxfb_outl( hactivestart + (hactivestop << 16) , HACTIVESTRTSTOP );
    ep93xxfb_outl( hblankstart + (hblankstop << 16) , HBLANKSTRTSTOP );
    ep93xxfb_outl( hclkstart + (hclkstop << 16) , HCLKSTRTSTOP );
		    
    ep93xxfb_outl(0, LINECARRY);
						
}

static int ep93xxfb_setcol(struct fb_info *info, int bpp)
{

    DPRINTK("ep93xxfb_setcol %dbpp\n",bpp);
    switch(bpp) {
	case 8:
        info->var.bits_per_pixel = 8;
	    info->var.red.length = 8;
	    info->var.green.length = 8;
	    info->var.blue.length = 8;
    	info->fix.visual = FB_VISUAL_PSEUDOCOLOR;
	    break;
	case 16:
        info->var.bits_per_pixel = 16;
	    info->var.red.offset = 11;
	    info->var.red.length = 5;
	    info->var.green.offset = 5;
	    info->var.green.length = 6;
	    info->var.blue.offset = 0;
	    info->var.blue.length = 5;
    	info->fix.visual = FB_VISUAL_TRUECOLOR;
	    break;
	case 24:
	    info->var.bits_per_pixel = 24;
	    info->var.red.length = 8;
	    info->var.blue.length = 8;
	    info->var.green.length = 8;
	    info->var.red.offset = 16;
	    info->var.green.offset = 8;
	    info->var.blue.offset = 0;
	    info->fix.visual = FB_VISUAL_TRUECOLOR;
	    break;
	case 32:
        info->var.bits_per_pixel = 32;
	    info->var.red.length = 8;
	    info->var.blue.length = 8;
	    info->var.green.length = 8;
	    info->var.transp.length = 0;
	    info->var.red.offset = 16;
	    info->var.green.offset = 8;
	    info->var.blue.offset = 0;
	    info->var.transp.offset = 0;
	    info->fix.visual = FB_VISUAL_TRUECOLOR;
	    break;
	default:
	    return 0;
    }
    return 1;		
}

static int ep93xxfb_setclk()
{
    unsigned long calc_clk,act_clk;
    
    if ( epinfo.clk_src == CLK_INTERNAL ) {
		/*
		outl(0xaa, SYSCON_SWLOCK);*
    	    outl(inl(SYSCON_DEVCFG) & ~SYSCON_DEVCFG_EXVC, SYSCON_DEVCFG);
		*/
		SysconSetLocked(SYSCON_DEVCFG,inl(SYSCON_DEVCFG) & ~SYSCON_DEVCFG_EXVC);
    	calc_clk = epinfo.refresh;
    	act_clk = ep93xx_set_video_div( calc_clk );
		if ( act_clk == -1 )
    		return -ENODEV;
													       
		epinfo.refresh = act_clk;
		epinfo.pixclock = 1000000000 / (act_clk / 1000);
    }
    else {
		/*
    	    outl(0xaa, SYSCON_SWLOCK);
    	    outl(0, SYSCON_VIDDIV);
    	    outl(0xaa, SYSCON_SWLOCK);
		outl(inl(SYSCON_DEVCFG) | SYSCON_DEVCFG_EXVC, SYSCON_DEVCFG);
		*/
		SysconSetLocked(SYSCON_VIDDIV,0);
		SysconSetLocked(SYSCON_DEVCFG,inl(SYSCON_DEVCFG) | SYSCON_DEVCFG_EXVC);
    }

    return 0;
}

static void ep93xxfb_get_par(struct fb_info *info)
{
	DPRINTK("ep93xxfb_get_par - enter vout %d, vmode %d \n", vout, vmode);
    epinfo.configure = NULL;
    epinfo.on  = NULL;
    epinfo.off = NULL;
				     
    switch( vout ) {
	case LCD_OUT:
#ifdef CONFIG_FB_EP93XX_BACKLIGHT
			// BackLight ON/OFF
//			epinfo.on		 = philips_lb064v02_on;
//			epinfo.off		 = philips_lb064v02_off;
			epinfo.on		 = falinux_on;
			epinfo.off		 = falinux_off;
			// Brightness
			ep93xxfb_outl( 0xFEFF, BRIGHTNESS );
#endif
	case CRT_OUT:		            
    	    epinfo.xres		 = ep93xxfb_vmods[vmode].hres;
    	    epinfo.xsync	 = ep93xxfb_vmods[vmode].hsync;    
    	    epinfo.xfp		 = ep93xxfb_vmods[vmode].hfp;
    	    epinfo.xbp		 = ep93xxfb_vmods[vmode].hbp;
    	    epinfo.xtotal	 = epinfo.xres + epinfo.xsync + epinfo.xfp + epinfo.xbp;    	

    	    epinfo.yres		 = ep93xxfb_vmods[vmode].vres;
    	    epinfo.ysync	 = ep93xxfb_vmods[vmode].vsync;
    	    epinfo.yfp		 = ep93xxfb_vmods[vmode].vfp;
    	    epinfo.ybp		 = ep93xxfb_vmods[vmode].vbp;
    	    epinfo.ytotal	 = epinfo.yres + epinfo.ysync + epinfo.yfp + epinfo.ybp;
    	    
	    	epinfo.refresh	 = ep93xxfb_vmods[vmode].refresh;					
    	    epinfo.refresh	 = epinfo.xtotal * epinfo.ytotal * epinfo.refresh;
	    	epinfo.pixclock  = 1000000000 / ( epinfo.refresh / 1000);
	    	epinfo.bpp		 = depth;		
    			
    	    epinfo.clk_src	 = ep93xxfb_vmods[vmode].clk_src;
    	    epinfo.clk_edge	 = ep93xxfb_vmods[vmode].clk_edge;
    	    epinfo.pol_blank = ep93xxfb_vmods[vmode].pol_blank;
    	    epinfo.pol_xsync = ep93xxfb_vmods[vmode].pol_hsync;
    	    epinfo.pol_ysync = ep93xxfb_vmods[vmode].pol_vsync;
	    	break;    
							        
	case TV_OUT:
    	    epinfo.xres		 = cxmods[vmode].hres;
    	    epinfo.xsync	 = 4;
			epinfo.xbp		 = cxmods[vmode].hblank;
			epinfo.xfp		 = cxmods[vmode].hclktotal - epinfo.xbp - cxmods[vmode].hres - 4;
			epinfo.xtotal	 = cxmods[vmode].hclktotal; 

	    	epinfo.yres		 = cxmods[vmode].vres;
	    	epinfo.ysync	 = 2;
			epinfo.ybp		 = cxmods[vmode].vblank;
			epinfo.yfp		 = cxmods[vmode].vclktotal - epinfo.ybp - cxmods[vmode].vres - 2;
			epinfo.ytotal	 = cxmods[vmode].vclktotal;
	    	epinfo.bpp		 = depth;
    	
	    	epinfo.clk_src	 = CLK_EXTERNAL;
	    	epinfo.automods	 = cxmods[vmode].automode;
	    	epinfo.clk_edge	 = EDGE_FALLING;
	    	epinfo.pol_blank = POL_LOW;
	    	epinfo.pol_xsync = POL_LOW,
	    	epinfo.pol_ysync = POL_LOW;	
            
            epinfo.refresh	 = cxmods[vmode].clkfrequency;
			epinfo.pixclock	 = 1000000000 / ( epinfo.refresh / 1000);
	    	    
			epinfo.configure = cx25871_config;
			epinfo.on		 = cx25871_on;
			epinfo.off		 = cx25871_off;
			break;
    }    
}

static int ep93xxfb_alloc_videomem(void)
{
    unsigned long adr,size,pgsize;
    int order;

    DPRINTK("ep93xxfb_alloc_videomem - enter \n");

	epinfo.fb_log = NULL;
	epinfo.fb_size = PAGE_ALIGN( MAX_FBMEM_SIZE/*ep93xxfb_vmods[vmode].hres * ep93xxfb_vmods[vmode].vres * (depth / 8)*/ );
	order = get_order( epinfo.fb_size );	
	epinfo.fb_log = (void*) __get_free_pages( GFP_KERNEL, order );
	
	if (epinfo.fb_log) {
		epinfo.fb_phys = __virt_to_phys((int) epinfo.fb_log );
		adr = (unsigned long)epinfo.fb_log;
		size = epinfo.fb_size;
		pgsize = 1 << order;
		do {
			adr += pgsize;
			SetPageReserved(virt_to_page(adr));
		} while(size -= pgsize);
	}
	else{
		printk("%s memory fail \n",__FUNCTION__);
		return -ENOMEM;
	}

/*
    epinfo.fb_log = NULL;
    epinfo.fb_size = ep93xxfb_vmods[vmode].hres * ep93xxfb_vmods[vmode].vres * (depth / 8);
    epinfo.fb_log = dma_alloc_writecombine(NULL, epinfo.fb_size, &epinfo.fb_phys, GFP_KERNEL);
    if (!epinfo.fb_log){
		printk("%s memory fail \n",__FUNCTION__);
		return -ENOMEM;
    }
*/
    memset(epinfo.fb_log,0x00,epinfo.fb_size);
    DPRINTK("   fb_log_addres = 0x%x\n",epinfo.fb_log);
    DPRINTK("   fb_phys_address = 0x%x\n",epinfo.fb_phys);
    DPRINTK("   fb_size = %lu\n",epinfo.fb_size);
    DPRINTK("   fb_page_order = %d\n",order);
    DPRINTK("ep93xxfb_alloc_videomem - exit \n");
    return 0;		   
}

static void ep93xxfb_release_videomem(void)
{
    unsigned long adr,size,psize;
    int order;
    	
    DPRINTK("ep93xxfb_release_videomem - enter \n");

    
    if (epinfo.fb_log) {
		order = get_order(epinfo.fb_size);
		adr = (unsigned long)epinfo.fb_log;
		size = epinfo.fb_size;
		psize = 1 << order ;
		do {
			adr += psize;
			ClearPageReserved(virt_to_page(adr));
		} while(size -= psize);
		free_pages((unsigned long)epinfo.fb_log, order );
	}

/*
    if (epinfo.fb_log) {
    	dma_free_coherent(NULL, epinfo.fb_size, epinfo.fb_log, epinfo.fb_phys);
    }
*/
    DPRINTK("ep93xxfb_release_videomem - exit \n");
}

static void ep93xxfb_setinfo(struct fb_info *info)
{
    info->pseudo_palette = pseudo_palette;
    info->var.xres = epinfo.xres;
    info->var.yres = epinfo.yres;
    info->var.xres_virtual = epinfo.xres;
    info->var.yres_virtual = epinfo.yres;
	       
    ep93xxfb_setcol( info, depth );
		   
    info->var.activate = FB_ACTIVATE_NOW;
    info->var.left_margin = epinfo.xbp;
    info->var.right_margin = epinfo.xfp;
    info->var.upper_margin = epinfo.ybp;
    info->var.lower_margin = epinfo.yfp;
    info->var.hsync_len = epinfo.xsync;
    info->var.vsync_len = epinfo.ysync;
    						       
    if( epinfo.pol_xsync == POL_HIGH )
        info->var.sync |= FB_SYNC_HOR_HIGH_ACT;
    if( epinfo.pol_ysync == POL_HIGH )
        info->var.sync |= FB_SYNC_VERT_HIGH_ACT;
			
    info->var.vmode = FB_VMODE_NONINTERLACED | FB_VMODE_YWRAP;
    info->fix.smem_start = epinfo.fb_phys;
    info->fix.smem_len = epinfo.fb_size;
    info->fix.type = FB_TYPE_PACKED_PIXELS;
    info->fix.line_length = epinfo.xres * (epinfo.bpp / 8);
    info->screen_base = epinfo.fb_log;
    info->var.pixclock = epinfo.pixclock;
    info->fix.ypanstep = 1;
    info->fix.ywrapstep = 1;		    
}
    
static int ep93xxfb_config(struct fb_info *info)
{
    unsigned long attribs;
	int			  arb;

    DPRINTK("ep93xxfb_config - enter\n"); 

    ep93xxfb_get_par( info );
    if( ep93xxfb_alloc_videomem() != 0 ) {
        printk("Unable to allocate video memory\n");
        return -ENOMEM;
    }
			    
    if( ep93xxfb_setclk() != 0 ) {
	printk("Unable to set pixel clock\n");
        ep93xxfb_release_videomem();
	return -ENODEV;			    
    }
    
    SysconSetLocked(SYSCON_DEVCFG, inl(SYSCON_DEVCFG)  | SYSCON_DEVCFG_RasOnP3);
    ep93xxfb_timing_signal_generation();

    /* set video memory parameters */
    outl(epinfo.fb_phys, VIDSCRNPAGE);
    outl(epinfo.yres , SCRNLINES);
    outl(((epinfo.xres * epinfo.bpp) / 32) - 1, LINELENGTH);
    outl((epinfo.xres * epinfo.bpp) / 32, VLINESTEP);

    				
    /* set pixel mode */
    ep93xxfb_pixelmod(depth);

	// [FALINUX] - Bus Master Arbitration Regiter(Priority Order)
  	arb = inl(SYSCON_BMAR);
    arb = (arb & 0x3f8) | 0x03;
    outl(arb, SYSCON_BMAR); 
	 
	attribs = 0;

#ifdef CONFIG_EP93XX_SDCS0
    attribs |= 0 << VIDEOATTRIBS_SDSEL_SHIFT;
#endif
#ifdef CONFIG_EP93XX_SDCS1
    attribs |= 1 << VIDEOATTRIBS_SDSEL_SHIFT;
#endif
#ifdef CONFIG_EP93XX_SDCS2
    attribs |= 2 << VIDEOATTRIBS_SDSEL_SHIFT;
#endif
#ifdef CONFIG_EP93XX_SDCS3
    attribs |= 3 << VIDEOATTRIBS_SDSEL_SHIFT;
#endif
    
    if(epinfo.clk_edge == EDGE_RISING)
		attribs |= VIDEOATTRIBS_INVCLK;
    if(epinfo.pol_blank == POL_HIGH)
		attribs |= VIDEOATTRIBS_BLKPOL;
    if(epinfo.pol_xsync == POL_HIGH)
		attribs |= VIDEOATTRIBS_HSPOL;
    if(epinfo.pol_ysync == POL_HIGH)
		attribs |= VIDEOATTRIBS_VCPOL;

    ep93xxfb_outl(attribs, VIDEOATTRIBS);
    ep93xxfb_setinfo( info );

    if(epinfo.configure)
		(epinfo.configure)( epinfo.automods );

    ep93xxfb_blank( 0 , info );
    
    DPRINTK("ep93xxfb_config - exit\n");
    return 0;
}

// [FALINUX]-----------------------------------------------------------------------------
DECLARE_WAIT_QUEUE_HEAD( vsync_queue );

static int vsync_wait = 0;
static int vsync_debug_disp = 0;

static irqreturn_t ep93xx_vsync_isr(int irq, void *dev_id, struct pt_regs *regs)
{
    unsigned long tmpdata;

    tmpdata = inl( VIDEOATTRIBS );
    tmpdata &= (~VIDEOATTRIBS_INT);
    ep93xxfb_outl( tmpdata, VIDEOATTRIBS );

    wake_up_interruptible( &vsync_queue );
	return IRQ_HANDLED;
}

int ep93xxfb_ioctl(struct fb_info *info,unsigned int cmd, unsigned long arg)
{
    flip_set_visual_addr_t flip_set_visual_addr;
	unsigned long flags;
    int lp;

    struct fb_fillrect fill;
    struct fb_copyarea cparea;
    struct fb_image img; 
    struct ep93xx_line line;           
    struct ep93xx_cursor cursor;
        
    switch (cmd) {
	case FBIO_EP93XX_CURSOR:
            copy_from_user(&cursor, (void *)arg, sizeof(struct ep93xx_cursor));
			ep93xxfb_cursor(info,&cursor);
	    	break;
	case FBIO_EP93XX_LINE:
            copy_from_user(&line, (void *)arg, sizeof(struct ep93xx_line));
	    	ep93xxfb_line(info,&line);
	    	break;
	case FBIO_EP93XX_FILL:
	    	copy_from_user(&fill, (void *)arg, sizeof(struct fb_fillrect));
	    	ep93xxfb_fillrect(info,&fill);	
	    	break;        
	case FBIO_EP93XX_BLIT:
            copy_from_user(&img, (void *)arg, sizeof(struct fb_image));
	    	ep93xxfb_imageblit(info, &img);
	    	break;
	case FBIO_EP93XX_COPY:
			copy_from_user(&cparea, (void *)arg, sizeof(struct fb_copyarea));
	    	ep93xxfb_copyarea(info,&cparea);
	    	break;

    case FB_IOCTL_FLIP_GET_INFO : return copy_to_user(arg, &flip_info, sizeof(flip_info)) ? -EFAULT : 0;

    case FB_IOCTL_FLIP_SET_ADDR : if(copy_from_user(&flip_set_visual_addr, arg, sizeof(flip_set_visual_addr))) return -EFAULT;
                                  if( flip_set_visual_addr.wait )
                                  {
                                      interruptible_sleep_on( &vsync_queue );
                                  }
                                  for( lp = 0; lp < 60; lp++ ) udelay(10); // 35 - 50
                                  ep93xxfb_outl( (unsigned int)epinfo.fb_phys + flip_set_visual_addr.offset, VIDSCRNPAGE );
                                  break;

	default:
			return -EFAULT;
    }																							     
    return 0;																							     
}																									    
// [FALINUX]-----------------------------------------------------------------------------																									    

static struct fb_ops ep93xxfb_ops = {
    .owner          = THIS_MODULE,
    .fb_setcolreg   = ep93xxfb_setcolreg,
    .fb_check_var   = ep93xxfb_check_var,
    .fb_set_par     = ep93xxfb_set_par,
    .fb_blank       = ep93xxfb_blank,
    .fb_pan_display = ep93xx_pan_display,
    .fb_fillrect    = ep93xxfb_fillrect,
    .fb_copyarea    = ep93xxfb_copyarea,
    .fb_imageblit   = cfb_imageblit,
    .fb_cursor      = ep93xxfb_cursor,
    .fb_ioctl       = ep93xxfb_ioctl,
    .fb_mmap        = ep93xxfb_mmap,
};


static struct resource ep93xxfb_raster_resources = {
    .start          = EP93XX_RASTER_PHYS_BASE,
    .end            = EP93XX_RASTER_PHYS_BASE + 0x1ffff,
    .flags          = IORESOURCE_MEM,
};										


static int __init ep93xxfb_probe(struct platform_device *device)
{
	struct fb_info *info = NULL;
	struct resource *res = NULL;
	int ret = 0;
	int arb = 0;
	unsigned long tmpdata;  // [FALINUX]

	DPRINTK("ep93xxfb_probe - enter \n");

	if(!device) {
		printk("error : to_platform_device\n");
		return -ENODEV;
	}
	res = platform_get_resource( device, IORESOURCE_MEM, 0);
	if(!res) {
		printk("error : platform_get_resource \n");
		return -ENODEV;
	}				
	cursor_data = kmalloc( 64 * 64 * 2, GFP_KERNEL );			       
	memset( cursor_data, 0x00, 64 * 64 * 2 );
	if(!cursor_data) {
		printk("Unable to allocate memory for hw_cursor\n");
		return -ENOMEM;
	}
	if (!request_mem_region(res->start,res->end - res->start + 1, FBDEV_NAME ))
		return -EBUSY;

	info = framebuffer_alloc(sizeof(u32) * 256, &device->dev);

	if(!info) {
		printk("Unable to allocate memory for frame buffer\n");
		return -ENOMEM;
	}

	info->flags = FBINFO_DEFAULT;
	strncpy(info->fix.id, FBDEV_NAME, sizeof(info->fix.id));
	info->fix.mmio_start = res->start;
	info->fix.mmio_len = res->end - res->start + 1;
	info->fbops = &ep93xxfb_ops;
	info->pseudo_palette = info->par;
	info->state = FBINFO_STATE_RUNNING;

	if (fb_alloc_cmap(&info->cmap, 256, 0) < 0) {
		ret = -ENOMEM;
		goto fbuff;
	}

	if ((ret = ep93xxfb_config(info)) < 0)
		goto clmap;

	if (register_framebuffer(info) < 0) {
		printk(KERN_ERR "Unable to register ep93xxfb frame buffer\n");
		ret = -EINVAL;
		goto clmap;
	}
	platform_set_drvdata(device, info);
	printk(KERN_INFO "fb%d: EP93xx frame buffer at %dx%dx%dbpp\n", info->node,
			info->var.xres, info->var.yres, info->var.bits_per_pixel);

	register_reboot_notifier(&cx25871_notifier);

	/*change the raster arb to the highest one--Bo*/
	arb = inl(SYSCON_BMAR);
	arb = (arb & 0x3f8) | 0x03;
	outl(arb,SYSCON_BMAR);

	// [FALINUX]-----------------------------------------------------------------------------
	tmpdata = request_irq(IRQ_EP93XX_VSYNC, ep93xx_vsync_isr, SA_INTERRUPT, "ep93xx_vsync", NULL );
	if (tmpdata) {
		printk(KERN_WARNING "ep93xx_vsync: failed to get vsync IRQ\n");
	}

	tmpdata = inl( VIDEOATTRIBS );
	ep93xxfb_outl( tmpdata |  VIDEOATTRIBS_INTEN, VIDEOATTRIBS );
	//---------------------------------------------------------------------------------------

#if !defined(CONFIG_FRAMEBUFFER_CONSOLE) && defined(CONFIG_LOGO)
	if (fb_prepare_logo(info, FB_ROTATE_UR)) {
		printk("Start display and show logo\n");
		/* Start display and show logo on boot */
		fb_set_cmap(&info->cmap, info);
		fb_show_logo(info, FB_ROTATE_UR);
	}
#endif

	DPRINTK("ep93xxfb_probe - exit \n");
	return 0;

clmap:
	fb_dealloc_cmap(&info->cmap);

fbuff:
	framebuffer_release(info);
	return ret;
}

static int ep93xxfb_remove(struct platform_device *device)
{
    struct resource *res;
    struct fb_info *info;			        
    struct ep93xx_cursor cursor;
        
    DPRINTK("ep93xxfb_remove - enter \n");

    info = platform_get_drvdata(device);
       
    ep93xxfb_release_videomem();
   
    res = platform_get_resource( device, IORESOURCE_MEM, 0);
    release_mem_region(res->start, res->end - res->start + 1);
           			    	
    platform_set_drvdata(device, NULL);
    unregister_framebuffer(info);
    
    fb_dealloc_cmap(&info->cmap);
    framebuffer_release(info);

    cursor.flags = CURSOR_OFF;
    ep93xxfb_cursor(info,&cursor);
    if(cursor_data!=NULL)
	kfree(cursor_data);		

    unregister_reboot_notifier(&cx25871_notifier);    
    ep93xxfb_blank( 1, info );
        		    
    DPRINTK("ep93xxfb_remove - exit \n");
    return 0;
}

static void ep93xxfb_platform_release(struct device *device)
{
    DPRINTK("ep93xxfb_platform_release - enter\n");
}

static int ep93xxfb_check_param(void)
{
    switch(vout) {
	case CRT_OUT:
	    	if( vmode >=(sizeof(ep93xxfb_vmods)/sizeof(ep93xxfb_vmods[0]))){ 
        		vmode = 3;
        		depth = DEFAULT_BPP;
	    		return 0;
	    	}	
	    	break;

	case LCD_OUT:
			if( vmode >= 3 || depth != 16 ) {
        		vmode = 0;
        		depth = DEFAULT_BPP;
	    		return 0;
	    	}
	    	break;

	case TV_OUT:
			if( vmode >= CXMODES_COUNT ) {
				vmode = DEFAULT_MODE;
        		depth = DEFAULT_BPP;
	    		return 0;	
	    	}
	    	break;

	default:
	    	vmode = DEFAULT_MODE;
	    	depth = DEFAULT_BPP;
	    	vout  = DEFAULT_OUT;
	    	return 0;
	    	break;
    }

    if(!((depth == 8) || (depth == 16) || (depth == 24) || (depth == 32)))
		depth = DEFAULT_BPP;

    return 1;
}

int __init ep93xxfb_setup(char *options)
{
    char *opt;

    DPRINTK("ep93xxfb_setup - %s \n",options );
    		
    if (!options || !*options)
	return 0;

    while ((opt = strsep(&options, ",")) != NULL) {
		if (!strncmp(opt, "vout=", 5))
	    	vout = simple_strtoul(opt + 5, NULL, 0);
		else if (!strncmp(opt, "vmode=", 6))
	    	vmode = simple_strtoul(opt + 6, NULL, 0);
		else if (!strncmp(opt, "depth=", 6))
	    	depth = simple_strtoul(opt + 6, NULL, 0);
    }
    ep93xxfb_check_param();
    return 0;
}


static struct platform_driver ep93xxfb_driver = {
    .probe  = ep93xxfb_probe,
    .remove = ep93xxfb_remove,
    .driver = {
		 .name   = FBDEV_NAME,
	      },
};
				
static struct platform_device ep93xxfb_device = {
    .name      = FBDEV_NAME,
    .id        = -1,
    .dev    = { 
		.release = ep93xxfb_platform_release,
	      },
    .num_resources  = 1,
    .resource       = &ep93xxfb_raster_resources,
};

int __init ep93xxfb_init(void)
{
    int ret = 0;
    char *option = NULL;
	           
    DPRINTK("ep93xxfb_init - enter\n");		
	    
    if (fb_get_options("ep93xxfb", &option))
		return -ENODEV;

    ep93xxfb_setup(option);
    
//	if( !ep93xxfb_check_param() ) {
//		printk("Unsupported format \n");
//		return -1;
//    }
    
    /*Add the Hardware accel irq */
    outl(0x00000000, BLOCKCTRL);
    ret = request_irq(IRQ_EP93XX_GRAPHICS, ep93xxfb_irq_handler, SA_INTERRUPT, "graphics",NULL);
    if (ret != 0) {
    	printk("%s: can't get irq %i, err %d\n",__FUNCTION__, IRQ_EP93XX_GRAPHICS, ret);
        return -EBUSY;
    }

    /*-------------------------------*/	    
    ret = platform_driver_register(&ep93xxfb_driver);
    
    if (!ret) {
	ret = platform_device_register(&ep93xxfb_device);
	if (ret)
	    platform_driver_unregister(&ep93xxfb_driver);
    }
				   
    DPRINTK("ep93xxfb_init - exit\n");
    return ret;
}



static void __exit ep93xxfb_exit(void)
{
    DPRINTK("ep93xxfb_exit - enter\n");
    platform_driver_unregister(&ep93xxfb_driver);
    platform_device_unregister(&ep93xxfb_device);
    DPRINTK("ep93xxfb_exit - exit\n");
}

module_init(ep93xxfb_init);
module_exit(ep93xxfb_exit);


module_param( vmode, int, 1);
MODULE_PARM_DESC(vmode, "Specify the video mode number that should be used");
module_param( vout , int , 0 );
MODULE_PARM_DESC(vout ,"Specify video output (0 = CRT ,1 = LCD , 2 = TV)");
module_param( depth , int, 16);
MODULE_PARM_DESC(depth ,"Color depth (8,16,24,32)");
MODULE_LICENSE("GPL");
