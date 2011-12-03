/*-----------------------------------------------------------------------------
  �� �� : touch_wm9712.h
  �� �� : 
  �� �� : ����� freefrug@falinux.com
  �� ¥ : 2007-11-22
  �� �� :

-------------------------------------------------------------------------------*/
#ifndef _TOUCH_WM9712_H_
#define _TOUCH_WM9712_H_


#define	TRACE()	printk( " __%s (%d)\n", __FUNCTION__, __LINE__ )

/*
 * WM97xx AC97 Touchscreen registers
 */
#define AC97_WM97XX_DIGITISER1		0x76
#define AC97_WM97XX_DIGITISER2		0x78
#define AC97_WM97XX_DIGITISER_RD 	0x7a
#define AC97_WM9713_DIG1			0x74
#define AC97_WM9713_DIG2			AC97_WM97XX_DIGITISER1
#define AC97_WM9713_DIG3			AC97_WM97XX_DIGITISER2


#define AC97_WM9712_GPIO_CFG        0x4c
#define AC97_WM9712_GPIO_FUNC       0x56

/*
 * WM97xx register bits
 */
#define WM97XX_POLL			0x8000	/* initiate a polling measurement */
#define WM97XX_ADCSEL_X		0x1000	/* x coord measurement */
#define WM97XX_ADCSEL_Y		0x2000	/* y coord measurement */
#define WM97XX_ADCSEL_PRES	0x3000	/* pressure measurement */
#define WM97XX_ADCSEL_MASK	0x7000
#define WM97XX_COO			0x0800	/* enable coordinate mode */
#define WM97XX_CTC			0x0400	/* enable continuous mode */
#define WM97XX_CM_RATE_93	0x0000	/* 93.75Hz continuous rate */
#define WM97XX_CM_RATE_187	0x0100	/* 187.5Hz continuous rate */
#define WM97XX_CM_RATE_375	0x0200	/* 375Hz continuous rate */
#define WM97XX_CM_RATE_750	0x0300	/* 750Hz continuous rate */
#define WM97XX_CM_RATE_8K	0x00f0	/* 8kHz continuous rate */
#define WM97XX_CM_RATE_12K	0x01f0	/* 12kHz continuous rate */
#define WM97XX_CM_RATE_24K	0x02f0	/* 24kHz continuous rate */
#define WM97XX_CM_RATE_48K	0x03f0	/* 48kHz continuous rate */
#define WM97XX_CM_RATE_MASK	0x03f0
#define WM97XX_RATE(i)		(((i & 3) << 8) | ((i & 4) ? 0xf0 : 0))
#define WM97XX_DELAY(i)		((i << 4) & 0x00f0)	/* sample delay times */
#define WM97XX_DELAY_MASK	0x00f0
#define WM97XX_SLEN			0x0008	/* slot read back enable */
#define WM97XX_SLT(i)		((i - 5) & 0x7)	/* touchpanel slot selection (5-11) */
#define WM97XX_SLT_MASK		0x0007
#define WM97XX_PRP_DETW		0x4000	/* pen detect on, digitiser off, wake up */
#define WM97XX_PRP_DET		0x8000	/* pen detect on, digitiser off, no wake up */
#define WM97XX_PRP_DET_DIG	0xc000	/* pen detect on, digitiser on */
#define WM97XX_RPR			0x2000	/* wake up on pen down */
#define WM97XX_PEN_DOWN		0x8000	/* pen is down */
#define WM97XX_ADCSRC_MASK	0x7000	/* ADC source mask */

#define WM97XX_AUX_ID1		0x8001
#define WM97XX_AUX_ID2		0x8002
#define WM97XX_AUX_ID3		0x8003
#define WM97XX_AUX_ID4		0x8004


/* WM9712 Bits */
#define WM9712_45W			0x1000	/* set for 5-wire touchscreen */
#define WM9712_PDEN			0x0800	/* measure only when pen down */
#define WM9712_WAIT			0x0200	/* wait until adc is read before next sample */
#define WM9712_PIL			0x0100	/* current used for pressure measurement. set 400uA else 200uA */
#define WM9712_MASK_HI		0x0040	/* hi on mask pin (47) stops conversions */
#define WM9712_MASK_EDGE	0x0080	/* rising/falling edge on pin delays sample */
#define	WM9712_MASK_SYNC	0x00c0	/* rising/falling edge on mask initiates sample */
#define WM9712_RPU(i)		(i&0x3f)	/* internal pull up on pen detect (64k / rpu) */
#define WM9712_PD(i)		(0x1 << i)	/* power management */

/* WM9712 Registers */
#define AC97_WM9712_POWER	0x24
#define AC97_WM9712_REV		0x58

/* WM9705 Bits */
#define WM9705_PDEN			0x1000	/* measure only when pen is down */
#define WM9705_PINV			0x0800	/* inverts sense of pen down output */
#define WM9705_BSEN			0x0400	/* BUSY flag enable, pin47 is 1 when busy */
#define WM9705_BINV			0x0200	/* invert BUSY (pin47) output */
#define WM9705_WAIT			0x0100	/* wait until adc is read before next sample */
#define WM9705_PIL			0x0080	/* current used for pressure measurement. set 400uA else 200uA */
#define WM9705_PHIZ			0x0040	/* set PHONE and PCBEEP inputs to high impedance */
#define WM9705_MASK_HI		0x0010	/* hi on mask stops conversions */
#define WM9705_MASK_EDGE	0x0020	/* rising/falling edge on pin delays sample */
#define	WM9705_MASK_SYNC	0x0030	/* rising/falling edge on mask initiates sample */
#define WM9705_PDD(i)		(i & 0x000f)	/* pen detect comparator threshold */


/* WM9713 Bits */
#define WM9713_PDPOL		0x0400	/* Pen down polarity */
#define WM9713_POLL			0x0200	/* initiate a polling measurement */
#define WM9713_CTC			0x0100	/* enable continuous mode */
#define WM9713_ADCSEL_X		0x0002	/* X measurement */
#define WM9713_ADCSEL_Y		0x0004	/* Y measurement */
#define WM9713_ADCSEL_PRES	0x0008	/* Pressure measurement */
#define WM9713_COO			0x0001	/* enable coordinate mode */
#define WM9713_PDEN			0x0800	/* measure only when pen down */
#define WM9713_ADCSEL_MASK	0x00fe	/* ADC selection mask */

/* AUX ADC ID's */
#define TS_COMP1			0x0
#define TS_COMP2			0x1
#define TS_BMON				0x2
#define TS_WIPER			0x3

/* ID numbers */
#define WM97XX_ID1			0x574d
#define WM9712_ID2			0x4c12
#define WM9705_ID2			0x4c05
#define WM9713_ID2			0x4c13

/* Codec GPIO's */
#define WM97XX_MAX_GPIO		16
#define WM97XX_GPIO_1		(1 << 1)
#define WM97XX_GPIO_2		(1 << 2)
#define WM97XX_GPIO_3		(1 << 3)
#define WM97XX_GPIO_4		(1 << 4)
#define WM97XX_GPIO_5		(1 << 5)
#define WM97XX_GPIO_6		(1 << 6)
#define WM97XX_GPIO_7		(1 << 7)
#define WM97XX_GPIO_8		(1 << 8)
#define WM97XX_GPIO_9		(1 << 9)
#define WM97XX_GPIO_10		(1 << 10)
#define WM97XX_GPIO_11		(1 << 11)
#define WM97XX_GPIO_12		(1 << 12)
#define WM97XX_GPIO_13		(1 << 13)
#define WM97XX_GPIO_14		(1 << 14)
#define WM97XX_GPIO_15		(1 << 15)

#define AC97_LINK_FRAME		21	/* time in uS for AC97 link frame */



#endif  


