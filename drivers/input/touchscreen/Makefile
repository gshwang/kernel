#
# Makefile for the mouse drivers.
#

# Each configuration option enables a list of files.

wm97xx-ts-y := wm97xx-core.o

obj-$(CONFIG_TOUCHSCREEN_ADS7846)	+= ads7846.o
obj-$(CONFIG_TOUCHSCREEN_BITSY)	+= h3600_ts_input.o
obj-$(CONFIG_TOUCHSCREEN_CORGI)	+= corgi_ts.o
obj-$(CONFIG_TOUCHSCREEN_GUNZE)	+= gunze.o
obj-$(CONFIG_TOUCHSCREEN_ELO)	+= elo.o
obj-$(CONFIG_TOUCHSCREEN_MTOUCH) += mtouch.o
obj-$(CONFIG_TOUCHSCREEN_MK712)	+= mk712.o
obj-$(CONFIG_TOUCHSCREEN_HP600)	+= hp680_ts_input.o
obj-$(CONFIG_TOUCHSCREEN_PENMOUNT)	+= penmount.o
obj-$(CONFIG_TOUCHSCREEN_TOUCHRIGHT)	+= touchright.o
obj-$(CONFIG_TOUCHSCREEN_TOUCHWIN)	+= touchwin.o
obj-$(CONFIG_TOUCHSCREEN_UCB1400)	+= ucb1400_ts.o
obj-$(CONFIG_TOUCHSCREEN_S3C2410)   += s3c2410_ts.o
obj-$(CONFIG_TOUCHSCREEN_EP93XX) += ep93xx_ts.o
#obj-$(CONFIG_TOUCHSCREEN_FALINUX_WM9712) += pxa-wm97xx_ts.o wm97xx-core.o
obj-$(CONFIG_TOUCHSCREEN_FALINUX_WM9712) += falinux_wm9712_ts.o
obj-$(CONFIG_TOUCHSCREEN_WM97XX)        += wm97xx-ts.o
wm97xx-ts-$(CONFIG_TOUCHSCREEN_WM9705)  += wm9705.o
wm97xx-ts-$(CONFIG_TOUCHSCREEN_WM9712)  += wm9712.o
wm97xx-ts-$(CONFIG_TOUCHSCREEN_WM9713)  += wm9713.o
obj-$(CONFIG_TOUCHSCREEN_WM97XX_MAINSTONE) += mainstone-wm97xx.o


