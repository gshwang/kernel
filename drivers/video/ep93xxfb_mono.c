/*
 *  drivers/video/ep93xxfb_mono.c -- grayscale on mono LCD driver for
 *  Cirrus Logic EP93xx.
 *
 *  Copyright (C) 2007 Cirrus Logic
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 *  
 *  This driver works for the following two LCD:
 *  SHARP LM121VB1T01   -   A dual scan 640x480 monochrome LCD.
 *  HOSIDEN HLM6323     -   A single scan 320x240 monochrome LCD.
 *
 *  And support two gray modes:
 *  8 levels of gray    -   Actually is 7 levels of gray. Two of the levels
 *                          have the same gray.
 *  16 levels of gray   -   Extending the gray levels by switching the LUT
 *                          for each frame.
 *  
 *  HW connection for SHARP LM121VB1T01:
 *      P12 <------> LCD_U0
 *      P8  <------> LCD_U1
 *      P4  <------> LCD_U2
 *      P0  <------> LCD_U3
 *      P14 <------> LCD_L0
 *      P10 <------> LCD_L1
 *      P6  <------> LCD_L2
 *      P2  <------> LCD_L3
 *  HW connection for HOSIDEN HLM6323:
 *      P12 <------> LCD_0
 *      P8  <------> LCD_1
 *      P4  <------> LCD_2
 *      P0  <------> LCD_3
 *      
 */

#include <linux/version.h>
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
#include <asm/hardware.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)
#include <linux/platform_device.h>
#else
#define     IRQ_EP93XX_VSYNC        IRQ_VSYNC
#endif


#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
#if defined(CONFIG_MACH_EDB9312) || defined(CONFIG_MACH_EDB9315) || \
    defined(CONFIG_MACH_EDB9307) || defined(CONFIG_MACH_EDB9301) || \
    defined(CONFIG_MACH_EDB9302)

#define CONFIG_EP93XX_SDCS3
#else
#define CONFIG_EP93XX_SDCS0
#endif
#endif

#undef DEBUG
#ifdef DEBUG
#define DPRINTK( fmt, arg... )  printk( fmt, ##arg )
#else
#define DPRINTK( fmt, arg... )
#endif

#define FBDEV_NAME "ep93xxfb"

#define ep93xxfb_lock_outl(value, reg)              \
{                                              \
    outl(RASTER_SWLOCK_VALUE, RASTER_SWLOCK);  \
    outl(value, reg);                          \
    DPRINTK(#reg"=0x%08x\n", (unsigned int)(value));         \
}

#define ep93xxfb_outl(value, reg)              \
{                                              \
    outl(value, reg);                          \
    DPRINTK(#reg"=0x%08x\n", (unsigned int)(value));         \
}

static unsigned int pseudo_palette[256];

struct ep93xxfb_mono_videomodes
{
    const char *name;

    unsigned long hres; 	// Horizontal Valid
    unsigned long vres;		// Vertical Valid
    unsigned int  freq;
    unsigned int  dualscan;
    unsigned int  bpp;
    unsigned int  graylevel;

    void (*configure)(unsigned char value);
    void (*on)(unsigned char value);
    void (*off)(unsigned char value);
};

struct ep93xxfb_mono_info
{
    dma_addr_t          fb_phys;
    void	            *fb_log;
    unsigned long       fb_size;
    unsigned long		fb_actsize;

    unsigned int        xres;
    unsigned int        yres;

    unsigned int        freq;
    unsigned int        dualscan;
    unsigned int        bpp;
    unsigned int        graylevel;

    void (*configure)(unsigned char value);
    void (*on)(unsigned char value);
    void (*off)(unsigned char value);
};


void LM121VB1T01_configure(unsigned char value);
void HOSIDEN_HLM6323_configure(unsigned char value);

static int vmode = 1;

static struct ep93xxfb_mono_info epinfo;
static struct ep93xxfb_mono_videomodes ep93xxfb_vmods[] = 
{
    {
        "SHARP-LM121VB1T01-8GRAY",
        640, 480, 100, 
        1,  //dual scan
        4,  //4bpp
        8,  //8-level grayscale
        LM121VB1T01_configure,
        NULL,
        NULL,
    },
    {
        "SHARP-LM121VB1T01-16GRAY",
        640, 480, 120,
        1,  //dual scan
        4,  //4bpp
        16, //16-level grayscale
        LM121VB1T01_configure,
        NULL,
        NULL,
    },
    {
        "HOSIDEN HLM6323",
        320, 240, 115,
        0,  //single scan
        4,  //4bpp
        8,  //8-level grayscale
        HOSIDEN_HLM6323_configure,
        NULL,
        NULL,
    },
    {
        "HOSIDEN HLM6323",
        320, 240, 115,
        0,  //single scan
        4,  //4bpp
        16, //16-level grayscale
        HOSIDEN_HLM6323_configure,
        NULL,
        NULL,
    },
};


#define EP93XX_GS_OFFSET(lut, frame, pixel) ( (lut) + ( (pixel) << 2) + ((frame) << 5 ))

static unsigned long DY_LUT[2][16];

static unsigned long GSLUT[32] = 
{
    0x00070000, 0x00070000, 0x00070000, 0x00070000,     /*0%*/ 
    0x00078241, 0x00074182, 0x00071428, 0x00072814,     /*25%*/
    0x00000412, 0x00000241, 0x00000124, 0x00000000,     /*33%*/
    0x0007aa55, 0x000755aa, 0x000755aa, 0x0007aa55,     /*50%*/
    0x00000bed, 0x00000dbe, 0x00000edb, 0x00000000,     /*66%*/
    0x00077dbe, 0x0007be7d, 0x0007ebd7, 0x0007d7eb,     /*75%*/
    0x0007ffff, 0x0007ffff, 0x0007ffff, 0x0007ffff,     /*100%*/
    0x0007ffff, 0x0007ffff, 0x0007ffff, 0x0007ffff,
};

static void ep93xxfb_8gray_palette_init(void)
{
    unsigned int cont, i, n;
    unsigned int frame, pixval, gslut;
    cont = inl(LUTCONT);
    for (i=0; i< 16; i++)
    {
        n = (i & 0xe) << 4;
        outl( n, (COLOR_LUT+(i<<2)) );
    }
    for (pixval=0; pixval < 8; pixval++) 
    {
        for (frame=0; frame < 4; frame++) 
        {
            gslut = GSLUT[pixval*4 + frame];
            outl(gslut,EP93XX_GS_OFFSET(GS_LUT,  frame, pixval));
        }
    }
    outl( cont ^ LUTCONT_RAM1, LUTCONT );
}

static void ep93xxfb_16gray_palette_switch(int index)
{
    unsigned int cont, i, n;
    cont = inl(LUTCONT);
    n = index & 0x1;
    for (i=0; i< 16; i++)
    {
        outl( DY_LUT[n][i], (COLOR_LUT+(i<<2)) );
    }
    outl( cont ^ LUTCONT_RAM1, LUTCONT );
}

static void ep93xxfb_16gray_palette_init(void)
{
    int i;
    unsigned int cont;
    unsigned int frame, pixval, gslut;
    int split_table[16][2] =
    {
        {0,  0 },
        {0,  2 },
        {1,  1 },
        {3,  0 },

        {2,  2 },
        {4,  0 },
        {3,  2 },
        {4,  2 },

        {3,  3 },       //  {6,  0 },
        {3,  4 },
        {4,  4 },
        {6,  2 },

        {5,  5 },
        {3,  6 },
        {4,  6 },
        {6,  6 },
    };

    cont = inl(LUTCONT);
    for (i=0; i< 16; i++)
    {
        DY_LUT[0][i]=split_table[i][0] << 5;
        DY_LUT[1][i]=split_table[i][1] << 5;

        outl( DY_LUT[0][i], (COLOR_LUT+(i<<2)) );
    }

    for (pixval=0; pixval < 8; pixval++) 
    {
        for (frame=0; frame < 4; frame++) 
        {
            gslut = GSLUT[pixval*4 + frame];
            outl(gslut,EP93XX_GS_OFFSET(GS_LUT,  frame, pixval));
            outl(gslut,EP93XX_GS_OFFSET(GS_LUT2,  frame, pixval));
            outl(gslut,EP93XX_GS_OFFSET(GS_LUT3,  frame, pixval));
        }
    }
    outl( cont ^ LUTCONT_RAM1, LUTCONT );
}

static int ep93xxfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
    struct fb_var_screeninfo tmp_var;
    DPRINTK("ep93xxfb_check_var - enter\n");

    memcpy (&tmp_var, var, sizeof (tmp_var));

    if (var->xres_virtual != var->xres)
        var->xres_virtual = var->xres;
    if (var->yres_virtual < var->yres)
        var->yres_virtual = var->yres;

    if (var->xoffset < 0)
        var->xoffset = 0;
    if (var->yoffset < 0)
        var->yoffset = 0;

    switch (tmp_var.bits_per_pixel) 
    {
        case 4:
            break;									  
        default:
            return -EINVAL;    
    }

    DPRINTK("ep93xxfb_check_var - exit\n");
    return 0;
}

static int ep93xxfb_set_par(struct fb_info *info)
{
    DPRINTK("ep93xxfb_set_par\n");	
    switch (info->var.bits_per_pixel) {
        case 4:
            info->fix.visual = FB_VISUAL_PSEUDOCOLOR;
            break;

        default:
            return -EINVAL;
    }

    return 0;
}																					       


static int ep93xxfb_blank(int blank_mode,struct fb_info *info)
{
    unsigned long attribs;
    DPRINTK("ep93xxfb_blank - enter\n");	
    attribs = inl(VIDEOATTRIBS);

    if (blank_mode) {
        if (epinfo.off)
            (epinfo.off)( 0 );

        ep93xxfb_lock_outl(attribs & ~(VIDEOATTRIBS_DATAEN | 
                    VIDEOATTRIBS_SYNCEN | VIDEOATTRIBS_PCLKEN |
                    VIDEOATTRIBS_EN), VIDEOATTRIBS);
    }
    else {

        if (epinfo.configure)
            (epinfo.configure)( (unsigned char) epinfo.graylevel );
        if (epinfo.on)
            (epinfo.on)( 0 );
    }
    return 0;
}

static void ep93xxfb_get_par(struct fb_info *info)
{

    DPRINTK("ep93xxfb_get_par - enter\n");

    epinfo.configure = ep93xxfb_vmods[vmode].configure;
    epinfo.on = ep93xxfb_vmods[vmode].on;
    epinfo.off = ep93xxfb_vmods[vmode].off;

    epinfo.freq = ep93xxfb_vmods[vmode].freq;		
    epinfo.dualscan = ep93xxfb_vmods[vmode].dualscan;		
    epinfo.bpp = ep93xxfb_vmods[vmode].bpp;		
    epinfo.graylevel = ep93xxfb_vmods[vmode].graylevel;		

    epinfo.xres = ep93xxfb_vmods[vmode].hres;
    epinfo.yres = ep93xxfb_vmods[vmode].vres;

}

static int ep93xxfb_alloc_videomem(void)
{
    unsigned long adr,size,pgsize;
    int order;

    DPRINTK("ep93xxfb_alloc_videomem - enter \n");

    epinfo.fb_log = NULL;
    epinfo.fb_size = epinfo.xres*epinfo.yres*epinfo.bpp/8;
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
    else
        return -ENOMEM;

    memset(epinfo.fb_log,0x00,epinfo.fb_size);

    DPRINTK("   fb_log_addres = 0x%x\n",    (unsigned int)epinfo.fb_log);
    DPRINTK("   fb_phys_address = 0x%x\n",  (unsigned int)epinfo.fb_phys);
    DPRINTK("   fb_size = %lu\n",           (unsigned long)epinfo.fb_size);
    DPRINTK("   fb_page_order = %d\n",      (unsigned int)order);
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
    DPRINTK("ep93xxfb_release_videomem - exit \n");
}

static void ep93xxfb_setinfo(struct fb_info *info)
{

    DPRINTK("ep93xxfb_setinfo - enter \n");
    info->pseudo_palette = pseudo_palette;
    info->var.xres = epinfo.xres;
    info->var.yres = epinfo.yres;
    info->var.xres_virtual = epinfo.xres;
    info->var.yres_virtual = epinfo.yres;

    info->var.bits_per_pixel = epinfo.bpp;
    info->var.red.length = epinfo.bpp;
    info->var.green.length = epinfo.bpp;
    info->var.blue.length = epinfo.bpp;
    info->fix.visual = FB_VISUAL_PSEUDOCOLOR;
    info->var.red.offset = 0;
    info->var.green.offset =0;
    info->var.blue.offset = 0;

    info->fix.smem_start = epinfo.fb_phys;
    info->fix.smem_len = epinfo.fb_size;
    info->fix.type = FB_TYPE_PACKED_PIXELS;
    info->fix.line_length = (epinfo.xres * epinfo.bpp) / 8;
    info->fix.accel = FB_ACCEL_NONE;           
    info->screen_base = epinfo.fb_log;
    info->fix.ypanstep = 1;
    info->fix.ywrapstep = 1;		    

    DPRINTK("ep93xxfb_setinfo - exit \n");
}

static int ep93xxfb_config(struct fb_info *info)
{
    DPRINTK("ep93xxfb_config - enter\n"); 

    ep93xxfb_get_par( info );
    if( ep93xxfb_alloc_videomem() != 0 ) {
        printk("Unable to allocate video memory\n");
        return -ENOMEM;
    }

    /* set video memory parameters */
    ep93xxfb_outl(epinfo.fb_phys, VIDSCRNPAGE);
    if(epinfo.dualscan)
    {
        ep93xxfb_outl(epinfo.fb_phys + (epinfo.bpp*epinfo.xres*epinfo.yres/16)
                , VIDSCRNHPG);
    }

    DPRINTK("   fb_phys = 0x%x\n",  inl(VIDSCRNPAGE) );
    DPRINTK("   fb_phys_hpg = 0x%x\n",  inl(VIDSCRNHPG));

    ep93xxfb_outl(epinfo.yres , SCRNLINES);
    ep93xxfb_outl(((epinfo.xres * epinfo.bpp) / 32) - 1, LINELENGTH);
    ep93xxfb_outl((epinfo.xres * epinfo.bpp) / 32, VLINESTEP);

    if(epinfo.configure)
        (epinfo.configure)( (unsigned char) epinfo.graylevel );

    ep93xxfb_setinfo( info );


    DPRINTK("ep93xxfb_config - exit\n");
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

    ps = (pll & SYSCON_CLKSET1_PLL1_PS_MASK) >>	SYSCON_CLKSET1_PLL1_PS_SHIFT;
    fb1 = ((pll & SYSCON_CLKSET1_PLL1_X1FBD1_MASK) >> SYSCON_CLKSET1_PLL1_X1FBD1_SHIFT);
    fb2 = ((pll & SYSCON_CLKSET1_PLL1_X2FBD2_MASK) >> SYSCON_CLKSET1_PLL1_X2FBD2_SHIFT);
    ipd = ((pll & SYSCON_CLKSET1_PLL1_X2IPD_MASK) >> SYSCON_CLKSET1_PLL1_X2IPD_SHIFT);

    freq = (((0x00e10000 * (fb1+1)) / (ipd+1)) * (fb2+1)) >> ps;
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

    f = SYSCON_VIDDIV_VENA | (esel ? SYSCON_VIDDIV_ESEL : 0) |
        (psel ? SYSCON_VIDDIV_PSEL : 0) |
        (pdiv << SYSCON_VIDDIV_PDIV_SHIFT) |
        (div << SYSCON_VIDDIV_VDIV_SHIFT);
    outl(0xaa, SYSCON_SWLOCK);
    outl(f, SYSCON_VIDDIV);

    return freq + err;
}

static int interrupt_hooked = 0;
static int vs_counter = 0;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,17)
static irqreturn_t ep93xxfb_irq_handler(int i, void *blah)
#else
static irqreturn_t ep93xxfb_irq_handler(int i, void *blah, struct pt_regs *regs)
#endif
{

    outl(RASTER_SWLOCK_VALUE, RASTER_SWLOCK);
    outl( 
#ifdef CONFIG_EP93XX_SDCS0
            (0 << VIDEOATTRIBS_SDSEL_SHIFT) |
#endif
#ifdef CONFIG_EP93XX_SDCS1
            (1 << VIDEOATTRIBS_SDSEL_SHIFT) |
#endif
#ifdef CONFIG_EP93XX_SDCS2
            (2 << VIDEOATTRIBS_SDSEL_SHIFT) |
#endif
#ifdef CONFIG_EP93XX_SDCS3
            (3 << VIDEOATTRIBS_SDSEL_SHIFT) |
#endif
            VIDEOATTRIBS_VCPOL | VIDEOATTRIBS_HSPOL | 
            VIDEOATTRIBS_DATAEN | VIDEOATTRIBS_SYNCEN | VIDEOATTRIBS_INVCLK |
            VIDEOATTRIBS_PCLKEN | VIDEOATTRIBS_EN | VIDEOATTRIBS_INTEN ,
            VIDEOATTRIBS       ); 

    ep93xxfb_16gray_palette_switch(vs_counter++);

    return IRQ_HANDLED;
}

void LM121VB1T01_configure(unsigned char value)
{

    int n;
    unsigned long attribs;
    printk("LM121VB1T01_configure\n");		

    switch(value)
    {
        case 8:
            ep93xxfb_8gray_palette_init();
            break;
        case 16:
            ep93xxfb_16gray_palette_init();
            break;
        default:
            return;
    }

    SysconSetLocked(SYSCON_DEVCFG, (inl(SYSCON_DEVCFG) & ~SYSCON_DEVCFG_EXVC) | SYSCON_DEVCFG_RasOnP3);

    ep93xx_set_video_div(epinfo.freq*240*1280);

    ep93xxfb_lock_outl( 0x00000000   ,   VIDEOATTRIBS       ); 

    n = 240;
    ep93xxfb_lock_outl( n + 3  ,   VLINESTOTAL        ); 
    ep93xxfb_lock_outl( ((n)<<16) + n+1   ,   VSYNCSTRTSTOP      ); 
    ep93xxfb_lock_outl( ((2)<<16) + n+2   ,   VACTIVESTRTSTOP    ); 
    ep93xxfb_lock_outl( ((3)<<16) + n+3   ,   VBLANKSTRTSTOP     ); 
    ep93xxfb_lock_outl( ((n+3)<<16) + n+3    ,   VCLKSTRTSTOP       );  

    n = 1280;
    ep93xxfb_lock_outl( n + 15  ,   HCLKSTOTAL         ); 
    ep93xxfb_lock_outl( ((n+5)<<16) + n+ 14    ,   HSYNCSTRTSTOP      ); 
    ep93xxfb_lock_outl( ((15)<<16) + n + 15   ,   HACTIVESTRTSTOP    ); 
    ep93xxfb_lock_outl( ((n+15)<<16) + 15 ,   HBLANKSTRTSTOP       ); 
    ep93xxfb_lock_outl( ((n)<<16) + n   ,   HCLKSTRTSTOP       ); 

    ep93xxfb_lock_outl( 14   ,   LINECARRY          ); 

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

    switch(value)
    {
        case 8:
            ep93xxfb_lock_outl( PIXELMODE_DSCAN | 
                PIXELMODE_S_8PPC | PIXELMODE_P_4BPP |
                PIXELMODE_C_GSLUT   ,   PIXELMODE          ); 

            ep93xxfb_lock_outl( 
                    attribs | VIDEOATTRIBS_VCPOL | VIDEOATTRIBS_HSPOL | 
                    VIDEOATTRIBS_DATAEN | VIDEOATTRIBS_SYNCEN | VIDEOATTRIBS_INVCLK |
                    VIDEOATTRIBS_PCLKEN | VIDEOATTRIBS_EN ,
                    VIDEOATTRIBS       ); 
            break;
        case 16:
            if(!interrupt_hooked)
            {
                request_irq(IRQ_EP93XX_VSYNC, ep93xxfb_irq_handler, SA_INTERRUPT, "lut switch interrupt", NULL);
                interrupt_hooked = 1;
            }
            ep93xxfb_lock_outl( PIXELMODE_DSCAN | 
                PIXELMODE_S_8PPC | PIXELMODE_P_4BPP | PIXELMODE_C_GSLUT, PIXELMODE ); 

            ep93xxfb_lock_outl( 
                    attribs | VIDEOATTRIBS_VCPOL | VIDEOATTRIBS_HSPOL | 
                    VIDEOATTRIBS_DATAEN | VIDEOATTRIBS_SYNCEN | VIDEOATTRIBS_INVCLK |
                    VIDEOATTRIBS_PCLKEN | VIDEOATTRIBS_EN | VIDEOATTRIBS_INTEN,
                    VIDEOATTRIBS       ); 
            break;
        default:
            return;
    }

}

void HOSIDEN_HLM6323_configure(unsigned char value)
{
    int n;
    unsigned long attribs;

    printk("HOSIDEN_HLM6323_configure\n");		

    switch(value)
    {
        case 8:
            ep93xxfb_8gray_palette_init();
            break;
        case 16:
            ep93xxfb_16gray_palette_init();
            break;
        default:
            return;
    }

    SysconSetLocked(SYSCON_DEVCFG, (inl(SYSCON_DEVCFG) & ~SYSCON_DEVCFG_EXVC) | SYSCON_DEVCFG_RasOnP3);

    ep93xxfb_lock_outl( 0x00000000   ,   VIDEOATTRIBS       ); 

    ep93xx_set_video_div(epinfo.freq*320*240);
    mdelay(10);

    n = 240;
    ep93xxfb_lock_outl( n + 3 ,   VLINESTOTAL        ); 
    ep93xxfb_lock_outl( ((n+1)<<16) + n +2   ,   VSYNCSTRTSTOP      ); 
    ep93xxfb_lock_outl( ((3)<<16) + n +3 ,   VACTIVESTRTSTOP    ); 
    ep93xxfb_lock_outl( ((3)<<16) + n +3 ,   VBLANKSTRTSTOP     ); 
    ep93xxfb_lock_outl( ((n+3)<<16) + n +3,   VCLKSTRTSTOP       );  

    n = 320;
    ep93xxfb_lock_outl( n + 3,   HCLKSTOTAL         ); 
    ep93xxfb_lock_outl( ((n+1)<<16) + n+2    ,   HSYNCSTRTSTOP      ); 
    ep93xxfb_lock_outl( ((3)<<16) + n+3  ,   HACTIVESTRTSTOP    ); 
    ep93xxfb_lock_outl( ((3)<<16) + n+3 ,   HBLANKSTRTSTOP       ); 
    ep93xxfb_lock_outl( ((n+3)<<16) + n+3  ,   HCLKSTRTSTOP       ); 

    ep93xxfb_lock_outl( 3   ,   LINECARRY          ); 

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

    switch(value)
    {
        case 8:
            ep93xxfb_lock_outl(  
                PIXELMODE_S_4PPC | PIXELMODE_P_4BPP | PIXELMODE_C_GSLUT,   PIXELMODE ); 
            ep93xxfb_lock_outl( 
                    attribs | VIDEOATTRIBS_VCPOL | VIDEOATTRIBS_HSPOL | 
                    VIDEOATTRIBS_DATAEN | VIDEOATTRIBS_SYNCEN | VIDEOATTRIBS_INVCLK |
                    VIDEOATTRIBS_PCLKEN | VIDEOATTRIBS_EN ,
                    VIDEOATTRIBS       ); 
            break;
        case 16:
            ep93xxfb_lock_outl(  
                PIXELMODE_S_4PPC | PIXELMODE_P_4BPP | PIXELMODE_C_GSLUT,   PIXELMODE ); 
            if(!interrupt_hooked)
            {
                request_irq(IRQ_EP93XX_VSYNC, ep93xxfb_irq_handler, SA_INTERRUPT, "lut switch interrupt", NULL);
                interrupt_hooked = 1;
            }
            ep93xxfb_lock_outl( 
                    attribs | VIDEOATTRIBS_VCPOL | VIDEOATTRIBS_HSPOL | 
                    VIDEOATTRIBS_DATAEN | VIDEOATTRIBS_SYNCEN | VIDEOATTRIBS_INVCLK |
                    VIDEOATTRIBS_PCLKEN | VIDEOATTRIBS_EN | VIDEOATTRIBS_INTEN,
                    VIDEOATTRIBS       ); 
            break;
        default:
            return;
    }
}

#define FB_WRITEL fb_writel
#define FB_READL  fb_readl
#define LEFT_POS(bpp)          (0)
#define SHIFT_HIGH(val, bits)  ((val) << (bits))
#define SHIFT_LOW(val, bits)   ((val) >> (bits))
static inline void color_imageblit(const struct fb_image *image, 
        struct fb_info *p, u8 *dst1, 
        u32 start_index,
        u32 pitch_index)
{
    /* Draw the penguin */
    u32 *dst, *dst2;
    u32 color = 0, val, shift;
    int i, n, bpp = p->var.bits_per_pixel;
    u32 null_bits = 32 - bpp;
    u32 *palette = (u32 *) p->pseudo_palette;
    const u8 *src = image->data;

    dst2 = (u32 *) dst1;
    for (i = image->height; i--; ) {
        n = image->width;
        dst = (u32 *) dst1;
        shift = 0;
        val = 0;

        if (start_index) {
            u32 start_mask = ~(SHIFT_HIGH(~(u32)0, start_index));
            val = FB_READL(dst) & start_mask;
            shift = start_index;
        }
        while (n--) {
            if (p->fix.visual == FB_VISUAL_TRUECOLOR ||
                    p->fix.visual == FB_VISUAL_DIRECTCOLOR )
                color = palette[*src];
            else
                color = *src;
            color <<= LEFT_POS(bpp);
            val |= SHIFT_HIGH(color, shift);
            if (shift >= null_bits) {
                FB_WRITEL(val, dst++);

                val = (shift == null_bits) ? 0 : 
                    SHIFT_LOW(color, 32 - shift);
            }
            shift += bpp;
            shift &= (32 - 1);
            src++;
        }
        if (shift) {
            u32 end_mask = SHIFT_HIGH(~(u32)0, shift);

            FB_WRITEL((FB_READL(dst) & end_mask) | val, dst);
        }
        dst1 += p->fix.line_length;
        if (pitch_index) {
            dst2 += p->fix.line_length;
            dst1 = (u8 *)((long __force)dst2 & ~(sizeof(u32) - 1));

            start_index += pitch_index;
            start_index &= 32 - 1;
        }
    }
}

static const int reversebit[]=
{
    7, 6, 5, 4, 3, 2, 1, 0,
    15,14,13,12,11,10, 9, 8,
    23,22,21,20,19,18,17,16,
    31,30,29,28,27,26,25,24,
};
static inline void slow_imageblit(const struct fb_image *image, struct fb_info *p, 
        u8 *dst1, u32 fgcolor,
        u32 bgcolor, 
        u32 start_index,
        u32 pitch_index)
{
    u32 shift, color = 0, bpp = p->var.bits_per_pixel;
    u32 *dst, *dst2;
    u32 val, pitch = p->fix.line_length;
    u32 null_bits = 32 - bpp;
    u32 spitch = (image->width+7)/8;
    const u8 *src = image->data, *s;
    u32 i, j, l;

    dst2 = (u32 *) dst1;
    fgcolor <<= LEFT_POS(bpp);
    bgcolor <<= LEFT_POS(bpp);
    for (i = image->height; i--; ) {
        shift = val = 0;
        l = 8;
        j = image->width;
        dst = (u32 *) dst1;
        s = src;

        /* write leading bits */
        if (start_index) {
            u32 start_mask = ~(SHIFT_HIGH(~(u32)0,start_index));
            val = FB_READL(dst) & start_mask;
            shift = start_index;
        }

        while (j--) {
            l--;
            color = (*s & (1 << l)) ? fgcolor : bgcolor;
            val |= SHIFT_HIGH(color, reversebit[shift]);
            /* Did the bitshift spill bits to the next long? */
            if (shift >= null_bits) {
                FB_WRITEL(val, dst++);
                val = (shift == null_bits) ? 0 :
                    SHIFT_LOW(color, 32 - reversebit[shift]);
            }
            shift += bpp;
            shift &= (32 - 1);
            if (!l) { l = 8; s++; };
        }

        /* write trailing bits */
        if (shift) {
            u32 end_mask = SHIFT_HIGH(~(u32)0, shift);

            FB_WRITEL((FB_READL(dst) & end_mask) | val, dst);
        }

        dst1 += pitch;
        src += spitch;	
        if (pitch_index) {
            dst2 += pitch;
            dst1 = (u8 *)((long __force)dst2 & ~(sizeof(u32) - 1));
            start_index += pitch_index;
            start_index &= 32 - 1;
        }

    }
}

static void ep93xx_imageblit(struct fb_info *p, const struct fb_image *image)
{
    u32 fgcolor, bgcolor, start_index, bitstart, pitch_index = 0;
    u32 bpl = sizeof(u32), bpp = p->var.bits_per_pixel;
    u32 dx = image->dx, dy = image->dy;
    u8 *dst1;

    if (p->state != FBINFO_STATE_RUNNING)
        return;

    bitstart = (dy * p->fix.line_length * 8) + (dx * bpp);
    start_index = bitstart & (32 - 1);
    pitch_index = (p->fix.line_length & (bpl - 1)) * 8;

    bitstart /= 8;
    bitstart &= ~(bpl - 1);
    dst1 = p->screen_base + bitstart;

    if (p->fbops->fb_sync)
        p->fbops->fb_sync(p);

    if (image->depth == 1) {
        if (p->fix.visual == FB_VISUAL_TRUECOLOR ||
                p->fix.visual == FB_VISUAL_DIRECTCOLOR) {
            fgcolor = ((u32*)(p->pseudo_palette))[image->fg_color];
            bgcolor = ((u32*)(p->pseudo_palette))[image->bg_color];
        } else {
            fgcolor = image->fg_color;
            bgcolor = image->bg_color;
        }	
        slow_imageblit(image, p, dst1, fgcolor, bgcolor,
                start_index, pitch_index);
    } else
        color_imageblit(image, p, dst1, start_index, pitch_index);
}


#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,8)

int ep93xxfb_ioctl(struct fb_info *info,unsigned int cmd, unsigned long arg)
{
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


static struct fb_ops ep93xxfb_ops = {
    .owner          = THIS_MODULE,
    .fb_check_var   = ep93xxfb_check_var,
    .fb_set_par     = ep93xxfb_set_par,
    .fb_blank       = ep93xxfb_blank,
    .fb_fillrect    = cfb_fillrect,
    .fb_copyarea    = cfb_copyarea,
    //    .fb_imageblit   = cfb_imageblit,
    .fb_imageblit   = ep93xx_imageblit,
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

    printk("mmio_start = 0x%08x\n", res->start);
    printk("mmio_len = 0x%08x\n", res->end - res->start + 1);

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

    /*change the raster arb to the highest one--Bo*/
    arb = inl(SYSCON_BMAR);
    arb = (arb & 0x3f8) | 0x01;
    ep93xxfb_outl(arb,SYSCON_BMAR);

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

    DPRINTK("ep93xxfb_remove - enter \n");

    info = platform_get_drvdata(device);

    ep93xxfb_release_videomem();

    res = platform_get_resource( device, IORESOURCE_MEM, 0);
    release_mem_region(res->start, res->end - res->start + 1);

    platform_set_drvdata(device, NULL);
    unregister_framebuffer(info);

    fb_dealloc_cmap(&info->cmap);
    framebuffer_release(info);

    ep93xxfb_blank( 1, info );

    DPRINTK("ep93xxfb_remove - exit \n");
    return 0;
}

static void ep93xxfb_platform_release(struct device *device)
{
    DPRINTK("ep93xxfb_platform_release - enter\n");
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

    DPRINTK("ep93xxfb_init - enter\n");		

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

#else  //  LINUX_VERSION_CODE


int ep93xxfb_setcolreg(unsigned regno, unsigned red, unsigned green,
        unsigned blue, unsigned transp,
        struct fb_info *info)
{
    return 0;
}
static struct fb_ops ep93xxfb_ops = {
    .owner          = THIS_MODULE,
    .fb_setcolreg   = ep93xxfb_setcolreg,
    .fb_check_var   = ep93xxfb_check_var,
    .fb_set_par     = ep93xxfb_set_par,
    .fb_blank       = ep93xxfb_blank,
    .fb_fillrect    = cfb_fillrect,
    .fb_copyarea    = cfb_copyarea,
    .fb_imageblit   = ep93xx_imageblit,
    .fb_cursor	= soft_cursor,
};

static int __init ep93xxfb_probe(struct device *device)
{
    struct platform_device *pdev = to_platform_device(device);
    struct fb_info *info = NULL;
    struct resource *res = NULL;
    int ret = 0;
    int arb = 0;

    DPRINTK("ep93xxfb_probe - enter \n");


    if(!device) {
        printk("error : to_platform_device\n");
        return -ENODEV;
    }
    res = platform_get_resource( pdev, IORESOURCE_MEM, 0);
    if(!res) {
        printk("error : platform_get_resource \n");
        return -ENODEV;
    }				
    if (!request_mem_region(res->start,res->end - res->start + 1, FBDEV_NAME ))
        return -EBUSY;

    info = framebuffer_alloc(sizeof(u32) * 256, &pdev->dev);

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
    dev_set_drvdata(device, info);
    printk(KERN_INFO "fb%d: EP93xx frame buffer at %dx%dx%dbpp\n", info->node,
            info->var.xres, info->var.yres, info->var.bits_per_pixel);

    /*change the raster arb to the highest one--Bo*/
    arb = inl(SYSCON_BMAR);
    arb = (arb & 0x3f8) | 0x01;
    ep93xxfb_outl(arb,SYSCON_BMAR);

    DPRINTK("ep93xxfb_probe - exit \n");
    return 0;

clmap:
    fb_dealloc_cmap(&info->cmap);

fbuff:
    framebuffer_release(info);
    return ret;
}

static int ep93xxfb_remove(struct device *device)
{
    struct platform_device *pdev = to_platform_device(device);
    struct resource *res;
    struct fb_info *info;			        

    DPRINTK("ep93xxfb_remove - enter \n");

    info = dev_get_drvdata(device);

    ep93xxfb_release_videomem();

    res = platform_get_resource( pdev, IORESOURCE_MEM, 0);
    release_mem_region(res->start, res->end - res->start + 1);

    dev_set_drvdata(device, NULL);
    unregister_framebuffer(info);

    fb_dealloc_cmap(&info->cmap);
    framebuffer_release(info);

    ep93xxfb_blank( 1, info );

    DPRINTK("ep93xxfb_remove - exit \n");
    return 0;
}
static struct device_driver ep93xxfb_driver = {
    .name		= FBDEV_NAME,
    .bus		= &platform_bus_type,
    .probe		= ep93xxfb_probe,
    .remove		= ep93xxfb_remove,
};
int __init ep93xxfb_init(void)
{
    DPRINTK("ep93xxfb_init\n");
    return driver_register(&ep93xxfb_driver);
}

static void __exit ep93xxfb_exit(void)
{
    DPRINTK("ep93xxfb_exit\n");
    return driver_unregister(&ep93xxfb_driver);
}

int __init ep93xxfb_setup(char *options)
{
    DPRINTK("ep93xxfb_setup\n");
    return 0;
}

#endif  //  LINUX_VERSION_CODE


module_init(ep93xxfb_init);
module_exit(ep93xxfb_exit);
MODULE_AUTHOR("John Zheng <yujiang.zheng@cirrus.com>");
MODULE_LICENSE("GPL");

