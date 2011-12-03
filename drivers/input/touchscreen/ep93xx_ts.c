/*
 *  linux/drivers/input/touchscreen/ep93xx_ts.c
 *
 *  Copyright (C) 2003-2004 Cirrus Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/input.h>
#include <linux/pci.h>

#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>

/*
 * To customize for a new touchscreen, there are various macros that
 * have to be set.  If you allow UART_HACK_DEBUG to be defined, you
 * will get real time ts data scrolling up your serial terminal
 * screen that will help you empirically determine good values for these.
 *
 *
 * These are used as trigger levels to know when we have pen up/down
 *
 * The rules:
 * 1.  TS_HEAVY_INV_PRESSURE < TS_LIGHT_INV_PRESSURE because these
 *    are Inverse pressure.
 * 2.  Any touch lighter than TS_LIGHT_INV_PRESSURE is a pen up.
 * 3.  Any touch heavier than TS_HEAVY_INV_PRESSURE is a pen down.
 */
#define TS_HEAVY_INV_PRESSURE	(0x1000 - pressure_max)
#define TS_LIGHT_INV_PRESSURE	(0x1000 - pressure_min)

/*
 * If the x, y, or inverse pressure changes more than these values
 * between two succeeding points, the point is not reported.
 */
#define TS_MAX_VALID_PRESSURE_CHANGE	(pressure_jitter)
#define TS_MAX_VALID_XY_CHANGE  (xy_jitter)

/* "improved" defaults */
#define TS_X_MIN	0x2d9
#define TS_Y_MIN	0xd0a
#define TS_X_MAX	0xd42
#define TS_Y_MAX	0x2e0

static uint16_t pressure_min = 0x001;
static uint16_t pressure_max = 0x010;
static uint16_t pressure_jitter = 0x300;
static uint16_t xy_jitter = 0x100;

module_param(pressure_min, ushort, 0);
module_param(pressure_max, ushort, 0);
module_param(pressure_jitter, ushort, 0);
module_param(xy_jitter, ushort, 0);
MODULE_PARM_DESC(pressure_min, "Minimum pressure (0 - 4095)");
MODULE_PARM_DESC(pressure_max, "Maximum pressure (0 - 4095)");
MODULE_PARM_DESC(pressure_jitter, "Minimum pressure jitter (0 - 4095)");
MODULE_PARM_DESC(xy_jitter, "Minimum X-Y jitter (0 - 4095)");

/* This is the minimum Z1 Value that is valid. */
#define MIN_Z1_VALUE 0x50

/*
 * Settling delay for taking each ADC measurement.  Increase this
 * if ts is jittery.
 */
#define EP93XX_TS_ADC_DELAY_USEC 2000

/* Delay between TS points. */

#define EP93XX_TS_PER_POINT_DELAY_USEC 10000

/*-----------------------------------------------------------------------------
 * Debug messaging thru the UARTs
 *-----------------------------------------------------------------------------
 *
 *  Hello there!  Are you trying to get this driver to work with a new
 *  touschscreen?  Turn this on and you will get useful info coming
 *  out of your serial port.
 */

/* #define PRINT_CALIBRATION_FACTORS */
#ifdef PRINT_CALIBRATION_FACTORS
#define UART_HACK_DEBUG 1
int iMaxX = 0, iMaxY = 0, iMinX = 0xfff, iMinY = 0xfff;
#endif

/*
 * For debugging, let's spew messages out serial port 1 or 3 at 57,600 baud.
 */
#undef UART_HACK_DEBUG
#if defined(UART_HACK_DEBUG) && defined (CONFIG_DEBUG_LL)
static char szBuf[256];
void UARTWriteString(char *msg);
extern void printascii(const char *msg);
#define DPRINTK( x... )   \
    sprintf( szBuf, ##x ); \
    printascii( szBuf );
#else
#define DPRINTK( x... )
#endif

#define TSSETUP_DEFAULT  ( TSSETUP_NSMP_32 | TSSETUP_DEV_64 |  \
                           ((128<<TSSETUP_SDLY_SHIFT) & TSSETUP_SDLY_MASK) | \
                           ((128<<TSSETUP_DLY_SHIFT)  & TSSETUP_DLY_MASK) )

#define TSSETUP2_DEFAULT (TSSETUP2_NSIGND)

static unsigned int guiLastX, guiLastY;
static unsigned int guiLastInvPressure;
static int bCurrentPenDown;
static DECLARE_MUTEX(open_sem);

enum ts_mode_t {
	TS_MODE_UN_INITIALIZED,
	TS_MODE_HARDWARE_SCAN,
	TS_MODE_SOFT_SCAN
};

static enum ts_mode_t gScanningMode;

enum ts_states_t {
	TS_STATE_STOPPED = 0,
	TS_STATE_Z1,
	TS_STATE_Z2,
	TS_STATE_Y,
	TS_STATE_X,
	TS_STATE_DONE
};

struct ts_struct_t {
	struct input_dev *dev;
	unsigned int uiX;
	unsigned int uiY;
	unsigned int uiZ1;
	unsigned int uiZ2;
	enum ts_states_t state;
};

static struct ts_struct_t sTouch;

/*
 * From the spec, here's how to set up the touch screen's switch registers.
 */
struct SwitchStructType {
	unsigned int uiDetect;
	unsigned int uiDischarge;
	unsigned int uiXSample;
	unsigned int uiYSample;
	unsigned int uiSwitchZ1;
	unsigned int uiSwitchZ2;
};

/*
 * Here's the switch settings for a 4-wire touchscreen.  See the spec
 * for how to handle a 4, 7, or 8-wire.
 */
const static struct SwitchStructType sSwitchSettings =
/*     s28en=0
 *   TSDetect    TSDischarge  TSXSample  TSYSample    SwitchZ1   SwitchZ2
 */
{ 0x00403604, 0x0007fe04, 0x00081604, 0x00104601, 0x00101601, 0x00101608 };

static void ep93xx_ts_set_direct(unsigned int uiADCSwitch);
static irqreturn_t ep93xx_ts_isr(int irq, void *dev_id, struct pt_regs *regs);
static irqreturn_t ep93xx_timer2_isr(int irq, void *dev_id,
				     struct pt_regs *regs);
static void ep93xx_hw_setup(void);
static void ep93xx_hw_shutdown(void);
static unsigned int CalculateInvPressure(void);
static unsigned int ADCGetData(unsigned int uiSamples, unsigned int uiMaxDiff);
static void TS_Soft_Scan_Mode(void);
static void TS_Hardware_Scan_Mode(void);
static void ProcessPointData(struct input_dev *dev);
static void Set_Timer2_uSec(unsigned int Delay_mSec);
static void Stop_Timer2(void);

static irqreturn_t ep93xx_ts_isr(int irq, void *dev_id, struct pt_regs *regs)
{
	/*
	 * Note that we don't clear the interrupt here.  The interrupt
	 * gets cleared in TS_Soft_Scan_Mode when the TS ENABLE
	 * bit is cleared.
	 *

	 *
	 * Set the ts to manual polling mode and schedule a callback.
	 * That way we can return from the isr in a reasonable amount of
	 * time and process the touch in the callback after a brief delay.
	 */
	TS_Soft_Scan_Mode();
	return IRQ_HANDLED;
}

static int ep93xx_ts_open(struct input_dev *dev)
{
	int err;

	if (down_trylock(&open_sem))
		return -EBUSY;

	err = request_irq(IRQ_EP93XX_TOUCH, ep93xx_ts_isr, SA_INTERRUPT, "ep93xx_ts", dev);
	if (err) {
		printk(KERN_WARNING
		       "ep93xx_ts: failed to get touchscreen IRQ\n");
		return err;
	}

	err = request_irq(IRQ_EP93XX_TIMER2, ep93xx_timer2_isr,
			  SA_INTERRUPT, "ep93xx_timer2", dev);
	if (err) {
		printk(KERN_WARNING "ep93xx_ts: failed to get timer2 IRQ\n");
		free_irq(IRQ_EP93XX_TOUCH, dev);
		return err;
	}

	ep93xx_hw_setup();

	return 0;
}

static void ep93xx_ts_close(struct input_dev *dev)
{
	Stop_Timer2();

	ep93xx_hw_shutdown();

	free_irq(IRQ_EP93XX_TIMER2, dev);
	free_irq(IRQ_EP93XX_TOUCH, dev);

	up(&open_sem);
}

static void ep93xx_hw_setup(void)
{
	unsigned int uiKTDIV, uiTSXYMaxMin;

	/*
	 * Set the TSEN bit in KTDIV so that we are enabling the clock
	 * for the touchscreen.
	 */
	uiKTDIV = readl(SYSCON_KTDIV);
	uiKTDIV |= SYSCON_KTDIV_TSEN;
	SysconSetLocked(SYSCON_KTDIV, uiKTDIV);

	writel(TSSETUP_DEFAULT, TSSetup);
	writel(TSSETUP2_DEFAULT, TSSetup2);

	/* Set the the touch settings.  */
	writel(0xaa, TSSWLock);
	writel(sSwitchSettings.uiDischarge, TSDirect);

	writel(0xaa, TSSWLock);
	writel(sSwitchSettings.uiDischarge, TSDischarge);

	writel(0xaa, TSSWLock);
	writel(sSwitchSettings.uiSwitchZ1, TSXSample);

	writel(0xaa, TSSWLock);
	writel(sSwitchSettings.uiSwitchZ2, TSYSample);

	writel(0xaa, TSSWLock);
	writel(sSwitchSettings.uiDetect, TSDetect);

	/*
	 * X,YMin set to 0x40 = have to drag that many pixels for a new irq.
	 * X,YMax set to 0x40 = 1024 pixels is the maximum movement within the
	 * time scan limit.
	 */
	uiTSXYMaxMin = (50 << TSMAXMIN_XMIN_SHIFT) & TSMAXMIN_XMIN_MASK;
	uiTSXYMaxMin |= (50 << TSMAXMIN_YMIN_SHIFT) & TSMAXMIN_YMIN_MASK;
	uiTSXYMaxMin |= (0xff << TSMAXMIN_XMAX_SHIFT) & TSMAXMIN_XMAX_MASK;
	uiTSXYMaxMin |= (0xff << TSMAXMIN_YMAX_SHIFT) & TSMAXMIN_YMAX_MASK;
	writel(uiTSXYMaxMin, TSXYMaxMin);

	bCurrentPenDown = 0;
	guiLastX = 0;
	guiLastY = 0;
	guiLastInvPressure = 0xffffff;

	/* Enable the touch screen scanning engine. */

	TS_Hardware_Scan_Mode();

}

static void ep93xx_hw_shutdown(void)
{
	unsigned int uiKTDIV;

	sTouch.state = TS_STATE_STOPPED;
	Stop_Timer2();

	/*
	 * Disable the scanning engine.
	 */
	writel(0, TSSetup);
	writel(0, TSSetup2);

	/*
	 * Clear the TSEN bit in KTDIV so that we are disabling the clock
	 * for the touchscreen.
	 */
	uiKTDIV = readl(SYSCON_KTDIV);
	uiKTDIV &= ~SYSCON_KTDIV_TSEN;
	SysconSetLocked(SYSCON_KTDIV, uiKTDIV);

}

static irqreturn_t ep93xx_timer2_isr(int irq, void *dev_id, struct pt_regs *regs)
{
	switch (sTouch.state) {
	case TS_STATE_STOPPED:
		TS_Hardware_Scan_Mode();
		break;

		/*
		 * Get the Z1 value for pressure measurement and set up
		 * the switch register for getting the Z2 measurement.
		 */
	case TS_STATE_Z1:
		Set_Timer2_uSec(EP93XX_TS_ADC_DELAY_USEC);
		sTouch.uiZ1 = ADCGetData(2, 200);
		ep93xx_ts_set_direct(sSwitchSettings.uiSwitchZ2);
		sTouch.state = TS_STATE_Z2;
		break;

		/*
		 * Get the Z2 value for pressure measurement and set up
		 * the switch register for getting the Y measurement.
		 */
	case TS_STATE_Z2:
		sTouch.uiZ2 = ADCGetData(2, 200);
		ep93xx_ts_set_direct(sSwitchSettings.uiYSample);
		sTouch.state = TS_STATE_Y;
		break;

		/*
		 * Get the Y value and set up the switch register for
		 * getting the X measurement.
		 */
	case TS_STATE_Y:
		sTouch.uiY = ADCGetData(4, 20);
		ep93xx_ts_set_direct(sSwitchSettings.uiXSample);
		sTouch.state = TS_STATE_X;
		break;

		/*
		 * Read the X value.  This is the last of the 4 adc values
		 * we need so we continue on to process the data.
		 */
	case TS_STATE_X:
		Stop_Timer2();

		sTouch.uiX = ADCGetData(4, 20);

		writel(0xaa, TSSWLock);
		writel(sSwitchSettings.uiDischarge, TSDirect);

		sTouch.state = TS_STATE_DONE;

		/* Process this set of ADC readings. */
		ProcessPointData(dev_id);

		break;

		/* Shouldn't get here.  But if we do, we can recover... */
	case TS_STATE_DONE:
		TS_Hardware_Scan_Mode();
		break;
	}

	/* Clear the timer2 interrupt. */
	writel(1, TIMER2CLEAR);
	return IRQ_HANDLED;
}

/*---------------------------------------------------------------------
 * ProcessPointData
 *
 * This routine processes the ADC data into usable point data and then
 * puts the driver into hw or sw scanning mode before returning.
 *
 * We calculate inverse pressure (lower number = more pressure) then
 * do a hystheresis with the two pressure values 'light' and 'heavy'.
 *
 * If we are above the light, we have pen up.
 * If we are below the heavy we have pen down.
 * As long as the pressure stays below the light, pen stays down.
 * When we get above the light again, pen goes back up.
 *
 */
static void ProcessPointData(struct input_dev *dev)
{
	int bValidPoint = 0;
	unsigned int uiXDiff, uiYDiff, uiInvPressureDiff;
	unsigned int uiInvPressure;

	/* Calculate the current pressure. */
	uiInvPressure = CalculateInvPressure();

	DPRINTK(" X=0x%x, Y=0x%x, Z1=0x%x, Z2=0x%x, InvPressure=0x%x",
			  sTouch.uiX, sTouch.uiY, sTouch.uiZ1, sTouch.uiZ2,
			  uiInvPressure);

	/*
	 * If pen pressure is so light that it is greater than the 'max' setting
	 * then we consider this to be a pen up.
	 */
	if (uiInvPressure >= TS_LIGHT_INV_PRESSURE) {
		bCurrentPenDown = 0;
/*		input_report_key(dev, BTN_TOUCH, 0); */
		input_report_abs(dev, ABS_PRESSURE, 0);
		input_sync(dev);
		TS_Hardware_Scan_Mode();
		return;
	}
	/*
	 * Hystheresis:
	 * If the pen pressure is hard enough to be less than the 'min' OR
	 * the pen is already down and is still less than the 'max'...
	 */
	if ((uiInvPressure < TS_HEAVY_INV_PRESSURE) ||
	    (bCurrentPenDown && (uiInvPressure < TS_LIGHT_INV_PRESSURE))) {
		if (bCurrentPenDown) {
			/*
			 * If pen was previously down, check the difference between
			 * the last sample and this one... if the difference between
			 * samples is too great, ignore the sample.
			 */
			uiXDiff = abs(guiLastX - sTouch.uiX);
			uiYDiff = abs(guiLastY - sTouch.uiY);
			uiInvPressureDiff = abs(guiLastInvPressure - uiInvPressure);

			if ((uiXDiff < TS_MAX_VALID_XY_CHANGE) &&
			    (uiYDiff < TS_MAX_VALID_XY_CHANGE) &&
			    (uiInvPressureDiff < TS_MAX_VALID_PRESSURE_CHANGE))
			{
				bValidPoint = 1;
			} 
		} else {
			bValidPoint = 1;
		}

		/*
		 * If either the pen was put down or dragged make a note of it.
		 */
		if (bValidPoint) {
			guiLastX = sTouch.uiX;
			guiLastY = sTouch.uiY;
			guiLastInvPressure = uiInvPressure;
			bCurrentPenDown = 1;
			input_report_abs(dev, ABS_X, sTouch.uiX);
			input_report_abs(dev, ABS_Y, sTouch.uiY);
/* 			input_report_key(dev, BTN_TOUCH, 1); */
			input_report_abs(dev, ABS_PRESSURE, 1);
			input_sync(dev);
		}
		TS_Soft_Scan_Mode();

		return;
	}

	TS_Hardware_Scan_Mode();
}

static void ep93xx_ts_set_direct(unsigned int uiADCSwitch)
{
	unsigned int uiResult;

	/* Set the switch settings in the direct register. */
	writel(0xaa, TSSWLock);
	writel(uiADCSwitch, TSDirect);

	/* Read and throw away the first sample. */
	do {
		uiResult = readl(TSXYResult);
	} while (!(uiResult & TSXYRESULT_SDR));

}

static unsigned int ADCGetData(unsigned int uiSamples, unsigned int uiMaxDiff)
{
	unsigned int uiResult, uiValue, uiCount, uiLowest, uiHighest, uiSum, uiAve;

	do {
		/* Initialize our values. */
		uiLowest = 0xfffffff;
		uiHighest = 0;
		uiSum = 0;

		for (uiCount = 0; uiCount < uiSamples; uiCount++) {
			/* Read the touch screen four more times and average. */
			do {
				uiResult = readl(TSXYResult);
			} while (!(uiResult & TSXYRESULT_SDR));

			uiValue =
			    (uiResult & TSXYRESULT_AD_MASK) >>
			    TSXYRESULT_AD_SHIFT;
			uiValue =
			    ((uiValue >> 4) +
			     ((1 +
			       TSXYRESULT_X_MASK) >> 1)) & TSXYRESULT_X_MASK;

			/* Add up the values. */
			uiSum += uiValue;

			/* Get the lowest and highest values. */
			if (uiValue < uiLowest)
				uiLowest = uiValue;

			if (uiValue > uiHighest)
				uiHighest = uiValue;

		}

	} while ((uiHighest - uiLowest) > uiMaxDiff);

	/* Calculate the Average value. */
	uiAve = uiSum / uiSamples;

	return uiAve;
}

/**
 * CalculateInvPressure
 *
 * Is the Touch Valid.  Touch is not valid if the X or Y value is not
 * in range and the pressure is not  enough.
 *
 * Touch resistance can be measured by the following formula:
 *
 *          Rx * X *     Z2
 * Rtouch = --------- * (-- - 1)
 *           4096        Z1
 *
 * This is simplified in the ration of Rtouch to Rx.  The lower the value, the
 * higher the pressure.
 *
 *                     Z2
 * InvPressure =  X * (-- - 1)
 *                     Z1
 */
static unsigned int CalculateInvPressure(void)
{
	unsigned int uiInvPressure;

	/* Check to see if the point is valid. */
	if (sTouch.uiZ1 < MIN_Z1_VALUE)
		uiInvPressure = 0x10000;

	/* Can omit the pressure calculation if you need to get rid of the division. */
	else {
		uiInvPressure =
		    ((sTouch.uiX * sTouch.uiZ2) / sTouch.uiZ1) - sTouch.uiX;
	}

	return uiInvPressure;
}

/**
 * TS_Hardware_Scan_Mode
 * Enables the ep93xx ts scanning engine so that when the pen goes down
 * we will get an interrupt.
 */
static void TS_Hardware_Scan_Mode(void)
{
	unsigned int uiDevCfg;

	/* Disable the soft scanning engine. */
	sTouch.state = TS_STATE_STOPPED;
	Stop_Timer2();

	/*
	 * Clear the TIN (Touchscreen INactive) bit so we can go to
	 * automatic scanning mode.
	 */
	uiDevCfg = readl(SYSCON_DEVCFG);
	SysconSetLocked(SYSCON_DEVCFG, (uiDevCfg & ~SYSCON_DEVCFG_TIN));

	/*
	 * Enable the touch screen scanning state machine by setting
	 * the ENABLE bit.
	 */
	writel((TSSETUP_DEFAULT | TSSETUP_ENABLE), TSSetup);

	/* Set the flag to show that we are in interrupt mode. */
	gScanningMode = TS_MODE_HARDWARE_SCAN;

	/* Initialize TSSetup2 register. */
	writel(TSSETUP2_DEFAULT, TSSetup2);
}

/**
 * TS_Soft_Scan_Mode - Set the touch screen to manual polling mode.
 */
static void TS_Soft_Scan_Mode(void)
{
	unsigned int uiDevCfg;

	if (gScanningMode != TS_MODE_SOFT_SCAN) {
		/*
		 * Disable the touch screen scanning state machine by clearing
		 * the ENABLE bit.
		 */
		writel(TSSETUP_DEFAULT, TSSetup);

		/* Set the TIN bit so we can do manual touchscreen polling. */
		uiDevCfg = readl(SYSCON_DEVCFG);
		SysconSetLocked(SYSCON_DEVCFG, (uiDevCfg | SYSCON_DEVCFG_TIN));
	}
	/* Set the switch register up for the first ADC reading */
	ep93xx_ts_set_direct(sSwitchSettings.uiSwitchZ1);

	/*
	 * Initialize our software state machine to know which ADC
	 * reading to take
	 */
	sTouch.state = TS_STATE_Z1;

	/*
	 * Set the timer so after a mSec or two settling delay it will
	 * take the first ADC reading.
	 */
	Set_Timer2_uSec(EP93XX_TS_PER_POINT_DELAY_USEC);

	/* Note that we are in sw scanning mode not hw scanning mode. */
	gScanningMode = TS_MODE_SOFT_SCAN;

}

static void Set_Timer2_uSec(unsigned int uiDelay_uSec)
{
	unsigned int uiClockTicks;

	/*
	 * Stop timer 2
	 */
	writel(0, TIMER2CONTROL);

	uiClockTicks = ((uiDelay_uSec * 508) + 999) / 1000;
	writel(uiClockTicks, TIMER2LOAD);
	writel(uiClockTicks, TIMER2VALUE);

	/*
	 * Set up Timer 2 for 508 kHz clock and periodic mode.
	 */
	writel(0xC8, TIMER2CONTROL);

}

static void Stop_Timer2(void)
{
	writel(0, TIMER2CONTROL);
}

static int __init ep93xx_ts_init(void)
{
	struct input_dev *input_dev;

	/* Initialise input stuff */
	memset(&sTouch, 0, sizeof(struct ts_struct_t));
	input_dev = input_allocate_device();
	if (!input_dev) {
		printk(KERN_ERR "Unable to allocate the input device !!\n");
		return -ENOMEM;
	}

	sTouch.dev = input_dev;
	sTouch.dev->evbit[0]  	= BIT(EV_SYN) | BIT(EV_KEY) | BIT(EV_ABS);
	sTouch.dev->absbit[0] 	= BIT(ABS_X)  | BIT(ABS_Y)  | BIT(ABS_PRESSURE);
	sTouch.dev->keybit[LONG(BTN_TOUCH)] = BIT(BTN_TOUCH);

	sTouch.dev->private		= &sTouch;
	sTouch.dev->name		= "Cirrus Logic EP93xx Touchscreen";
	sTouch.dev->phys		= "ep93xx_ts/input0";
	sTouch.dev->id.bustype	= BUS_HOST;
	sTouch.dev->id.vendor	= PCI_VENDOR_ID_CIRRUS;
	sTouch.dev->id.product	= 0x9300;

	sTouch.dev->open		= ep93xx_ts_open;
	sTouch.dev->close		= ep93xx_ts_close;

	sTouch.state			= TS_STATE_STOPPED;
	gScanningMode			= TS_MODE_UN_INITIALIZED;

	input_set_abs_params(sTouch.dev, ABS_X, TS_X_MIN, TS_X_MAX, 0, 0);
	input_set_abs_params(sTouch.dev, ABS_Y, TS_Y_MIN, TS_Y_MAX, 0, 0);
	input_register_device(sTouch.dev);

	printk(KERN_NOTICE "EP93xx touchscreen driver configured for 4-wire operation\n");

	return 0;
}

static void __exit ep93xx_ts_exit(void)
{
	input_unregister_device(sTouch.dev);
}

module_init(ep93xx_ts_init);
module_exit(ep93xx_ts_exit);

MODULE_AUTHOR("FALINUX, tsheaven@falinux.com");
MODULE_DESCRIPTION("Cirrus EP93xx touchscreen driver");
MODULE_SUPPORTED_DEVICE("touchscreen/ep93xx");
