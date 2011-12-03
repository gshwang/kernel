/*
 *  drivers/mtd/nand/falinux_ezboot_part.c
 *
 *  Copyright (C) 2007 jaekyoung oh (freefrug@falinux.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>

#define	 SZ_1M	(1024*1024)

//------------------------------------------------------------------------------
// 설명 : 디폴트 파티션 정보
//------------------------------------------------------------------------------
#define EZBOOT_PART_MIN_NR     3
#define EZBOOT_PART_MAX_NR     8
struct mtd_partition ezboot_partition_info[EZBOOT_PART_MAX_NR] = {
	{
    	.name   = "falinux boot/config/logo partition",
		.offset = 0,
		.size   = 2*SZ_1M,
	},
	{
    	.name   = "falinux kernel/ramdisk partition",
		.offset = 2*SZ_1M,
		.size   = 8*SZ_1M,
	},
	{
    	.name   = "falinux yaffs partition",
		.offset = 10*SZ_1M,
		.size   = 54*SZ_1M,
	},
	{
    	.name   = "falinux logging partition",
		.offset = 64*SZ_1M,
		.size   = 0*SZ_1M,
	},
};

int  ezboot_part_nr      = EZBOOT_PART_MIN_NR; // default
int  ezboot_yaffs_offset = 10*SZ_1M;           // default

EXPORT_SYMBOL(ezboot_part_nr);
EXPORT_SYMBOL(ezboot_partition_info);
EXPORT_SYMBOL(ezboot_yaffs_offset);

//------------------------------------------------------------------------------
// 설명 : 커널 커맨드라인 파싱
//------------------------------------------------------------------------------
static void fixup_partition_info( char *cmdline_par )
{
	char *delim_ = ",";
	int  argc;
	char *argv[256];
	char *tok;

    int part_index = 0;
    int offset    = 0;
    int size;
	
	argc       = 0;
	argv[argc] = NULL;

	for (tok = strsep( &cmdline_par, delim_); tok; tok = strsep( &cmdline_par, delim_) ) 
    {
		argv[argc++] = tok;
	}

	if ( argc >  EZBOOT_PART_MAX_NR )  argc = EZBOOT_PART_MAX_NR;
	
	if ( argc >= EZBOOT_PART_MIN_NR )
	{
		for ( part_index = 0 ;  part_index < argc ; part_index++)
		{
			size = simple_strtoul( argv[part_index],NULL,0 );

			ezboot_partition_info[ part_index ].offset = offset * SZ_1M;
			ezboot_partition_info[ part_index ].size   = size   * SZ_1M;
			
			offset += size;
		}
		
		ezboot_part_nr      = argc;
		ezboot_yaffs_offset = ezboot_partition_info[2].offset;
	}
}


static int __init nandpart_setup(char *s)
{
	fixup_partition_info( s );
	return 1;
}

__setup("nandparts=", nandpart_setup);



