/******************************************************************************
 * drivers/char/ep93xx_eeprom.c 
 *
 * Support for SST 25F0x0 serial flash on EDB93XX boards
 *
 * Copyright (C) 2006  Cirrus Logic
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
 *
 ****************************************************************************/

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/delay.h>

#define NAME "ep93xx_eeprom"

static int eeprom_size=0;
static int sector_size=0;

MODULE_AUTHOR("Cirrus Logic (c) 2006");
MODULE_DESCRIPTION("CL EP93XX SST Serial Flash Driver");
MODULE_LICENSE("GPL");

static int major = 0;		/* default to dynamic major */
//MODULE_PARM(major, "i");
module_param(major, int, 0);
MODULE_PARM_DESC(major, "Major device number");


/*SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI SPI */


static void
delay(long t)
{
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(t);
}


static void spi_empty_fifos(void)
{
    u32 tmp;
	/*
	 * Let TX fifo clear out.  Poll the Transmit Fifo Empty bit.
	 */
	while( !( inl(SSPSR) & SSPSR_TFE ) );

	while( inl(SSPSR) & SSPSR_RNE )
	{
		tmp = inl(SSPDR);
	}
}

#define EEPROM_CS	0x80 /* EGPIO7 == PA D7 */
#define AUDIO_CS	0x40 /* EGPIO6 == PA D6 */

static int spi_setup(void)
{
	u32 tmp;
	
	
	tmp = inl(GPIO_PADR);
	tmp |= EEPROM_CS;
	outl(tmp, GPIO_PADR);
	tmp = inl(GPIO_PADDR);
	tmp |= EEPROM_CS;
	outl(tmp, GPIO_PADDR);

	tmp = inl(GPIO_PADR);
	tmp |= AUDIO_CS;
	outl(tmp, GPIO_PADR);
	tmp = inl(GPIO_PADDR);
	tmp |= AUDIO_CS;
	outl(tmp, GPIO_PADDR);
	
		
	/*
	 * Disable the SSP and disable interrupts
	 */
	outl( 0, SSPCR1 );

	/*
	 * Clock will be 14.7 MHz divided by 4.
	 */
	outl( 2, SSPCPSR );

	/*
	 * Motorola format, mode 3, 8 bit.
	 */

	outl( (SSPCR0_FRF_MOTOROLA | SSPCR0_SPO | SSPCR0_SPH | SSPCR0_DSS_8BIT), SSPCR0 );

	/*
	 * Configure the device as master, reenable the device.
	 */
	 //                
	outl( SSPC1_SSE , SSPCR1 );

	/*
	    SPI fifos should be empty at this point 
	    but better make sure they really are empty
	*/
	spi_empty_fifos();

	return 0;
}



/* longest frames possible are 8 bytes, since the fifos are 8 entries deep and
the spi i/f is programmed for 8bit frame sizes */
static void spi_transaction( u8 *txdat, u8 *rxdat, int len)
{
	int idx;
	
	idx=0;
	
	outl( 0 , SSPCR1 );
	while(len>0)
	{
	// send the byte
	    outl( txdat[idx], SSPDR );
	    len--;
	    idx++;
	}
	outl( SSPC1_SSE , SSPCR1 );

	idx=0;
	// wait for tx complete    
	while( !( inl(SSPSR) & SSPSR_TFE ) );
	udelay(30);    
	// wait for rx data    
	while( ( inl(SSPSR) & SSPSR_RNE ) )
	{
	    rxdat[idx]=inl(SSPDR);
	    idx++;
	    udelay(1);
	}
//	printk("spit i=%d, %02x %02x %02x %02x %02x %02x %02x %02x\n", idx, rxdat[0], rxdat[1], rxdat[2], rxdat[3], rxdat[4], rxdat[5], rxdat[6], rxdat[7] );
}


static void eeprom_select(void)
{
    u32 tmp;
	tmp = inl(GPIO_PADR);
	tmp &= ~EEPROM_CS;
	outl(tmp, GPIO_PADR);
}

static void eeprom_deselect(void)
{
    u32 tmp;
	tmp = inl(GPIO_PADR);
	tmp |= EEPROM_CS;
	outl(tmp, GPIO_PADR);
}


// eeprom interface-------------eeprom interface-------------eeprom interface-------------eeprom interface-------------



//static void spi_transaction( u8 *txdat, u8 *rxdat, int len);


static u16 eprom_id(void)
{
    u8 txdat[8];
    u8 rxdat[8];
    
//    txdat[0] = EESPI_RDID;
    txdat[0] = 0x90;
    txdat[1]=0;
    txdat[2]=0;
    txdat[3]=0;
    txdat[4]=0;
    txdat[5]=0;
    
    spi_transaction( txdat, rxdat, 6);

    return ( (rxdat[4] << 8) | rxdat[5] );
}

//return 1 when ready
static int eeprom_ready(void)
{
    u8 txdat[2];
    u8 rxdat[2];
    
    txdat[0] = 0x05;
    txdat[1]=0;

    spi_transaction( txdat, rxdat, 2);
    
    if(rxdat[1] & 0x01)
	return 0;
    
    return 1;
}


static void eeprom_unprotect(void)
{
    u8 txdat[4];
    u8 rxdat[4];
    
    txdat[0] = 0x50;
    spi_transaction( txdat, rxdat, 1);

    txdat[0] = 0x01;
    txdat[1] = 0;
    spi_transaction( txdat, rxdat, 2);
}


static void eeprom_erase(u32 adr)
{
    u8 txdat[4];
    u8 rxdat[4];
    
    if(adr >= eeprom_size)
	return;
    
    eeprom_unprotect();
    
    txdat[0] = 0x06;
    spi_transaction( txdat, rxdat, 1);

    txdat[0] = 0x20;
    txdat[1] = adr >> 16;
    txdat[2] = adr >> 8;
    txdat[3] = adr;
    spi_transaction( txdat, rxdat, 4);

    while( !eeprom_ready() )
    {
	delay(1);
    }
}


/* don't change this */
#define MAXLEN 1
/* program upto MAXLEN bytes */

static int eeprom_program(u32 adr, u8 *udata, int len)
{
    int i;
    u8 txdat[8];
    u8 rxdat[8];


    if(len>MAXLEN)
	len = MAXLEN;
    
    txdat[0] = 0x06;
    spi_transaction( txdat, rxdat, 1);

    txdat[0] = 0x02;
    txdat[1] = (adr >> 16) & 0x0ff;
    txdat[2] = (adr >> 8) & 0x0ff;
    txdat[3] =  adr & 0x0ff;
    
    for(i=0; i<len; i++)
    {
	txdat[4+i] = udata[i];
    }
    
    spi_transaction( txdat, rxdat, 4+len);

    udelay(5);
    while( !eeprom_ready() )
    {
	udelay(1);
    }
    udelay(5);

    return len;
}


/* read upto 4 bytes */

static int eeprom_read(u32 adr, u8 *udata, int len)
{
    int i;
    u8 txdat[8];
    u8 rxdat[8];

    if(len>4)
	len = 4;
    
    txdat[0] = 0x03;
    txdat[1] = (adr >> 16) & 0x0ff;
    txdat[2] = (adr >> 8) & 0x0ff;
    txdat[3] = adr & 0x0ff;
    
    txdat[4] = 0;
    txdat[5] = 0;
    txdat[6] = 0;
    txdat[7] = 0;
    spi_transaction( txdat, rxdat, 4+len);

    for(i=0; i<len; i++)
    {
	udata[i] = rxdat[4+i];
    }
//    printk("erd r: %02x %02x %02x %02x\n", rxdat[4], rxdat[5], rxdat[6], rxdat[7] );
//    printk("erd u: %02x %02x %02x %02x\n", udata[0], udata[1], udata[2], udata[3] );

    return len;
}


static u8 *eeprom_kernel_buffer=NULL;

//----------KERNEL INTERFACE-----------------KERNEL INTERFACE-----------------KERNEL INTERFACE-----------------KERNEL INTERFACE-------

static ssize_t ep93xx_eeprom_write(struct file *file, const char __user *data, 
				 size_t len, loff_t *ppos)
{
    int adr;
    size_t wlen;
    int rv;
    int bdx;
    
    
    if(len > 16384)
	return -ENOBUFS;
    
    adr = (int) *ppos;
    if(adr >= eeprom_size)
	return -ENOSPC;
    if( (adr + len) > eeprom_size )
    {
	len = eeprom_size - adr;
    }

    wlen = len;
    bdx = 0;

    copy_from_user(eeprom_kernel_buffer, data, len);

    while(len > 0)
    {						
	rv=eeprom_program( adr, &eeprom_kernel_buffer[bdx], len );
//	printk("eeprom_program(idx=0x%08x, &eeprom_kernel_buffer[idx], len=%d)\n", idx, rv );
	len-=rv;
	adr+=rv;
	bdx+=rv;
    }

    *ppos+=wlen;

//    printk("ewr wlen=%d\n", wlen );
    return wlen;
}

static ssize_t ep93xx_eeprom_read(struct file *file, char __user *buf,
				size_t len, loff_t *ppos)
{
    int adr;
    int bdx;
    int rv;
    int rlen;
    

    if(len > 16384)
	return -ENOBUFS;

    if( *ppos >= eeprom_size)
	return 0;
		
    if( (*ppos + len) > eeprom_size )
    {
	len = eeprom_size - *ppos;
    }
    
    adr = *ppos;
    bdx = 0;
    rlen = len;
    
    while(len > 0)
    {						
	rv=eeprom_read( adr, &eeprom_kernel_buffer[bdx], len);
//	printk("epr rv=%d\n", rv);
	len-=rv;
	adr+=rv;
	bdx+=rv;
    }	

    copy_to_user(buf, eeprom_kernel_buffer, rlen);

    *ppos += rlen;

    return rlen;
}


static int ep93xx_eeprom_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	unsigned long arg)
{
    int i;
    int block_size;
    
    block_size = eeprom_size / 4;    
    switch(cmd)
    {
	case 1: 
	    for(i=0; i<(block_size); i+=sector_size)
	    {
		eeprom_erase(i);
	    }
	break;

	case 2: 
	    for(i=(block_size); i<(2*block_size); i+=sector_size)
	    {
		eeprom_erase(i);
	    }
	break;

	case 3: 
	    for(i=(2*block_size); i<(3*block_size); i+=sector_size)
	    {
		eeprom_erase(i);
	    }
	break;

	case 4: 
	    for(i=(3*block_size); i<(4*block_size); i+=sector_size)
	    {
		eeprom_erase(i);
	    }
	break;

	default:
	break;
    }


    return 0;
}


static int ep93xx_eeprom_open(struct inode *inode, struct file *file)
{

	eeprom_select();
//	printk("%s\n", __FUNCTION__ );
	return 0;
}

static int ep93xx_eeprom_release(struct inode *inode, struct file *file)
{
	eeprom_deselect();
//	printk("%s\n", __FUNCTION__ );
	return 0;
}

static struct file_operations ep93xx_eeprom_fops = {
	.owner   = THIS_MODULE,
	.write   = ep93xx_eeprom_write,
	.read    = ep93xx_eeprom_read,
	.ioctl   = ep93xx_eeprom_ioctl,
	.open    = ep93xx_eeprom_open,
	.release = ep93xx_eeprom_release,
};

static int __init ep93xx_eeprom_init(void)
{
	int r=0;
	u16 id;
	printk(KERN_DEBUG NAME ": CL EP93XX Serial Flash Driver for SST25F0x0 and compatible devices\n");
	
	eeprom_kernel_buffer = kmalloc( 16384, GFP_KERNEL);
	if(eeprom_kernel_buffer==NULL)
	    return -1;
	
	spi_setup();

	eeprom_select();
	id = eprom_id();
	eeprom_deselect();

	printk("eeprom id = 0x%04x\n", id );
	if( (id & 0xff00) != 0xbf00 )
	{
	    printk("this driver only supports SST25LF0x0 type serial flash memory\n");
	    return -1;
	}

	/* not tested (25LF010)*/
	if((id & 0x0ff) == 0x42)
	{
	    eeprom_size=(128*1024);
	    sector_size=4096;
	}
	
	/* this one has been tested (25LF020)*/
	if((id & 0x0ff) == 0x43)
	{
	    eeprom_size=(256*1024);
	    sector_size=4096;
	}
	
	/* this one has been tested (25LF040)*/
	if((id & 0x0ff) == 0x44)
	{
	    eeprom_size=(512*1024);
	    sector_size=4096;
	}

	/* not tested (25LF080)*/
	if((id & 0x0ff) == 0x45)
	{
	    eeprom_size=(1024*1024);
	    sector_size=4096;
	}
	
	printk("eeprom size = %d kbytes\n", eeprom_size / 1024 );
	
	r = register_chrdev(major, NAME, &ep93xx_eeprom_fops);
	if (r < 0) {
		printk(KERN_ERR NAME ": unable to register character device\n");
		return r;
	}
	if (!major) {
		major = r;
		printk(KERN_DEBUG NAME ": got dynamic major %d\n", major);
	}
	printk("CL EP93XX Serial Flash Driver registered at major %d\n", major);
	return 0;
}

static void __exit ep93xx_eeprom_cleanup(void)
{
    if(major)
	unregister_chrdev(major, NAME);
    kfree(eeprom_kernel_buffer);
}

module_init(ep93xx_eeprom_init);
module_exit(ep93xx_eeprom_cleanup);

