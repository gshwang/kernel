/*
 * linux/drivers/video/s3c2410fb.c
 *	Copyright (c) Arnaud Patard, Ben Dooks
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	    S3C2410 LCD Controller Frame Buffer Driver
 *	    based on skeletonfb.c, sa1100fb.c and others
 *
 * ChangeLog
 * 2005-04-07: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - u32 state -> pm_message_t state
 *      - S3C2410_{VA,SZ}_LCD -> S3C24XX
 *
 * 2005-03-15: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - Removed the ioctl
 *      - use readl/writel instead of __raw_writel/__raw_readl
 *
 * 2004-12-04: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - Added the possibility to set on or off the
 *      debugging mesaages
 *      - Replaced 0 and 1 by on or off when reading the
 *      /sys files
 *
 * 2005-03-23: Ben Dooks <ben-linux@fluff.org>
 *	- added non 16bpp modes
 *	- updated platform information for range of x/y/bpp
 *	- add code to ensure palette is written correctly
 *	- add pixel clock divisor control
 *
 * 2004-11-11: Arnaud Patard <arnaud.patard@rtp-net.org>
 * 	- Removed the use of currcon as it no more exist
 * 	- Added LCD power sysfs interface
 *
 * 2004-11-03: Ben Dooks <ben-linux@fluff.org>
 *	- minor cleanups
 *	- add suspend/resume support
 *	- s3c2410fb_setcolreg() not valid in >8bpp modes
 *	- removed last CONFIG_FB_S3C2410_FIXED
 *	- ensure lcd controller stopped before cleanup
 *	- added sysfs interface for backlight power
 *	- added mask for gpio configuration
 *	- ensured IRQs disabled during GPIO configuration
 *	- disable TPAL before enabling video
 *
 * 2004-09-20: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - Suppress command line options
 *
 * 2004-09-15: Arnaud Patard <arnaud.patard@rtp-net.org>
 * 	- code cleanup
 *
 * 2004-09-07: Arnaud Patard <arnaud.patard@rtp-net.org>
 * 	- Renamed from h1940fb.c to s3c2410fb.c
 * 	- Add support for different devices
 * 	- Backlight support
 *
 * 2004-09-05: Herbert Pötzl <herbert@13thfloor.at>
 *	- added clock (de-)allocation code
 *	- added fixem fbmem option
 *
 * 2004-07-27: Arnaud Patard <arnaud.patard@rtp-net.org>
 *	- code cleanup
 *	- added a forgotten return in h1940fb_init
 *
 * 2004-07-19: Herbert Pötzl <herbert@13thfloor.at>
 *	- code cleanup and extended debugging
 *
 * 2004-07-15: Arnaud Patard <arnaud.patard@rtp-net.org>
 *	- First version
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#include <asm/mach/map.h>
#include <asm/arch/regs-lcd.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/fb.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#include "s3c2410fb.h"


static struct s3c2410fb_mach_info *mach_info;

#ifdef CONFIG_FB_S3C24XX_PARAMETERS
#define S3C24XXFB_OPTIONS_SIZE 256
static char g_options[S3C24XXFB_OPTIONS_SIZE] __initdata = "";
#endif 

/* Debugging stuff */
#ifdef CONFIG_FB_S3C2410_DEBUG
static int debug	   = 1;
#else
static int debug	   = 0;
#endif

#define dprintk(msg...)	if (debug) { printk("s3c2410fb: " msg); }

static int panel_index = 2; /* default is zero */


/* useful functions */
static struct s3c2410fb_mach_info known_lcd_panels[] = 
{
	[0] = { /* QVGA 240X320 */
		.regs   = {
			.lcdcon1 = S3C2410_LCDCON1_TFT16BPP | S3C2410_LCDCON1_TFT | S3C2410_LCDCON1_CLKVAL(0x06),
			.lcdcon2 = S3C2410_LCDCON2_VBPD(2)  | S3C2410_LCDCON2_LINEVAL(320-1) | S3C2410_LCDCON2_VFPD(2) | S3C2410_LCDCON2_VSPW(10),
			.lcdcon3 = S3C2410_LCDCON3_HBPD(2)  | S3C2410_LCDCON3_HOZVAL (240-1) | S3C2410_LCDCON3_HFPD(2),
			.lcdcon4 = S3C2410_LCDCON4_MVAL(0)  | S3C2410_LCDCON4_HSPW(41),
			.lcdcon5 = S3C2410_LCDCON5_FRM565   | S3C2410_LCDCON5_INVVLINE | S3C2410_LCDCON5_INVVFRAME | S3C2410_LCDCON5_PWREN | S3C2410_LCDCON5_HWSWP,
		},

		/* currently setup by downloader */
		.gpccon         = 0xaaaa56a9,
		.gpccon_mask    = 0xffffffff,
		.gpcup          = 0x0000ffff,
		.gpcup_mask     = 0xffffffff,
		.gpdcon         = 0xaaaaaaaa,
		.gpdcon_mask    = 0xffffffff,
		.gpdup          = 0x0000ffff,
		.gpdup_mask     = 0xffffffff,

		.lpcsel         = 0xF84,
		.type       	= S3C2410_LCDCON1_TFT,

		.width  = 240,
		.height = 320,

		.xres       = {
			.min    = 240,
			.max    = 240,
			.defval = 240,
		},

		.yres       = {
			.min    = 320,
			.max    = 320,
			.defval = 320,
		},
		.bpp        = {
			.min    = 16,
			.max    = 16,
			.defval = 16,
		},
	},
	[1] = { /* 480X272 */
		.regs   = {
			.lcdcon1 = S3C2410_LCDCON1_TFT16BPP | S3C2410_LCDCON1_TFT | S3C2410_LCDCON1_CLKVAL(0x05),
			.lcdcon2 = S3C2410_LCDCON2_VBPD(0)  | S3C2410_LCDCON2_LINEVAL(272-1) | S3C2410_LCDCON2_VFPD(4) | S3C2410_LCDCON2_VSPW(10),
			.lcdcon3 = S3C2410_LCDCON3_HBPD(2)  | S3C2410_LCDCON3_HOZVAL (480-1) | S3C2410_LCDCON3_HFPD(2),
			.lcdcon4 = S3C2410_LCDCON4_MVAL(0)  | S3C2410_LCDCON4_HSPW(41),
			.lcdcon5 = S3C2410_LCDCON5_FRM565   | S3C2410_LCDCON5_INVVLINE | S3C2410_LCDCON5_INVVFRAME | S3C2410_LCDCON5_PWREN | S3C2410_LCDCON5_HWSWP,
		},

		/* currently setup by downloader */
		.gpccon         = 0xaaaa56a9,
		.gpccon_mask    = 0xffffffff,
		.gpcup          = 0x0000ffff,
		.gpcup_mask     = 0xffffffff,
		.gpdcon         = 0xaaaaaaaa,
		.gpdcon_mask    = 0xffffffff,
		.gpdup          = 0x0000ffff,
		.gpdup_mask     = 0xffffffff,

		.lpcsel         = 0xF84,
		.type       	= S3C2410_LCDCON1_TFT,

		.width  = 480,
		.height = 272,

		.xres       = {
			.min    = 480,
			.max    = 480,
			.defval = 480,
		},

		.yres       = {
			.min    = 272,
			.max    = 272,
			.defval = 272,
		},
		.bpp        = {
			.min    = 16,
			.max    = 16,
			.defval = 16,
		},
	},
	[2] = { /* 640X480 */
		.regs   = {
			.lcdcon1 = S3C2410_LCDCON1_TFT16BPP | S3C2410_LCDCON1_TFT | S3C2410_LCDCON1_CLKVAL(0x02),
			.lcdcon2 = S3C2410_LCDCON2_VBPD(10) | S3C2410_LCDCON2_LINEVAL(480-1) | S3C2410_LCDCON2_VFPD(33) | S3C2410_LCDCON2_VSPW(2),
			.lcdcon3 = S3C2410_LCDCON3_HBPD(16) | S3C2410_LCDCON3_HOZVAL (640-1) | S3C2410_LCDCON3_HFPD(16),
			.lcdcon4 = S3C2410_LCDCON4_MVAL(0)  | S3C2410_LCDCON4_HSPW(24),
			.lcdcon5 = S3C2410_LCDCON5_FRM565   | S3C2410_LCDCON5_INVVLINE | S3C2410_LCDCON5_INVVFRAME | S3C2410_LCDCON5_PWREN | S3C2410_LCDCON5_HWSWP,
		},

		/* currently setup by downloader */
		.gpccon         = 0xaaaa56a9,
		.gpccon_mask    = 0xffffffff,
		.gpcup          = 0x0000ffff,
		.gpcup_mask     = 0xffffffff,
		.gpdcon         = 0xaaaaaaaa,
		.gpdcon_mask    = 0xffffffff,
		.gpdup          = 0x0000ffff,
		.gpdup_mask     = 0xffffffff,

		.lpcsel         = 0xF84,
		.type       	= S3C2410_LCDCON1_TFT,

		.width  = 640,
		.height = 480,

		.xres       = {
			.min    = 640,
			.max    = 640,
			.defval = 640,
		},

		.yres       = {
			.min    = 480,
			.max    = 480,
			.defval = 480,
		},
		.bpp        = {
			.min    = 16,
			.max    = 16,
			.defval = 16,
		},
	},
	[3] = { /* 800X480 7" LMS700KF05 */
		.regs   = {
			.lcdcon1 = S3C2410_LCDCON1_TFT16BPP | S3C2410_LCDCON1_TFT | S3C2410_LCDCON1_CLKVAL(0x02),
			.lcdcon2 = S3C2410_LCDCON2_VBPD(5) | S3C2410_LCDCON2_LINEVAL(480-1) | S3C2410_LCDCON2_VFPD(5) | S3C2410_LCDCON2_VSPW(1),
			.lcdcon3 = S3C2410_LCDCON3_HBPD(16) | S3C2410_LCDCON3_HOZVAL (800-1) | S3C2410_LCDCON3_HFPD(8),
			.lcdcon4 = S3C2410_LCDCON4_MVAL(0)  | S3C2410_LCDCON4_HSPW(1),
			.lcdcon5 = S3C2410_LCDCON5_FRM565   | S3C2410_LCDCON5_INVVLINE | S3C2410_LCDCON5_INVVFRAME | S3C2410_LCDCON5_PWREN | S3C2410_LCDCON5_HWSWP,
		},

		/* currently setup by downloader */
		.gpccon         = 0xaaaa56a9,
		.gpccon_mask    = 0xffffffff,
		.gpcup          = 0x0000ffff,
		.gpcup_mask     = 0xffffffff,
		.gpdcon         = 0xaaaaaaaa,
		.gpdcon_mask    = 0xffffffff,
		.gpdup          = 0x0000ffff,
		.gpdup_mask     = 0xffffffff,

		.lpcsel         = 0xF84,
		.type       	= S3C2410_LCDCON1_TFT,

		.width  = 800,
		.height = 480,

		.xres       = {
			.min    = 800,
			.max    = 800,
			.defval = 800,
		},

		.yres       = {
			.min    = 480,
			.max    = 480,
			.defval = 480,
		},
		.bpp        = {
			.min    = 16,
			.max    = 16,
			.defval = 16,
		},
	},};


/* s3c2410fb_set_lcdaddr
 *
 * initialise lcd controller address pointers
*/

static void s3c2410fb_set_lcdaddr(struct s3c2410fb_info *fbi)
{
	struct fb_var_screeninfo *var = &fbi->fb->var;
	unsigned long saddr1, saddr2, saddr3;

	saddr1  = fbi->fb->fix.smem_start >> 1;
	saddr2  = fbi->fb->fix.smem_start;
	saddr2 += (var->xres * var->yres * var->bits_per_pixel)/8;
	saddr2>>= 1;

	saddr3 =  S3C2410_OFFSIZE(0) | S3C2410_PAGEWIDTH((var->xres * var->bits_per_pixel / 16) & 0x3ff);

	dprintk("LCDSADDR1 = 0x%08lx\n", saddr1);
	dprintk("LCDSADDR2 = 0x%08lx\n", saddr2);
	dprintk("LCDSADDR3 = 0x%08lx\n", saddr3);

	writel(saddr1, S3C2410_LCDSADDR1);
	writel(saddr2, S3C2410_LCDSADDR2);
	writel(saddr3, S3C2410_LCDSADDR3);
}

/* s3c2410fb_calc_pixclk()
 *
 * calculate divisor for clk->pixclk
*/

static unsigned int s3c2410fb_calc_pixclk(struct s3c2410fb_info *fbi,
					  unsigned long pixclk)
{
	unsigned long clk = clk_get_rate(fbi->clk);
	unsigned long long div;

	/* pixclk is in picoseoncds, our clock is in Hz
	 *
	 * Hz -> picoseconds is / 10^-12
	 */

	div = (unsigned long long)clk * pixclk;
	do_div(div,1000000UL);
	do_div(div,1000000UL);

	dprintk("pixclk %ld, divisor is %ld\n", pixclk, (long)div);
	return div;
}

/*
 *	s3c2410fb_check_var():
 *	Get the video params out of 'var'. If a value doesn't fit, round it up,
 *	if it's too big, return -EINVAL.
 *
 */
static int s3c2410fb_check_var(struct fb_var_screeninfo *var,
			       struct fb_info *info)
{
	struct s3c2410fb_info *fbi = info->par;

	dprintk("check_var(var=%p, info=%p)\n", var, info);

	/* validate x/y resolution */

	if (var->yres > fbi->mach_info->yres.max)
		var->yres = fbi->mach_info->yres.max;
	else if (var->yres < fbi->mach_info->yres.min)
		var->yres = fbi->mach_info->yres.min;

	if (var->xres > fbi->mach_info->xres.max)
		var->yres = fbi->mach_info->xres.max;
	else if (var->xres < fbi->mach_info->xres.min)
		var->xres = fbi->mach_info->xres.min;
	
	/* validate bpp */

	if (var->bits_per_pixel > fbi->mach_info->bpp.max)
		var->bits_per_pixel = fbi->mach_info->bpp.max;
	else if (var->bits_per_pixel < fbi->mach_info->bpp.min)
		var->bits_per_pixel = fbi->mach_info->bpp.min;

	/* set r/g/b positions */
	switch (var->bits_per_pixel) {
		case 1:
		case 2:
		case 4:
			var->red.offset    	= 0;
			var->red.length    	= var->bits_per_pixel;
			var->green         	= var->red;
			var->blue          	= var->red;
			var->transp.offset 	= 0;
			var->transp.length 	= 0;
			break;
		case 8:
			if ( fbi->mach_info->type != S3C2410_LCDCON1_TFT ) {
				/* 8 bpp 332 */
				var->red.length		= 3;
				var->red.offset		= 5;
				var->green.length	= 3;
				var->green.offset	= 2;
				var->blue.length	= 2;
				var->blue.offset	= 0;
				var->transp.length	= 0;
			} else {
				var->red.offset    	= 0;
				var->red.length    	= var->bits_per_pixel;
				var->green         	= var->red;
				var->blue          	= var->red;
				var->transp.offset 	= 0;
				var->transp.length 	= 0;
			}
			break;
		case 12:
			/* 12 bpp 444 */
			var->red.length		= 4;
			var->red.offset		= 8;
			var->green.length	= 4;
			var->green.offset	= 4;
			var->blue.length	= 4;
			var->blue.offset	= 0;
			var->transp.length	= 0;
			break;

		default:
		case 16:
			if (fbi->regs.lcdcon5 & S3C2410_LCDCON5_FRM565 ) {
				/* 16 bpp, 565 format */
				var->red.offset		= 11;
				var->green.offset	= 5;
				var->blue.offset	= 0;
				var->red.length		= 5;
				var->green.length	= 6;
				var->blue.length	= 5;
				var->transp.length	= 0;
			} else {
				/* 16 bpp, 5551 format */
				var->red.offset		= 11;
				var->green.offset	= 6;
				var->blue.offset	= 1;
				var->red.length		= 5;
				var->green.length	= 5;
				var->blue.length	= 5;
				var->transp.length	= 0;
			}
			break;
		case 24:
			/* 24 bpp 888 */
			var->red.length		= 8;
			var->red.offset		= 16;
			var->green.length	= 8;
			var->green.offset	= 8;
			var->blue.length	= 8;
			var->blue.offset	= 0;
			var->transp.length	= 0;
			break;
	}
	dprintk("-----END check_var(var=%p, info=%p)\n", var, info);
	return 0;
}


/* s3c2410fb_activate_var
 *
 * activate (set) the controller from the given framebuffer
 * information
*/

static void s3c2410fb_activate_var(struct s3c2410fb_info *fbi,
				   struct fb_var_screeninfo *var)
{
	int hs;

	fbi->regs.lcdcon1 &= ~S3C2410_LCDCON1_MODEMASK;
	fbi->regs.lcdcon1 &= ~S3C2410_LCDCON1_TFT;

	dprintk("%s: var->xres  = %d\n", __FUNCTION__, var->xres);
	dprintk("%s: var->yres  = %d\n", __FUNCTION__, var->yres);
	dprintk("%s: var->bpp   = %d\n", __FUNCTION__, var->bits_per_pixel);

	fbi->regs.lcdcon1 |= fbi->mach_info->type;

	if (fbi->mach_info->type == S3C2410_LCDCON1_TFT)
		switch (var->bits_per_pixel) {
		case 1:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_TFT1BPP;
			break;
		case 2:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_TFT2BPP;
			break;
		case 4:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_TFT4BPP;
			break;
		case 8:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_TFT8BPP;
			break;
		case 16:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_TFT16BPP;
			break;

		default:
			/* invalid pixel depth */
			dev_err(fbi->dev, "invalid bpp %d\n", var->bits_per_pixel);
		}
	else
		switch (var->bits_per_pixel) {
		case 1:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_STN1BPP;
			break;
		case 2:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_STN2GREY;
			break;
		case 4:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_STN4GREY;
			break;
		case 8:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_STN8BPP;
			break;
		case 12:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_STN12BPP;
			break;

		default:
			/* invalid pixel depth */
			dev_err(fbi->dev, "invalid bpp %d\n", var->bits_per_pixel);
		}

	/* check to see if we need to update sync/borders */

	if (!fbi->mach_info->fixed_syncs) {
		dprintk("setting vert: up=%d, low=%d, sync=%d\n",
			var->upper_margin, var->lower_margin,
			var->vsync_len);

		dprintk("setting horz: lft=%d, rt=%d, sync=%d\n",
			var->left_margin, var->right_margin,
			var->hsync_len);

		fbi->regs.lcdcon2 =
			S3C2410_LCDCON2_VBPD(var->upper_margin - 1) |
			S3C2410_LCDCON2_VFPD(var->lower_margin - 1) |
			S3C2410_LCDCON2_VSPW(var->vsync_len - 1);

		fbi->regs.lcdcon3 =
			S3C2410_LCDCON3_HBPD(var->right_margin - 1) |
			S3C2410_LCDCON3_HFPD(var->left_margin - 1);

		fbi->regs.lcdcon4 &= ~S3C2410_LCDCON4_HSPW(0xff);
		fbi->regs.lcdcon4 |=  S3C2410_LCDCON4_HSPW(var->hsync_len - 1);
	}

	/* update X/Y info */

	fbi->regs.lcdcon2 &= ~S3C2410_LCDCON2_LINEVAL(0x3ff);
	fbi->regs.lcdcon2 |=  S3C2410_LCDCON2_LINEVAL(var->yres - 1);

	switch(fbi->mach_info->type) {
		case S3C2410_LCDCON1_DSCAN4:
		case S3C2410_LCDCON1_STN8:
			hs = var->xres / 8;
			break;
		case S3C2410_LCDCON1_STN4:
			hs = var->xres / 4;
			break;
		default:
		case S3C2410_LCDCON1_TFT:
			hs = var->xres;
			break;

	}

	/* Special cases : STN color displays */
	if ( ((fbi->regs.lcdcon1 & S3C2410_LCDCON1_MODEMASK) == S3C2410_LCDCON1_STN8BPP) \
	  || ((fbi->regs.lcdcon1 & S3C2410_LCDCON1_MODEMASK) == S3C2410_LCDCON1_STN12BPP) ) {
		hs = hs * 3;
	}


	fbi->regs.lcdcon3 &= ~S3C2410_LCDCON3_HOZVAL(0x7ff);
	fbi->regs.lcdcon3 |=  S3C2410_LCDCON3_HOZVAL(hs - 1);

	if (var->pixclock > 0) {
		int clkdiv = s3c2410fb_calc_pixclk(fbi, var->pixclock);

		if (fbi->mach_info->type == S3C2410_LCDCON1_TFT) {
			clkdiv = (clkdiv / 2) -1;
			if (clkdiv < 0)
				clkdiv = 0;
		}
		else {
			clkdiv = (clkdiv / 2);
			if (clkdiv < 2)
				clkdiv = 2;
		}

		fbi->regs.lcdcon1 &= ~S3C2410_LCDCON1_CLKVAL(0x3ff);
		fbi->regs.lcdcon1 |=  S3C2410_LCDCON1_CLKVAL(clkdiv);
	}

	/* write new registers */

	dprintk("new register set:\n");
	dprintk("lcdcon[1] = 0x%08lx\n", fbi->regs.lcdcon1);
	dprintk("lcdcon[2] = 0x%08lx\n", fbi->regs.lcdcon2);
	dprintk("lcdcon[3] = 0x%08lx\n", fbi->regs.lcdcon3);
	dprintk("lcdcon[4] = 0x%08lx\n", fbi->regs.lcdcon4);
	dprintk("lcdcon[5] = 0x%08lx\n", fbi->regs.lcdcon5);

	writel(fbi->regs.lcdcon1 & ~S3C2410_LCDCON1_ENVID, S3C2410_LCDCON1);
	writel(fbi->regs.lcdcon2, S3C2410_LCDCON2);
	writel(fbi->regs.lcdcon3, S3C2410_LCDCON3);
	writel(fbi->regs.lcdcon4, S3C2410_LCDCON4);
	writel(fbi->regs.lcdcon5, S3C2410_LCDCON5);

	/* set lcd address pointers */
	s3c2410fb_set_lcdaddr(fbi);

	writel(fbi->regs.lcdcon1, S3C2410_LCDCON1);
}


/*
 *      s3c2410fb_set_par - Optional function. Alters the hardware state.
 *      @info: frame buffer structure that represents a single frame buffer
 *
 */
static int s3c2410fb_set_par(struct fb_info *info)
{
	struct s3c2410fb_info *fbi = info->par;
	struct fb_var_screeninfo *var = &info->var;

	switch (var->bits_per_pixel)
	{
		case 16:
			fbi->fb->fix.visual = FB_VISUAL_TRUECOLOR;
			break;
		case 1:
			 fbi->fb->fix.visual = FB_VISUAL_MONO01;
			 break;
		default:
			 fbi->fb->fix.visual = FB_VISUAL_PSEUDOCOLOR;
			 break;
	}

	fbi->fb->fix.line_length     = (var->width*var->bits_per_pixel)/8;

	/* activate this new configuration */

	s3c2410fb_activate_var(fbi, var);
	return 0;
}

static void schedule_palette_update(struct s3c2410fb_info *fbi,
				    unsigned int regno, unsigned int val)
{
	unsigned long flags;
	unsigned long irqen;

	local_irq_save(flags);

	fbi->palette_buffer[regno] = val;

	if (!fbi->palette_ready) {
		fbi->palette_ready = 1;

		/* enable IRQ */
		irqen = readl(S3C2410_LCDINTMSK);
		irqen &= ~S3C2410_LCDINT_FRSYNC;
		writel(irqen, S3C2410_LCDINTMSK);
	}

	local_irq_restore(flags);
}

/* from pxafb.c */
static inline unsigned int chan_to_field(unsigned int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static int s3c2410fb_setcolreg(unsigned regno,
			       unsigned red, unsigned green, unsigned blue,
			       unsigned transp, struct fb_info *info)
{
	struct s3c2410fb_info *fbi = info->par;
	unsigned int val;

	/* dprintk("setcol: regno=%d, rgb=%d,%d,%d\n", regno, red, green, blue); */

	switch (fbi->fb->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/* true-colour, use pseuo-palette */

		if (regno < 16) {
			u32 *pal = fbi->fb->pseudo_palette;

			val  = chan_to_field(red,   &fbi->fb->var.red);
			val |= chan_to_field(green, &fbi->fb->var.green);
			val |= chan_to_field(blue,  &fbi->fb->var.blue);

			pal[regno] = val;
		}
		break;

	case FB_VISUAL_PSEUDOCOLOR:
		if (regno < 256) {
			/* currently assume RGB 5-6-5 mode */
			val  = ((red   >>  0) & 0xf800);
			val |= ((green >>  5) & 0x07e0);
			val |= ((blue  >> 11) & 0x001f);

			writel(val, S3C2410_TFTPAL(regno));
			schedule_palette_update(fbi, regno, val);
		}

		break;

	default:
		return 1;   /* unknown type */
	}

	return 0;
}


/**
 *      s3c2410fb_blank
 *	@blank_mode: the blank mode we want.
 *	@info: frame buffer structure that represents a single frame buffer
 *
 *	Blank the screen if blank_mode != 0, else unblank. Return 0 if
 *	blanking succeeded, != 0 if un-/blanking failed due to e.g. a
 *	video mode which doesn't support it. Implements VESA suspend
 *	and powerdown modes on hardware that supports disabling hsync/vsync:
 *	blank_mode == 2: suspend vsync
 *	blank_mode == 3: suspend hsync
 *	blank_mode == 4: powerdown
 *
 *	Returns negative errno on error, or zero on success.
 *
 */
static int s3c2410fb_blank(int blank_mode, struct fb_info *info)
{
	dprintk("blank(mode=%d, info=%p)\n", blank_mode, info);

	if (mach_info == NULL)
		return -EINVAL;

	if (blank_mode == FB_BLANK_UNBLANK)
		writel(0x0, S3C2410_TPAL);
	else {
		dprintk("setting TPAL to output 0x000000\n");
		writel(S3C2410_TPAL_EN, S3C2410_TPAL);
	}

	return 0;
}

static int s3c2410fb_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", debug ? "on" : "off");
}
static int s3c2410fb_debug_store(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t len)
{
	if (mach_info == NULL)
		return -EINVAL;

	if (len < 1)
		return -EINVAL;

	if (strnicmp(buf, "on", 2) == 0 ||
	    strnicmp(buf, "1", 1) == 0) {
		debug = 1;
		printk(KERN_DEBUG "s3c2410fb: Debug On");
	} else if (strnicmp(buf, "off", 3) == 0 ||
		   strnicmp(buf, "0", 1) == 0) {
		debug = 0;
		printk(KERN_DEBUG "s3c2410fb: Debug Off");
	} else {
		return -EINVAL;
	}

	return len;
}


static DEVICE_ATTR(debug, 0666,
		   s3c2410fb_debug_show,
		   s3c2410fb_debug_store);

static struct fb_ops s3c2410fb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= s3c2410fb_check_var,
	.fb_set_par	= s3c2410fb_set_par,
	.fb_blank	= s3c2410fb_blank,
	.fb_setcolreg	= s3c2410fb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};


/*
 * s3c2410fb_map_video_memory():
 *	Allocates the DRAM memory for the frame buffer.  This buffer is
 *	remapped into a non-cached, non-buffered, memory region to
 *	allow palette and pixel writes to occur without flushing the
 *	cache.  Once this area is remapped, all virtual memory
 *	access to the video memory should occur at the new region.
 */
static int __init s3c2410fb_map_video_memory(struct s3c2410fb_info *fbi)
{
	dprintk("map_video_memory(fbi=%p)\n", fbi);

	fbi->map_size = PAGE_ALIGN(fbi->fb->fix.smem_len + PAGE_SIZE);
	fbi->map_cpu  = dma_alloc_writecombine(fbi->dev, fbi->map_size,
					       &fbi->map_dma, GFP_KERNEL);

	fbi->map_size = fbi->fb->fix.smem_len;

	if (fbi->map_cpu) {
		/* prevent initial garbage on screen */
		dprintk("map_video_memory: clear %p:%08x\n",
			fbi->map_cpu, fbi->map_size);
		memset(fbi->map_cpu, 0xf0, fbi->map_size);

		fbi->screen_dma		= fbi->map_dma;
		fbi->fb->screen_base	= fbi->map_cpu;
		fbi->fb->fix.smem_start  = fbi->screen_dma;

		dprintk("map_video_memory: dma=%08x cpu=%p size=%08x\n",
			fbi->map_dma, fbi->map_cpu, fbi->fb->fix.smem_len);
	}

	return fbi->map_cpu ? 0 : -ENOMEM;
}

static inline void s3c2410fb_unmap_video_memory(struct s3c2410fb_info *fbi)
{
	dma_free_writecombine(fbi->dev,fbi->map_size,fbi->map_cpu, fbi->map_dma);
}

static inline void modify_gpio(void __iomem *reg,
			       unsigned long set, unsigned long mask)
{
	unsigned long tmp;

	tmp = readl(reg) & ~mask;
	writel(tmp | set, reg);
}


/*
 * s3c2410fb_init_registers - Initialise all LCD-related registers
 */

static int s3c2410fb_init_registers(struct s3c2410fb_info *fbi)
{
	unsigned long flags;

	/* Initialise LCD with values from haret */

	local_irq_save(flags);

	/* modify the gpio(s) with interrupts set (bjd) */

	modify_gpio(S3C2410_GPCUP,  mach_info->gpcup,  mach_info->gpcup_mask);
	modify_gpio(S3C2410_GPCCON, mach_info->gpccon, mach_info->gpccon_mask);
	modify_gpio(S3C2410_GPDUP,  mach_info->gpdup,  mach_info->gpdup_mask);
	modify_gpio(S3C2410_GPDCON, mach_info->gpdcon, mach_info->gpdcon_mask);

	local_irq_restore(flags);

	writel(fbi->regs.lcdcon1, S3C2410_LCDCON1);
	writel(fbi->regs.lcdcon2, S3C2410_LCDCON2);
	writel(fbi->regs.lcdcon3, S3C2410_LCDCON3);
	writel(fbi->regs.lcdcon4, S3C2410_LCDCON4);
	writel(fbi->regs.lcdcon5, S3C2410_LCDCON5);

 	s3c2410fb_set_lcdaddr(fbi);

	dprintk("LPCSEL    = 0x%08lx\n", mach_info->lpcsel);
	writel(mach_info->lpcsel, S3C2410_LPCSEL);

	dprintk("replacing TPAL %08x\n", readl(S3C2410_TPAL));

	/* ensure temporary palette disabled */
	writel(0x00, S3C2410_TPAL);

	/* Enable video by setting the ENVID bit to 1 */
	fbi->regs.lcdcon1 |= S3C2410_LCDCON1_ENVID;
	writel(fbi->regs.lcdcon1, S3C2410_LCDCON1);
	return 0;
}

static void s3c2410fb_write_palette(struct s3c2410fb_info *fbi)
{
	unsigned int i;
	unsigned long ent;

	fbi->palette_ready = 0;

	for (i = 0; i < 256; i++) {
		if ((ent = fbi->palette_buffer[i]) == PALETTE_BUFF_CLEAR)
			continue;

		writel(ent, S3C2410_TFTPAL(i));

		/* it seems the only way to know exactly
		 * if the palette wrote ok, is to check
		 * to see if the value verifies ok
		 */

		if (readw(S3C2410_TFTPAL(i)) == ent)
			fbi->palette_buffer[i] = PALETTE_BUFF_CLEAR;
		else
			fbi->palette_ready = 1;   /* retry */
	}
}

static irqreturn_t s3c2410fb_irq(int irq, void *dev_id)
{
	struct s3c2410fb_info *fbi = dev_id;
	unsigned long lcdirq = readl(S3C2410_LCDINTPND);

	if (lcdirq & S3C2410_LCDINT_FRSYNC) {
		if (fbi->palette_ready)
			s3c2410fb_write_palette(fbi);

		writel(S3C2410_LCDINT_FRSYNC, S3C2410_LCDINTPND);
		writel(S3C2410_LCDINT_FRSYNC, S3C2410_LCDSRCPND);
	}

	return IRQ_HANDLED;
}

#ifdef CONFIG_FB_S3C24XX_PARAMETERS
static int __init s3c24xxfb_parse_options(struct device *pdev, char *options)
{
	char *this_opt;
	unsigned long vpixclock;


	mach_info = pdev->platform_data;

	if (!options || !*options)
		return 0;

	dev_info(pdev, "options are \"%s\"\n", options ? options : "null");

	/* could be made table driven or similar?... */
	while ((this_opt = strsep(&options, ",")) != NULL) 
	{
//		dev_err(pdev, "---->option: %s\n", this_opt);
		if (!strncmp(this_opt, "mode:", 5)) 
		{
			const char *name = this_opt+5;
			unsigned int namelen = strlen(name);
			int res_specified = 0, bpp_specified = 0;
			unsigned int xres = 0, yres = 0, bpp = 0;
			int yres_specified = 0;
			int i;

			for (i = namelen-1; i >= 0; i--) 
			{
				switch (name[i]) 
				{
				case '-':
					namelen = i;
					if (!bpp_specified && !yres_specified) 
					{
						bpp = simple_strtoul(&name[i+1], NULL, 0);
						bpp_specified = 1;
					} 
					else
						goto done;
					break;
				case 'x':
					if (!yres_specified) 
					{
						yres = simple_strtoul(&name[i+1], NULL, 0);
						yres_specified = 1;
					} 
					else
						goto done;
					break;
				case '0'...'9':
					break;
				default:
					goto done;
				}
			}
			if (i < 0 && yres_specified) 
			{
				xres = simple_strtoul(name, NULL, 0);
				res_specified = 1;
			}
	done:
			if (res_specified) 
			{
				dev_info(pdev, "overriding resolution: %dx%d\n", xres, yres);
				mach_info->xres.defval	= xres; 
				mach_info->xres.min		= xres; 
				mach_info->xres.max		= xres; 					
				mach_info->yres.defval	= yres;
				mach_info->yres.min		= yres;
				mach_info->yres.max		= yres;										

			}
			if (bpp_specified)
			{	
				switch (bpp) 
				{
				case 1:
				case 2:
				case 4:
				case 8:
				case 16:
					mach_info->bpp.defval = bpp;
					mach_info->bpp.min	  =	bpp;
					mach_info->bpp.max	  = bpp;												
					mach_info->regs.lcdcon1 = S3C2410_LCDCON1_TFT16BPP;
					dev_info(pdev, "overriding bit depth: %d\n", bpp);
					break;
				case 24:
					mach_info->bpp.defval = bpp;
					mach_info->bpp.min	  =	bpp;
					mach_info->bpp.max	  = bpp;												
					mach_info->regs.lcdcon1 = S3C2410_LCDCON1_TFT24BPP;
					dev_info(pdev, "overriding bit depth: %d\n", bpp);
					break;
				default:
					dev_err(pdev, "Depth %d is not valid\n", bpp);
				}
			}	
		} 
 		else if (!strncmp(this_opt, "pixclock:", 9)) 
 		{
        	vpixclock = simple_strtoul(this_opt+9, NULL, 0);
			mach_info->regs.lcdcon1 |= S3C2410_LCDCON1_CLKVAL((13300000-(2*vpixclock))/(2*vpixclock));
		}
		else if (!strncmp(this_opt, "upper:", 6)) 
		{
			mach_info->regs.lcdcon2 = S3C2410_LCDCON2_VFPD(simple_strtoul(this_opt+6, NULL, 0));
		} 
		else if (!strncmp(this_opt, "lower:", 6)) 
		{
			mach_info->regs.lcdcon2 |= S3C2410_LCDCON2_VBPD(simple_strtoul(this_opt+6, NULL, 0));
		} 
		else if (!strncmp(this_opt, "vsynclen:", 9)) 
		{
			mach_info->regs.lcdcon2 |= S3C2410_LCDCON2_VSPW(simple_strtoul(this_opt+9, NULL, 0));
		} 
		else if (!strncmp(this_opt, "left:", 5)) 
		{
			mach_info->regs.lcdcon3 = S3C2410_LCDCON3_HFPD(simple_strtoul(this_opt+5, NULL, 0));
		} 
		else if (!strncmp(this_opt, "right:", 6)) 
		{
			mach_info->regs.lcdcon3 |= S3C2410_LCDCON3_HBPD(simple_strtoul(this_opt+6, NULL, 0));
		}	 
		else if (!strncmp(this_opt, "hsynclen:", 9)) 
		{
			mach_info->regs.lcdcon4 = S3C2410_LCDCON4_HSPW(simple_strtoul(this_opt+9, NULL, 0));
		} 
		else if (!strncmp(this_opt, "hsync:", 6)) 
		{
			if (simple_strtoul(this_opt+6, NULL, 0) == 0) 
			{
				dev_info(pdev, "override hsync: Active Low\n");
				mach_info->regs.lcdcon5 &= ~S3C2410_LCDCON5_INVVLINE;
			} 
			else 
			{
				dev_info(pdev, "override hsync: Active High\n");
				mach_info->regs.lcdcon5 |= S3C2410_LCDCON5_INVVLINE;
			}
		} 
		else if (!strncmp(this_opt, "vsync:", 6)) 
		{
			if (simple_strtoul(this_opt+6, NULL, 0) == 0) 
			{
				dev_info(pdev, "override vsync: Active Low\n");
				mach_info->regs.lcdcon5 &= ~S3C2410_LCDCON5_INVVFRAME;
			} 
			else 
			{
				dev_info(pdev, "override vsync: Active High\n");
				mach_info->regs.lcdcon5 |= S3C2410_LCDCON5_INVVFRAME;
			}
		} 
		else if (!strncmp(this_opt, "output:", 7)) 
		{
			if (simple_strtoul(this_opt+7, NULL, 0) == 0) 
			{
				dev_info(pdev, "override output enable: active low\n");
				mach_info->regs.lcdcon5 &= ~S3C2410_LCDCON5_INVVD;
			} 
			else 
			{
				dev_info(pdev, "override output enable: active high\n");
				mach_info->regs.lcdcon5 |= S3C2410_LCDCON5_INVVD;
			}
		} 
		else if (!strncmp(this_opt, "outputen:", 9)) 
		{
			if (simple_strtoul(this_opt+9, NULL, 0) == 0) 
			{
				dev_info(pdev, "override output enable: active low\n");
				mach_info->regs.lcdcon5 &= ~S3C2410_LCDCON5_INVVDEN;
			} 
			else 
			{
				dev_info(pdev, "override output enable: active high\n");
				mach_info->regs.lcdcon5 |= S3C2410_LCDCON5_INVVDEN;
			}
		} 
		else if (!strncmp(this_opt, "pixclockpol:", 12)) 
		{
			if (simple_strtoul(this_opt+12, NULL, 0) == 0) 
			{
				dev_info(pdev, "override pixel clock polarity: falling edge\n");
				mach_info->regs.lcdcon5 &= ~S3C2410_LCDCON5_INVVCLK;
			} 
			else 
			{
				dev_info(pdev, "override pixel clock polarity: rising edge\n");
				mach_info->regs.lcdcon5 |= S3C2410_LCDCON5_INVVCLK;
			}
		} 
		else if (!strncmp(this_opt, "panel:", 6)) 
		{
			long int panel_index;
			int 	 num_panels = ARRAY_SIZE(known_lcd_panels);
			
			/* First check for index, which allows
			 * to short circuit this mess */
			panel_index = simple_strtoul(this_opt+6, NULL, 0);
			if ((panel_index < 0) || (panel_index >= num_panels)) {
				printk("Panel %s not supported!(default : 640x480)", this_opt);
				panel_index = 2;
			}
			mach_info = &known_lcd_panels[panel_index];
		}
		else 
		{
			dev_err(pdev, "unknown option: %s\n", this_opt);
			return -EINVAL;
		}
	}

	mach_info->regs.lcdcon1 |= S3C2410_LCDCON1_TFT;
	mach_info->regs.lcdcon2 |= S3C2410_LCDCON2_LINEVAL(mach_info->yres.defval-1);
	mach_info->regs.lcdcon3 |= S3C2410_LCDCON3_HOZVAL(mach_info->xres.defval-1);
	mach_info->regs.lcdcon4 |= S3C2410_LCDCON4_MVAL(0);
	mach_info->regs.lcdcon5 |= S3C2410_LCDCON5_FRM565 | S3C2410_LCDCON5_PWREN | S3C2410_LCDCON5_HWSWP;

	return 0;
}
#endif

static char driver_name[]="s3c2410fb";

static int __init s3c2410fb_probe(struct platform_device *pdev)
{
	struct s3c2410fb_info *info;
	struct fb_info	   *fbinfo;
	struct s3c2410fb_hw *mregs;
	int ret;
	int irq;
	int i;
	u32 lcdcon1;

	mach_info = pdev->dev.platform_data;
	if (mach_info == NULL) {
		dev_err(&pdev->dev,"no platform data for lcd, cannot attach\n");
		return -EINVAL;
	}

#ifdef CONFIG_FB_S3C24XX_PARAMETERS
    /* Point to the panel selected */
	ret = s3c24xxfb_parse_options(&pdev->dev, g_options);
    if (ret < 0)
		return -ENOENT;
#endif
		
   
	mregs = &mach_info->regs;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no irq for device\n");
		return -ENOENT;
	}

	fbinfo = framebuffer_alloc(sizeof(struct s3c2410fb_info), &pdev->dev);
	if (!fbinfo) {
		return -ENOMEM;
	}

	info = fbinfo->par;
	info->fb = fbinfo;
 	info->dev = &pdev->dev;

	platform_set_drvdata(pdev, fbinfo);

	dprintk("devinit\n");

	strcpy(fbinfo->fix.id, driver_name);

	memcpy(&info->regs, &mach_info->regs, sizeof(info->regs));
	
	/* Stop the video and unset ENVID if set */
	info->regs.lcdcon1 &= ~S3C2410_LCDCON1_ENVID;
	lcdcon1 = readl(S3C2410_LCDCON1);
	writel(lcdcon1 & ~S3C2410_LCDCON1_ENVID, S3C2410_LCDCON1);

	info->mach_info		    	= pdev->dev.platform_data;
                            	
	fbinfo->fix.type	    	= FB_TYPE_PACKED_PIXELS;
	fbinfo->fix.type_aux		= 0;
	fbinfo->fix.xpanstep		= 0;
	fbinfo->fix.ypanstep		= 0;
	fbinfo->fix.ywrapstep		= 0;
	fbinfo->fix.accel	    	= FB_ACCEL_NONE;
                            	
	fbinfo->var.nonstd	    	= 0;
	fbinfo->var.activate		= FB_ACTIVATE_NOW;
	fbinfo->var.height	    	= mach_info->height;
	fbinfo->var.width	    	= mach_info->width;
	fbinfo->var.accel_flags 	= 0;
	fbinfo->var.vmode	    	= FB_VMODE_NONINTERLACED;
                            	
	fbinfo->fbops		    	= &s3c2410fb_ops;
	fbinfo->flags		    	= FBINFO_FLAG_DEFAULT;
	fbinfo->pseudo_palette  	= &info->pseudo_pal;

	fbinfo->var.xres	    	= mach_info->xres.defval;
	fbinfo->var.xres_virtual    = mach_info->xres.defval;
	fbinfo->var.yres	    	= mach_info->yres.defval;
	fbinfo->var.yres_virtual    = mach_info->yres.defval;
	fbinfo->var.bits_per_pixel  = mach_info->bpp.defval;

	fbinfo->var.upper_margin    = S3C2410_LCDCON2_GET_VBPD(mregs->lcdcon2) + 1;
	fbinfo->var.lower_margin    = S3C2410_LCDCON2_GET_VFPD(mregs->lcdcon2) + 1;
	fbinfo->var.vsync_len	    = S3C2410_LCDCON2_GET_VSPW(mregs->lcdcon2) + 1;

	fbinfo->var.left_margin	    = S3C2410_LCDCON3_GET_HFPD(mregs->lcdcon3) + 1;
	fbinfo->var.right_margin    = S3C2410_LCDCON3_GET_HBPD(mregs->lcdcon3) + 1;
	fbinfo->var.hsync_len	    = S3C2410_LCDCON4_GET_HSPW(mregs->lcdcon4) + 1;

	fbinfo->var.red.offset      = 11;
	fbinfo->var.green.offset    = 5;
	fbinfo->var.blue.offset     = 0;
	fbinfo->var.transp.offset   = 0;
	fbinfo->var.red.length      = 5;
	fbinfo->var.green.length    = 6;
	fbinfo->var.blue.length     = 5;
	fbinfo->var.transp.length   = 0;
	fbinfo->fix.smem_len        =	mach_info->xres.max *
									mach_info->yres.max *
									mach_info->bpp.max / 8;
	
	for (i = 0; i < 256; i++)
		info->palette_buffer[i] = PALETTE_BUFF_CLEAR;

	if (!request_mem_region((unsigned long)S3C24XX_VA_LCD, SZ_1M, "s3c2410-lcd")) {
		ret = -EBUSY;
		goto dealloc_fb;
	}


	dprintk("got LCD region\n");

	ret = request_irq(irq, s3c2410fb_irq, IRQF_DISABLED, pdev->name, info);
	if (ret) {
		dev_err(&pdev->dev, "cannot get irq %d - err %d\n", irq, ret);
		ret = -EBUSY;
		goto release_mem;
	}

	info->clk = clk_get(NULL, "lcd");
	if (!info->clk || IS_ERR(info->clk)) {
		printk(KERN_ERR "failed to get lcd clock source\n");
		ret = -ENOENT;
		goto release_irq;
	}

	clk_enable(info->clk);
	dprintk("got and enabled clock\n");

	msleep(1);

	/* Initialize video memory */
	ret = s3c2410fb_map_video_memory(info);
	if (ret) {
		printk( KERN_ERR "Failed to allocate video RAM: %d\n", ret);
		ret = -ENOMEM;
		goto release_clock;
	}
	dprintk("got video memory\n");

	ret = s3c2410fb_init_registers(info);

#ifndef CONFIG_FB_S3C24XX_PARAMETERS
	ret = s3c2410fb_check_var(&fbinfo->var, fbinfo);
#endif
	ret = register_framebuffer(fbinfo);
	if (ret < 0) {
		printk(KERN_ERR "Failed to register framebuffer device: %d\n", ret);
		goto free_video_memory;
	}

	/* create device files */
	device_create_file(&pdev->dev, &dev_attr_debug);
	printk("fb%d: %s frame buffer device ( %dx%dx%d )\n", 
			fbinfo->node, fbinfo->fix.id, 
			fbinfo->var.xres, fbinfo->var.yres,fbinfo->var.bits_per_pixel);

#if !defined(CONFIG_FRAMEBUFFER_CONSOLE) && defined(CONFIG_LOGO)
	if (fb_prepare_logo(&fbinfo, FB_ROTATE_UR)) {
		printk("Start display and show logo\n");
		/* Start display and show logo on boot */
		fb_set_cmap(&fbinfo->cmap, &fbinfo);
		fb_show_logo(&fbinfo, FB_ROTATE_UR);
	}	
#endif

	return 0;

free_video_memory:
	s3c2410fb_unmap_video_memory(info);
release_clock:
	clk_disable(info->clk);
	clk_put(info->clk);
release_irq:
	free_irq(irq,info);
release_mem:
 	release_mem_region((unsigned long)S3C24XX_VA_LCD, S3C24XX_SZ_LCD);
dealloc_fb:
	framebuffer_release(fbinfo);
	return ret;
}

/* s3c2410fb_stop_lcd
 *
 * shutdown the lcd controller
*/

static void s3c2410fb_stop_lcd(struct s3c2410fb_info *fbi)
{
	unsigned long flags;

	local_irq_save(flags);

	fbi->regs.lcdcon1 &= ~S3C2410_LCDCON1_ENVID;
	writel(fbi->regs.lcdcon1, S3C2410_LCDCON1);

	local_irq_restore(flags);
}

/*
 *  Cleanup
 */
static int s3c2410fb_remove(struct platform_device *pdev)
{
	struct fb_info	   *fbinfo = platform_get_drvdata(pdev);
	struct s3c2410fb_info *info = fbinfo->par;
	int irq;

	s3c2410fb_stop_lcd(info);
	msleep(1);

	s3c2410fb_unmap_video_memory(info);

 	if (info->clk) {
 		clk_disable(info->clk);
 		clk_put(info->clk);
 		info->clk = NULL;
	}

	irq = platform_get_irq(pdev, 0);
	free_irq(irq,info);
	release_mem_region((unsigned long)S3C24XX_VA_LCD, S3C24XX_SZ_LCD);
	unregister_framebuffer(fbinfo);

	return 0;
}

#ifdef CONFIG_PM

/* suspend and resume support for the lcd controller */

static int s3c2410fb_suspend(struct platform_device *dev, pm_message_t state)
{
	struct fb_info	   *fbinfo = platform_get_drvdata(dev);
	struct s3c2410fb_info *info = fbinfo->par;

	s3c2410fb_stop_lcd(info);

	/* sleep before disabling the clock, we need to ensure
	 * the LCD DMA engine is not going to get back on the bus
	 * before the clock goes off again (bjd) */

	msleep(1);
	clk_disable(info->clk);

	return 0;
}

static int s3c2410fb_resume(struct platform_device *dev)
{
	struct fb_info	   *fbinfo = platform_get_drvdata(dev);
	struct s3c2410fb_info *info = fbinfo->par;

	clk_enable(info->clk);
	msleep(1);

	s3c2410fb_init_registers(info);

	return 0;
}

#else
#define s3c2410fb_suspend NULL
#define s3c2410fb_resume  NULL
#endif

static struct platform_driver s3c2410fb_driver = {
	.probe		= s3c2410fb_probe,
	.remove		= s3c2410fb_remove,
	.suspend	= s3c2410fb_suspend,
	.resume		= s3c2410fb_resume,
	.driver		= {
		.name	= "s3c2410-lcd",
		.owner	= THIS_MODULE,
	},
};

#ifndef MODULE
int __devinit s3c24xxfb_setup(char *options)
{
	char	*this_opt;
	int 	num_panels = ARRAY_SIZE(known_lcd_panels);
	int 	panel_idx = -1;
	
#ifdef CONFIG_FB_S3C24XX_PARAMETERS
    if (options)
        strlcpy(g_options, options, sizeof(g_options));
#endif
    return 0;
}
#else
#ifdef CONFIG_FB_S3C24XX_PARAMETERS
module_param_string(options, g_options, sizeof(g_options), 0);
MODULE_PARM_DESC(options, "LCD parameters (see Documentation/fb/s3c24xxfb.txt)");
#endif
#endif

int __devinit s3c2410fb_init(void)
{
#ifndef MODULE
    char *option = NULL;

    if (fb_get_options("ezfb", &option))
        return -ENODEV;
    s3c24xxfb_setup(option);
#endif
	return platform_driver_register(&s3c2410fb_driver);
}

static void __exit s3c2410fb_cleanup(void)
{
	platform_driver_unregister(&s3c2410fb_driver);
}


module_init(s3c2410fb_init);
module_exit(s3c2410fb_cleanup);

MODULE_AUTHOR("Arnaud Patard <arnaud.patard@rtp-net.org>, Ben Dooks <ben-linux@fluff.org>");
MODULE_DESCRIPTION("Framebuffer driver for the s3c2410");
MODULE_LICENSE("GPL");
