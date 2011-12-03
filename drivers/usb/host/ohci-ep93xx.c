/*
 * OHCI HCD (Host Controller Driver) for USB.
 *
 * Bus Glue for EP93XX USB Controller
 *
 * Based on ohci-lh7a404 driver and previous EP93XX driver.
 *
 *
 * This file is licensed under the GPL.
 */
#include <linux/platform_device.h>
#include <asm/mach-types.h>
#include <asm/arch/hardware.h>

extern int usb_disabled(void);

/*-------------------------------------------------------------------------*/

static void ep93xx_start_hc(struct platform_device *pdev)
{
#if defined(/*CONFIG_ARCH_EDB9315*/CONFIG_MACH_EDB9315)
	unsigned int uiPBDR, uiPBDDR;
#endif

	pr_debug("Starting EP93XX OHCI USB Controller\n");
     
	/*
	 * Configure the power sense and control lines.  Place the USB
	 * host controller in reset.
	 */
#if defined(/*CONFIG_ARCH_EDB9315*/CONFIG_MACH_EDB9315)
	/*
	 * For EDB9315 boards, turn on USB by clearing EGPIO9 (Port B, Bit 1).
	 */
	uiPBDR = inl(GPIO_PBDR) & 0xfd;
	outl(uiPBDR, GPIO_PBDR );

	uiPBDDR = inl(GPIO_PBDDR) | 0x02;
	outl(uiPBDDR, GPIO_PBDDR );
#endif

	/*
	 * Now, carefully enable the USB clock, and take
	 * the USB host controller out of reset.
	 */
	writel(readl((void *)EP93XX_SYSCON_CLOCK_CONTROL) | EP93XX_SYSCON_CLOCK_USH_EN,(void *)EP93XX_SYSCON_CLOCK_CONTROL);
}

static void ep93xx_stop_hc(struct platform_device *pdev)
{
#if defined(/*CONFIG_ARCH_EDB9315*/CONFIG_MACH_EDB9315)
	unsigned int uiPBDR;
#endif

	pr_debug("Stopping EP93XX OHCI USB Controller\n");

	/* Shut down the USB block */
	writel(readl((void *) EP93XX_SYSCON_CLOCK_CONTROL) & (~EP93XX_SYSCON_CLOCK_USH_EN),(void *)EP93XX_SYSCON_CLOCK_CONTROL);

#if defined(/*CONFIG_ARCH_EDB9315*/CONFIG_MACH_EDB9315)
	/*
	 * For EDB9315 boards, turn off USB by setting EGPIO9 (Port B, Bit 1).
	 */
	uiPBDR = inl(GPIO_PBDR) | 0x02;
	outl(uiPBDR, GPIO_PBDR );
#endif 
}


/*-------------------------------------------------------------------------*/

/*
static irqreturn_t usb_hcd_ep93xx_hcim_irq (int irq, void *__hcd,
					     struct pt_regs * r)
{
	struct usb_hcd *hcd = __hcd;

	return usb_hcd_irq(irq, hcd, r);
}
*/
/*-------------------------------------------------------------------------*/

//void usb_hcd_ep93xx_remove (struct usb_hcd *, struct platform_device *);

/* configure so an HC device and id are always provided */
/* always called with process context; sleeping is OK */


/**
 * usb_hcd_ep93xx_probe - initialize EP93XX HCD
 * Context: !in_interrupt()
 *
 * Allocates basic resources for this USB host controller, and
 * then invokes the start() method for the HCD associated with it
 * through the hotplug entry's driver_data.
 *
 */
int usb_hcd_ep93xx_probe (const struct hc_driver *driver,
			  /*struct usb_hcd **hcd_out,*/
			 struct platform_device *pdev)
{
	int retval;
	struct usb_hcd *hcd = 0;
	
	if (pdev->num_resources != 2) {
		pr_debug("hcd probe: invalid num_resources %x\n",pdev->num_resources);
		return -ENODEV;
	}

	if ((pdev->resource[0].flags != IORESOURCE_MEM) || (pdev->resource[1].flags != IORESOURCE_IRQ)) {
		pr_debug("hcd probe: invalid resource type\n");
		return -ENODEV;
	}

	hcd = usb_create_hcd(driver, &pdev->dev, "ep93xx");
	if (!hcd){
		pr_debug("usb_create_hcd fail\n");
		return -ENOMEM;
	}

	hcd->rsrc_start = pdev->resource[0].start;
	hcd->rsrc_len = pdev->resource[0].end - pdev->resource[0].start + 1;

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len, hcd_name)) {
		pr_debug("request_mem_region failed\n");
		retval = -EBUSY;
		goto err1;
	}

	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs) {
		pr_debug("ioremap failed\n");
		retval = -EIO;
		goto err2;
	}

	ep93xx_start_hc(pdev);
	ohci_hcd_init(hcd_to_ohci(hcd));

	retval = usb_add_hcd(hcd, pdev->resource[1].start, SA_INTERRUPT);
	if (retval == 0){
		pr_debug("usb add hcd failed\n");
		return retval;
	}
	
	/* Error handling */
	ep93xx_stop_hc(pdev);

	iounmap(hcd->regs);

 err2:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);

 err1:
	usb_put_hcd(hcd);
	return retval;

}


/* may be called without controller electrically present */
/* may be called with controller, bus, and devices active */

/**
 * usb_hcd_ep93xx_remove - shutdown processing for EP93XX HCD
 * @dev: USB Host Controller being removed
 * Context: !in_interrupt()
 *
 * Reverses the effect of usb_hcd_ep93xx_probe(), first invoking
 * the HCD's stop() method.  It is always called from a thread
 * context, normally "rmmod", "apmd", or something similar.
 *
 */
void usb_hcd_ep93xx_remove (struct usb_hcd *hcd, struct platform_device *pdev)
{
	usb_remove_hcd(hcd);
	ep93xx_stop_hc(pdev);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
}

/*-------------------------------------------------------------------------*/

static int __devinit
ohci_ep93xx_start (struct usb_hcd *hcd)
{
	struct ohci_hcd	*ohci = hcd_to_ohci (hcd);
	int ret;

	ohci_dbg(ohci, "ohci_ep93xx_start, ohci:%p", ohci);
			
	if ((ret = ohci_init(ohci)) < 0){
		ohci_dbg(ohci, "ohci_ep93xx_start, ohci_init error=%x\n",ret);
		return ret;
	}
		
	ohci_dbg(ohci, "ohci_ep93xx_start, ohci->hcca:%p", ohci->hcca);
	
	if ((ret = ohci_run (ohci)) < 0) {
		pr_debug ("can't start %s", hcd->self.bus_name);
		ohci_stop (hcd);
		return ret;
	}

	create_debug_files(ohci);

#ifdef	DEBUG
	ohci_dump(ohci, 1);
#endif /*DEBUG*/

	return 0;
}

/*-------------------------------------------------------------------------*/

static const struct hc_driver ohci_ep93xx_hc_driver = {
	.description		= hcd_name,
	.product_desc		= "EP93xx OHCI",
	.hcd_priv_size		= sizeof(struct ohci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq				= ohci_irq,
	.flags				= HCD_USB2 | HCD_MEMORY,

	/*
	 * basic lifecycle operations
	 */
	.start				= ohci_ep93xx_start,
	.stop				= ohci_stop,
 	.shutdown			= ohci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue		= ohci_urb_enqueue,
	.urb_dequeue		= ohci_urb_dequeue,
	.endpoint_disable	= ohci_endpoint_disable,

	/*
	 * scheduling support
	 */
	.get_frame_number	= ohci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data	= ohci_hub_status_data,
	.hub_control		= ohci_hub_control,

#ifdef CONFIG_PM
	.bus_suspend		= ohci_bus_suspend,
	.bus_resume			= ohci_bus_resume,
#endif
	.start_port_reset	= ohci_start_port_reset,
};

/*-------------------------------------------------------------------------*/

static int ohci_hcd_ep93xx_drv_probe(struct platform_device *dev)
{
	struct platform_device *pdev = dev;/*to_platform_device(dev);*/
	int ret;

	if (usb_disabled())
		return -ENODEV;

	ret = usb_hcd_ep93xx_probe(&ohci_ep93xx_hc_driver, pdev);

	return ret;
}

static int ohci_hcd_ep93xx_drv_remove(struct platform_device *dev)
{
	struct platform_device *pdev = dev; /*to_platform_device(dev);*/
	struct usb_hcd *hcd = platform_get_drvdata(dev);/*dev_get_drvdata(dev);*/

	usb_hcd_ep93xx_remove(hcd, pdev);
	return 0;
}

#ifdef  CONFIG_PM
static int ohci_hcd_ep93xx_drv_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct usb_hcd *hcd = dev_get_drvdata(dev);

	return 0;
}
static int ohci_hcd_ep93xx_drv_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct usb_hcd *hcd = dev_get_drvdata(dev);


	return 0;
}
#endif

static struct /*device_driver*/platform_driver ohci_hcd_ep93xx_driver = {
	.probe		= ohci_hcd_ep93xx_drv_probe,
	.remove		= ohci_hcd_ep93xx_drv_remove,
 	.shutdown   = usb_hcd_platform_shutdown,
#ifdef CONFIG_PM
	.suspend	= ohci_hcd_ep93xx_drv_suspend, 
	.resume	= ohci_hcd_ep93xx_drv_resume, 
#endif
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "ep93xx-usb",
	},
};

