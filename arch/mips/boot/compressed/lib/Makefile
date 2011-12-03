#
# Makefile for some libs needed by zImage.
#

#lib-y := $(addprefix ../../../../../lib/zlib_inflate/, \
#        infblock.o infcodes.o inffast.o inflate.o inftrees.o infutil.o) \
#        $(addprefix ../../../../../lib/, ctype.o string.o) \
#        $(addprefix ../../../../../arch/mips/lib/, memcpy.o) \

lib-y := $(addprefix ../../../../../lib/zlib_inflate/, \
        inffast.o inflate.o inftrees.o inflate_syms.o) \
        $(addprefix ../../../../../lib/, ctype.o string.o) \
        $(addprefix ../../../../../arch/mips/lib/, memcpy.o) \
