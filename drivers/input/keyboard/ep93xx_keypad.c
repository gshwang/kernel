/******************************************************************************
 * 
 *  File:   linux/drivers/input/keyboard/ep93xx_keypad.c
 *
 *  Purpose:    Support for 8x8 keypad (With Scroll Wheel) for a Cirrus Logic EP93xx
 *
 *  History:    
 *              For Sirius (Internet Radio) Project.
 *
 * Copyright 2006 Cirrus Logic Inc.
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/compiler.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/input.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>

#include "ep93xx_keypad.h"

static struct ep93xx_keypad_dev dev;
static char *name = "Cirrus EP93xx 8x8 keypad driver";

static void handle_scancode(struct ep93xx_keypad_dev* pdev, unsigned char scancode, int down)
{   
#ifdef FIX_FOR_EC12E2424404
    int i, wheel1_count=0, wheel2_count=0;
    if(down && ((scancode == KEY_WHEEL_1) || (scancode == KEY_WHEEL_2)) )
    {
        pdev->wheelkey_count &= 0x03;
        pdev->wheelkey_buffer[pdev->wheelkey_count] = scancode;
        pdev->wheelkey_count ++;

        if(pdev->wheelkey_hit)
        {
            for(i=0;i<=pdev->wheelkey_count;i++)
            {
                if(pdev->wheelkey_buffer[i]==KEY_WHEEL_1) wheel1_count++;
                if(pdev->wheelkey_buffer[i]==KEY_WHEEL_2) wheel2_count++;
            }
            if(wheel1_count >=3)
            {
                input_report_key((struct input_dev*)dev.input, KEY_WHEEL_1, 1);
                input_report_key((struct input_dev*)dev.input, KEY_WHEEL_1, 0);
                input_sync((struct input_dev*)dev.input);
            }
            else if(wheel2_count >=3)
            {
                input_report_key((struct input_dev*)dev.input, KEY_WHEEL_2, 1);
                input_report_key((struct input_dev*)dev.input, KEY_WHEEL_2, 0);
                input_sync((struct input_dev*)dev.input);
            }
            pdev->wheelkey_hit = 0;
            pdev->wheelkey_count = 0;
        }
    }
    else
#endif
    if(scancode > 0)
    {
        input_report_key((struct input_dev*)dev.input, scancode, down);
        input_sync((struct input_dev*)dev.input);
    }
}

static irqreturn_t ep93xx_keypad_isr(int irq, void *dev_id)
{
    int nkey1,nkey2;
#ifdef SCROLL_WHEEL_KEY_SUPPORT
    int phase,step;
#endif
    unsigned long status;
    struct ep93xx_keypad_dev* pdev = (struct ep93xx_keypad_dev*)dev_id;
    
    status = inl(KEY_REG);
    
    status &= 0x3fff;

/*  printk("status = %08x\n", (int)status);
    printk("%d\n",wheel_phase_table[pdev->wheelkey]);
*/
#ifdef SCROLL_WHEEL_KEY_SUPPORT
    phase = wheel_phase_table[pdev->wheelkey&0x3];
#endif

    if(status & KEYREG_KEY2)
    {
        nkey1 = KeyPadTable[ (status & KEYREG_KEY1_MASK)>>KEYREG_KEY1_SHIFT ];
        nkey2 = KeyPadTable[ (status & KEYREG_KEY2_MASK)>>KEYREG_KEY2_SHIFT ];
#ifdef SCROLL_WHEEL_KEY_SUPPORT
        if( (nkey1 == KEY_WHEEL_1) || (nkey2 == KEY_WHEEL_1) )
        {
            pdev->wheelkey |= WHEEL_FLAG1;
        }
        if( (nkey1 == KEY_WHEEL_2) || (nkey2 == KEY_WHEEL_2) )
        {
            pdev->wheelkey |= WHEEL_FLAG2;
        }
        if(pdev->last_key1 >0 || pdev->last_key2 >0)
        {
            if((pdev->last_key1 > 0) && (nkey1 != pdev->last_key1) && (nkey2 != pdev->last_key1) )
            {
                handle_scancode(pdev, pdev->last_key1, 0);
                pdev->last_key1 = 0;
            }
            if((pdev->last_key2 > 0) && (nkey1 != pdev->last_key2) && (nkey2 != pdev->last_key2) )
            {
                handle_scancode(pdev, pdev->last_key2, 0);
                pdev->last_key2 = 0;
            }
        }

        if( (nkey1 != KEY_WHEEL_1) && (nkey1 != KEY_WHEEL_2) )
        {
            if( (nkey1 != pdev->last_key1) && (nkey1 != pdev->last_key2) )
            {
                handle_scancode(pdev, nkey1, 1);
                pdev->last_key1 = nkey1;
            }
        }

        if( (nkey2 != KEY_WHEEL_1) && (nkey2 != KEY_WHEEL_2) )
        {
            if( (nkey2 != pdev->last_key1) && (nkey2 != pdev->last_key2) )
            {
                handle_scancode(pdev, nkey2, 1);
                pdev->last_key2 = nkey2;
            }
        }
#else
        if(pdev->last_key1 >0 || pdev->last_key2 >0)
        {
            if((pdev->last_key1 > 0) && (nkey1 != pdev->last_key1) && (nkey2 != pdev->last_key1) )
            {
                handle_scancode(pdev, pdev->last_key1, 0);
                pdev->last_key1 = 0;
            }
            if((pdev->last_key2 > 0) && (nkey1 != pdev->last_key2) && (nkey2 != pdev->last_key2) )
            {
                handle_scancode(pdev, pdev->last_key2, 0);
                pdev->last_key2 = 0;
            }
        }

        if( (nkey1 != pdev->last_key1) && (nkey1 != pdev->last_key2) )
        {
            handle_scancode(pdev, nkey1, 1);
            pdev->last_key1 = nkey1;
        }
        if( (nkey2 != pdev->last_key1) && (nkey2 != pdev->last_key2) )
        {
            handle_scancode(pdev, nkey2, 1);
            pdev->last_key2 = nkey2;
        }
#endif
    }
    else if(status & KEYREG_KEY1)
    {
        nkey1 = KeyPadTable[ (status & KEYREG_KEY1_MASK)>>KEYREG_KEY1_SHIFT ];
#ifdef SCROLL_WHEEL_KEY_SUPPORT
        if(nkey1 == KEY_WHEEL_1)
        {
            pdev->wheelkey |= WHEEL_FLAG1;
            if(pdev->wheelkey & WHEEL_FLAG2)
            {
                pdev->wheelkey &= ~WHEEL_FLAG2;
            }
            if(pdev->last_key1 >0 || pdev->last_key2 >0)
            {
                if((pdev->last_key1 > 0) && (nkey1 != pdev->last_key1) )
                {
                    handle_scancode(pdev, pdev->last_key1, 0);
                    pdev->last_key1 = 0;
                }
                if((pdev->last_key2 > 0) && (nkey1 != pdev->last_key2) )
                {
                    handle_scancode(pdev, pdev->last_key2, 0);
                    pdev->last_key2 = 0;
                }
            }
        }
        else if(nkey1 == KEY_WHEEL_2)
        {
            pdev->wheelkey |= WHEEL_FLAG2;
            if(pdev->wheelkey & WHEEL_FLAG1)
            {
                pdev->wheelkey &= ~WHEEL_FLAG1;
            }
            if(pdev->last_key1 >0 || pdev->last_key2 >0)
            {
                if((pdev->last_key1 > 0) && (nkey1 != pdev->last_key1) )
                {
                    handle_scancode(pdev, pdev->last_key1, 0);
                    pdev->last_key1 = 0;
                }
                if((pdev->last_key2 > 0) && (nkey1 != pdev->last_key2) )
                {
                    handle_scancode(pdev, pdev->last_key2, 0);
                    pdev->last_key2 = 0;
                }
            }
        }
        else
        {
            if(pdev->wheelkey & WHEEL_FLAG2)
            {
                pdev->wheelkey &= ~WHEEL_FLAG2;
            }
            if(pdev->wheelkey & WHEEL_FLAG1)
            {
                pdev->wheelkey &= ~WHEEL_FLAG1;
            }
            if(pdev->last_key1 >0 || pdev->last_key2 >0)
            {
                if((pdev->last_key1 > 0) && (nkey1 != pdev->last_key1) )
                {
                    handle_scancode(pdev, pdev->last_key1, 0);
                    pdev->last_key1 = 0;
                }
                if((pdev->last_key2 > 0) && (nkey1 != pdev->last_key2) )
                {
                    handle_scancode(pdev, pdev->last_key2, 0);
                    pdev->last_key2 = 0;
                }
            }
            handle_scancode(pdev, nkey1, 1);
            pdev->last_key1 = nkey1;
        }
#else
        if(pdev->last_key1 >0 || pdev->last_key2 >0)
        {
            if((pdev->last_key1 > 0) && (nkey1 != pdev->last_key1) )
            {
                handle_scancode(pdev, pdev->last_key1, 0);
                pdev->last_key1 = 0;
            }
            if((pdev->last_key2 > 0) && (nkey1 != pdev->last_key2) )
            {
                handle_scancode(pdev, pdev->last_key2, 0);
                pdev->last_key2 = 0;
            }
        }
        handle_scancode(pdev, nkey1, 1);
        pdev->last_key1 = nkey1;
#endif
    }
    else
    {
#ifdef SCROLL_WHEEL_KEY_SUPPORT
        if(pdev->wheelkey & WHEEL_FLAG2)
        {
            pdev->wheelkey &= ~WHEEL_FLAG2;
        }
        if(pdev->wheelkey & WHEEL_FLAG1)
        {
            pdev->wheelkey &= ~WHEEL_FLAG1;
        }
#ifdef FIX_FOR_EC12E2424404
        pdev->wheelkey_hit = 1;
#endif

#endif
        if(pdev->last_key1)
        {
            handle_scancode(pdev, pdev->last_key1, 0);
            pdev->last_key1 = 0;
        }
        if(pdev->last_key2)
        {
            handle_scancode(pdev, pdev->last_key2, 0);
            pdev->last_key2 = 0;
        }
    }
#ifdef SCROLL_WHEEL_KEY_SUPPORT
    if(phase != wheel_phase_table[pdev->wheelkey&0x3])
    {
        step = ((wheel_phase_table[pdev->wheelkey&0x3] - phase + 4 ) % 4 );
        if(step == 2)
            step = pdev->last_wheel_step;
        else
            pdev->last_wheel_step = step;
        
        /*printk("step %d\n",step);*/
        switch(step)
        {
        case 1:
            handle_scancode(pdev, KEY_WHEEL_1, 1);
            handle_scancode(pdev, KEY_WHEEL_1, 0);
            break;
        case 3:
            handle_scancode(pdev, KEY_WHEEL_2, 1);
            handle_scancode(pdev, KEY_WHEEL_2, 0);
            break;
        default:
            break;
        }
    }
#endif
    return(IRQ_HANDLED);
}
 
unsigned long setprescale (unsigned short nScale)
{
    unsigned long nValue;

    nValue = inl(SCANINIT);
    nValue = ( nValue & (~SCANINIT_PRSCL_MASK) ) | (nScale & SCANINIT_PRSCL_MASK);
    outl(nValue, SCANINIT);
    return nValue;
}

unsigned long setdebounce (unsigned char nDebounce)
{
    unsigned long nValue;

    nValue = inl(SCANINIT);
    nValue = ( nValue & (~SCANINIT_DBNC_MASK) ) | ((nDebounce<<16) & SCANINIT_DBNC_MASK);
    outl(nValue, SCANINIT);
    return nValue;
}

void keypad_enable ( unsigned char bState )
{
    unsigned long nreg;
    if( bState ) 
    {
        nreg = inl(SYSCON_KTDIV);
        nreg |= SYSCON_KTDIV_KEN;
        SysconSetLocked( SYSCON_KTDIV, nreg );    
    }
    else
    {
        nreg = inl(SYSCON_KTDIV);
        nreg &= (~SYSCON_KTDIV_KEN);
        SysconSetLocked( SYSCON_KTDIV, nreg );    
    }
    return;
}

//=============================================================================
// ep93xx_keypad_init
//=============================================================================
int __init ep93xx_keypad_init(void)
{
    int i;
    int retval;

    printk("%s\n", name);

    dev.input = input_allocate_device();
    if(dev.input==0){
    	printk("Allocate input device \n");
        return -1;
    }

#ifdef SCROLL_WHEEL_KEY_SUPPORT
    dev.wheelkey = 0;
    dev.last_wheel_step = 0;
#ifdef FIX_FOR_EC12E2424404
    dev.wheelkey_count = 0;
    dev.wheelkey_hit = 0;
#endif

#endif

    dev.last_key1 = 0;
    dev.last_key2 = 0;
    dev.input->name = name;
    dev.input->evbit[0] = BIT(EV_KEY);

    for (i = 0; i < KeyPad_SIZE; i++)
        set_bit(KeyPadTable[i], dev.input->keybit);

#ifdef FIX_FOR_EC12E2424404
    setdebounce(0xfe);
    setprescale(0x8);
#else
    setdebounce(0xfe);
    setprescale(0x80);
#endif
    retval = request_irq( IRQ_EP93XX_KEY, ep93xx_keypad_isr, SA_INTERRUPT, "ep93xx_keypad", (void*)&dev);
    if( retval )
    {
        printk(KERN_WARNING "ep93xx_keypad: failed to get keypad IRQ\n");
        return retval;
    }
    
    input_register_device((struct input_dev*)dev.input);

    keypad_enable(1);

    return 0;
}

void __exit ep93xx_keypad_cleanup(void)
{
    input_unregister_device((struct input_dev*)dev.input);
}

module_init(ep93xx_keypad_init);
module_exit(ep93xx_keypad_cleanup);
MODULE_LICENSE("GPL");
