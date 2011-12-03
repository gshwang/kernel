/*
 * --  SoC audio for EZ-PXA270
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
 *		   : gemini@falinux.com
 *		     For PXA270
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
#include "pxa2xx-pcm.h"
#include "pxa2xx-ac97.h"

static struct snd_soc_machine emx7;

static struct snd_soc_dai_link emx7_dai[] = {
{
	.name = "AC97",
	.stream_name = "AC97 HiFi",
	.cpu_dai = &pxa_ac97_dai[0],
	.codec_dai = &ac97_dai,
},
};

static struct snd_soc_machine emx7 = {
	.name = "EZ-PXA270",
	.dai_link = emx7_dai,
	.num_links = ARRAY_SIZE(emx7_dai),
};

static struct snd_soc_device emx7_snd_ac97_devdata = {
	.machine = &emx7,
	.platform = &pxa2xx_soc_platform,
	.codec_dev = &soc_codec_dev_ac97,
};

static struct platform_device *emx7_snd_ac97_device;

static int __init emx7_init(void)
{
	int ret;

	emx7_snd_ac97_device = platform_device_alloc("soc-audio", -1);
	if (!emx7_snd_ac97_device)
		return -ENOMEM;

	platform_set_drvdata(emx7_snd_ac97_device, &emx7_snd_ac97_devdata);
	emx7_snd_ac97_devdata.dev = &emx7_snd_ac97_device->dev;
	ret = platform_device_add(emx7_snd_ac97_device);

	if (ret)
		platform_device_put(emx7_snd_ac97_device);

	return ret;
}

static void __exit emx7_exit(void)
{
	platform_device_unregister(emx7_snd_ac97_device);
}

module_init(emx7_init);
module_exit(emx7_exit);

/* Module information */
MODULE_AUTHOR("Graeme Gregory, graeme.gregory@wolfsonmicro.com, www.wolfsonmicro.com");
MODULE_DESCRIPTION("ALSA SoC EZ-PXA270(FALinux)");
MODULE_LICENSE("GPL");
