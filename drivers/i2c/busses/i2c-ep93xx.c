/* ------------------------------------------------------------------------ *
 * i2c-ep933xx.c I2C bus glue for Cirrus EP93xx                             *
 * ------------------------------------------------------------------------ *

   Copyright (C) 2004 Michael Burian
   
   Based on i2c-parport-light.c
   Copyright (C) 2003-2004 Jean Delvare <khali@linux-fr.org>
  
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * ------------------------------------------------------------------------ */


//#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>

//1/(2*clockfrequency)
#define EE_DELAY_USEC       50

#if defined(CONFIG_FALINUX_EP9312)
#define GPIOA_EECLK ( 1 << 4 )  // GPIO A[4]
#define GPIOA_EEDAT ( 1 << 5 )  // GPIO A[5]
#else
#define GPIOG_EECLK 1
#define GPIOG_EEDAT 2
#endif


/* ----- I2C algorithm call-back functions and structures ----------------- */

// TODO: optimize
static void ep93xx_setscl(void *data, int state)
{
#if defined(CONFIG_FALINUX_EP9312)
	unsigned int uiPADR, uiPADDR;
	
	uiPADR  = inl(GPIO_PADR);
	uiPADDR = inl(GPIO_PADDR);

	/* Configure the clock line as output. */
	uiPADDR |= GPIOA_EECLK;
	outl(uiPADDR, GPIO_PADDR);

	/* Set clock line to state */
	if (state)
		uiPADR |=  GPIOA_EECLK;
	else
		uiPADR &= ~GPIOA_EECLK;

	outl(uiPADR, GPIO_PADR);
#else
	unsigned int uiPGDR, uiPGDDR;
	
	uiPGDR  = inl(GPIO_PGDR);
	uiPGDDR = inl(GPIO_PGDDR);

	/* Configure the clock line as output. */
	uiPGDDR |= GPIOG_EECLK;
	outl(uiPGDDR, GPIO_PGDDR);

	/* Set clock line to state */
	if(state)
		uiPGDR |= GPIOG_EECLK;
	else
		uiPGDR &= ~GPIOG_EECLK;
	
	outl(uiPGDR, GPIO_PGDR);
#endif
}

static void ep93xx_setsda(void *data, int state)
{
#if defined(CONFIG_FALINUX_EP9312)
	unsigned int uiPADR, uiPADDR;
	
    uiPADR  = inl(GPIO_PADR);
    uiPADDR = inl(GPIO_PADDR);

    /* Configure the data line as output. */
    uiPADDR |= GPIOA_EEDAT;
    outl(uiPADDR, GPIO_PADDR);

    /* Set data line to state */
    if (state)
		uiPADR |=  GPIOA_EEDAT;
    else
		uiPADR &= ~GPIOA_EEDAT;

    outl(uiPADR, GPIO_PADR);

//	uiPADR  = inl(GPIO_PADR);
//	uiPADDR = inl(GPIO_PADDR);
#else	
	unsigned int uiPGDR, uiPGDDR;

	uiPGDR  = inl(GPIO_PGDR);
	uiPGDDR = inl(GPIO_PGDDR);

	/* Configure the data line as output. */
	uiPGDDR |= GPIOG_EEDAT;
	outl(uiPGDDR, GPIO_PGDDR);

	/* Set data line to state */
	if(state)
		uiPGDR |= GPIOG_EEDAT;
	else
		uiPGDR &= ~GPIOG_EEDAT;
	
	outl(uiPGDR, GPIO_PGDR);
#endif
}

static int ep93xx_getscl(void *data)
{
#if defined(CONFIG_FALINUX_EP9312)
	unsigned int uiPADR, uiPADDR;
	
    uiPADR   = inl(GPIO_PADR);
    uiPADDR  = inl(GPIO_PADDR);

    /* Configure the clock line as input */
    uiPADDR &= ~GPIOA_EECLK;
    outl(uiPADDR, GPIO_PADDR);

    /* Return state of the clock line */
    return (inl(GPIO_PADR) & GPIOA_EECLK) ? 1 : 0;
#else
	unsigned int uiPGDR, uiPGDDR;
	
	uiPGDR   = inl(GPIO_PGDR);
	uiPGDDR  = inl(GPIO_PGDDR);

	/* Configure the clock line as input */
	uiPGDDR &= ~GPIOG_EECLK;
	outl(uiPGDDR, GPIO_PGDDR);
	
	/* Return state of the clock line */
	return (inl(GPIO_PGDR) & GPIOG_EECLK) ? 1 : 0;
#endif
}

static int ep93xx_getsda(void *data)
{
#if defined(CONFIG_FALINUX_EP9312)
	unsigned int uiPADR, uiPADDR;

    uiPADR   = inl(GPIO_PADR);
    uiPADDR  = inl(GPIO_PADDR);

    /* Configure the data line as input */
    uiPADDR &= ~GPIOA_EEDAT;
    writel(uiPADDR, GPIO_PADDR);

    /* Return state of the data line */
    return (inl(GPIO_PADR) & GPIOA_EEDAT) ? 1 : 0;
#else
	unsigned int uiPGDR, uiPGDDR;

	uiPGDR = inl(GPIO_PGDR);
	uiPGDDR = inl(GPIO_PGDDR);

	/* Configure the data line as input */
	uiPGDDR &= ~GPIOG_EEDAT;
	outl(uiPGDDR, GPIO_PGDDR);

	/* Return state of the data line */
	return (inl(GPIO_PGDR) & GPIOG_EEDAT) ? 1 : 0;
#endif
}

/* ------------------------------------------------------------------------
 * Encapsulate the above functions in the correct operations structure.
 * This is only done when more than one hardware adapter is supported.
 */

/* last line (us, ms, timeout)
 * us dominates the bit rate: 10us  means: 100Kbit/sec(25 means 40kbps)
 *                            10ms  not known
 *                            100ms timeout
 */
static struct i2c_algo_bit_data ep93xx_data = {
	.setsda		= ep93xx_setsda,
	.setscl		= ep93xx_setscl,
	.getsda		= ep93xx_getsda,
	.getscl		= ep93xx_getscl,
	.udelay		= 10,
//	.mdelay		= 10,
	.timeout	= HZ,
};

/* ----- I2c structure ---------------------------------------------------- */
static struct i2c_adapter ep93xx_adapter = {
	.owner		= THIS_MODULE,
	.class		= I2C_CLASS_HWMON,
	.id			= I2C_HW_B_LP,
	.algo_data	= &ep93xx_data,
	.name		= "EP93XX I2C bit-bang interface",
};

/* ----- Module loading, unloading and information ------------------------ */

static int __init i2c_ep93xx_init(void)
{
	printk("i2c-ep93xx: enable to register with I2C\n");
	
#if defined(CONFIG_FALINUX_EP9312)
	unsigned long uiPADR, uiPADDR;

    /* Read the current value of the GPIO data and data direction registers. */
    uiPADR   = inl(GPIO_PADR);
    uiPADDR  = inl(GPIO_PADDR);

    /* If the GPIO pins have not been configured since reset, the data
     * and clock lines will be set as inputs and with data value of 0.
     * External pullup resisters are pulling them high.
     * Set them both high before configuring them as outputs. */
    uiPADR  |= (GPIOA_EEDAT | GPIOA_EECLK);
    outl(uiPADR, GPIO_PADR);

    /* Delay to meet the EE Interface timing specification. */
    udelay(EE_DELAY_USEC);

    /* Configure the EE data and clock lines as outputs. */
    uiPADDR |= (GPIOA_EEDAT | GPIOA_EECLK);
    outl(uiPADDR, GPIO_PADDR);
#else
	unsigned long uiPGDR, uiPGDDR;

	/* Read the current value of the GPIO data and data direction registers. */
	uiPGDR = inl(GPIO_PGDR);
	uiPGDDR = inl(GPIO_PGDDR);
	
	/* If the GPIO pins have not been configured since reset, the data 
	 * and clock lines will be set as inputs and with data value of 0.
	 * External pullup resisters are pulling them high.
	 * Set them both high before configuring them as outputs. */
	uiPGDR |= (GPIOG_EEDAT | GPIOG_EECLK);
	outl(uiPGDR, GPIO_PGDR);

	/* Delay to meet the EE Interface timing specification. */
	udelay(EE_DELAY_USEC);
	
	/* Configure the EE data and clock lines as outputs. */
	uiPGDDR |= (GPIOG_EEDAT | GPIOG_EECLK);
	outl(uiPGDDR, GPIO_PGDDR);
#endif

	/* Delay to meet the EE Interface timing specification. */
	udelay(EE_DELAY_USEC);

	/* Reset hardware to a sane state (SCL and SDA high) */
	ep93xx_setsda(NULL, 1);
	ep93xx_setscl(NULL, 1);

	if (i2c_bit_add_bus(&ep93xx_adapter) > 0) {
		printk(KERN_ERR "i2c-ep93xx: Unable to register with I2C\n");
		return -ENODEV;
	}
    
    return 0;
}

static void __exit i2c_ep93xx_exit(void)
{
	//i2c_bit_del_bus(&ep93xx_adapter);
	i2c_del_adapter(&ep93xx_adapter);
}

MODULE_AUTHOR("Michael Burian");
MODULE_DESCRIPTION("I2C bus glue for Cirrus EP93xx processors");
MODULE_LICENSE("GPL");

module_init(i2c_ep93xx_init);
module_exit(i2c_ep93xx_exit);
