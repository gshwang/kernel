# arch/arm/mach-s3c2440/Makefile
#
# Copyright 2007 Simtec Electronics
#
# Licensed under GPLv2

obj-y				:=
obj-m				:=
obj-n				:=
obj-				:=

obj-$(CONFIG_CPU_S3C2440)	+= s3c2440.o dsc.o
obj-$(CONFIG_CPU_S3C2440)	+= irq.o
obj-$(CONFIG_CPU_S3C2440)	+= clock.o
obj-$(CONFIG_S3C2440_DMA)	+= dma.o

# Machine support

obj-$(CONFIG_MACH_ANUBIS)	+= mach-anubis.o
obj-$(CONFIG_MACH_OSIRIS)	+= mach-osiris.o
obj-$(CONFIG_MACH_RX3715)	+= mach-rx3715.o
obj-$(CONFIG_ARCH_S3C2440)	+= mach-smdk2440.o
obj-$(CONFIG_MACH_NEXCODER_2440) += mach-nexcoder.o

#obj-$(CONFIG_FALINUX_S3C2440)	+= mach-falinux.o
obj-$(CONFIG_MACH_EZ_S3C2440)	+= falinux-ez-s3c2440.o
obj-$(CONFIG_MACH_EDK_S3C2440)	+= falinux-edk-s3c2440.o
obj-$(CONFIG_MACH_ESP_GACU)	    += falinux-esp-gacu.o
obj-$(CONFIG_MACH_ESP_TMCB)	    += falinux-esp-tmcb.o
