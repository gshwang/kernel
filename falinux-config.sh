#!/bin/sh

pwd=$( pwd )

arch_arm()
{
	if [ -f $(pwd)/Makefile ]; then 
		rm -rf Makefile
	fi

	ln -s Makefile.arm Makefile
	make mrproper
	make distclean
}

arch_mips()
{
	if [ -f $(pwd)/Makefile ]; then 
		rm -rf Makefile
	fi

	ln -s Makefile.mips Makefile
	make mrproper
	make distclean
}

print_help()
{
	echo "falinux support ... you can set to compiling for a first time"
	echo ""
	echo "   ./falinux-config.sh <board>"
	echo ""
	echo "   board : ez-x5, esp-cx, esp-ns                > PXA255  "
	echo "   board : ez-pxa270                            > PXA270  "
	echo "   board : ez-ixp420                            > IXP420  "
	echo "   board : ez-s3c2410                           > S3C2410 "
	echo "   board : ez-s3c2440, esp-gacu, esp-tmcb       > S3C2440 "
	echo "   board : ez-ep9312, esp-mmi                   > EP9312  "
	echo "   board : ez-au1200                            > AU1200, AU1250"
	echo ""
}

if [ "$1" != "" ]; then

	case "$1" in
		ez-x5) 
			arch_arm
			make ezx5_defconfig
			;;
		esp-cx)
			arch_arm
			make espcx_defconfig
			;;
		esp-ns)
			arch_arm
			make espns_defconfig
			;;
		ez-s3c2410)
			arch_arm
			make ezs3c2410_defconfig
			;;
		ez-s3c2440)
			arch_arm
			make ezs3c2440_defconfig
			;;
		esp-gacu)
			arch_arm
			make espgacu_defconfig
			;;
		esp-tmcb)
			arch_arm
			make esptmcb_defconfig
			;;
		ez-pxa270)
			arch_arm
			make ezpxa270_defconfig
			;;
		ez-ixp420)
			arch_arm
			make ezixp420_defconfig
			;;
		ez-ep9312)
			arch_arm
			make ezep9312_defconfig
			;;
		esp-mmi)
			arch_arm
			make espmmi_defconfig
			;;
		ez-au1200)
			arch_mips
			make ezau1200_defconfig
			;;

		*)
			print_help
			;;
	esac
	
else
	print_help
fi
