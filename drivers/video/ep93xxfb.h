#ifndef __EP93XXFB_H__
#define __EP93XXFB_H__


#define POL_HIGH				1
#define POL_LOW        			0
#define EDGE_RISING     		1
#define EDGE_FALLING    		0
#define CLK_INTERNAL    		1
#define CLK_EXTERNAL    		0

#define CRT_OUT         		0
#define LCD_OUT         		1
#define TV_OUT          		2

#define MAX_XRES                1280
#define MAX_YRES                1024
#define MAX_BPP                 16
#define MAX_FBMEM_SIZE          3686400/*1920000*/

#define MAX_XRES_CRT            MAX_XRES
#define MAX_YRES_CRT            MAX_YRES
#define MAX_XRES_SVIDEO         1024
#define MAX_YRES_SVIDEO         768

#define PIXEL_FORMAT_SHIFT      17
#define PIXEL_FORMAT_4          ( 1 << PIXEL_FORMAT_SHIFT )
#define PIXEL_FORMAT_8          ( 2 << PIXEL_FORMAT_SHIFT )
#define PIXEL_FORMAT_16         ( 4 << PIXEL_FORMAT_SHIFT )
#define PIXEL_FORMAT_24         ( 6 << PIXEL_FORMAT_SHIFT )
#define PIXEL_FORMAT_32         ( 7 << PIXEL_FORMAT_SHIFT )
		

struct ep93xxfb_videomodes
{
    const char *name;

    unsigned long hres; 	// Horizontal Valid
    unsigned long hfp;		// Horizontal Front Porch
    unsigned long hsync;	// Horizontal Sync Width
    unsigned long hbp;		// Horizontal Back Porch
								
    unsigned long vres;		// Vertical Valid
    unsigned long vfp;		// Vertical Front Porch
    unsigned long vsync;	// Vertical Sync Width
    unsigned long vbp;		// Vertical Back Porch
									
    unsigned long refresh;	// Vertical Sync Frequency

    unsigned long clk_src;
    unsigned long clk_edge;
    unsigned long pol_blank;
    unsigned long pol_hsync;
    unsigned long pol_vsync;
};


struct ep93xxfb_info
{
    dma_addr_t		fb_phys;
    void			*fb_log;
    unsigned long	fb_size;
    unsigned long	fb_actsize;
    
    unsigned long	xtotal;
    unsigned long	ytotal;
		    
    unsigned int	xres;
    unsigned int	xfp;
    unsigned int	xsync;
    unsigned int	xbp;

    unsigned int	yres;
    unsigned int	yfp;
    unsigned int	ysync;
    unsigned int	ybp;
    unsigned int	bpp;
    
    unsigned long	refresh;
    unsigned long	pixclock;
    unsigned long	pixformat;

    unsigned int	clk_src;
    unsigned int	clk_edge;
    unsigned int	pol_blank;
    unsigned int	pol_xsync;
    unsigned int	pol_ysync;

    unsigned char	automods;
    
    void (*configure)(unsigned char value);
    void (*on)(unsigned char value);
    void (*off)(unsigned char value);
};

static int  ep93xxfb_setclk(void);
static int  ep93xx_get_max_video_clk(void);
static void ep93xxfb_pixelmod(int bpp);
static void ep93xxfb_timing_signal_generation(void);
static int  ep93xxfb_blank(int blank_mode,struct fb_info *info);

#define EE_DELAY_USEC			2
#define EE_READ_TIMEOUT			100
#define CX25871_DEV_ADDRESS		0x88
#define GPIOG_EEDAT				2
#define GPIOG_EECLK				1
#define CXMODES_COUNT			24

struct cx25871_vmodes
{

    const char          *name;
    unsigned char       automode;
    unsigned int        hres;
    unsigned int        vres;
    unsigned int        hclktotal;
    unsigned int        vclktotal;
    unsigned int        hblank;
    unsigned int        vblank;
    unsigned long       clkfrequency;
				    
};
			    
			    
int write_reg(unsigned char ucRegAddr, unsigned char ucRegValue);
void cx25871_on(unsigned char value);
void cx25871_off(unsigned char value);
void cx25871_config(unsigned char value);
																				
static void philips_lb064v02_on(unsigned char value);
static void philips_lb064v02_off(unsigned char value);

static void falinux_on(unsigned char value);
static void falinux_off(unsigned char value);


#define FBIO_EP93XX_CURSOR      0x000046c1
#define FBIO_EP93XX_LINE        0x000046c2
#define FBIO_EP93XX_FILL        0x000046c3
#define FBIO_EP93XX_BLIT        0x000046c4
#define FBIO_EP93XX_COPY        0x000046c5


#define CURSOR_BLINK            0x00000001
#define CURSOR_MOVE             0x00000002
#define CURSOR_SETSHAPE         0x00000004
#define CURSOR_SETCOLOR         0x00000008
#define CURSOR_ON               0x00000010
#define CURSOR_OFF              0x00000020
					       
					       
/*
* ioctl(fd, FBIO_EP93XX_CURSOR, ep93xx_cursor *)
*
* "data" points to an array of pixels that define the cursor; each row should
* be a multiple of 32-bit values (i.e. 16 pixels).  Each pixel is two bits,
* where the values are:
*
*     00 => transparent    01 => invert    10 => color1    11 => color2
*
* The data is arranged as follows (per word):
*
*    bits: |31-30|29-28|27-26|25-24|23-22|21-20|19-18|17-16|
*   pixel: | 12  | 13  | 14  | 15  |  8  |  9  | 10  | 11  |
*    bits: |15-14|13-12|11-10| 9-8 | 7-6 | 5-4 | 3-2 | 1-0 |
*   pixel: |  4  |  5  |  6  |  7  |  0  |  1  |  2  |  3  |
*
* Regardless of the frame buffer color depth, "color1", "color2",
* "blinkcolor1", and "blinkcolor2" are 24-bit colors since the cursor is
* injected into the data stream right before the video DAC.
*
* When "blinkrate" is not zero, pixel value 10 will alternate between "color1"
* and "blinkcolor1" (similar for pixel value 11 and "color2"/"blinkcolor2").
*
* "blinkrate" ranges between 0 and 255.  When 0, blinking is disabled.  255 is
* the fastest blink rate and 1 is the slowest.
*
* Both "width" and "height" must be between 1 and 64; it is preferable to have
* "width" a multiple of 16.
*/
struct ep93xx_cursor {
    unsigned long flags;
    unsigned long dx;           // Only used if CURSOR_MOVE is set
    unsigned long dy;           // Only used if CURSOR_MOVE is set
    unsigned long width;        // Only used if CURSOR_SETSHAPE is set
    unsigned long height;       // Only used if CURSOR_SETSHAPE is set
    const char *data;   // Only used if CURSOR_SETSHAPE is set
    unsigned long blinkrate;    // Only used if CURSOR_BLINK is set
    unsigned long color1;       // Only used if CURSOR_SETCOLOR is set
    unsigned long color2;       // Only used if CURSOR_SETCOLOR is set
    unsigned long blinkcolor1;  // Only used if CURSOR_SETCOLOR is set
    unsigned long blinkcolor2;  // Only used if CURSOR_SETCOLOR is set
};
														       

/*
 * The bits in the flags field of ep93xx_line.
*/
/*
* ioctl(fd, FBIO_EP93XX_LINE, ep93xx_line *)
*
* The line starts at ("x1","y1") and ends at ("x2","y2").  This means that
* when using a pattern, the two coordinates are not transitive (i.e. swapping
* ("x1","y1") with ("x2","y2") will not necessarily draw the exact same line,
* pattern-wise).
*
* "pattern" is a 2 to 16 bit pattern (since a 1 bit pattern isn't much of a
* pattern).  The lower 16 bits define the pattern (1 being foreground, 0 being
* background or transparent), and bits 19-16 define the length of the pattern
* (as pattern length - 1).  So, for example, "0xf00ff" defines a 16 bit
* with the first 8 pixels in the foreground color and the next 8 pixels in the
* background color or transparent.
*
* LINE_PRECISE is used to apply angularly corrected patterns to line.  It
* should only be used when LINE_PATTERN is also set.  The pattern will be
* applied along the length of the line, instead of along the length of the
* major axis.  This may result in the loss of fine details in the pattern, and
* will take more time to draw the line in most cases.
*/

#define LINE_PATTERN            0x00000001
#define LINE_PRECISE            0x00000002
#define LINE_BACKGROUND         0x00000004

struct ep93xx_line {
    unsigned long flags;
    unsigned long x1;
    unsigned long y1;
    unsigned long x2;
    unsigned long y2;
    unsigned long fgcolor;
    unsigned long bgcolor;      // Only used if LINE_BACKGROUND is set
    unsigned long pattern;      // Only used if LINE_PATTERN is set
};
				
#endif /* __EP93XXFB_H__ */

