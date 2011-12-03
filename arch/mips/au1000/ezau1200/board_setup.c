/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Alchemy Db1x00 board setup.
 *
 * Copyright 2000 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
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
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <asm/cpu.h>
#include <asm/bootinfo.h>
#include <asm/irq.h>
#include <asm/mipsregs.h>
#include <asm/reboot.h>
#include <asm/pgtable.h>

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/console.h>
#include <linux/mc146818rtc.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/resource.h>
#include <linux/serial_8250.h>

#if defined(CONFIG_BLK_DEV_IDE_AU1XXX)
#include <linux/ide.h>
#endif

#include <asm/mach-au1x00/au1000.h>
#include <asm/mach-au1x00/au1xxx_dbdma.h>
#include <asm/mach-falinux/ezau1200.h>

#include <linux/falinux-common.h>


// irq struct ===========================================================
au1xxx_irq_map_t __initdata au1xxx_irq_map[] = {
	// arch/mips/au1000/common/au1xxx_irqmap.c
};

int __initdata au1xxx_nr_irqs = ARRAY_SIZE(au1xxx_irq_map);


// ax88796b -------------------------------------------------------------
static struct resource ax88796b_resources[] = {
	[0] = {
		.name	= "ax88796b-regs",
		.start	= FALINUX_AU1200_PHYS_CS2,
		.end	= FALINUX_AU1200_PHYS_CS2 + 0xff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.name	= "ax88796b-irq",
		.start	= AU1000_GPIO_2,
		.end	= AU1000_GPIO_2,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device ax88796b_device = {
	.name          = "ax88796b",
	.id	           = -1,
	.num_resources = ARRAY_SIZE(ax88796b_resources),
	.resource      = ax88796b_resources,
};


// wm9712 touch --------------------------------------------------------
static struct resource wm9712ts_resources[] = {
[0] = {
	.name   = "wm9712ts-int",
	.start  = AU1000_GPIO_10,
	.end    = AU1000_GPIO_10,
	.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device wm9712ts_device = {
	.name		   = "wm9712ts",
	.id            = -1,
	.num_resources = ARRAY_SIZE(wm9712ts_resources),
	.resource      = wm9712ts_resources,
};

// i2c bit banging -------------------------------------------------------------
static void i2c_bit_setscl(void *data, int val)
{
	int gpnr = ((struct falinux_i2c_platform_data*)data)->scl_pin;

	if (val)
		au1xxx_gpio_tristate( gpnr );
	else 
		au1xxx_gpio_write( gpnr, val );
	au_sync();
}
static void i2c_bit_setsda(void *data, int val)
{
	int gpnr = ((struct falinux_i2c_platform_data*)data)->sda_pin;

	if (val)
		au1xxx_gpio_tristate( gpnr );
	else 
		au1xxx_gpio_write( gpnr, val );
	au_sync();
}
static int i2c_bit_getscl(void *data )
{
	int gpnr = ((struct falinux_i2c_platform_data*)data)->scl_pin;
	au1xxx_gpio_tristate( gpnr );
	au_sync();
	return au1xxx_gpio_read( gpnr );	
}
static int i2c_bit_getsda(void *data )
{
	int gpnr = ((struct falinux_i2c_platform_data*)data)->sda_pin;
	au1xxx_gpio_tristate( gpnr );
	au_sync();
	return au1xxx_gpio_read( gpnr );	
}

static struct falinux_i2c_platform_data i2c_platform_data[] = {
	{
		.devcnt        = 1,           // 배열의 개수를 이곳에 적는다
		.sda_pin       = 9,
		.scl_pin       = 13,
		.id            = 0x10100,     // 임의 ID 중복되지 않게 조심
		.udelay        = 15,
		.timeout       = 100,
		.bit_setscl    = &i2c_bit_setscl,	
		.bit_setsda    = &i2c_bit_setsda,	
		.bit_getscl    = &i2c_bit_getscl,	
		.bit_getsda    = &i2c_bit_getsda,	
	}
};

static struct platform_device i2c_bit_device = {
	.name       = "i2c-bit",
	.id         = 0,
	.dev        = {
		.platform_data = i2c_platform_data,
	},
	.num_resources = 0,
};



// resource struct =======================================================
static struct platform_device *emau1200_platform_devices[] __initdata = {

	&wm9712ts_device,
	&ax88796b_device,
	&i2c_bit_device,
	
};


//------------------------------------------------------------------------
// AU1200 이 제공하는 기본적인 설정외에 공통적이지 않은 설정을
// 이곳에서 기술한다.
//------------------------------------------------------------------------
#define	PIN_FUNC(n,v)	if (v) pin_func |= (1<<(n)); else pin_func &= ~(1<<(n))

void __init board_setup(void)
{
	char *argptr = NULL;
	u32 pin_func, freq0, clksrc;

	// === set alternate function. data sheet 10.3.1 ==================================

	pin_func = au_readl(SYS_PINFUNC);
	au_sync();
	                 	
	PIN_FUNC(31, 0 );	//  0 : GPIO-12                   1 : DMA_REQ1
	PIN_FUNC(30, 0 );	//  0 : GPIO-28,19,17             1 : SD0_CMD,DAT0,CLK
	PIN_FUNC(29, 1 );	//  0 : PCMCIA- .....             1 : SD1_DAT0,DAT1,DAT2,CMD,CLK
	PIN_FUNC(28, 1 );	//  0 : UART1.dcd                 1 : LCD_PWM0
	PIN_FUNC(27, 1 );	//  0 : UART1.ri                  1 : LCD_PWM1
	PIN_FUNC(26, 0 );	//  0 : GPIO-211                  1 : LCD_D16
	PIN_FUNC(25, 0 );	//  0 : GPIO-210                  1 : LCD_D8
	PIN_FUNC(24, 0 );	//  0 : GPIO-201                  1 : LCD_D1
	PIN_FUNC(23, 0 );	//  0 : GPIO-200                  1 : LCD_D0
	PIN_FUNC(22, 0 );	// 00 : PSC1.D0,D1,CLK,SYNC0     10 : xxx
	PIN_FUNC(21, 1 );	// 01 : PSC1.D0,CLK GPIO-22,20   11 : GPIO-24,22,20,11
	PIN_FUNC(20, 1 );	//  0 : PSC1_EXTCLK               1 : GPIO-23
	PIN_FUNC(19, 1 );	//  0 : is not clock_in           1 : optional clock source
	PIN_FUNC(18, 0 );	// 00 : PSC0_D1,SYNC0,GPIO-16    10 :GPIO-215,31,16
	PIN_FUNC(17, 1 );	// 01 : PSC0_D1,SYNC0,SYNC1      11 : xxx 
	PIN_FUNC(16, 0 );	//  0 : EXTCLK0 divider           1 : 32KHz OSC
	PIN_FUNC(15, 1 );	//  0 : GPIO-214:212,209:202      1 : CIM_CLK,FS,LS,D9:2
	PIN_FUNC(14, 0 );	//  0 : GPIO-21                   1 : PSC1_SYNC1
	
	PIN_FUNC(12, 0 );	//  0 : UART1.txd                 1 : GPIO-15
	PIN_FUNC(11, 0 );	//  0 : UART1.rxd                 1 : GPIO-30
	PIN_FUNC(10, 0 );	//  0 : GPIO-3                    1 : EXT_CLK1
	PIN_FUNC( 9, 0 );	//  0 : GPIO-2                    1 : EXT_CLK0
	PIN_FUNC( 8, 1 );	//  0 : GPIO-29                   1 : UART0.rxd
	PIN_FUNC( 7, 0 );	//  0 : GPIO-14,13,10,9           1 : UART1.rts,cts,dsr,dtr
	PIN_FUNC( 6, 0 );	//  0 : GPIO-8,6                  1 : SD0_DAT1,DAT2
	PIN_FUNC( 5, 1 );	//  0 : SD0.DAT3                  1 : GPIO-26
	PIN_FUNC( 4, 0 );	//  0 : PSC0_CLK,D0               1 : GPIO-25,18
	PIN_FUNC( 3, 0 );	//  0 : UART0.txd                 1 : GPIO-27
	PIN_FUNC( 2, 0 );	//  0 : SD1.dat3                  1 : PCMCIA.pwe

	au_writel(pin_func, SYS_PINFUNC);
	au_sync();



	// i2c use PSC1 ==================================================================
	// The i2c driver depends on 50Mhz clock
	// 492Mhz / (4+1)*2 == 49.2Mhz 
	freq0 = au_readl(SYS_FREQCTRL0);
	au_sync();
	
	freq0 &= ~(SYS_FC_FRDIV1_MASK | SYS_FC_FS1 | SYS_FC_FE1);
	freq0 |= (4<<SYS_FC_FRDIV1_BIT);
	
	au_writel(freq0, SYS_FREQCTRL0);
	au_sync();

	freq0 |= SYS_FC_FE1;	// enable
	au_writel(freq0, SYS_FREQCTRL0);
	au_sync();

	// psc1_intclk enable 
	clksrc = au_readl(SYS_CLKSRC);
	au_sync();
	
	clksrc &= ~(0x1F <<25);
	clksrc |=  (0x3  <<27);	// select : freq1

	au_writel(clksrc, SYS_CLKSRC);
	au_sync();

    
    // === set GPIO direction ======================================================
    
    // * mmc.CD input
    au1xxx_gpio_tristate( GPNR_MMC_CD );	// define <asm/mach-falinux/emau1200.h>


	// === get kernel param  =======================================================

    argptr = prom_getcmdline();


    printk("...EM-AU1200 Board (pin_func=0x%08x)\n", pin_func );
    
}

//------------------------------------------------------------------------
// reset
//------------------------------------------------------------------------
void board_reset (void)
{

}
//------------------------------------------------------------------------
// LCD 관련 설정
//------------------------------------------------------------------------
int board_au1200fb_panel (void)
{
	return 0;
}
//------------------------------------------------------------------------
// LCD 관련 설정
//------------------------------------------------------------------------
int board_au1200fb_panel_init (void)
{
	return 0;
}
//------------------------------------------------------------------------
// LCD 관련 설정
//------------------------------------------------------------------------
int board_au1200fb_panel_shutdown (void)
{
	return 0;
}

//------------------------------------------------------------------------
// 보드만의 플랫폼 등록
//------------------------------------------------------------------------
int emau1200_board_platform_init(void)
{
	return platform_add_devices( emau1200_platform_devices, ARRAY_SIZE(emau1200_platform_devices));
}

arch_initcall(emau1200_board_platform_init);













