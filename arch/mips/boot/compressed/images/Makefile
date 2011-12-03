# 
# This dir holds all of the images for MIPS machines. 
# Tom Rini      January 2001 
# Pete Popov    2004 
 
extra-y         := vmlinux.bin vmlinux.gz 
 
OBJCOPYFLAGS_vmlinux.bin := -O binary 
$(obj)/vmlinux.bin: vmlinux FORCE 
	$(call if_changed,objcopy) 
 
$(obj)/vmlinux.gz: $(obj)/vmlinux.bin FORCE 
	$(call if_changed,gzip) 
 
# Files generated that shall be removed upon make clean 
clean-files     := vmlinux* zImage* 
