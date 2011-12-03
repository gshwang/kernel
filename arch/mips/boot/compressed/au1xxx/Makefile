# arch/mips/boot/compressed/au1xxx/Makefile
#
# Makefile for AMD Alchemy Semiconductor Au1x based boards.
# All of the boot loader code was derived from the ppc
# boot code.
#
# Copyright 2001,2002 MontaVista Software Inc.
#
# Author: Mark A. Greer
#         mgreer@mvista.com
#
# Copyright 2004 Embedded Alley Solutions, Inc
# Ported and modified for mips 2.6 support by
# Pete Popov <ppopov@embeddedalley.com>
#
# This program is free software; you can redistribute  it and/or modify it
# under  the terms of  the GNU General  Public License as published by the
# Free Software Foundation;  either version 2 of the  License, or (at your
# option) any later version.

boot            := arch/mips/boot
compressed      := $(boot)/compressed
utils           := $(compressed)/utils
lib             := $(compressed)/lib
images          := $(compressed)/images
common          := $(compressed)/common

#########################################################################
# START BOARD SPECIFIC VARIABLES

# These two variables control where the zImage is stored
# in flash and loaded in memory.  It only controls how the srec
# file is generated, the code is the same.
RAM_RUN_ADDR = 0x80D00000

ifdef CONFIG_MIPS_XXS1500
FLASH_LOAD_ADDR = 0xBF000000
else
FLASH_LOAD_ADDR = 0xBFC00000
endif

# These two variables specify the free ram region
# that can be used for temporary malloc area
AVAIL_RAM_START=0x80600000
AVAIL_RAM_END=0x80b00000
#AVAIL_RAM_START=0x81400000
#AVAIL_RAM_END=0x81800000

# This one must match the LOADADDR in arch/mips/Makefile!
LOADADDR=0x80100000

# WARNING WARNING WARNING
# Note that with a LOADADDR of 0x80100000 and AVAIL_RAM_START of
# 0x80500000, the max decompressed kernel size can be 4MB. Else we
# start overwriting ourselve. You can change these vars as needed;
# it would be much better if we just figured everything out on the fly.

# END BOARD SPECIFIC VARIABLES
#########################################################################

OBJECTS := $(obj)/head.o $(common)/misc-common.o $(common)/misc-simple.o \
           $(common)/au1k_uart.o
LIBS := $(lib)/lib.a

ENTRY := $(utils)/entry
OFFSET := $(utils)/offset
SIZE := $(utils)/size
 
LD_ARGS := -T $(compressed)/ld.script -Ttext $(RAM_RUN_ADDR) -Bstatic
 
ifdef CONFIG_CPU_LITTLE_ENDIAN
OBJCOPY_ARGS = -O elf32-tradlittlemips
else
OBJCOPY_ARGS = -O elf32-tradbigmips
endif
 
$(obj)/head.o: $(obj)/head.S $(TOPDIR)/vmlinux
	$(CC) -I $(TOPDIR)/include $(AFLAGS) \
	-DKERNEL_ENTRY=$(shell sh $(ENTRY) $(NM) $(TOPDIR)/vmlinux ) \
	-c -o $@ $<

$(common)/misc-simple.o:
	$(CC) -I $(TOPDIR)/include $(CFLAGS) -DINITRD_OFFSET=0 -DINITRD_SIZE=0 -DZIMAGE_OFFSET=0 \
		-DAVAIL_RAM_START=$(AVAIL_RAM_START) \
		-DAVAIL_RAM_END=$(AVAIL_RAM_END) \
		-DLOADADDR=$(LOADADDR) \
		-DZIMAGE_SIZE=0 -c -o $@ $(common)/misc-simple.c

$(obj)/zvmlinux: $(OBJECTS) $(LIBS) $(srctree)/$(compressed)/ld.script $(images)/vmlinux.gz $(common)/dummy.o
	$(OBJCOPY) \
		--add-section=.image=$(images)/vmlinux.gz \
		--set-section-flags=.image=contents,alloc,load,readonly,data \
		$(common)/dummy.o $(common)/image.o
	$(LD) $(LD_ARGS) -o $@ $(OBJECTS) $(common)/image.o $(LIBS)
	$(OBJCOPY) $(OBJCOPY_ARGS) $@ $@ -R __kcrctab -R __ksymtab_strings \
	-R .comment -R .stab -R .stabstr -R .initrd -R .sysmap

# Here we manipulate the image in order to get it the necessary
# srecord file we need.
zImage: $(obj)/zvmlinux
	mv $(obj)/zvmlinux $(images)/zImage
	$(OBJCOPY) -O srec $(images)/zImage $(images)/zImage.srec
	$(OBJCOPY) -O binary $(images)/zImage $(images)/zImage.bin

zImage.flash: zImage
	( \
	flash=${FLASH_LOAD_ADDR} ; \
	ram=${RAM_RUN_ADDR} ; \
	adjust=$$[ $$flash - $$ram ] ; \
	$(OBJCOPY) -O srec --adjust-vma `printf '0x%08x' $$adjust` \
		$(images)/zImage $(images)/zImage.flash.srec ; \
	)
