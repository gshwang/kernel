/* linux/arch/arm/mach-s3c2410/falinux-lcd.h
 *
*/

/* LCD driver info */
#if defined( CONFIG_FB_EZ_S24XX_640X480 )  

	#define LCD_XRES            640		// x resolition
	#define LCD_YRES            480		// y resolution
	#define LCD_BPP             16		// RGB 5-6-5 format
		                                 		
	#define LCD_HFRONTPORCH     16		// Front Porch
	#define LCD_HBACKPORCH      16		// Back Porch
	#define LCD_HSYNCWIDTH      24		// Hsync Width
		                                 		
	#define LCD_VFRONTPORCH     33		// Front Porch
	#define LCD_VBACKPORCH      10		// Back Porch
	#define LCD_VSYNCWIDTH      2		// Vsync Width
		                                 		
	#define LPCSEL_VALUE        0xF84	// LPC3600 Disable (0x06 ?)

	#define LCD_CLKVAL			0x02	// Determine the rates of VCLK

#elif defined( CONFIG_FB_EZ_S24XX_480X272 )

	#define LCD_XRES            480		// x resolition
	#define LCD_YRES            272		// y resolution
	#define LCD_BPP             16		// RGB 5-6-5 format
		                                 		
	#define LCD_HFRONTPORCH     2		// Front Porch	2
	#define LCD_HBACKPORCH      2		// Back Porch	2
	#define LCD_HSYNCWIDTH      41		// Hsync Width	41
		                                 		
	#define LCD_VFRONTPORCH     2		// Front Porch	2
	#define LCD_VBACKPORCH      2		// Back Porch	2
	#define LCD_VSYNCWIDTH      10		// Vsync Width	10
		                                 		
	#define LPCSEL_VALUE        0xF84	// LPC3600 Disable
	#define LCD_CLKVAL			0x05	// Determine the rates of VCLK
	
#elif defined( CONFIG_FB_EZ_S24XX_240X320 )  

	#define LCD_XRES            240		// x resolition
	#define LCD_YRES            320		// y resolution
	#define LCD_BPP             16		// RGB 5-6-5 format
		                                 		
	#define LCD_HFRONTPORCH     2		// Front Porch
	#define LCD_HBACKPORCH      2		// Back Porch
	#define LCD_HSYNCWIDTH      41		// Hsync Width
		                                 		
	#define LCD_VFRONTPORCH     2		// Front Porch
	#define LCD_VBACKPORCH      2		// Back Porch
	#define LCD_VSYNCWIDTH      10		// Vsync Width
		                                 		
	#define LPCSEL_VALUE        0xF84	// LPC3600 Disable

	#define LCD_CLKVAL			0x06	// Determine the rates of VCLK

#else									// 320x240

	#define LCD_XRES            320		// x resolition
	#define LCD_YRES            240		// y resolution
	#define LCD_BPP             16		// RGB 5-6-5 format
		                                 		
	#define LCD_HFRONTPORCH     67		// Front Porch
	#define LCD_HBACKPORCH      2		// Back Porch
	#define LCD_HSYNCWIDTH      30		// Hsync Width
		                                 		
	#define LCD_VFRONTPORCH     17		// Front Porch
	#define LCD_VBACKPORCH      7		// Back Porch
	#define LCD_VSYNCWIDTH      3		// Vsync Width
		                                 		
	#define LPCSEL_VALUE        0xF84	// LPC3600 Disable

	#define LCD_CLKVAL			0x06	// Determine the rates of VCLK

#endif


static struct s3c2410fb_mach_info falinuxs3c2410_lcd_cfg __initdata = {
		.regs	= {

		.lcdcon1 = S3C2410_LCDCON1_TFT16BPP |
				   S3C2410_LCDCON1_TFT |
				   S3C2410_LCDCON1_CLKVAL(LCD_CLKVAL),

		.lcdcon2 = S3C2410_LCDCON2_VBPD(LCD_VBACKPORCH) |
				   S3C2410_LCDCON2_LINEVAL(LCD_YRES-1) |
				   S3C2410_LCDCON2_VFPD(LCD_VFRONTPORCH) |
				   S3C2410_LCDCON2_VSPW(LCD_VSYNCWIDTH),

		.lcdcon3 = S3C2410_LCDCON3_HBPD(LCD_HBACKPORCH) |
				   S3C2410_LCDCON3_HOZVAL(LCD_XRES-1) |
				   S3C2410_LCDCON3_HFPD(LCD_HFRONTPORCH),

		.lcdcon4 = S3C2410_LCDCON4_MVAL(0) |
				   S3C2410_LCDCON4_HSPW(LCD_HSYNCWIDTH),

		.lcdcon5 = S3C2410_LCDCON5_FRM565 |
				   S3C2410_LCDCON5_INVVLINE |
				   S3C2410_LCDCON5_INVVFRAME |
				   S3C2410_LCDCON5_PWREN |
				   S3C2410_LCDCON5_HWSWP,
	},

	/* currently setup by downloader */
	.gpccon			= 0xaaaa56a9,
	.gpccon_mask	= 0xffffffff,
	.gpcup			= 0x0000ffff,
	.gpcup_mask		= 0xffffffff,
	.gpdcon			= 0xaaaaaaaa,
	.gpdcon_mask	= 0xffffffff,
	.gpdup			= 0x0000ffff,
	.gpdup_mask		= 0xffffffff,

	.lpcsel			= LPCSEL_VALUE,
    .type       = S3C2410_LCDCON1_TFT,

	.width	= LCD_XRES,
	.height	= LCD_YRES,

	.xres		= {
		.min	= LCD_XRES,
		.max	= LCD_XRES,
		.defval	= LCD_XRES,
	},

	.yres		= {
		.min	= LCD_YRES,
		.max	= LCD_YRES,
		.defval = LCD_YRES,
	},

	.bpp		= {
		.min	= LCD_BPP,
		.max	= LCD_BPP,
		.defval = LCD_BPP,
	},
};

