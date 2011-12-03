/*
 * SIR IR port driver for the Cirrus Logic EP93xx.
 *
 * SIR on the EP93xx hangs off of UART2 so this is emplemented
 * as a dongle driver.
 *
 * Copyright 2003 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/init.h>

#include <net/irda/irda.h>
#include <net/irda/irmod.h>
#include <net/irda/irda_device.h>

#include <asm/io.h>
#include <asm/hardware.h>
#include <linux/amba/bus.h>
#include <linux/amba/serial.h>

#include "sir-dev.h"

#define MIN_DELAY 25      /* 15 us, but wait a little more to be sure */
#define MAX_DELAY 10000   /* 1 ms */


/*#define w388sd_STRINGIFY_HELPER(n)	#n */
/*#define w388sd_STRINGIFY(n)         w388sd_STRINGIFY_HELPER(n)*/
/*#define WDEBUG_HDR				"IRDA-SIR" ":" w388sd_STRINGIFY(__LINE__) ":"*/


#if 1
#undef DEBUG
//#define DEBUG 1
#ifdef DEBUG
#define DPRINTK( x... )  printk( ##x )
#else
#define DPRINTK( x... )
#endif
#endif

/*static int power_level = 3;*/
/*static int tx_lpm;*/
/*static int max_rate = 4000000;*/

/*
 * We need access to the UART2 control register so we can
 * do a read-modify-write and enable SIR.
 */

static int  ep93xx_sir_open(struct sir_dev *dev);
static int  ep93xx_sir_close(struct sir_dev *dev);
static int  ep93xx_sir_change_speed(struct sir_dev *, unsigned);
static int  ep93xx_sir_reset(struct sir_dev *);

/* These are the baudrates supported, in the order available */
/* Note : the 220L doesn't support 38400, but we will fix that below */
static unsigned baud_rates[] = { 9600, 19200, 57600, 115200, 38400 };

#define MAX_SPEEDS (sizeof(baud_rates)/sizeof(baud_rates[0]))

static struct dongle_driver ep93xx_dongle = {
	.owner		= THIS_MODULE,
	.driver_name	= "IRDA EP93XX SIR",
	.type		= IRDA_EP93XX_SIR,
	.open		= ep93xx_sir_open,
	.close		= ep93xx_sir_close,
	.reset		= ep93xx_sir_reset,
	.set_speed	= ep93xx_sir_change_speed,
};

/*static DEFINE_SPINLOCK(ep93xx_lock);*/

static int ep93xx_sir_open(struct sir_dev *dev)
{
	unsigned int uiTemp;
	/*unsinged int flags;*/
	int ret=0;
	struct qos_info *qos = &dev->qos;	

	unsigned long value=0;
	DPRINTK("----------------\n ep93xx_sir_open \n-----------------\n");
      
	value = inl(EP93XX_SYSCON_POWER_STATE);
	value = inl(EP93XX_SYSCON_CLOCK_CONTROL);
	value = inl(EP93XX_SYSCON_CLKSET1);
	value = inl(EP93XX_SYSCON_CLKSET2);

	/*spin_lock_irqsave(&ep93xx_lock, flags);*/
	/* 
	 * Set UART2 to be an IrDA interface
	 */
	uiTemp = inl(/*SYSCON_DEVCFG*/EP93XX_SYSCON_DEVICE_CONFIG);
	SysconSetLocked(/*SYSCON_DEVCFG*/EP93XX_SYSCON_DEVICE_CONFIG, (uiTemp | SYSCON_DEVCFG_IonU2) );



    /*
     * Set the pulse width.
     */
	outl( 3, UART2ILPR );

    /*
     * Set SIREN bit in UART2 - this enables the SIR encoder/decoder.
     */
	uiTemp = inl(UART2CR);
	outl( (uiTemp | /*AMBA_UARTCR_SIREN*/UART01x_CR_SIREN), UART2CR );


    /*
     * Enable Ir in SIR mode.
     */
    /* Write the reg twice because of the IrDA errata.  */
	outl( IrEnable_EN_SIR, IrEnable );
	outl( IrEnable_EN_SIR, IrEnable );
	/*spin_unlock_irqrestore(&ep93xx_lock, flags);*/

	ret=sirdev_set_dtr_rts(dev, TRUE, TRUE);

	/* Set the speeds we can accept */
	qos->baud_rate.bits &= IR_9600|IR_19200|IR_38400|IR_57600|IR_115200;

	/* Remove support for 38400 if this is not a 220L+ dongle */
	if (dev->dongle_drv->type == IRDA_ACTISYS_DONGLE)
		qos->baud_rate.bits &= ~IR_38400;

	qos->min_turn_time.bits = 0x7f; /* Needs 0.01 ms */
	irda_qos_bits_to_value(qos);

	/* irda thread waits 50 msec for power settling */

	return 0;
	

//    MOD_INC_USE_COUNT;
}

static int  ep93xx_sir_close(struct sir_dev *dev)
{
    unsigned int uiTemp;

    DPRINTK("----------------\n ep93xx_sir_close \n-----------------\n");

    /*
     * Disable Ir.
     */
     /* for now, don't write irda regs due to errata. */
	//outl( IrEnable_EN_NONE, IrEnable );

    /* 
     * Set UART2 to be an UART
     */
    uiTemp = inl( /*SYSCON_DEVCFG*/EP93XX_SYSCON_DEVICE_CONFIG );
    SysconSetLocked(/*SYSCON_DEVCFG*/EP93XX_SYSCON_DEVICE_CONFIG, (uiTemp & ~(SYSCON_DEVCFG_IonU2)) );

//    MOD_DEC_USE_COUNT;
	/* Power off the dongle */
	sirdev_set_dtr_rts(dev, FALSE, FALSE);

	return 0;
}

/*
 * Function ep93xx_sir_change_speed (task)
 *
 *   Change speed of the EP93xx I/R port. We don't have to do anything
 *   here as long as the rate is being changed at the serial port
 *   level.  irtty.c should take care of that.
 */
static int ep93xx_sir_change_speed(struct sir_dev *dev, unsigned speed)
{

	int ret = 0,ret1=0;
	int i = 0;

        IRDA_DEBUG(4,"%s(), speed=%d (was %d)\n", __FUNCTION__,speed, dev->speed);

	/* dongle was already resetted from irda_request state machine,
	 * we are in known state (dongle default)
	 */

	/* 
	 * Now, we can set the speed requested. Send RTS pulses until we
         * reach the target speed 
	 */
	for (i = 0; i < MAX_SPEEDS; i++) {
		if (speed == baud_rates[i]) {
			dev->speed = speed;
			break;
		}
		/* Set RTS low for 10 us */
		ret1 = sirdev_set_dtr_rts(dev, TRUE, FALSE);
		
		udelay(MIN_DELAY);

		/* Set RTS high for 10 us */
		ret1 = sirdev_set_dtr_rts(dev, TRUE, TRUE);
		
		udelay(MIN_DELAY);
	}

	/* Check if life is sweet... */
	if (i >= MAX_SPEEDS) {
		ep93xx_sir_reset(dev);
		ret = -EINVAL;  /* This should not happen */
	}

	/* Basta lavoro, on se casse d'ici... */
	return ret;
	

	DPRINTK("----------------\n ep93xx_sir_change_speed \n-----------------\n");
/*
	irda_task_next_state(task, IRDA_TASK_DONE);	
	return 0;
*/
}

/*
 * Function ep93xx_sir_reset (task)
 *
 *      Reset the EP93xx IrDA. We don't really have to do anything.
 *
 */
static int ep93xx_sir_reset(struct sir_dev *dev)
{
    DPRINTK("----------------\n ep93xx_sir_reset \n-----------------\n");
/*
    irda_task_next_state(task, IRDA_TASK_DONE);
    return 0;
*/
	/* Reset the dongle : set DTR low for 10 us */
	sirdev_set_dtr_rts(dev, FALSE, TRUE);
	udelay(MIN_DELAY);

	/* Go back to normal mode */
	sirdev_set_dtr_rts(dev, TRUE, TRUE);
	
	dev->speed = 9600;	/* That's the default */
	
	return 0;	
}

/*
 * Function ep93xx_sir_init(void)
 *
 *    Initialize EP93xx IrDA block in SIR mode.
 *
 */
int __init ep93xx_sir_init(void)
{
	int ret;

    	DPRINTK("----------------\n ep93xx_sir_init \n-----------------\n");
/*	return irda_device_register_dongle(&dongle);  */
	
	/* First, register an Actisys 220L dongle */
	ret = irda_register_dongle(&ep93xx_dongle);
	if (ret < 0)
		return ret;

	return 0;
	
}

/*
 * Function ep93xx_sir_cleanup(void)
 *
 *    Cleanup EP93xx IrDA module
 *
 */
static void __exit ep93xx_sir_cleanup(void)
{
	DPRINTK("----------------\n ep93xx_sir_cleanup \n-----------------\n");
	/*irda_device_unregister_dongle(&dongle);*/
 	irda_unregister_dongle(&ep93xx_dongle);
}


//MODULE_DESCRIPTION("EP93xx SIR IrDA driver");
// MODULE_LICENSE("GPL");
        
//#ifdef MODULE
module_init(ep93xx_sir_init);
module_exit(ep93xx_sir_cleanup);
//#endif

MODULE_AUTHOR("Oliver Ran <rmk@arm.linux.org.uk>");
MODULE_DESCRIPTION("EP93XX IrDA driver");
MODULE_LICENSE("GPL");
MODULE_PARM_DESC(power_level, "IrDA power level, 1 (low) to 3 (high)");
MODULE_PARM_DESC(tx_lpm, "Enable transmitter low power (1.6us) mode");
MODULE_PARM_DESC(max_rate, "Maximum baud rate (4000000, 115200, 57600, 38400, 19200, 9600)");


