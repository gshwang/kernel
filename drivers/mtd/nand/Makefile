#
# linux/drivers/nand/Makefile
#
# $Id: Makefile.common,v 1.15 2004/11/26 12:28:22 dedekind Exp $

obj-$(CONFIG_MTD_NAND)			+= nand.o nand_ecc.o

obj-$(CONFIG_MTD_NAND_IDS)		      += nand_ids.o
obj-$(CONFIG_MTD_NAND_CAFE)		      += cafe_nand.o
obj-$(CONFIG_MTD_NAND_SPIA)		      += spia.o
obj-$(CONFIG_MTD_NAND_AMS_DELTA)	  += ams-delta.o
obj-$(CONFIG_MTD_NAND_TOTO)		      += toto.o
obj-$(CONFIG_MTD_NAND_AUTCPU12)		  += autcpu12.o
obj-$(CONFIG_MTD_NAND_EDB7312)		  += edb7312.o
obj-$(CONFIG_MTD_NAND_EDB93xx)          += ep93xx_nand.o
obj-$(CONFIG_MTD_NAND_AU1550)		  += au1550nd.o
obj-$(CONFIG_MTD_NAND_PPCHAMELEONEVB) += ppchameleonevb.o
obj-$(CONFIG_MTD_NAND_S3C2410)		  += s3c2410.o
obj-$(CONFIG_MTD_NAND_DISKONCHIP)	  += diskonchip.o
obj-$(CONFIG_MTD_NAND_H1900)		  += h1910.o
obj-$(CONFIG_MTD_NAND_RTC_FROM4)	  += rtc_from4.o
obj-$(CONFIG_MTD_NAND_SHARPSL)		  += sharpsl.o
obj-$(CONFIG_MTD_NAND_TS7250)		  += ts7250.o
obj-$(CONFIG_MTD_NAND_NANDSIM)		  += nandsim.o
obj-$(CONFIG_MTD_NAND_CS553X)		  += cs553x_nand.o
obj-$(CONFIG_MTD_NAND_NDFC)		      += ndfc.o
obj-$(CONFIG_MTD_NAND_AT91)		      += at91_nand.o
obj-$(CONFIG_MTD_NAND_BASLER_EXCITE)  += excite_nandflash.o
                                      
obj-$(CONFIG_MTD_NAND_EZ_AU1200)	  += falinux_ezboot_part.o falinux_au1200_nand.o  
obj-$(CONFIG_MTD_LOGIC_NAND_FALINUX)  += falinux_ezboot_part.o falinux_logic_nand.o 

nand-objs := nand_base.o nand_bbt.o
cafe_nand-objs := cafe.o cafe_ecc.o
