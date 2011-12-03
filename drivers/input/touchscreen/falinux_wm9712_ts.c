/*-----------------------------------------------------------------------------
  파 일 : touch_wm9712.c
  설 명 : AU12xx MCU 에서  AC97 WM9712에서 지원하는 터치를 구현한다.
  작 성 : 오재경 freefrug@falinux.com
  날 짜 : 2007-11-22
  주 의 :

-------------------------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h> 
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/ioport.h>
#include <linux/slab.h>     // kmalloc() 
#include <linux/poll.h>     // poll
#include <linux/proc_fs.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/platform_device.h>

#include "falinux_wm9712_ts.h"

#ifdef CONFIG_PXA27x
#include <linux/wm97xx.h>
#include <sound/soc.h>
#endif //CONFIG_PXA27x


#define WM9712TS_VERSION		0x0100

#define WM9712TS_ADC_MAX_MASK   0x0fff
#define DEFAULT_PRESSURE		10

#define	DELAY_TIME				((HZ)/50)


static char *wm9712ts_name = "wm9712 TouchScreen 0.30";

/*
 * ADC sample delay times in uS
 */
static const int delay_table[] = {
	21,    // 1 AC97 Link frames
	42,    // 2
	84,    // 4
	167,   // 8
	333,   // 16
	667,   // 32
	1000,  // 48
	1333,  // 64
	2000,  // 96
	2667,  // 128
	3333,  // 160
	4000,  // 192
	4667,  // 224
	5333,  // 256
	6000,  // 288
	0      // No delay, switch matrix always on
};

/*
 * Set internal pull up for pen detect.
 *
 * Pull up is in the range 1.02k (least sensitive) to 64k (most sensitive)
 * i.e. pull up resistance = 64k Ohms / rpu.
 *
 * Adjust this value if you are having problems with pen detect not
 * detecting any down evenwm->
 */
static int rpu = 0;
module_param(rpu, int, 0);
MODULE_PARM_DESC(rpu, "Set internal pull up resitor for pen detect.");

/*
 * Set current used for pressure measurement.
 *
 * Set pil = 2 to use 400uA
 *     pil = 1 to use 200uA and
 *     pil = 0 to disable pressure measurement.
 *
 * This is used to increase the range of values returned by the adc
 * when measureing touchpanel pressure.
 */
static int pil = 0;
module_param(pil, int, 0);
MODULE_PARM_DESC(pil, "Set current used for pressure measurement.");


/*
 * Set threshold for pressure measurement.
 *
 * Pen down pressure below threshold is ignored.
 */
static int pressure = DEFAULT_PRESSURE & 0xfff;
module_param(pressure, int, 0);
MODULE_PARM_DESC(pressure, "Set threshold for pressure measurement.");


/*
 * Set adc sample delay.
 *
 * For accurate touchpanel measurements, some settling time may be
 * required between the switch matrix applying a voltage across the
 * touchpanel plate and the ADC sampling the signal.
 *
 * This delay can be set by setting delay = n, where n is the array
 * position of the delay in the array delay_table below.
 * Long delays > 1ms are supported for completeness, but are not
 * recommended.
 */
static int delay_mode = 4;
module_param(delay_mode, int, 0);
MODULE_PARM_DESC(delay_mode, "Set adc sample delay_mode.");


static int adc_shift = 0;
static int showmsg   = 0;

module_param(adc_shift, int, 0);
module_param(showmsg  , int, 0);


struct wm9712ts {
	struct input_dev   *dev;
	struct work_struct irq_work;
	struct timer_list  timer;
	
	int    irq;
	
	unsigned short dig1, dig2;	
	int xp;
	int yp;
	int press;
};

static struct wm9712ts ts;
static struct wm97xx *wm;

static void wm9712ts_delay_timer( void );
/*-----------------------------------------------------------------------------
  설 명 : ac97 통신 설정.
  주 의 :
-------------------------------------------------------------------------------*/
void wm97xx_setup( struct platform_device *pdev )
{
#ifdef CONFIG_PXA27x	
	if (!(wm = kmalloc(sizeof(struct wm97xx), GFP_KERNEL))) {
		return;
	}
	wm->dev = &pdev->dev;
	wm->ac97 = to_ac97_t(&pdev->dev);
	wm->ac97->num = 0;
	
	set_irq_type(ts.irq, IRQT_RISING);
#endif //CONFIG_PXA27x	
}

/*-----------------------------------------------------------------------------
  설 명 : ac97 통신 설정.
  주 의 :
-------------------------------------------------------------------------------*/
void wm97xx_closeup(void)
{
#ifdef CONFIG_PXA27x
	kfree(wm);
#endif //CONFIG_PXA27x
}

/*-----------------------------------------------------------------------------
  설 명 : wm9712 엑세스 함수
  주 의 :
-------------------------------------------------------------------------------*/
#ifdef CONFIG_PXA27x
  #define WRITE_WM97XX_REG(r,v)	soc_ac97_ops.write(wm->ac97, r, v)
  #define READ_WM97XX_REG(r)	soc_ac97_ops.read(wm->ac97, r)
#else		// au12xx
  #define WRITE_WM97XX_REG(r,v)	falinux_ac97_reg_write( r, v )
  #define READ_WM97XX_REG(r)	falinux_ac97_reg_read( r )

  extern void falinux_ac97_reg_write( int reg, int val );
  extern int  falinux_ac97_reg_read ( int reg );
#endif //CONFIG_PXA27x

static inline void wm97xx_write_reg( int reg, int val )
{
	WRITE_WM97XX_REG(reg, val);
}

static inline int wm97xx_read_reg( int reg )
{
	return READ_WM97XX_REG(reg);
}
/*-----------------------------------------------------------------------------
  설 명 : 샘플 시작
  주 의 :
-------------------------------------------------------------------------------*/
static void wm9712ts_sample_start( void )
{
	int dumy;

	ts.dig2 |= WM97XX_PRP_DET_DIG;
	wm97xx_write_reg( AC97_WM97XX_DIGITISER2, ts.dig2 );
	
	// dummy read 
	dumy = wm97xx_read_reg( AC97_WM97XX_DIGITISER_RD ); 
}
/*-----------------------------------------------------------------------------
  설 명 : 샘플 정지
  주 의 :
-------------------------------------------------------------------------------*/
static void wm9712ts_sample_stop( void )
{
	int dumy;
	
	ts.dig2 &= ~WM97XX_PRP_DET_DIG;
	wm97xx_write_reg( AC97_WM97XX_DIGITISER2, ts.dig2 );
	
	// dummy read 
	dumy = wm97xx_read_reg( AC97_WM97XX_DIGITISER_RD ); 
}
/*-----------------------------------------------------------------------------
  설 명 : 샘플 얻기
  주 의 :
-------------------------------------------------------------------------------*/
static int wm9712ts_sample_get( int adcsel, unsigned int *sample )
{
	unsigned short digv = 0;
	int timeout; 				// = 5 * delay_mode;
	
	*sample = 0;
	
	switch( adcsel )
	{
	case WM97XX_ADCSEL_X    : digv = ts.dig1 | WM97XX_ADCSEL_X    | WM97XX_POLL; break;
	case WM97XX_ADCSEL_Y    : digv = ts.dig1 | WM97XX_ADCSEL_Y    | WM97XX_POLL; break;
	case WM97XX_ADCSEL_PRES : digv = ts.dig1 | WM97XX_ADCSEL_PRES | WM97XX_POLL; break;
	default : return -1;
	}
	
	timeout = delay_table[delay_mode]/AC97_LINK_FRAME;
	timeout += 5;

	// set up digitiser ADC
	wm97xx_write_reg( AC97_WM97XX_DIGITISER1, digv );

	// wait 3 AC97 time slots + delay for conversion 
	// poll_delay (delay);
	// udelay (3 * AC97_LINK_FRAME + delay_table [delay]);
	udelay (AC97_LINK_FRAME*3);

	// wait for POLL to go low
	while ( (wm97xx_read_reg(AC97_WM97XX_DIGITISER1) & WM97XX_POLL ) && timeout ) 
	{
		udelay(AC97_LINK_FRAME);
		timeout--;
	}
	
	if (timeout <= 0) 
	{
		if (showmsg) printk("adc sample timeout\n");
		return -1;
	}
	
	
	{
		*sample = wm97xx_read_reg( AC97_WM97XX_DIGITISER_RD );

//if ( adcsel == WM97XX_ADCSEL_X )
//	printk("%d\n",  ( *sample & WM9712TS_ADC_MAX_MASK) >> adc_shift );		

	}


	return 0;
}


/*-----------------------------------------------------------------------------
  설 명 : 타이머 함수
  주 의 : 
-------------------------------------------------------------------------------*/
#ifdef CONFIG_PXA27x
#define READ_GPIO(IRQ)	pxa_gpio_get_value(IRQ_TO_GPIO(IRQ))
#else		// au12xx
	// AU1200 gpio read
#define READ_GPIO(IRQ)	au1xxx_gpio_read(IRQ - 32 )
#endif //CONFIG_PXA27x


static void wm9712ts_timer_handler( unsigned long arg )
{
	int rtn, x, y, p;
	int gpio_value;
	
	if ( 0 != READ_GPIO(ts.irq))
	{
		wm9712ts_sample_get( WM97XX_ADCSEL_X, &x );
		if ( x & WM97XX_PEN_DOWN )
		{
			wm9712ts_sample_get( WM97XX_ADCSEL_Y, &y );
			if ( y & WM97XX_PEN_DOWN )
			{
				ts.xp    = (x & WM9712TS_ADC_MAX_MASK) >> adc_shift;
				ts.yp    = (y & WM9712TS_ADC_MAX_MASK) >> adc_shift;
				//ts.press = (p & WM9712TS_ADC_MAX_MASK) >> adc_shift;
	 			input_report_abs(ts.dev, ABS_X, ts.xp);
	 			input_report_abs(ts.dev, ABS_Y, ts.yp);
	    
	 			input_report_key(ts.dev, BTN_TOUCH   , 1       );
	 			input_report_abs(ts.dev, ABS_PRESSURE, 1       );	//input_report_abs(ts.dev, ABS_PRESSURE, ts.press);
	 			input_sync(ts.dev);
				
				wm9712ts_delay_timer();
				
				if (showmsg) 
					printk( "touch %d:%d (%d)\n", ts.xp, ts.yp, ts.press );
				
				return;	
			}
		}
	}

	// Pen Up								
	{
 		input_report_key(ts.dev, BTN_TOUCH   , 0);
 		input_report_abs(ts.dev, ABS_PRESSURE, 0);
 		input_sync(ts.dev);
 		
 		if (showmsg) 
 			printk( "touch up\n" );
 		
 		// 인터럽트 활성화.
 		enable_irq(ts.irq);
	}
}
/*-----------------------------------------------------------------------------
  설 명 : 타이머 설정
  주 의 :
-------------------------------------------------------------------------------*/
static void wm9712ts_delay_timer( void )
{
	init_timer( &ts.timer );
	ts.timer.expires  = get_jiffies_64() + DELAY_TIME;
	ts.timer.function = wm9712ts_timer_handler;
	ts.timer.data     = 0;
	
	add_timer( &ts.timer );
}

/*-----------------------------------------------------------------------------
  설 명 : 인터럽트
  주 의 :
-------------------------------------------------------------------------------*/
static irqreturn_t wm9712ts_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	wm9712ts_delay_timer();
	disable_irq(ts.irq);

	return IRQ_HANDLED;
}
/*-----------------------------------------------------------------------------
  설 명 : wm9712ts setup
  주 의 :
-------------------------------------------------------------------------------*/
static void wm9712ts_setup( void )
{
	ts.dig1 = 0; //WM97XX_COO;
	ts.dig2 = WM97XX_RPR | WM9712_RPU(1);	// four wire  d12 = 0
	
	// WM9712 rpu
	if (rpu) 
	{
		ts.dig2 &= 0xffc0;
		ts.dig2 |= WM9712_RPU(rpu);
	}
	
	// touchpanel pressure current
	if  (pil == 2) 
	{
		ts.dig2 |= WM9712_PIL;
		if (showmsg) printk("setting pressure measurement current to 400uA.");
	} 
	else if (pil) 
	{
		if (showmsg) printk("setting pressure measurement current to 200uA.");
	}
	
	if( pil ) pressure = 0;

	// polling mode sample settling delay
	if ( delay_mode!=4 ) 
	{
		if (delay_mode < 0 || delay_mode > 15) 
		{
		    if (showmsg) printk("supplied delay out of range.");
		    delay_mode = 4;
		}
	}
	ts.dig1 &= 0xff0f;
	ts.dig1 |= WM97XX_DELAY(delay_mode);
	
	printk("setting adc sample delay to %d u Secs.", delay_table[delay_mode] );
	
	wm97xx_write_reg(AC97_WM97XX_DIGITISER1, ts.dig1 );
	wm97xx_write_reg(AC97_WM97XX_DIGITISER2, ts.dig2 );
	
	// setup pendown-pin
	{
		unsigned short gpv;
		
		// select gpio3 is a output pin
		gpv = wm97xx_read_reg(AC97_WM9712_GPIO_CFG );
		gpv &= ~(1<<3);
		wm97xx_write_reg( AC97_WM9712_GPIO_CFG, gpv );

		// select gpio3 is the pendown function
		gpv = wm97xx_read_reg(AC97_WM9712_GPIO_FUNC );
		gpv &= ~(1<<3);
		wm97xx_write_reg( AC97_WM9712_GPIO_FUNC, gpv );
	}
}

static unsigned int first_open_flag = 0;
static struct platform_device *ts_pdev;
/*-----------------------------------------------------------------------------
  설 명 : open 
  			최초의 호출은 커널 부팅 시 호출되며 부팅 시 호출은 무시 하며, 
  			어플리케이션의 오픈시에만 wm9712 설정 및 터치를 샘플링한다.
  주 의 :
-------------------------------------------------------------------------------*/
int wm9712ts_open(struct input_dev *dev)
{
	if( first_open_flag )
	{
#ifdef CONFIG_PXA27x
		wm97xx_setup(ts_pdev);
#endif //CONFIG_PXA27x
		
		// wm9712 설정
		wm9712ts_setup();
		wm9712ts_sample_start();
	}
	else
	{
		//
		first_open_flag++;
		return -1;
	}
}

/*-----------------------------------------------------------------------------
  설 명 : probe
  주 의 :
-------------------------------------------------------------------------------*/
static int __init wm9712ts_probe( struct platform_device *pdev )
{
	struct input_dev *input_dev;

	memset( &ts, 0, sizeof(ts) );	

	// input 디바이스를 등록한다.	
	input_dev = input_allocate_device();
	if ( !input_dev ) 
	{
		printk( "Unable to allocate the input device !!\n" );
		return -ENOMEM;
	}
	
	// 터치 스크린 등록 설정.
	ts.dev = input_dev;
	
	ts.dev->evbit[0]  = BIT(EV_SYN) | BIT(EV_KEY) | BIT(EV_ABS);
	ts.dev->absbit[0] = BIT(ABS_X)  | BIT(ABS_Y)  | BIT(ABS_PRESSURE);
	ts.dev->keybit[LONG(BTN_TOUCH)] = BIT(BTN_TOUCH);

	ts.dev->private = &ts;
	ts.dev->absfuzz[ABS_X] = 16;
	ts.dev->absfuzz[ABS_Y] = 16;
	ts.dev->name    = wm9712ts_name;
	ts.dev->open 	= wm9712ts_open;
	ts.dev->id.bustype = BUS_RS232;
	ts.dev->id.vendor  = 0xfa00;
	ts.dev->id.product = 0x0000;
	ts.dev->id.version = WM9712TS_VERSION;
	ts_pdev = pdev;
	
	
	// irq  를 얻어온다.
	ts.irq = platform_get_irq(pdev, 0);
	if ( request_irq( ts.irq, wm9712ts_interrupt, SA_SAMPLE_RANDOM, wm9712ts_name, &ts ) ) 
	{
		input_free_device( ts.dev );
		
		printk( "Could not get resource irq=%d\n", ts.irq);
		return -EIO;
	}

	// 등록
	input_set_abs_params(ts.dev, ABS_X       , 0, WM9712TS_ADC_MAX_MASK >> adc_shift, 0, 0);
	input_set_abs_params(ts.dev, ABS_Y       , 0, WM9712TS_ADC_MAX_MASK >> adc_shift, 0, 0);
	input_set_abs_params(ts.dev, ABS_PRESSURE, 0, WM9712TS_ADC_MAX_MASK >> adc_shift, 0, 0);
	input_register_device( ts.dev );

	printk( "wm9712ts registe complete irq=%d\n", ts.irq );

	return 0;
}

/*-----------------------------------------------------------------------------
  설 명 : remove
  주 의 :
-------------------------------------------------------------------------------*/
static int wm9712ts_remove(struct platform_device *pdev)
{
	wm97xx_closeup();

	free_irq( ts.irq, &ts );

    del_timer( &ts.timer );
	
	input_unregister_device(ts.dev);
	input_free_device( ts.dev );
	
	return 0;
}


/*-----------------------------------------------------------------------------
  설 명 : 드라이버 구조체
  주 의 :
-------------------------------------------------------------------------------*/
static struct platform_driver wm9712ts_driver = {
       .driver         = {
	       .name   = "wm9712ts",
	       .owner  = THIS_MODULE,
       },
       .probe          = wm9712ts_probe,
       .remove         = wm9712ts_remove,
};

/*-----------------------------------------------------------------------------
  설 명 : 최초 호출
  주 의 :
-------------------------------------------------------------------------------*/
static int __init wm9712ts_init(void)
{
	return platform_driver_register(&wm9712ts_driver);
}
/*-----------------------------------------------------------------------------
  설 명 : 종료시 호출
  주 의 :
-------------------------------------------------------------------------------*/
static void __exit wm9712ts_exit(void)
{
	platform_driver_unregister(&wm9712ts_driver);
}

module_init(wm9712ts_init);
module_exit(wm9712ts_exit);


MODULE_AUTHOR("freefrug@falinux.com");
MODULE_LICENSE("GPL");


#ifdef CONFIG_PXA27x
EXPORT_SYMBOL_GPL(wm97xx_read_reg);
EXPORT_SYMBOL_GPL(wm97xx_write_reg);
#endif //CONFIG_PXA27x


/* end */










