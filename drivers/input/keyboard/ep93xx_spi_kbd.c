/******************************************************************************
 * 
 *  File:	linux/drivers/char/ep93xx_spi_kbd.c
 *
 *  Purpose:	Support for SPI Keyboard for a Cirrus Logic EP93xx
 *
 *  History:	
 *
 *  Limitations:
 *  Break and Print Screen keys not handled yet!
 *
 *
 *  Copyright 2003 Cirrus Logic Inc.
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
 ******************************************************************************/

/*
 * FIXME: There are more parity problems than there ought to be.
 * TODO: Track down
 * WHERE: grep for "BAD_PARITY" 
 * WORKAROUND: Do not press too many keys and do not type too fast
 *             type key another time if it got lost 
 *
 * FIXME2: Keymap should be done properly
 */
#include <linux/init.h>
#include <linux/input.h>
#include <linux/module.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/arch/ssp.h>

#include "ep93xx_spi_kbd.h"

#define EP93XX_MAX_KEY_DOWN_COUNT 6

void DataCallback(unsigned int Data);
static int g_SSP_Handle;
static struct input_dev * ep93xxkbd_dev;
static char *name = "Cirrus EP93xx PS/2 keyboard driver";

//-----------------------------------------------------------------------------
//  Debug stuff...
//-----------------------------------------------------------------------------

#undef UART_HACK_DEBUG
//#define UART_HACK_DEBUG 1

#if defined(UART_HACK_DEBUG) && defined(CONFIG_DEBUG_LL)
char szBuf[256];
#define DPRINTK( x... )   \
	sprintf( szBuf, ##x ); \
	printascii( szBuf );
#else
#define DPRINTK( x... )
#endif

typedef struct {
	unsigned char scancode;
	unsigned char count;
} key_down_tracker_t;

//
// In the interest of efficiency, let's only allow 5 keys to be down
// at a time, maximum.  So if anybody is having a temper tantrum on
// their keyboard, they may get stuck keys, but that's to be expected.
//

#define MAX_KEYS_DOWN 8
static key_down_tracker_t KeyTracker[MAX_KEYS_DOWN];

static unsigned char SPI2KScan(unsigned int uiSPIValue, int *pValid);
static void InitSniffer(void);
static void KeySniffer(unsigned char scancode, int down);
static void Check4StuckKeys(void);


/* 
 * wrapper: old style (2.4.x) to 2.6.x input subsystem 
 */
static void handle_scancode(unsigned char scancode, int down)
{
	DPRINTK("handle_scancode(scancode=%04X, down=%d); %02X ", scancode,
		down, scancode & 0x7f);

	if( scancode > KSCAN_TABLE_SIZE)
		scancode &= ~EXTENDED_KEY;
	
	input_report_key(ep93xxkbd_dev, KScanCodeToVKeyTable[scancode], down);

	input_sync(ep93xxkbd_dev);

	DPRINTK("(virtual) scancode=%02X\n", scancode);
}

//=============================================================================
// InitSniffer
//=============================================================================
static void InitSniffer(void)
{
	int i;

	//
	// Clear our struct to indicate that no keys are down now.
	// If somebody boots this thing while holding down keys, then they'll
	// get what they deserve.
	//
	for (i = 0; i < MAX_KEYS_DOWN; i++) {
		KeyTracker[i].count = 0;
		KeyTracker[i].scancode = 0;
	}
}

//=============================================================================
// KeySniffer
//=============================================================================
// To prevent stuck keys, keep track of what keys are down.  This information
// is used by Check4StuckKeys().
//=============================================================================
static void KeySniffer(unsigned char scancode, int down)
{
	int i;

	//
	// There are certain keys that will definately get held down
	// and we can't interfere with that.
	//
	switch (scancode) {
	case 0x12:		/* left  shift */
	case 0x59:		/* right shift */
	case 0x14:		/* left  ctrl  */
	case 0x94:		/* right ctrl  */
	case 0x11:		/* left  alt   */
	case 0x91:		/* right alt   */
	case 0x58:		/* caps lock   */
	case 0x77:		/* Num lock    */
		//printk("Snuff - %02x, %d\n", scancode, down);
		handle_scancode(scancode, down);
		return;

	default:
		break;
	}
	
	//printk("Sniff - %02x, %d\n", scancode, down );

	//
	// Go thru our array, looking for the key.  If it already
	// is recorded, update its count.
	// Also look for empty cells in the array in case we
	// need one.
	//
	for (i = 0; i < MAX_KEYS_DOWN; i++) {
		//
		// If this is a key up in our list then we are done.
		//
		if (down == 0) {
			if (KeyTracker[i].scancode == scancode) {
				KeyTracker[i].count = 0;
				KeyTracker[i].scancode = 0;
				handle_scancode(scancode, down);
				break;
			}
		}
		//
		// Hey here's an unused cell.  Save its index.
		//
		else if (KeyTracker[i].count == 0) {
			KeyTracker[i].scancode = scancode;
			KeyTracker[i].count = 1;
			handle_scancode(scancode, down);
			break;
		}
	}
}

//=============================================================================
// Check4StuckKeys
//=============================================================================
// When a key is held down longer than 1/2 sec, it start repeating
// 10 times a second.  What we do is watch how long each key is
// held down.  If longer than X where X is less than 1/2 second
// then we assume it is stuck and issue the key up.  If we were
// wrong and the key really is being held down, no problem because
// the keyboard is about to start sending it to us repeatedly
// anyway.
//=============================================================================
static void Check4StuckKeys(void)
{
	int i;

	for (i = 0; i < MAX_KEYS_DOWN; i++) {
		if (KeyTracker[i].count) {
			KeyTracker[i].count++;
			if (KeyTracker[i].count >= EP93XX_MAX_KEY_DOWN_COUNT) {
				handle_scancode(KeyTracker[i].scancode, 0);
				KeyTracker[i].count = 0;
				KeyTracker[i].scancode = 0;
			}
		}
	}
}


//=============================================================================
// HandleKeyPress
//=============================================================================
// Checks if there are any keys in the FIFO and processes them if there are.
//=============================================================================
void HandleKeyPress(unsigned int Data)
{
	static unsigned char ucKScan[4] = { 0, 0, 0, 0 };
	static unsigned int ulNum = 0;
	int bParityValid;

	//
	// No keys to decode, but the timer went off and is calling us
	// to check for stuck keys.
	//
	if (Data == -1) {
		Check4StuckKeys();
		return;
	}
	//
	// Read in the value from the SPI controller.
	//
	ucKScan[ulNum++] = SPI2KScan(Data, &bParityValid);

	//
	// Bad parity?  We should read the rest of the fifo and
	// throw it away, because it will all be bad.  Then the
	// SSP will be reset when we close the SSP driver and 
	// all will be good again.
	//
	if (!bParityValid){
		//	printk("_BAD_PARITY_");  
		ulNum = 0;
	}
	//
	// If we have one character in the array, do the following.
	//
	if (ulNum == 1) {
		//
		// If it is a simple key without the extended scan code perform 
		// following.
		//
		if (ucKScan[0] < KSCAN_TABLE_SIZE) {
			DPRINTK("1:Dn %02x\n", ucKScan[0]);
			KeySniffer(ucKScan[0], 1);
			ulNum = 0;
		}
		//
		// I don't know what type of character this is so erase the 
		// keys stored in the buffer and continue.
		//
		else if ((ucKScan[0] != 0xF0) && (ucKScan[0] != 0xE0)) {
			DPRINTK("1:oops - %02x\n", ucKScan[0]);
			ulNum = 0;
		}
	} else if (ulNum == 2) {
		//
		// 0xF0 means that a key has been released.
		//
		if (ucKScan[0] == 0xF0) {
			//
			// If it is a simple key without the extended scan code 
			// perform the following.
			//
			if (ucKScan[1] < KSCAN_TABLE_SIZE) {
				DPRINTK("2:Up %02x %02x\n", ucKScan[0],
					ucKScan[1]);
				KeySniffer(ucKScan[1], 0);
				ulNum = 0;
			}
			//
			// If it a extended kscan continue to get the next byte.
			//
			else if (ucKScan[1] != 0xE0) {
				DPRINTK("2:oops - %02x %02x\n", ucKScan[0],
					ucKScan[1]);
				ulNum = 0;
			}
		}
		//
		// Find out what extended code it is.
		//
		else if (ucKScan[0] == 0xE0 && ucKScan[1] != 0xF0) {
			DPRINTK("2:Dn %02x %02x\n", ucKScan[0], ucKScan[1]);
			KeySniffer(EXTENDED_KEY | ucKScan[1], 1);
			ulNum = 0;
		}
	}
	//
	// This means that an extended code key has been released.
	//
	else if (ulNum == 3) {
		//
		// 0xF0 means that a key has been released.
		//
		if (ucKScan[0] == 0xE0 && ucKScan[1] == 0xF0) {
			DPRINTK("3:Up %02x %02x %02x",
				ucKScan[0], ucKScan[1], ucKScan[2]);
			KeySniffer(EXTENDED_KEY | ucKScan[2], 0);
		} else {
			DPRINTK("3:oops - %02x %02x %02x\n",
				ucKScan[0], ucKScan[1], ucKScan[2]);
		}
		ulNum = 0;
	}
}

//=============================================================================
// SPI2KScan
//=============================================================================
// Get a character from the spi port if it is available.
// 
// Below is a picture of the spi signal from the PS2. 
//
//CK HHllllHHLLLHHHLLLHHHLLLHHHLLLHHHLLLHHHLLLHHHLLLHHHLLLHHHLLLHHHLLLHHHLLLHllll
//DA HHHllllll000000111111222222333333444444555555666666777777ppppppssssssLLLLHHH
//        ^                                                                ^
//    start bit                                                   important bit
// 
//where:  l = 8042 driving the line 
//        L = KEYBOARD driving the line
//        1..7 data
//         = Parity 8042 driving
//        s = stop   8042 driving
//         = PARITY KEYBOARD driving the line
//        S = STOP   KEYBOARD driving the line
//
//  In our design the value comes high bit first and is inverted.  So we must
//  convert it to low byte first and then inverted it back.
//
//=============================================================================
static unsigned char SPI2KScan(unsigned int uiSPIValue, int *pValid)
{
	unsigned char ucKScan = 0;
	unsigned int uiParity = 0;
	unsigned int uiCount = 0;

	for (uiCount = 1; uiCount < 10; uiCount++) {
		uiParity += (uiSPIValue >> uiCount) & 0x1;
	}

	if (!(uiParity & 0x1) && (uiSPIValue & 0x401) == 0x400) {
		*pValid = 1;

		//
		// Invert the pattern.
		//
		uiSPIValue = ~uiSPIValue;

		//
		// Read in the value from the motorola spi file
		//
		ucKScan = (unsigned char)((uiSPIValue & 0x004) << 5);
		ucKScan |= (unsigned char)((uiSPIValue & 0x008) << 3);
		ucKScan |= (unsigned char)((uiSPIValue & 0x010) << 1);
		ucKScan |= (unsigned char)((uiSPIValue & 0x020) >> 1);
		ucKScan |= (unsigned char)((uiSPIValue & 0x040) >> 3);
		ucKScan |= (unsigned char)((uiSPIValue & 0x080) >> 5);
		ucKScan |= (unsigned char)((uiSPIValue & 0x100) >> 7);
		ucKScan |= (unsigned char)((uiSPIValue & 0x200) >> 9);
	} else {
		*pValid = 0;
	}

	return (ucKScan);
}

//=============================================================================
// EP93XXSpiKbdInit
//=============================================================================
int __init EP93XXSpiKbdInit(void)
{
	int i;
	printk("%s\n", name);

        ep93xxkbd_dev = input_allocate_device();
	if(ep93xxkbd_dev==0){
		DPRINTK("Allocate input device \n");
		return -1;
	}
	ep93xxkbd_dev->name = name;
	ep93xxkbd_dev->evbit[0] = BIT(EV_KEY);

	for (i = 0; i < KSCAN_TABLE_SIZE; i++)
		set_bit(KScanCodeToVKeyTable[i], ep93xxkbd_dev->keybit);

	input_register_device(ep93xxkbd_dev);

	/* Open SSP driver for Keyboard input. */
	g_SSP_Handle = SSPDriver->Open(PS2_KEYBOARD, HandleKeyPress);

	InitSniffer();

	DPRINTK("Leaving EP93XXSpiKbdInit()\n");

	return 0;
}

void __exit EP93XXSpiKbdCleanup(void)
{
	SSPDriver->Close(g_SSP_Handle);

	input_unregister_device(ep93xxkbd_dev);
}

module_init(EP93XXSpiKbdInit);
module_exit(EP93XXSpiKbdCleanup);
MODULE_LICENSE("GPL");
