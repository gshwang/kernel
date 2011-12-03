/*
 *  linux/drivers/serial/cs93xx.c
 *
 *  Driver for EP93xx serial ports
 *
 *  Based on drivers/char/serial.c, by Linus Torvalds, Theodore Ts'o,
 *  Deep Blue Solutions Ltd.
 *
 *  Copyright 1999 ARM Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd.
 *  Copyright (c) 2003 Cirrus Logic, Inc.
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
 *  $Id: linux-2.6.20.4-edb93xx.patch,v 1.1 2007/04/26 03:28:00 cwu Exp $
 *
 * The EP93xx serial ports are AMBA, but at different addresses from the
 * integrator.
 * This is a generic driver for ARM AMBA-type serial ports.  They
 * have a lot of 16550-like features, but are not register compatable.
 * Note that although they do have CTS, DCD and DSR inputs, they do
 * not have an RI input, nor do they have DTR or RTS outputs.  If
 * required, these have to be supplied via some other means (eg, GPIO)
 * and hooked into this driver.
 */
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/spinlock.h>
//#include <linux/device.h>
#include <linux/platform_device.h>

#include <asm/hardware.h>
#include <asm/io.h>
//#include <asm/irqs.h>

#if defined(CONFIG_SERIAL_EP93XX_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/serial_core.h>
#include <linux/amba/bus.h>
#include <linux/amba/serial.h>

#if defined(CONFIG_ARCH_EP9301) || defined(CONFIG_ARCH_EP9302)
#ifdef CONFIG_EP93XX_IRDA
#define UART_NR		1
#else
#define UART_NR		2
#endif
#else
#ifdef CONFIG_EP93XX_IRDA
#define UART_NR		2
#else
#define UART_NR		3
#endif
#endif

#define SERIAL_EP93XX_MAJOR		204
#define SERIAL_EP93XX_MINOR		16
#define SERIAL_EP93XX_NR		UART_NR

#define EP93XX_ISR_PASS_LIMIT	256

/*
 * Access macros for the AMBA UARTs
 */
#define UART_GET_INT_STATUS(p)	((readl((p)->membase + UARTIIR)) & 0xff)
#define UART_PUT_ICR(p, c)		writel((c), (p)->membase + UARTICR)
#define UART_GET_FR(p)			((readl((p)->membase + UARTFR)) & 0xff)
#define UART_GET_CHAR(p)		((readl((p)->membase + UARTDR)) & 0xff)
#define UART_PUT_CHAR(p, c)		writel((c), (p)->membase + UARTDR)
#define UART_GET_RSR(p)			((readl((p)->membase + UARTRSR)) & 0xff)
#define UART_PUT_RSR(p, c)		writel((c), (p)->membase + UARTRSR)
#define UART_GET_CR(p)			((readl((p)->membase + UARTCR)) & 0xff)
#define UART_PUT_CR(p,c)		writel((c), (p)->membase + UARTCR)
#define UART_GET_LCRL(p)		((readl((p)->membase + UARTCR_L)) & 0xff)
#define UART_PUT_LCRL(p,c)		writel((c), (p)->membase + UARTCR_L)
#define UART_GET_LCRM(p)		((readl((p)->membase + UARTCR_M)) & 0xff)
#define UART_PUT_LCRM(p,c)		writel((c), (p)->membase + UARTCR_M)
#define UART_GET_LCRH(p)		((readl((p)->membase + UARTCR_H)) & 0xff)
#define UART_PUT_LCRH(p,c)		writel((c), (p)->membase + UARTCR_H)
#define UART_RX_DATA(s)			(((s) & UARTFR_RXFE) == 0)
#define UART_TX_READY(s)		(((s) & UARTFR_TXFF) == 0)
#define UART_TX_EMPTY(p)		((UART_GET_FR(p) & UARTFR_TMSK) == 0)

#define UART_GET_MCR(p)			readl((p)->membase + UARTMCR)
#define UART_PUT_MCR(c, p)		writel((c), (p)->membase + UARTMCR)
#define UART_CLEAR_ECR(p)		writel( 0, (p)->membase + UARTECR)

#define UART_DUMMY_RSR_RX		256
#define UART_PORT_SIZE			65536

/*
 * We wrap our port structure around the generic uart_port.
 */
struct uart_ep93xx_port {
	struct uart_port	port;
	unsigned int		old_status;
};

static void
ep93xxuart_enable_clocks(struct uart_port *port)
{
	unsigned int uiSysDevCfg;

	/*
	 * Enable the clocks to this UART in CSC_syscon
	 * - Read DEVCFG
	 * - OR in the correct uart enable bit
	 * - Set the lock register
	 * - Write back to DEVCFG
	 */
	uiSysDevCfg = inl(SYSCON_DEVCFG);

	switch ((unsigned long)port->membase) {
	case UART1_BASE:
		uiSysDevCfg |=  SYSCON_DEVCFG_U1EN;
		uiSysDevCfg &= ~SYSCON_DEVCFG_MonG;  /* handle this better... */
		break;

	case UART2_BASE:
		uiSysDevCfg |=  SYSCON_DEVCFG_U2EN;
		break;

	case UART3_BASE:
		uiSysDevCfg |= ( SYSCON_DEVCFG_U3EN | SYSCON_DEVCFG_TonG );
		break;
	}

	SysconSetLocked( SYSCON_DEVCFG, uiSysDevCfg );
}

static void
ep93xxuart_disable_clocks(struct uart_port *port)
{
	unsigned int uiSysDevCfg;

	/*
	 * Disable the clocks to this UART in CSC_syscon
	 * - Read DEVCFG
	 * - AND to clear the correct uart enable bit
	 * - Set the lock register
	 * - Write back to DEVCFG
	 */
	uiSysDevCfg = inl(SYSCON_DEVCFG);

	switch ((unsigned long)port->membase) {
	case UART1_BASE:
		uiSysDevCfg &= ~((unsigned int)SYSCON_DEVCFG_U1EN);
		break;

	case UART2_BASE:
		uiSysDevCfg &= ~((unsigned int)SYSCON_DEVCFG_U2EN);
		break;

	case UART3_BASE:
		uiSysDevCfg &= ~((unsigned int)SYSCON_DEVCFG_U3EN | (unsigned int)SYSCON_DEVCFG_TonG);
		break;
	}

	SysconSetLocked( SYSCON_DEVCFG, uiSysDevCfg );
}

#if 1
static int
ep93xxuart_is_port_enabled(struct uart_port *port)
{
	unsigned int uiSysDevCfg;

	uiSysDevCfg = inl(SYSCON_DEVCFG);

	switch ((unsigned long)port->membase) {
	case UART1_BASE:
		uiSysDevCfg &= (unsigned int)SYSCON_DEVCFG_U1EN;
		break;

	case UART2_BASE:
		uiSysDevCfg &= (unsigned int)SYSCON_DEVCFG_U2EN;
		break;

	case UART3_BASE:
		uiSysDevCfg &= ((unsigned int)SYSCON_DEVCFG_U3EN | (unsigned int)SYSCON_DEVCFG_TonG);
		break;
	}

	return( uiSysDevCfg != 0 );
}
#endif

static void
ep93xxuart_stop_tx(struct uart_port *port, unsigned int tty_stop)
{
	unsigned int cr;

	cr = UART_GET_CR(port);
	cr &= ~UARTCR_TIE;
	UART_PUT_CR(port, cr);
}

static void
ep93xxuart_start_tx(struct uart_port *port, unsigned int tty_start)
{
	unsigned int cr;

	cr = UART_GET_CR(port);
	cr |= UARTCR_TIE;
	UART_PUT_CR(port, cr);
}

static void
ep93xxuart_stop_rx(struct uart_port *port)
{
	unsigned int cr;

	cr = UART_GET_CR(port);
	cr &= ~(UARTCR_RIE | UARTCR_RTIE);
	UART_PUT_CR(port, cr);
}

static void
ep93xxuart_enable_ms(struct uart_port *port)
{
	unsigned int cr;

	cr = UART_GET_CR(port);
	cr |= UARTCR_MSIE;
	UART_PUT_CR(port, cr);
}

static void
#ifdef SUPPORT_SYSRQ
ep93xxuart_rx_chars(struct uart_port *port, struct pt_regs *regs)
#else
ep93xxuart_rx_chars(struct uart_port *port)
#endif
{
	struct tty_struct *tty = port->info->tty;
	unsigned int status, ch, flag, rsr, max_count = 256;

	status = UART_GET_FR(port);
	while (UART_RX_DATA(status) && max_count--) {
		ch = UART_GET_CHAR(port);
		flag = TTY_NORMAL;

		port->icount.rx++;

		/*
		 * Note that the error handling code is
		 * out of the main execution path
		 */
		rsr = UART_GET_RSR(port) | UART_DUMMY_RSR_RX;
		UART_PUT_RSR(port, 0);
		if (rsr & UARTRSR_ANY) {
			if (rsr & UARTRSR_BE) {
				rsr &= ~(UARTRSR_FE | UARTRSR_PE);
				port->icount.brk++;
				if (uart_handle_break(port))
					goto ignore_char;
			} else if (rsr & UARTRSR_PE) {
				port->icount.parity++;
			} else if (rsr & UARTRSR_FE) {
				port->icount.frame++;
			}
			
			if (rsr & UARTRSR_OE)
				port->icount.overrun++;

			rsr &= port->read_status_mask;

			if (rsr & UARTRSR_BE)
				flag = TTY_BREAK;
			else if (rsr & UARTRSR_PE)
				flag = TTY_PARITY;
			else if (rsr & UARTRSR_FE)
				flag = TTY_FRAME;
		}

		if (uart_handle_sysrq_char(port, ch))
			goto ignore_char;

		uart_insert_char(port, rsr, UART01x_RSR_OE, ch, flag);

	ignore_char:
		status = UART_GET_FR(port);
	}
	tty_flip_buffer_push(tty);
	return;
}

static void
ep93xxuart_tx_chars(struct uart_port *port)
{
	struct circ_buf *xmit = &port->info->xmit;
	int count;

	if (port->x_char) {
		UART_PUT_CHAR(port, port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}
	if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
		ep93xxuart_stop_tx(port, 0);
		return;
	}

	count = port->fifosize >> 1;
	do {
		UART_PUT_CHAR(port, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	} while (--count > 0);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	if (uart_circ_empty(xmit))
		ep93xxuart_stop_tx(port, 0);
}

static void
ep93xxuart_modem_status(struct uart_port *port)
{
	struct uart_ep93xx_port *uap = (struct uart_ep93xx_port *)port;
	unsigned int status, delta;

	UART_PUT_ICR(port, 0);

	status = UART_GET_FR(port) & UARTFR_MODEM_ANY;

	delta = status ^ uap->old_status;
	uap->old_status = status;

	if (!delta)
		return;

	if (delta & UARTFR_DCD)
		uart_handle_dcd_change(port, status & UARTFR_DCD);

	if (delta & UARTFR_DSR)
		port->icount.dsr++;

	if (delta & UARTFR_CTS)
		uart_handle_cts_change(port, status & UARTFR_CTS);

	wake_up_interruptible(&port->info->delta_msr_wait);
}

static irqreturn_t
ep93xxuart_int(int irq, void *dev_id, struct pt_regs *regs)
{
	struct uart_port *port = dev_id;
	unsigned int status, pass_counter = EP93XX_ISR_PASS_LIMIT;

	spin_lock(&port->lock);

	status = UART_GET_INT_STATUS(port);

	do {
		if (status & (UARTIIR_RTIS | UARTIIR_RIS))
#ifdef SUPPORT_SYSRQ
			ep93xxuart_rx_chars(port, regs);
#else
			ep93xxuart_rx_chars(port);
#endif
		if (status & UARTIIR_TIS)
			ep93xxuart_tx_chars(port);
		if (status & UARTIIR_MIS)
			ep93xxuart_modem_status(port);

		if (pass_counter-- == 0)
			break;

		status = UART_GET_INT_STATUS(port);
	} while (status & (UARTIIR_RTIS | UARTIIR_RIS | UARTIIR_TIS));

	spin_unlock(&port->lock);

	return IRQ_HANDLED;
}

static unsigned int
ep93xxuart_tx_empty(struct uart_port *port)
{
	return UART_GET_FR(port) & UARTFR_BUSY ? 0 : TIOCSER_TEMT;
}

static unsigned int
ep93xxuart_get_mctrl(struct uart_port *port)
{
	unsigned int result = 0;
	unsigned int status;

   	/* fixme: use a wrapper struct w/ ms flags */
	if (port->line == 0) {
		status = UART_GET_FR(port);

		if (status & UARTFR_DCD)
			result |= TIOCM_CAR;
		if (status & UARTFR_DSR)
			result |= TIOCM_DSR;
		if (status & UARTFR_CTS)
			result |= TIOCM_CTS;
	}
	return result;
}

static void
ep93xxuart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	unsigned int ctrl = 0;

	ctrl = UART_GET_MCR(port);

#define BIT(tiocmbit, uartbit)		\
	if (mctrl & tiocmbit)		\
		ctrl |= uartbit;	\
	else				\
		ctrl &= ~uartbit

	if( port->line == 0 ) {
		BIT(TIOCM_RTS, UARTMCR_RTS);
		BIT(TIOCM_DTR, UARTMCR_DTR);
		BIT(TIOCM_OUT1, UARTMCR_OUT1);
		BIT(TIOCM_OUT2, UARTMCR_OUT2);
		BIT(TIOCM_LOOP, UARTMCR_LOOP);
	}	
	if( port->line == 2 ) {
		BIT(TIOCM_OUT1, UARTMCR_OUT1);
		BIT(TIOCM_OUT2, UARTMCR_OUT2);
	}	

#undef BIT

	UART_PUT_MCR(ctrl, port);
}

static void
ep93xxuart_break_ctl(struct uart_port *port, int break_state)
{
	unsigned long flags;
	unsigned int lcr_h;

	spin_lock_irqsave(&port->lock, flags);
	lcr_h = UART_GET_LCRH(port);
	if (break_state == -1)
		lcr_h |= UARTLCR_H_BRK;
	else
		lcr_h &= ~UARTLCR_H_BRK;
	UART_PUT_LCRH(port, lcr_h);
	spin_unlock_irqrestore(&port->lock, flags);
}

static int
ep93xxuart_startup(struct uart_port *port)
{
	struct uart_ep93xx_port *uap = (struct uart_ep93xx_port *)port;
	int retval = -EINVAL;
	char *name = 0;

	/*
	 * Enable the clocks for this port.
	 */
	ep93xxuart_enable_clocks(port);

	/*
	 * Allocate the IRQ
	 */
	retval = request_irq(port->irq, ep93xxuart_int, 0 ,  "ep93xxuart", port);
	if (retval) {
		ep93xxuart_disable_clocks(port);
		return retval;
	}

	/*
	 * initialise the old status of the modem signals
	 */
	uap->old_status = UART_GET_FR(port) & UARTFR_MODEM_ANY;

	/*
	 * Finally, enable interrupts
	 */
	spin_lock_irq(&port->lock);
	UART_PUT_CR(port, (UARTCR_UARTEN | UARTCR_RIE | UARTCR_RTIE) & ~0x6);
	spin_unlock_irq(&port->lock);
	
	return 0;
}

static void
ep93xxuart_shutdown(struct uart_port *port)
{
	/*
	 * disable all interrupts, disable the port
	 */
	UART_PUT_CR(port, 0);

	/*
	 * disable break condition and fifos
	 */
	UART_PUT_LCRH(port, UART_GET_LCRH(port) & ~(UARTLCR_H_BRK | UARTLCR_H_FEN));

	/*
	 * Free the interrupt
	 */
	free_irq(port->irq, port);

	/*
	 * Disable the clock.
	 */
	ep93xxuart_disable_clocks( port );
}

static void
ep93xxuart_set_termios(struct uart_port *port, struct termios *termios,
		       struct termios *old)
{
	unsigned int lcr_h, baud, quot;
	unsigned long flags;

	/*
	 * Ask the core to calculate the divisor for us.
	 */
	baud = uart_get_baud_rate(port, termios, old, 0, port->uartclk/16);
	quot = uart_get_divisor(port, baud) - 1;
	
	/* byte size and parity */
	switch (termios->c_cflag & CSIZE) {
	case CS5:
		lcr_h = UARTLCR_H_WLEN_5;
		break;
	case CS6:
		lcr_h = UARTLCR_H_WLEN_6;
		break;
	case CS7:
		lcr_h = UARTLCR_H_WLEN_7;
		break;
	default: // CS8
		lcr_h = UARTLCR_H_WLEN_8;
		break;
	}
	if (termios->c_cflag & CSTOPB)
		lcr_h |= UARTLCR_H_STP2;
	if (termios->c_cflag & PARENB) {
		lcr_h |= UARTLCR_H_PEN;
		if (!(termios->c_cflag & PARODD))
			lcr_h |= UARTLCR_H_EPS;
	}
	if (port->fifosize > 1)
		lcr_h |= UARTLCR_H_FEN;

	spin_lock_irqsave(&port->lock, flags);

	/*
	 * Update the per-port timeout.
	 */
	uart_update_timeout(port, termios->c_cflag, baud);

	port->read_status_mask = UARTRSR_OE;
	if (termios->c_iflag & INPCK)
		port->read_status_mask |= UARTRSR_FE | UARTRSR_PE;
	if (termios->c_iflag & (BRKINT | PARMRK))
		port->read_status_mask |= UARTRSR_BE;

	/*
	 * Characters to ignore
	 */
	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= UARTRSR_FE | UARTRSR_PE;
	if (termios->c_iflag & IGNBRK) {
		port->ignore_status_mask |= UARTRSR_BE;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (termios->c_iflag & IGNPAR)
			port->ignore_status_mask |= UARTRSR_OE;
	}

	/*
	 * Ignore all characters if CREAD is not set.
	 */
	if ((termios->c_cflag & CREAD) == 0)
		port->ignore_status_mask |= UART_DUMMY_RSR_RX;

	if (UART_ENABLE_MS(port, termios->c_cflag))
		ep93xxuart_enable_ms(port);

	UART_PUT_LCRL(port, quot & 0xff);
	UART_PUT_LCRM(port, (quot >> 8) & 0xff);
	UART_PUT_LCRH(port, lcr_h);

	spin_unlock_irqrestore(&port->lock, flags);
}

static const char *
ep93xxuart_type(struct uart_port *port)
{
	return port->type == PORT_AMBA ? "EP93XX" : NULL;
}

/*
 * Release the memory region(s) being used by 'port'
 */
static void
ep93xxuart_release_port(struct uart_port *port)
{
}

/*
 * Request the memory region(s) being used by 'port'
 */
static int
ep93xxuart_request_port(struct uart_port *port)
{
	return 0;
}

/*
 * Configure/autoconfigure the port.
 */
static void
ep93xxuart_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE)
		port->type = PORT_AMBA;
}

/*
 * verify the new serial_struct (for TIOCSSERIAL).
 */
static int
ep93xxuart_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	int ret = 0;
	if (ser->type != PORT_UNKNOWN && ser->type != PORT_AMBA)
		ret = -EINVAL;
	if (ser->irq < 0 || ser->irq >= NR_IRQS)
		ret = -EINVAL;
	if (ser->baud_base < 9600)
		ret = -EINVAL;
	return ret;
}

static struct uart_ops ep93xx_pops = {
	.tx_empty	= ep93xxuart_tx_empty,
	.set_mctrl	= ep93xxuart_set_mctrl,
	.get_mctrl	= ep93xxuart_get_mctrl,
	.stop_tx	= ep93xxuart_stop_tx,
	.start_tx	= ep93xxuart_start_tx,
	.stop_rx	= ep93xxuart_stop_rx,
	.enable_ms	= ep93xxuart_enable_ms,
	.break_ctl	= ep93xxuart_break_ctl,
	.startup	= ep93xxuart_startup,
	.shutdown	= ep93xxuart_shutdown,
	.set_termios	= ep93xxuart_set_termios,
	.type		= ep93xxuart_type,
	.release_port	= ep93xxuart_release_port,
	.request_port	= ep93xxuart_request_port,
	.config_port	= ep93xxuart_config_port,
	.verify_port	= ep93xxuart_verify_port,
};

static struct uart_ep93xx_port ep93xx_ports[UART_NR] = {
	{
		.port	= {
			.membase	= (void *)UART1_BASE,
			.mapbase	= /*HwRegToPhys(UART1_BASE)*/UART1_BASE_VIRT,
			.iotype		= UPIO_MEM,
			.irq		= IRQ_EP93XX_UART1,
			.uartclk	= 14745600,
			.fifosize	= 16,
			.ops		= &ep93xx_pops,
			.flags		= UPF_BOOT_AUTOCONF,
			.line		= 0,
		},
	},
#if !defined(CONFIG_EP93XX_IRDA)
	{
		.port	= {
			.membase	= (void *)UART2_BASE,
			.mapbase	= /*HwRegToPhys(UART2_BASE)*/UART2_BASE_VIRT,
			.iotype		= UPIO_MEM,
			.irq		= IRQ_EP93XX_UART2,
			.uartclk	= 14745600,
			.fifosize	= 16,
			.ops		= &ep93xx_pops,
			.flags		= UPF_BOOT_AUTOCONF,
			.line		= 1,
		},
	},
#endif
#if !defined(CONFIG_ARCH_EP9301) && !defined(CONFIG_ARCH_EP9302)
	{
		.port	= {
			.membase	= (void *)UART3_BASE,
			.mapbase	= /*HwRegToPhys(UART3_BASE)*/UART3_BASE_VIRT,
			.iotype		= UPIO_MEM,
			.irq		= IRQ_EP93XX_UART3,
			.uartclk	= 14745600,
			.fifosize	= 16,
			.ops		= &ep93xx_pops,
			.flags		= UPF_BOOT_AUTOCONF,
#if !defined(CONFIG_EP93XX_IRDA)
			.line		= 2,
#else
			.line		= 1,
#endif
		},
	}
#endif
};

#ifdef CONFIG_SERIAL_EP93XX_CONSOLE

static void
ep93xxuart_console_write_char(struct uart_port *port, char ch)
{
	unsigned int status;

	do {
		status = UART_GET_FR(port);
	} while (!UART_TX_READY(status));
	UART_PUT_CHAR(port, ch);
}

static void
ep93xxuart_console_write(struct console *co, const char *s, unsigned int count)
{
	struct uart_port *port = &ep93xx_ports[co->index].port;
	unsigned int status, old_cr, old_power;
	int i;

	old_power = ep93xxuart_is_port_enabled(port);
	if (!old_power)
		ep93xxuart_enable_clocks(port);

	/*
	 * First save the CR then disable the interrupts
	 */
	old_cr = UART_GET_CR(port);
	UART_PUT_CR(port, UARTCR_UARTEN);

	/*
	 * Now, do each character
	 */
	for (i = 0; i < count; i++) {
		ep93xxuart_console_write_char(port, s[i]);
		if (s[i] == '\n')
			ep93xxuart_console_write_char(port, '\r');
	}

	/*
	 * Finally, wait for transmitter to become empty
	 * and restore the TCR
	 */
	do {
		status = UART_GET_FR(port);
	} while (status & UARTFR_BUSY);
	UART_PUT_CR(port, old_cr);

	if (!old_power)
		ep93xxuart_disable_clocks(port);
}

static void __init
ep93xxuart_console_get_options(struct uart_port *port, int *baud, int *parity,
			       int *bits)
{
	if (UART_GET_CR(port) & UARTCR_UARTEN) {
		unsigned int lcr_h, lcr_m, lcr_l;

		lcr_h = UART_GET_LCRH(port);

		*parity = 'n';
		if (lcr_h & UARTLCR_H_PEN) {
			if (lcr_h & UARTLCR_H_EPS)
				*parity = 'e';
			else
				*parity = 'o';
		}

		if ((lcr_h & 0x60) == UARTLCR_H_WLEN_7)
			*bits = 7;
		else
			*bits = 8;

		lcr_m = UART_GET_LCRM(port);
		lcr_l = UART_GET_LCRL(port);

		*baud = port->uartclk / (16 * ((lcr_m * 256) + lcr_l + 1));
	}
}

static int __init
ep93xxuart_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = 57600;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	/*
	 * Check whether an invalid uart number has been specified, and
	 * if so, search for the first available port that does have
	 * console support.
	 */
	if (co->index >= UART_NR)
		co->index = 0;
	port = &ep93xx_ports[co->index].port;

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);
	else
		ep93xxuart_console_get_options(port, &baud, &parity, &bits);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

/*
 * The 'index' element of this field selects which UART to use for
 * console.  For ep93xx, valid values are 0, 1, and 2.  If you set
 * it to -1, then uart_get_console will search for the first UART
 * which is the same as setting it to 0.
 */
extern struct uart_driver ep93xx_reg;
static struct console ep93xx_console = {
	.name		= "ttyAM",
	.write		= ep93xxuart_console_write,
	.device		= uart_console_device,
	.setup		= ep93xxuart_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &ep93xx_reg,
};

static int __init
ep93xxuart_console_init(void)
{
	register_console(&ep93xx_console);
	return 0;
}
console_initcall(ep93xxuart_console_init);

#define EP93XX_CONSOLE	&ep93xx_console
#else
#define EP93XX_CONSOLE	NULL
#endif

static struct uart_driver ep93xx_reg = {
	.driver_name		= "ttyAM",
	.dev_name		= "ttyAM",
	.major			= SERIAL_EP93XX_MAJOR,
	.minor			= SERIAL_EP93XX_MINOR,
	.nr			= UART_NR,
	.cons			= EP93XX_CONSOLE,
};

static struct resource uarts[UART_NR] = {
	[0] = {
		.name		= "uart1",
		.start		= /*HwRegToPhys(UART1_BASE)*/UART1_BASE_VIRT,
		.end		= /*HwRegToPhys(UART1_BASE)*/UART1_BASE_VIRT + 0x0000ffff,
	},
#if !defined(CONFIG_EP93XX_IRDA)
	[1] = {
		.name		= "uart2",
		.start		= /*HwRegToPhys(UART2_BASE)*/UART2_BASE_VIRT,
		.end		= /*HwRegToPhys(UART2_BASE)*/UART2_BASE_VIRT + 0x0000ffff,
	},
#endif
#if !defined(CONFIG_ARCH_EP9301) && !defined(CONFIG_ARCH_EP9302)
	[2] = {
		.name		= "uart3",
		.start		= /*HwRegToPhys(UART3_BASE)*/UART3_BASE_VIRT,
		.end		= /*HwRegToPhys(UART3_BASE)*/UART3_BASE_VIRT + 0x0000ffff,
	},
#endif
};

static int __init
ep93xxuart_init(void)
{
	int ret, i;

	ret = uart_register_driver(&ep93xx_reg);
	if (ret == 0) {
		for (i = 0; i < UART_NR; i++) {
			request_resource(&iomem_resource, &uarts[i]);
			uart_add_one_port(&ep93xx_reg, &ep93xx_ports[i].port);
		}
	}
	return ret;
}

static void __exit ep93xxuart_exit(void)
{
	int i;

	for (i = 0; i < UART_NR; i++)
		uart_remove_one_port(&ep93xx_reg, &ep93xx_ports[i].port);
	uart_unregister_driver(&ep93xx_reg);
}

module_init(ep93xxuart_init);
module_exit(ep93xxuart_exit);

MODULE_AUTHOR("ARM Ltd/Deep Blue Solutions Ltd/Cirrus Logic, Inc.");
MODULE_DESCRIPTION("EP93xx ARM serial port driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV(SERIAL_EP93XX_MAJOR, SERIAL_EP93XX_MINOR);
