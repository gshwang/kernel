/*
 * falinux_wm9712.c  --  SoC audio for FALINUX S3C2440
 *
 * Copyright 2007 Wolfson Microelectronics PLC.
 * Author: Graeme Gregory
 *         graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  Revision history
 *    8th Mar 2007   Initial version.
 *
 *  Modify : tsheaven@falinux.com ( 14th Sep 2007 )
 *			 S3C2440 - CS4202
 *  Modify : tsheaven@falinux.com ( 14th Sep 2007 )
 *			 S3C2440 - WM9712
 *
 */

#include <linux/module.h>
#include <linux/device.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include "../codecs/ac97.h"
#include "s3c24xx-pcm.h"
#include "s3c24xx-ac97.h"

static struct snd_soc_machine falinuxs3c2440;

static struct snd_soc_dai_link falinuxs3c2440_dai[] = {
{
	.name = "AC97",
	.stream_name = "AC97 HiFi",
	.cpu_dai = &s3c2440_ac97_dai[0],
	.codec_dai = &ac97_dai,
},
};

static struct snd_soc_machine falinuxs3c2440 = {
	.name = "FALINUXS3C2440",
	.dai_link = falinuxs3c2440_dai,
	.num_links = ARRAY_SIZE(falinuxs3c2440_dai),
};

static struct snd_soc_device falinuxs3c2440_snd_ac97_devdata = {
	.machine = &falinuxs3c2440,
	.platform = &s3c24xx_soc_platform,
	.codec_dev = &soc_codec_dev_ac97,
};

static struct platform_device *falinuxs3c2440_snd_ac97_device;

static int __init falinuxs3c2440_init(void)
{
	int ret;

	falinuxs3c2440_snd_ac97_device = platform_device_alloc("soc-audio", -1);
	if (!falinuxs3c2440_snd_ac97_device)
		return -ENOMEM;

	platform_set_drvdata(falinuxs3c2440_snd_ac97_device, &falinuxs3c2440_snd_ac97_devdata);
	falinuxs3c2440_snd_ac97_devdata.dev = &falinuxs3c2440_snd_ac97_device->dev;
	ret = platform_device_add(falinuxs3c2440_snd_ac97_device);

	if (ret)
		platform_device_put(falinuxs3c2440_snd_ac97_device);

	return ret;
}

static void __exit falinuxs3c2440_exit(void)
{
	platform_device_unregister(falinuxs3c2440_snd_ac97_device);
}

module_init(falinuxs3c2440_init);
module_exit(falinuxs3c2440_exit);

/* Module information */
MODULE_AUTHOR("Graeme Gregory, graeme.gregory@wolfsonmicro.com, www.wolfsonmicro.com");
MODULE_DESCRIPTION("ALSA SoC WM9712 FALINUX S3C2440");
MODULE_LICENSE("GPL");
