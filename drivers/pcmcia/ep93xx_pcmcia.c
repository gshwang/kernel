/*======================================================================

  Device driver for the PCMCIA control functionality of PXA2xx
  microprocessors.

    The contents of this file may be used under the
    terms of the GNU Public License version 2 (the "GPL")

    (c) Ian Molton (spyro@f2s.com) 2003
    (c) Stefan Eletzhofer (stefan.eletzhofer@inquant.de) 2003,4

    derived from sa11xx_base.c

     Portions created by John G. Dorsey are
     Copyright (C) 1999 John G. Dorsey.

  ======================================================================*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/autoconf.h>
#include <linux/cpufreq.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>

#include <linux/delay.h>
//#include <linux/tqueue.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

#include <pcmcia/cs_types.h>
#include <pcmcia/ss.h>
#include <pcmcia/bulkmem.h>
#include <pcmcia/cistpl.h>


/*-------------------------------------------------------------------------------------------*/
#include "ep93xx_pcmcia.h"

#undef PCMCIA_DEBUG
#undef DEBUG
//#define PCMCIA_DEBUG 1
//#define DEBUG        1

#if 0
#define DEBUG(n, args...) printk(args);
#else
#define DEBUG(n, args...)

#ifdef PCMCIA_DEBUG
static int pc_debug = PCMCIA_DEBUG;
MODULE_PARM(pc_debug, "i");
#define DEBUG(n, args...) printk(KERN_DEBUG "ep93xx_pcmcia: " args);

#else
#define DEBUG(n, args...)
#endif

#endif

#ifdef EP93XX_PCMCIA_DEBUG
#define DBG(x...)	printk(x)
#else
#define DBG(x...)
#endif
#define PCMCIA_INFO(args...) printk(KERN_INFO "ep93xx_pcmcia: "args)
#define PCMCIA_ERROR(args...) printk(KERN_ERR "ep93xx_pcmcia: "args)

/*--------------------------------------------------------------------------------------------*/
/* Maximum number of IO windows per socket */
#define EP93XX_MAX_IO_WIN 2

/* Maximum number of memory windows per socket */
#define EP93XX_MAX_MEM_WIN 5

#define EP93XX_UNCONFIGURED_VOLTAGE 0xFF

struct pcmcia_irq_info {
	unsigned int sock;
	unsigned int irq;
};

static struct irqs {
	int irq;
	unsigned int gpio;
	const char *str;
} ep93xx_pcmcia_irqs[] =
{
	// Interrupt number       GPIO port F pin mask      String
	{ EP93XX_PCMCIA_INT_RDY,   EP93XX_PCMCIA_RDY,  "EP93XX PCMCIA RDY"  },
	{ EP93XX_PCMCIA_INT_CD1,   EP93XX_PCMCIA_CD1,  "EP93XX PCMCIA CD1"  },
	{ EP93XX_PCMCIA_INT_CD2,   EP93XX_PCMCIA_CD2,  "EP93XX PCMCIA CD2"  },
	{ EP93XX_PCMCIA_INT_BVD2,  EP93XX_PCMCIA_BVD2, "EP93XX PCMCIA BVD2" },
	{ EP93XX_PCMCIA_INT_BVD1,  EP93XX_PCMCIA_BVD1, "EP93XX PCMCIA BVD1" },
};

#define EP93XX_GPIOS        \
	( EP93XX_PCMCIA_RDY |   \
	  EP93XX_PCMCIA_CD1 |   \
	  EP93XX_PCMCIA_CD2 |   \
	  EP93XX_PCMCIA_BVD2 |  \
	  EP93XX_PCMCIA_BVD1 )



/* This structure maintains housekeeping state for each socket, such
 * as the last known values of the card detect pins, or the Card Services
 * callback value associated with the socket:
 */
struct socket_info_t {

	struct pcmcia_socket	socket;
	struct device		*dev;
	/*the number of the sockets*/
	unsigned int		nr;
	unsigned int		irq;
	struct timer_list		poll_timer;
	struct list_head		node;
	unsigned int		irq_processing;
	/*=======================================*/
	socket_state_t      		state;
	struct pccard_mem_map 	mem_map[EP93XX_MAX_MEM_WIN];
	struct pccard_io_map  	io_map[EP93XX_MAX_IO_WIN];

	void                		*virt_io;
	unsigned short      		speed_io, speed_attr, speed_mem;
	unsigned int        		uiStatus;
	unsigned long       		IrqsToEnable;
	u_char              		Vcc;
	
};

static struct socket_info_t socket_info[EP93XX_MAX_SOCK];

/*-------------------------------------------------------------------------------------------*/

static irqreturn_t ep93xx_interrupt(int irq, void *dev, struct pt_regs *regs);
static void ep93xx_interrupt_timer(int irq, void *dev, struct pt_regs *regs);
static void ep93xx_pcmcia_timer(void *data);
static int ep93xx_get_status( struct pcmcia_socket *sock, unsigned int  *uiStatus );


struct bittbl {
	unsigned int mask;
	const char *name;
};


/*
 * Implements the /sys/class/pcmcia_socket/??/status file.
 *
 * Returns: the number of characters added to the buffer
 */
static ssize_t show_status(struct class_device *class_dev, char *buf)
{
	return 0;

}
static CLASS_DEVICE_ATTR(status, S_IRUGO, show_status, NULL);


/*-------------------------------------------------------------------------------------------*/
static struct timer_list pcmcia_poll_timer;
#define EP9315_PCMCIA_POLL_PERIOD    (2*HZ)

static void ep93xx_pcmcia_timer(void *data)
{
       	struct socket_info_t * skt = &socket_info[0];
        	DEBUG(3, "poll event\n");
                                                                                                                             
        	mod_timer(&pcmcia_poll_timer, jiffies + EP9315_PCMCIA_POLL_PERIOD);

	if(skt->irq_processing==0)
        		ep93xx_interrupt_timer( 0, NULL, NULL );
	
}

/*----------------------------------------------------------------------------------------------*/

/*
 * We bit-bang the pcmcia power controller using this function.
 */
static void ep93xx_bitbang
( 
	unsigned long ulNewEEValue 
)
{
	unsigned long ulGdata;

	ulGdata = inl( GPIO_PGDR );

	ulGdata &= ~(GPIOA_EECLK | GPIOA_EEDAT | GPIOA_SLA0);

	ulNewEEValue &= (GPIOA_EECLK | GPIOA_EEDAT | GPIOA_SLA0);

	ulGdata |= ulNewEEValue;

	outl( ulGdata, GPIO_PGDR );
	ulGdata = inl( GPIO_PGDR ); // read to push write out wrapper
	
	// Voltage controller's data sheet says minimum pulse width is 
	// one microsecond.
	udelay(5);
}

static int
ep93xx_set_voltage( u_short sock, u_char NewVcc )
{
	struct socket_info_t * skt = &socket_info[sock];
	unsigned long ulSwitchSettings, ulDataBit, ulGdirection;
	int       i;
	
	if (sock >= EP93XX_MAX_SOCK){
		DEBUG(3, "ep93xx_set_voltage sock %x vcc %d\n",sock,NewVcc);
		return -EINVAL;}

	if( skt->Vcc == NewVcc ){
		DEBUG(3, "Power already set to %d\n", NewVcc );
		return 0;
	}
	
	ulSwitchSettings = EE_ADDRESS | ENABLE;
	switch( NewVcc ) 
	{
		case 0:
			DEBUG(3, "Configure the socket for 0 Volts\n");
			ulSwitchSettings |= AVCC_0V;
			break;

		case 50:
			ulSwitchSettings |= AVCC_5V;
			DEBUG(3, "Configure the socket for 5 Volts\n");
			break;
			
		case 33:
			DEBUG(3, "Configure the socket for 3.3 Volts\n");
			ulSwitchSettings |= AVCC_33V;
			break;

		default:
			printk(KERN_ERR "%s(): unrecognized Vcc %u\n", __FUNCTION__,
				NewVcc);
			return -1;
	}

	//
	// Configure the proper GPIO pins as outputs.
	//
	ep93xx_bitbang( GPIOA_EECLK | GPIOA_EEDAT );
	
	//
	// Read modify write the data direction register, set the
	// proper lines to be outputs.
	//
	ulGdirection = inl( GPIO_PGDDR );
	ulGdirection |= GPIOA_EECLK | GPIOA_EEDAT | GPIOA_SLA0;
	outl( ulGdirection, GPIO_PGDDR );
	ulGdirection = inl( GPIO_PGDDR ); // read to push write out wrapper
	
	//
	// Clear all except EECLK
	// Lower the clock.
	// 
	ep93xx_bitbang( GPIOA_EECLK );
	ep93xx_bitbang( 0 );

	//
	// Serial shift the command word out to the voltage controller.
	//
	for( i=18 ; i>=0 ; --i )
	{
		if( (ulSwitchSettings >> i) & 0x1 )
			ulDataBit = GPIOA_EEDAT;
		else
			ulDataBit = 0;
		
		//
		// Put the data on the bus and lower the clock.
		// Raise the clock to latch the data in.
		// Lower the clock again.
		//
		ep93xx_bitbang( ulDataBit );
		ep93xx_bitbang( ulDataBit | GPIOA_EECLK );
		ep93xx_bitbang( ulDataBit );
	}
		
	//
	// Raise and lower the Latch.
	// Raise EECLK, delay, raise EEDAT, leave them that way.
	//
	ep93xx_bitbang( GPIOA_SLA0 );
	ep93xx_bitbang( 0 );
	ep93xx_bitbang( GPIOA_EECLK );
	ep93xx_bitbang( GPIOA_EECLK | GPIOA_EEDAT );

	skt->Vcc = NewVcc;
	

	return 0;
}

/*---------------------------------------------------------------------------------------------*/
#define PCMCIA_BOARD_DELAY          40


//****************************************************************************
// CalculatePcmciaTimings
//****************************************************************************
// Calculate the pcmcia timings based on the register settings. 
// For example here is for Attribute/Memory Read.
//
//
// Address:   _______XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX___________
//
// Data:      __________________________XXXXXXXXXXXXXX________________________
//
// CE#:       -------______________________________________________-----------
//
// REG#:      -------______________________________________________-----------
//
// OE#:       --------------------___________________-------------------------
//
//
//                   |<---------------Cycle time ----------------->|
//
//                   |< Address >|<-- Access Time -->|< Hold Time >|
//                      Time           ta(CE)             th(CE)
//                      tsu(A)
//
//  See PCMCIA Electrical Specification Section 4.7 for the timing numbers.
//
//
static unsigned long ep93xx_calculate_timing( unsigned long ulNsSpeed )
{
	unsigned long ulAddressTime, ulHoldTime, ulAccessTime, ulHPeriod, ulSMC;
	unsigned long ulHAccessTime, ulHAddressTime, ulHHoldTime, ulHCLK = 50000000;
	
	switch( ulNsSpeed )
	{
		case 600:
		default:
			ulAccessTime = 600; ulAddressTime = 100; ulHoldTime = 35;
			break;
			
		case 300:
			ulAccessTime = 300; ulAddressTime = 30;	 ulHoldTime = 20;
			break;
			
		case 250:
			ulAccessTime = 250; ulAddressTime = 30;	 ulHoldTime = 20;
			break;
			
		case 200:
			ulAccessTime = 200; ulAddressTime = 20; ulHoldTime = 20;
			break;
			
		case 150:
			ulAccessTime = 150; ulAddressTime = 20;	ulHoldTime = 20;
			break;
			
		case 100:
			ulAccessTime = 100;	ulAddressTime = 10;	ulHoldTime = 15;
			break;
			
		// Special case for I/O all access.
		case 0:
			ulAccessTime = 165; ulAddressTime = 70; ulHoldTime = 20;
			break;
	}

	//
	// Add in a board delay.
	//
	ulAccessTime    += PCMCIA_BOARD_DELAY;
	ulAddressTime   += PCMCIA_BOARD_DELAY;
	ulHoldTime      += PCMCIA_BOARD_DELAY;
	
	//
	// This gives us the period in nanosecods.
	//
	// = 1000000000 (ns/s) / HCLK (cycle/s)
	//
	// = (ns/cycle)
	//
	ulHPeriod       = (1000000000/ ulHCLK);
	
	//
	// Find the number of hclk cycles for cycle time, address time and
	// hold time.
	//
	// = ulAccessTime  (ns) / ulHPeriod (ns/Cycles)
	// = ulAddressTime (ns) / ulHPeriod (ns/Cycles)
	// = ulHoldTime    (ns) / ulHPeriod (ns/Cycles)
	//
	ulHAccessTime    = ulAccessTime / ulHPeriod;
	if(ulHAccessTime > 0xFF)
		ulHAccessTime  = 0xFF;
	
	ulHAddressTime  = ulAddressTime / ulHPeriod;
	if(ulHAddressTime > 0xFF)
		ulHAddressTime = 0xFF;
			
	ulHHoldTime     = (ulHoldTime /ulHPeriod) + 1;
	if(ulHHoldTime >0xF)
		ulHHoldTime     = 0xF;

	ulSMC = (PCCONFIG_ADDRESSTIME_MASK & (ulHAddressTime << PCCONFIG_ADDRESSTIME_SHIFT)) |
			(PCCONFIG_HOLDTIME_MASK & (ulHHoldTime << PCCONFIG_HOLDTIME_SHIFT)) |
			(PCCONFIG_ACCESSTIME_MASK & (ulHAccessTime << PCCONFIG_ACCESSTIME_SHIFT)) ;

	
	return ulSMC;    
}

/*-------------------------------------------------------------------------------------------*/
static spinlock_t status_lock = SPIN_LOCK_UNLOCKED;

static irqreturn_t ep93xx_interrupt(int irq, void *dev, struct pt_regs *regs)
{
	unsigned long ulTemp;
	unsigned int uiStatus, uiNewEvents;
	
	// Assuming we have only one socket.
	struct socket_info_t * skt = &socket_info[0];
  	unsigned long flags;

	skt->irq_processing	=	1;
	spin_lock_irqsave(&status_lock, flags);

	ep93xx_get_status(&(skt->socket), &uiStatus );
	
	//
	// We're going to report only the events that have changed and that
	// are not masked off.
	//
	
	uiNewEvents = (uiStatus ^ skt->uiStatus) & skt->state.csc_mask;
	skt->uiStatus = uiStatus;
	
	if (uiNewEvents){
		pcmcia_parse_events(&skt->socket, uiNewEvents);
	}
	
	// Clear whatever interrupt we're servicing.
	switch( irq )
	{
		case EP93XX_PCMCIA_INT_CD1:
			outl( EP93XX_PCMCIA_CD1, GPIO_FEOI );
			break;

		case EP93XX_PCMCIA_INT_CD2:
			outl( EP93XX_PCMCIA_CD2, GPIO_FEOI );
			break;

		case EP93XX_PCMCIA_INT_BVD1:
			outl( EP93XX_PCMCIA_BVD1, GPIO_FEOI );
			break;

		case EP93XX_PCMCIA_INT_BVD2:
			outl( EP93XX_PCMCIA_BVD2, GPIO_FEOI );
			break;

		case EP93XX_PCMCIA_INT_RDY:
			outl( EP93XX_PCMCIA_RDY, GPIO_FEOI );
			break;

		default:
			break;
	}
	ulTemp = inl( GPIO_FEOI ); // read to push write out wrapper


	spin_unlock_irqrestore(&status_lock, flags);
	skt->irq_processing	=	0;

	return IRQ_HANDLED;
}

static void ep93xx_interrupt_timer(int irq, void *dev, struct pt_regs *regs)
{
	unsigned long ulTemp;
	unsigned int uiStatus, uiNewEvents;
	
	// Assuming we have only one socket.
	struct socket_info_t * skt = &socket_info[0];
  	unsigned long flags;

	skt->irq_processing	=	1;
	spin_lock_irqsave(&status_lock, flags);

	ep93xx_get_status((struct pcmcia_socket *) &(skt->socket), &uiStatus );
	
	//
	// We're going to report only the events that have changed and that
	// are not masked off.
	//
	
	uiNewEvents = (uiStatus ^ skt->uiStatus) & skt->state.csc_mask;
	skt->uiStatus = uiStatus;
	
	if (uiNewEvents){
		pcmcia_parse_events(&skt->socket, uiNewEvents);
	}
	
	// Clear whatever interrupt we're servicing.
	switch( irq )
	{
		case EP93XX_PCMCIA_INT_CD1:
			outl( EP93XX_PCMCIA_CD1, GPIO_FEOI );
			break;

		case EP93XX_PCMCIA_INT_CD2:
			outl( EP93XX_PCMCIA_CD2, GPIO_FEOI );
			break;

		case EP93XX_PCMCIA_INT_BVD1:
			outl( EP93XX_PCMCIA_BVD1, GPIO_FEOI );
			break;

		case EP93XX_PCMCIA_INT_BVD2:
			outl( EP93XX_PCMCIA_BVD2, GPIO_FEOI );
			break;

		case EP93XX_PCMCIA_INT_RDY:
			outl( EP93XX_PCMCIA_RDY, GPIO_FEOI );
			break;

		default:
			break;
	}
	ulTemp = inl( GPIO_FEOI ); // read to push write out wrapper


	spin_unlock_irqrestore(&status_lock, flags);
	skt->irq_processing	=	0;
	
}


/*-------------------------------------------------------------------------------------------*/
static int ep93xx_get_status( struct pcmcia_socket *sock, unsigned int  *uiStatus )
{
	struct socket_info_t * skt = &socket_info[sock->sock];
	
	unsigned long ulPFDR;
	

	if (sock->sock >= EP93XX_MAX_SOCK)
		return -EINVAL;

	//
	// Read the GPIOs that are connected to the PCMCIA slot.
	//
	ulPFDR  = inl(GPIO_PFDR);
	
	*uiStatus = 0;
	
	//
	// If both CD1 and CD2 are low, set SS_DETECT bit.
	//
	*uiStatus = ((ulPFDR & (EP93XX_PCMCIA_CD1|EP93XX_PCMCIA_CD2)) == 0) ?
				SS_DETECT : 0;

	*uiStatus |= (ulPFDR & EP93XX_PCMCIA_WP) ? SS_WRPROT : 0;

	if (skt->state.flags & SS_IOCARD)
	{
		*uiStatus |= (ulPFDR & EP93XX_PCMCIA_BVD1) ? SS_STSCHG : 0;
	}
	else 
	{
		*uiStatus |= (ulPFDR & EP93XX_PCMCIA_RDY) ? SS_READY : 0;
		*uiStatus |= (ulPFDR & EP93XX_PCMCIA_BVD1) ? 0 : SS_BATDEAD;
		*uiStatus |= (ulPFDR & EP93XX_PCMCIA_BVD2) ? 0 : SS_BATWARN;
	}
	
	*uiStatus |= skt->state.Vcc ? SS_POWERON : 0;
	
	*uiStatus |= (ulPFDR & EP93XX_PCMCIA_VS1) ? 0 : SS_3VCARD;
	

	return 0;
}


/* ------------------------------------------------------------------------- */

static int
ep93xx_set_socket( struct pcmcia_socket *sock, socket_state_t *state )
{
	struct socket_info_t * skt = &socket_info[sock->sock];
	
	unsigned long ulTemp, IrqsToEnable = 0;


	if (sock->sock >= EP93XX_MAX_SOCK)
		return -EINVAL;


	//
	// Set Vcc level.  If an illegal voltage is specified, bail w/ error.
	//
	if( ep93xx_set_voltage( sock->sock, state->Vcc ) < 0 )
		return -EINVAL;

	//
	// Enable PCMCIA, Enable Wait States, Set or Clear card reset.
	//
	ulTemp = (inl( SMC_PCMCIACtrl ) | PCCONT_WEN | PCCONT_PC1EN) & ~PCCONT_PC1RST;
	if( state->flags & SS_RESET )
		ulTemp |= PCCONT_PC1RST;

	outl( ulTemp, SMC_PCMCIACtrl );
	ulTemp = inl( SMC_PCMCIACtrl );
	
	//
	// Enable interrupts in hw that are specified in csc_mask.
	//
	if (state->csc_mask & SS_DETECT)
		IrqsToEnable |= EP93XX_PCMCIA_CD1 | EP93XX_PCMCIA_CD2;

	if( state->flags & SS_IOCARD ) 
	{
		if (state->csc_mask & SS_STSCHG)  IrqsToEnable |= EP93XX_PCMCIA_BVD1;
	} 
	else 
	{
		if (state->csc_mask & SS_BATDEAD) IrqsToEnable |= EP93XX_PCMCIA_BVD1;
		if (state->csc_mask & SS_BATWARN) IrqsToEnable |= EP93XX_PCMCIA_BVD2;
		if (state->csc_mask & SS_READY)   IrqsToEnable |= EP93XX_PCMCIA_RDY;
	}
	
	skt->IrqsToEnable = IrqsToEnable;

	ulTemp = inl( GPIO_FINTEN ) & ~EP93XX_GPIOS;
	outl( ulTemp, GPIO_FINTEN );
	ulTemp = inl( GPIO_FINTEN );

	//
	// Clear and enable the new interrupts.
	//
	outl( IrqsToEnable | EP93XX_PCMCIA_RDY, GPIO_FEOI );
	ulTemp = inl( GPIO_FEOI );

	ulTemp = inl( GPIO_FINTEN ) & ~EP93XX_GPIOS;
	ulTemp |= EP93XX_PCMCIA_RDY | IrqsToEnable;
	outl( ulTemp, GPIO_FINTEN );
	ulTemp = inl( GPIO_FINTEN );

	skt->state = *state;
	
	return 0;
}


static int
ep93xx_set_io_map(struct pcmcia_socket *sock, struct pccard_io_map *io_map)
{
	struct socket_info_t * skt = &socket_info[sock->sock];
	
	unsigned long ulSMC_PCIO;
	unsigned int speed=0, i;

	if (sock->sock >= EP93XX_MAX_SOCK)
		return -EINVAL;


	if ((io_map->map >= EP93XX_MAX_IO_WIN) || (io_map->stop < io_map->start)) 
		return -EINVAL;

	if (io_map->flags & MAP_ACTIVE) 
	{
		if (io_map->speed == 0)
			io_map->speed = EP93XX_PCMCIA_IO_ACCESS;

		speed = io_map->speed;

		// Go thru our array of mappings, find the lowest speed 
		// (largest # of nSec) for an active mapping...
		for( i = 0 ; i < EP93XX_MAX_IO_WIN ; i++ ) {
			if ( (skt->io_map[i].flags & MAP_ACTIVE) && 
				 (speed > skt->io_map[i].speed) )
				speed = skt->io_map[i].speed;
		}

		ulSMC_PCIO = ep93xx_calculate_timing( speed );

		if( io_map->flags & MAP_16BIT )
			ulSMC_PCIO |= PCCONFIG_MW_16BIT;

		outl( ulSMC_PCIO, SMC_PCIO ); 
		ulSMC_PCIO = inl( SMC_PCIO );

		skt->speed_io = speed;
	}

	if (io_map->stop == 1)
		io_map->stop = PAGE_SIZE-1;


	io_map->stop = (io_map->stop - io_map->start) + (unsigned int)skt->virt_io;
	io_map->start = (unsigned int)skt->virt_io;

	skt->io_map[io_map->map] = *io_map;


	return 0;
}



static int
ep93xx_set_mem_map(struct pcmcia_socket *sock, struct pccard_mem_map *map)
{
	struct socket_info_t * skt = &socket_info[sock->sock];

	
	unsigned long ulRegValue;
	unsigned int speed=0, i;


	if (sock->sock >= EP93XX_MAX_SOCK)
		return -EINVAL;


	if (map->map >= EP93XX_MAX_MEM_WIN){
		printk(KERN_ERR "%s(): map (%d) out of range\n", __FUNCTION__,
			map->map);
		return -1;
	}

	if (map->flags & MAP_ACTIVE){
		if (map->speed == 0){
			if (skt->state.Vcc == 33)
				map->speed = EP93XX_PCMCIA_3V_MEM_ACCESS;
			else
				map->speed = EP93XX_PCMCIA_5V_MEM_ACCESS;
		}
		
		speed = map->speed;

		if (map->flags & MAP_ATTRIB) 
		{
			// Go thru our array of mappings, find the lowest speed 
			// (largest # of nSec) for an active mapping...
			for( i = 0 ; i < EP93XX_MAX_MEM_WIN ; i++ ) {
				if( (skt->mem_map[i].flags & MAP_ACTIVE) && 
					(speed > skt->mem_map[i].speed) )
					speed = skt->mem_map[i].speed;
			}

			ulRegValue = ep93xx_calculate_timing( speed );

			if( map->flags & MAP_16BIT )
				ulRegValue |= PCCONFIG_MW_16BIT;

			outl( ulRegValue, SMC_PCAttribute ); 
			ulRegValue = inl( SMC_PCAttribute );
			
			skt->speed_attr = speed;
		} 
		else 
		{
			for( i = 0 ; i < EP93XX_MAX_MEM_WIN ; i++ )
			{
				if ((skt->mem_map[i].flags & MAP_ACTIVE) && 
					(speed > skt->mem_map[i].speed) )
					speed = skt->mem_map[i].speed;
			}

			ulRegValue = ep93xx_calculate_timing( speed );
			if( map->flags & MAP_16BIT )
				ulRegValue |= PCCONFIG_MW_16BIT;

			outl( ulRegValue, SMC_PCCommon ); 
			ulRegValue = inl( SMC_PCCommon );
			
			skt->speed_mem = speed;
		}
	}

	map->static_start = (map->flags & MAP_ATTRIB) ? EP93XX_PHYS_ADDR_PCMCIAATTR : EP93XX_PHYS_ADDR_PCMCIAMEM;

	skt->mem_map[map->map] = *map;

	return 0;
}


static void init_socket(struct pcmcia_socket *s)
{
	int i;
    	pccard_io_map io = { 0, 0, 0, 0, 1 };
    	pccard_mem_map mem = { 0, 0, 0, 0, 0, 0 };

    	mem.static_start = 0;
    	s->socket = dead_socket;
    	s->ops->set_socket(s,&s->socket);
    	for (i = 0; i < 2; i++) {
		io.map = i;
		s->ops->set_io_map(s,&io);
		s->io[i].InUse=0;
		s->io[i].Config=0;
    	}
    	for (i = 0; i < 5; i++) {
		mem.map = i;
		s->ops->set_mem_map(s,&mem);
    	}
}


static int ep93xx_pcmcia_sock_init(struct pcmcia_socket *sock)
{
	DEBUG(3, "ep93xx_pcmcia_sock_init  %x- enter\n",sock->sock);

	init_socket(sock);
	
	DEBUG(3, "ep93xx_pcmcia_sock_init - exit\n");

	return 0;
}


static int ep93xx_pcmcia_sock_suspend(struct pcmcia_socket *sock)
{
	
	DEBUG(3, "ep93xx_pcmcia_sock_suspendt - enter\n");

	return 0;
}


static struct pccard_operations ep93xx_pcmcia_operations = {
	.init			= ep93xx_pcmcia_sock_init,
	.suspend		= ep93xx_pcmcia_sock_suspend,
	.get_status		= ep93xx_get_status,
	.set_socket		= ep93xx_set_socket,
	.set_io_map		= ep93xx_set_io_map,
	.set_mem_map		= ep93xx_set_mem_map,
};
/*------------------------------------------------------------------------------------------*/
static int ep93xx_shutdown(void)
{
	unsigned long ulTemp;
	int i;

	DEBUG(3, "ep93xx_shutdown - enter\n");

	del_timer_sync(&pcmcia_poll_timer);

	//
	// Disable, clear pcmcia irqs in hw.
	//
	ulTemp = inl( GPIO_FINTEN );
	outl( ulTemp & ~EP93XX_GPIOS, GPIO_FINTEN );
	ulTemp = inl( GPIO_FINTEN );

	outl( EP93XX_GPIOS, GPIO_FEOI );
	ulTemp = inl( GPIO_FEOI );

	//
	// Set reset.
	//
	outl( (PCCONT_WEN | PCCONT_PC1RST | PCCONT_PC1EN), SMC_PCMCIACtrl );
	ulTemp = inl( SMC_PCMCIACtrl );

	//
	// Release memory
	// Free the pcmcia interrupts.
	// Set socket voltage to zero
	//
	iounmap(socket_info[0].virt_io);
	socket_info[0].Vcc = EP93XX_UNCONFIGURED_VOLTAGE;
	ep93xx_set_voltage( 0, 0 );

	for( i = 0; i < ARRAY_SIZE(ep93xx_pcmcia_irqs) ; i++ ) 
		free_irq( ep93xx_pcmcia_irqs[i].irq, socket_info );

	return 0;
}


/*-------------------------------------------------------------------------------------------*/



int ep93xx_drv_pcmcia_probe(struct device *dev)
{
	unsigned long ulRegValue;
	unsigned long ulTemp, flags;
	int ret=0, i;
	

	DEBUG(3, "ep93xx_init - enter\n");

	if (!dev ){
		DEBUG(3, "no device - error\n");
		return -ENODEV;

	}

	
	/*
	 * Initialise the per-socket structure.
	 */
	for (i = 0; i < EP93XX_MAX_SOCK; i++) {
		struct socket_info_t * skt = &socket_info[i];

		skt->socket.ops 	= &ep93xx_pcmcia_operations;
		skt->socket.owner 	= THIS_MODULE;
		skt->socket.dev.dev 	= dev;

		
		/*init the poll timer*/
		init_timer(&pcmcia_poll_timer);
	  	pcmcia_poll_timer.function = 	(void *)ep93xx_pcmcia_timer;
  		pcmcia_poll_timer.expires = 	jiffies + EP9315_PCMCIA_POLL_PERIOD;
		pcmcia_poll_timer.data =	(unsigned long)socket_info;

		skt->nr			=  i;
		skt->irq		= NO_IRQ;
		skt->dev		= dev;

		
		for( i = 0 ; i < EP93XX_MAX_IO_WIN ; i++ ){
		skt->io_map[i].flags = 0;
		skt->io_map[i].speed = EP93XX_PCMCIA_IO_ACCESS;
		}		


		for( i = 0 ; i < EP93XX_MAX_MEM_WIN ; i++ ){
		skt->mem_map[i].flags = 0;
		skt->mem_map[i].speed = EP93XX_PCMCIA_3V_MEM_ACCESS;
		}


		/*===========================================*/	
		skt->speed_io   = EP93XX_PCMCIA_IO_ACCESS;
		skt->speed_attr = EP93XX_PCMCIA_5V_MEM_ACCESS;
		skt->speed_mem  = EP93XX_PCMCIA_5V_MEM_ACCESS;

		skt->uiStatus = 0;
		skt->IrqsToEnable = 0;
		
		skt->Vcc = EP93XX_UNCONFIGURED_VOLTAGE;
		ep93xx_set_voltage(0,0);
		/*===========================================*/

		
		/*virt memory remap*/
		skt->virt_io    = ioremap( EP93XX_PHYS_ADDR_PCMCIAIO, 0x10000 );
		DEBUG(3, "virt_io is 0x%08x\n", (unsigned int)skt->virt_io );
		if (skt->virt_io == NULL) {
			ep93xx_shutdown();
			return -ENOMEM;
		}

		//
		// Disable interrupts in hw for pcmcia.
		//
		ulTemp = inl( GPIO_FINTEN );
		outl( (ulTemp & ~EP93XX_GPIOS), GPIO_FINTEN );
		ulTemp = inl( GPIO_FINTEN );
		
		//
		// Set data direction to input for pcmcia socket lines.
		//
		ulTemp = inl( GPIO_PFDDR );
		outl( (ulTemp & ~EP93XX_GPIOS), GPIO_PFDDR );
		ulTemp = inl( GPIO_PFDDR );

		//
		// Enable debounce for the card detect lines
		// Set interrupts to be edge sensitive, falling edge triggered.
		//
		ulTemp = inl( GPIO_FDB ) & ~EP93XX_GPIOS;
		outl( (EP93XX_PCMCIA_CD1 | EP93XX_PCMCIA_CD2) | ulTemp, GPIO_FDB );

		ulTemp = inl( GPIO_FINTTYPE1 ) & ~EP93XX_GPIOS;
		outl(  EP93XX_GPIOS | ulTemp, GPIO_FINTTYPE1 );

		ulTemp = inl( GPIO_FINTTYPE2 ) & ~EP93XX_GPIOS;
		outl( ulTemp, GPIO_FINTTYPE2 );
		ulTemp = inl( GPIO_FINTTYPE2 );

		//
		// Clear all interrupts for GPIO port F.
		//
		outl( EP93XX_GPIOS, GPIO_FEOI );
		ulTemp = inl( GPIO_FEOI );

		//
		// Register ISR.  EP93XX_PCMCIA_INT_RDY is a shared interrupt as
		// the kernel IDE stack has its own interrupt handler that it
		// will register for it.
		//
		for( i = 0; i < ARRAY_SIZE(ep93xx_pcmcia_irqs) ; i++ )
		{
			if ( ep93xx_pcmcia_irqs[i].irq == EP93XX_PCMCIA_INT_RDY )
				flags = SA_INTERRUPT | SA_SHIRQ;
			else
				flags = SA_INTERRUPT;
			
			if( request_irq( ep93xx_pcmcia_irqs[i].irq, ep93xx_interrupt, 
					flags, ep93xx_pcmcia_irqs[i].str, socket_info ) ) 
			{
				printk( KERN_ERR "%s: request for IRQ%d failed\n",
					   __FUNCTION__, ep93xx_pcmcia_irqs[i].irq );

				while (i--)
					free_irq( ep93xx_pcmcia_irqs[i].irq, socket_info );
				
				return -EINVAL;
			}
		}

		DEBUG(3, "ep93xx_init GPIO_F: INTEN=0x%02x  DDR=0x%02x DB=0x%02x INTTYP1=0x%02x INTTYP2=0x%02x\n",
			 inl( GPIO_FINTEN ), inl( GPIO_PFDDR ),	inl( GPIO_FDB ),
			 inl( GPIO_FINTTYPE1 ), inl( GPIO_FINTTYPE2 ) );

	
		/*================================================*/
		/*ops->set_timing(skt);*/
		
		//
		// Set speed to the defaults
		//
		ulRegValue = ep93xx_calculate_timing( skt->speed_io );
		outl( ulRegValue, SMC_PCIO );
		ulRegValue = ep93xx_calculate_timing( skt->speed_attr );
		outl( ulRegValue, SMC_PCAttribute ); 
		ulRegValue = ep93xx_calculate_timing( skt->speed_mem );
		outl( ulRegValue, SMC_PCCommon ); 
		ulRegValue = inl( SMC_PCCommon ); // Push the out thru the wrapper

		DEBUG(3, "INITIALIZING SMC: Attr:%08x  Comm:%08x  IO:%08x  Ctrl:%08x\n",
			inl( SMC_PCAttribute ), inl( SMC_PCCommon), 
			inl( SMC_PCIO ), inl( SMC_PCMCIACtrl ) );
		
		
		/*initialize the  socket info*/
		skt->socket.features=(SS_CAP_PAGE_REGS  | SS_CAP_VIRTUAL_BUS | SS_CAP_MEM_ALIGN |
		 			SS_CAP_STATIC_MAP | SS_CAP_PCCARD);
		skt->socket.irq_mask  = 0;
		skt->socket.map_size  = PAGE_SIZE;
		skt->socket.pci_irq   = EP93XX_PCMCIA_INT_RDY;

		skt->socket.sock   = i;
		skt->socket.io_offset = (unsigned long)skt->virt_io;
		skt->socket.resource_ops = &pccard_static_ops; 

		
		ret = pcmcia_register_socket(&skt->socket);
		if (ret){
			/*goto out_err_7;*/
			printk(KERN_ERR "Unable to register sockets\n");
			ep93xx_shutdown();
			return ret;

		}

		//
		// Clear and enable the RDY interrupt in hardware.  We leave this 
		// interrupt enabled because the kernel IDE stack will register its own
		// interrupt handler and not go thru this module to do it.  The
		// other interrupts are enabled in set_socket.
		//
		outl( EP93XX_PCMCIA_RDY, GPIO_FEOI );
		ulTemp = inl( GPIO_FEOI );
		
		ulTemp = inl( GPIO_FINTEN ) & ~EP93XX_GPIOS;
		outl( ulTemp | EP93XX_PCMCIA_RDY, GPIO_FINTEN );
		ulTemp = inl( GPIO_FINTEN );

		/*
		 * Start the event poll timer.  It will reschedule by itself afterwards.
		 */
		add_timer(&pcmcia_poll_timer);
		DEBUG(3, "ep93xx_init - poll evevt \n");
		
		class_device_create_file(&skt->socket.dev, &class_device_attr_status);
		DEBUG(3, "ep93xx_init - creat file \n");
	}

	dev_set_drvdata(dev, socket_info);

	DEBUG(3, "ep93xx_init - exit %x\n",ret);
	return ret;
}


int ep93xx_drv_pcmcia_remove(struct device *dev)
{
	int i;
	DEBUG(3, "ep93xx_remove - enter \n");
	for (i = 0; i < EP93XX_MAX_SOCK; i++) {
		/*struct soc_pcmcia_socket *skt = &sinfo->skt[i];*/
		struct socket_info_t * skt = &socket_info[i];

		pcmcia_unregister_socket(&skt->socket);

	}
	ep93xx_shutdown();
	DEBUG(3, "ep93xx_remove - exit \n");
	return 0;
}


static struct platform_driver ep93xx_pcmcia_platform_driver = {
	.driver		= {
        			.probe          = ep93xx_drv_pcmcia_probe,
			        .remove         = ep93xx_drv_pcmcia_remove,
			        .suspend        = pcmcia_socket_dev_suspend,
			        .resume         = pcmcia_socket_dev_resume,
			        .name           = "ep93xx-pcmcia",
				.owner		= THIS_MODULE,
	},
};

static int __init ep93xx_pcmcia_init(void)
{
	return platform_driver_register(&ep93xx_pcmcia_platform_driver);
}

static void __exit ep93xx_pcmcia_exit(void)
{
	platform_driver_unregister(&ep93xx_pcmcia_platform_driver);
}

module_init(ep93xx_pcmcia_init);
module_exit(ep93xx_pcmcia_exit);


MODULE_AUTHOR("Shrek Wu && Bo Liu");
MODULE_DESCRIPTION("Cirrus Logic EP93xx pcmcia socket driver");
MODULE_LICENSE("Dual MPL/GPL");
