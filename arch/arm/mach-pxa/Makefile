#
# Makefile for the linux kernel.
#

# Common support (must be linked before board specific support)
obj-y += clock.o generic.o irq.o dma.o time.o
obj-$(CONFIG_PXA25x) += pxa25x.o
obj-$(CONFIG_PXA27x) += pxa27x.o

# Specific board support
obj-$(CONFIG_ARCH_LUBBOCK) += lubbock.o
obj-$(CONFIG_MACH_LOGICPD_PXA270) += lpd270.o
obj-$(CONFIG_MACH_MAINSTONE) += mainstone.o
obj-$(CONFIG_ARCH_PXA_IDP) += idp.o
obj-$(CONFIG_MACH_EZ_X5) += falinux_ez-x5.o
obj-$(CONFIG_MACH_EZ_PXA270) += falinux_ez-pxa270.o
obj-$(CONFIG_MACH_EZ_PXA255) += falinux_ez-pxa255.o
obj-$(CONFIG_MACH_ESP_CX) += falinux_esp-cx.o
obj-$(CONFIG_MACH_ESP_NS) += falinux_esp-ns.o
obj-$(CONFIG_MACH_TRIZEPS4)	+= trizeps4.o
obj-$(CONFIG_PXA_SHARP_C7xx)	+= corgi.o corgi_ssp.o corgi_lcd.o sharpsl_pm.o corgi_pm.o
obj-$(CONFIG_PXA_SHARP_Cxx00)	+= spitz.o corgi_ssp.o corgi_lcd.o sharpsl_pm.o spitz_pm.o
obj-$(CONFIG_MACH_AKITA)	+= akita-ioexp.o
obj-$(CONFIG_MACH_POODLE)	+= poodle.o corgi_ssp.o
obj-$(CONFIG_MACH_TOSA)         += tosa.o

# Support for blinky lights
led-y := leds.o
led-$(CONFIG_ARCH_LUBBOCK) += leds-lubbock.o
led-$(CONFIG_MACH_MAINSTONE) += leds-mainstone.o
led-$(CONFIG_ARCH_PXA_IDP) += leds-idp.o
led-$(CONFIG_MACH_TRIZEPS4) += leds-trizeps4.o

obj-$(CONFIG_LEDS) += $(led-y)

# Misc features
obj-$(CONFIG_PM) += pm.o sleep.o
obj-$(CONFIG_PXA_SSP) += ssp.o

ifeq ($(CONFIG_PXA27x),y)
obj-$(CONFIG_PM) += standby.o
endif
