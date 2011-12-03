/*======================================================================

    drivers/mtd/maps/edb93xx.c: EDB93xx flash map driver

    Copyright (C) 2000 ARM Limited
    Copyright (C) 2003 Deep Blue Solutions Ltd.
    Copyright (C) 2004 Cirrus Logic, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

======================================================================*/
#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/init.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

#include <asm/mach/flash.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/system.h>

struct edb93xx_flash_info {
	struct flash_platform_data *plat;
	struct resource		*res;
	struct mtd_partition	*parts;
	struct mtd_info		*mtd;
	struct map_info		map;
};

#ifdef CONFIG_MTD_COMPLEX_MAPPINGS
#define MUNG_ADDR(x) ((x & 0xfffe0000) | ((x & 0x00010000) >> 15) | ((x & 0x0000ffff) << 1))

static map_word
edb93xx_flash_read(struct map_info *map, unsigned long ofs)
{
	map_word ret;
	if (ofs == 0x40)
		ret.x[0] = 0;
	else
		ret.x[0] = __raw_readw(map->virt + MUNG_ADDR(ofs));
	return ret;
}

static void
edb93xx_flash_copy_from(struct map_info *map, void *to, unsigned long from,
			ssize_t len)
{
	while (len) {
		*(short *)to++ = __raw_readw(map->virt + MUNG_ADDR(from));
		from += 2;
		len -= 2;
	}
}

static void
edb93xx_flash_write(struct map_info *map, const map_word d, unsigned long adr)
{
	__raw_writew(d.x[0], map->virt + MUNG_ADDR(adr));
}

static void
edb93xx_flash_copy_to(struct map_info *map, unsigned long to, const void *from,
		      ssize_t len)
{
	while (len) {
		__raw_writew(*(short *)from++, map->virt + MUNG_ADDR(to));
		to += 2;
		len -= 2;
	}
}
#endif

static void
edb93xx_flash_set_vpp(struct map_info *map, int on)
{
	struct edb93xx_flash_info *info = container_of(map, struct edb93xx_flash_info, map);

	if (info->plat && info->plat->set_vpp)
		info->plat->set_vpp(on);
}

static const char *probes[] = { "cmdlinepart", "RedBoot", NULL };

static int
edb93xx_flash_probe(struct platform_device *dev)
{
	/*struct platform_device *dev = to_platform_device(_dev);*/
	struct flash_platform_data *plat = dev->dev.platform_data;
	struct resource *res = dev->resource;
	unsigned int size = res->end - res->start + 1;
	struct edb93xx_flash_info *info;
	int err;
	void __iomem *base;

	info = kmalloc(sizeof(struct edb93xx_flash_info), GFP_KERNEL);
	if (!info) {
		err = -ENOMEM;
		goto out;
	}

	memset(info, 0, sizeof(struct edb93xx_flash_info));

	info->plat = plat;
	if (plat && plat->init) {
		err = plat->init();
		if (err)
			goto no_resource;
	}

	info->res = request_mem_region(res->start, size, "mtd");
	if (!info->res) {
		err = -EBUSY;
		goto no_resource;
	}

	base = ioremap(res->start, size);
	if (!base) {
		err = -ENOMEM;
		goto no_mem;
	}

	/*
	 * look for CFI based flash parts fitted to this board
	 */
	info->map.size		= size;
	info->map.bankwidth	= plat->width;
	info->map.phys		= res->start;
	info->map.virt		= base;
	info->map.name		= dev->dev.bus_id;
	info->map.set_vpp	= edb93xx_flash_set_vpp;


   /*  defined(CONFIG_ARCH_EDB9312) || \  */

#if defined(CONFIG_MTD_COMPLEX_MAPPINGS) && \
    (defined(CONFIG_ARCH_EDB9307) || \
     defined(CONFIG_ARCH_EDB9315))

#ifdef CONFIG_EP93XX_CS0
#define CONFIG inl(SMCBCR0)
#endif
#ifdef CONFIG_EP93XX_CS1
#define CONFIG inl(SMCBCR1)
#endif
#ifdef CONFIG_EP93XX_CS2
#define CONFIG inl(SMCBCR2)
#endif
#ifdef CONFIG_EP93XX_CS3
#define CONFIG inl(SMCBCR3)
#endif
#ifdef CONFIG_EP93XX_CS6
#define CONFIG inl(SMCBCR6)
#endif
#ifdef CONFIG_EP93XX_CS7
#define CONFIG inl(SMCBCR7)
#endif

	if ((CONFIG & SMCBCR_MW_MASK) == SMCBCR_MW_16) {
		info->map.read = edb93xx_flash_read;
		info->map.copy_from = edb93xx_flash_copy_from;
		info->map.write = edb93xx_flash_write;
		info->map.copy_to = edb93xx_flash_copy_to;
	} else
#endif
	simple_map_init(&info->map);

	/*
	 * Also, the CFI layer automatically works out what size
	 * of chips we have, and does the necessary identification
	 * for us automatically.
	 */
	printk("Cirrus Platform start to probe mtd device\n");
	info->mtd = do_map_probe(plat->map_name, &info->map);
	if (!info->mtd) {
		err = -ENXIO;
		goto no_device;
	}

#ifdef CONFIG_MTD_COMPLEX_MAPPINGS
	simple_map_init(&info->map);
#endif

	info->mtd->owner = THIS_MODULE;

	err = parse_mtd_partitions(info->mtd, probes, &info->parts, 0);
	if (err > 0) {
		err = add_mtd_partitions(info->mtd, info->parts, err);
		if (err)
			printk(KERN_ERR
			       "mtd partition registration failed: %d\n", err);
	}

	if (err == 0)
		platform_set_drvdata(dev, info);

	/*
	 * If we got an error, free all resources.
	 */
	if (err < 0) {
		if (info->mtd) {
			del_mtd_partitions(info->mtd);
			map_destroy(info->mtd);
		}
		/*if (info->parts)*/
		kfree(info->parts);

 no_device:
		iounmap(base);
 no_mem:
		release_mem_region(res->start, size);
 no_resource:
		if (plat && plat->exit)
			plat->exit();
		kfree(info);
	}
 out:
	return err;
}

static int
edb93xx_flash_remove(struct platform_device *dev)
{
	/*struct platform_device *dev = to_platform_device(_dev);*/
	struct edb93xx_flash_info *info = platform_get_drvdata(dev);

	platform_set_drvdata(dev, NULL);

	if (info) {
		if (info->mtd) {
			del_mtd_partitions(info->mtd);
			map_destroy(info->mtd);
		}
		/*if (info->parts)*/
		kfree(info->parts);

		iounmap(/*(void *)*/info->map.virt);
		release_resource(info->res);
		kfree(info->res);

		if (info->plat && info->plat->exit)
			info->plat->exit();

		kfree(info);
	}

	return 0;
}

static struct platform_driver edb93xx_flash_driver = {
	/*.bus		= &platform_bus_type,*/
	.probe		= edb93xx_flash_probe,
	.remove		= edb93xx_flash_remove,
	.driver		= {
		.name	= "edb93xxflash",
	},	
};

static int __init
edb93xx_flash_init(void)
{
	return platform_driver_register(&edb93xx_flash_driver);
}

static void __exit
edb93xx_flash_exit(void)
{
	platform_driver_unregister(&edb93xx_flash_driver);
}

module_init(edb93xx_flash_init);
module_exit(edb93xx_flash_exit);

MODULE_AUTHOR("Cirrus Logic, Inc.");
MODULE_DESCRIPTION("EDB93xx CFI map driver");
MODULE_LICENSE("GPL");
