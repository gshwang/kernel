#
# Makefile for the kernel mmc device drivers.
#

#
# Core
#
obj-$(CONFIG_MMC)		+= mmc_core.o

#
# Media drivers
#
obj-$(CONFIG_MMC_BLOCK)		+= mmc_block.o

#
# Host drivers
#
obj-$(CONFIG_MMC_ARMMMCI)	+= mmci.o
obj-$(CONFIG_MMC_PXA)		+= pxamci.o
obj-$(CONFIG_MMC_IMX)		+= imxmmc.o
obj-$(CONFIG_MMC_SDHCI)		+= sdhci.o
obj-$(CONFIG_MMC_WBSD)		+= wbsd.o
obj-$(CONFIG_MMC_AU1X)		+= au1xmmc.o
obj-$(CONFIG_MMC_OMAP)		+= omap.o
obj-$(CONFIG_MMC_AT91)		+= at91_mci.o
obj-$(CONFIG_MMC_TIFM_SD)	+= tifm_sd.o
obj-$(CONFIG_MMC_S3C)		+= s3cmci.o
obj-$(CONFIG_MIPS_EZ_AU1200_MMC) += falinux_au1xmmc.o
obj-$(CONFIG_EZ_S3C2440_MMC) += falinux_s3cmci.o


mmc_core-y := mmc.o mmc_sysfs.o
mmc_core-$(CONFIG_BLOCK) += mmc_queue.o

ifeq ($(CONFIG_MMC_DEBUG),y)
obj-$(CONFIG_MMC)		+= mmc_debug.o
EXTRA_CFLAGS += -DDEBUG
endif
