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
 *   gpio i2c  ���� ����ü  ( board_setup.c ���� �����ϰ� �����Ѵ�. )
 */
struct falinux_i2c_platform_data  {
	int devcnt;              // ����� ����
	
	int sda_pin;             // sda �� ����� gpio ��ȣ
	int scl_pin;             // scl �� ����� gpio ��ȣ

	int id;                  // i2c-ID  0x10100 �̻��� ����
	int udelay;              // ref = 5
	int timeout;             // ref = 100
	
	void(*bit_setscl)(void *data, int val);
	void(*bit_setsda)(void *data, int val);
	int (*bit_getscl)(void *data);
	int (*bit_getsda)(void *data);
};

/**
 *  i2c platform_data ����ü
 */
struct falinux_i2c_drv_data {
	struct falinux_i2c_platform_data *info;
	struct i2c_adapter adapter;
	struct i2c_algo_bit_data algo_data;
};


#endif

