#
# arch/mips/boot/compressed/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994 by Linus Torvalds
# Adapted for PowerPC by Gary Thomas
# modified by Cort (cort@cs.nmt.edu)
#
# Ported to MIPS by Pete Popov, ppopov@embeddedalley.com
#

boot            := arch/mips/boot
compressed      := arch/mips/boot/compressed

CFLAGS          += -fno-builtin -D__BOOTER__ -I$(compressed)/include

BOOT_TARGETS    = zImage zImage.flash

bootdir-$(CONFIG_SOC_AU1X00)    := au1xxx
subdir-y                        := common lib images

.PHONY: $(BOOT_TARGETS) $(bootdir-y)

$(BOOT_TARGETS): $(bootdir-y)

$(bootdir-y): $(addprefix $(obj)/,$(subdir-y)) \
              $(addprefix $(obj)/,$(hostprogs-y))
	$(Q)$(MAKE) $(build)=$(obj)/$@ $(MAKECMDGOALS)
