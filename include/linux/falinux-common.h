/**
 * Copyright (C) 2008  www.falinux.com
 * Author: JaeKyong,Oh  freefrug@falinux.com
 *
 * include/linux/falinux-common.h
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#ifndef _FALINUX_COMMON_H_
#define _FALINUX_COMMON_H_


#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

/**
 *   gpio i2c  정보 구조체  ( board_setup.c 에서 정의하고 구현한다. )
 */
struct falinux_i2c_platform_data  {
	int devcnt;              // 어레이의 개수
	
	int sda_pin;             // sda 로 사용할 gpio 번호
	int scl_pin;             // scl 로 사용할 gpio 번호

	int id;                  // i2c-ID  0x10100 이상을 쓰자
	int udelay;              // ref = 5
	int timeout;             // ref = 100
	
	void(*bit_setscl)(void *data, int val);
	void(*bit_setsda)(void *data, int val);
	int (*bit_getscl)(void *data);
	int (*bit_getsda)(void *data);
};

/**
 *  i2c platform_data 구조체
 */
struct falinux_i2c_drv_data {
	struct falinux_i2c_platform_data *info;
	struct i2c_adapter adapter;
	struct i2c_algo_bit_data algo_data;
};


#endif

