/*
 * Copyright (C) 2010-2013 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include <linux/motosh.h>

int motosh_load_brightness_table(struct motosh_data *ps_motosh,
		unsigned char *cmdbuff)
{
	int err = -ENOTTY;
	int index = 0;
	cmdbuff[0] = LUX_TABLE_VALUES;
	for (index = 0; index < LIGHTING_TABLE_SIZE; index++) {
		cmdbuff[(2 * index) + 1]
			= ps_motosh->pdata->lux_table[index] >> 8;
		cmdbuff[(2 * index) + 2]
			= ps_motosh->pdata->lux_table[index] & 0xFF;
	}
	err = motosh_i2c_write_no_reset(ps_motosh, cmdbuff,
				(2 * LIGHTING_TABLE_SIZE) + 1);
	if (err)
		return err;

	cmdbuff[0] = BRIGHTNESS_TABLE_VALUES;
	for (index = 0; index < LIGHTING_TABLE_SIZE; index++) {
		cmdbuff[index + 1]
				= ps_motosh->pdata->brightness_table[index];
	}
	err = motosh_i2c_write_no_reset(ps_motosh, cmdbuff,
			LIGHTING_TABLE_SIZE + 1);
	dev_dbg(&ps_motosh->client->dev, "Brightness tables loaded\n");
	return err;
}

void motosh_reset(struct motosh_platform_data *pdata, unsigned char *cmdbuff)
{
	dev_err(&motosh_misc_data->client->dev, "motosh_reset\n");
	msleep(motosh_i2c_retry_delay);
	gpio_set_value(pdata->gpio_reset, 0);
	msleep(MOTOSH_RESET_DELAY);
	gpio_set_value(pdata->gpio_reset, 1);
	msleep(MOTOSH_RESET_DELAY);
	motosh_detect_lowpower_mode(cmdbuff);

	if (!motosh_misc_data->in_reset_and_init) {
		/* sending reset to slpc hal */
		motosh_ms_data_buffer_write(motosh_misc_data, DT_RESET,
			NULL, 0);
	}
}

/* TODO: wleh01, remove this neuter of reset */
#define RESET_BRINGUP 0

int motosh_reset_and_init(void)
{
	struct motosh_platform_data *pdata;

	/* TODO: wleh01, remove this neuter of reset */
#if RESET_BRINGUP
	struct timespec current_time;
	unsigned int i;
#endif
	int err = 0, ret_err = 0;
	int reset_attempts = 0;
	unsigned char *rst_cmdbuff;
	int mutex_locked = 0;

	dev_dbg(&motosh_misc_data->client->dev, "motosh_reset_and_init\n");

	rst_cmdbuff = kmalloc(512, GFP_KERNEL);

	if (rst_cmdbuff == NULL)
		return -ENOMEM;

	wake_lock(&motosh_misc_data->reset_wakelock);

	motosh_misc_data->in_reset_and_init = true;

	pdata = motosh_misc_data->pdata;

	motosh_wake(motosh_misc_data);

	motosh_i2c_retry_delay = 200;

	do {
		motosh_reset(pdata, rst_cmdbuff);
		msleep(motosh_i2c_retry_delay);
		msleep(motosh_i2c_retry_delay);
		msleep(motosh_i2c_retry_delay);

		/* check for sign of life */
		rst_cmdbuff[0] = REV_ID;
		err = motosh_i2c_write_read_no_reset(motosh_misc_data,
						     rst_cmdbuff, 1, 1);
		if (err < 0) {
			dev_err(&motosh_misc_data->client->dev,
					"motosh not responding after reset (%d)",
					reset_attempts);
		} else {
			dev_dbg(&motosh_misc_data->client->dev, "HUB IS ALIVE");
		}

	} while (++reset_attempts < 3 && err < 0);

	if (err < 0)
		ret_err = err;
	else {
		motosh_i2c_retry_delay = 200;

		rst_cmdbuff[0] = ACCEL_UPDATE_RATE;
		rst_cmdbuff[1] = motosh_g_acc_delay;
		err = motosh_i2c_write_no_reset(motosh_misc_data,
						rst_cmdbuff, 2);
		if (err < 0)
			ret_err = err;

		motosh_i2c_retry_delay = 13;

		rst_cmdbuff[0] = MAG_UPDATE_RATE;
		rst_cmdbuff[1] = motosh_g_mag_delay;
		err = motosh_i2c_write_no_reset(motosh_misc_data,
						rst_cmdbuff, 2);
		if (err < 0)
			ret_err = err;

		rst_cmdbuff[0] = GYRO_UPDATE_RATE;
		rst_cmdbuff[1] = motosh_g_gyro_delay;
		err = motosh_i2c_write_no_reset(motosh_misc_data,
						rst_cmdbuff, 2);
		if (err < 0)
			ret_err = err;

/* TODO: wleh01, remove this neuter of reset */
#if RESET_BRINGUP
		rst_cmdbuff[0] = PRESSURE_UPDATE_RATE;
		rst_cmdbuff[1] = motosh_g_baro_delay;
		err = motosh_i2c_write_no_reset(motosh_misc_data,
						rst_cmdbuff, 2);
		if (err < 0)
			ret_err = err;

		rst_cmdbuff[0] = IR_GESTURE_RATE;
		rst_cmdbuff[1] = motosh_g_ir_gesture_delay;
		err = motosh_i2c_write_no_reset(motosh_misc_data,
						rst_cmdbuff, 2);
		if (err < 0)
			ret_err = err;

		rst_cmdbuff[0] = IR_RAW_RATE;
		rst_cmdbuff[1] = motosh_g_ir_raw_delay;
		err = motosh_i2c_write_no_reset(motosh_misc_data,
						rst_cmdbuff, 2);
		if (err < 0)
			ret_err = err;

		rst_cmdbuff[0] = NONWAKESENSOR_CONFIG;
		rst_cmdbuff[1] = motosh_g_nonwake_sensor_state & 0xFF;
		rst_cmdbuff[2] = (motosh_g_nonwake_sensor_state >> 8) & 0xFF;
		rst_cmdbuff[3] = motosh_g_nonwake_sensor_state >> 16;
		err = motosh_i2c_write_no_reset(motosh_misc_data,
						rst_cmdbuff, 4);
		if (err < 0)
			ret_err = err;

		rst_cmdbuff[0] = WAKESENSOR_CONFIG;
		rst_cmdbuff[1] = motosh_g_wake_sensor_state & 0xFF;
		rst_cmdbuff[2] = motosh_g_wake_sensor_state >> 8;
		err = motosh_i2c_write_no_reset(motosh_misc_data,
						rst_cmdbuff, 3);
		if (err < 0)
			ret_err = err;

		rst_cmdbuff[0] = ALGO_CONFIG;
		rst_cmdbuff[1] = motosh_g_algo_state & 0xFF;
		rst_cmdbuff[2] = motosh_g_algo_state >> 8;
		err = motosh_i2c_write_no_reset(motosh_misc_data,
						rst_cmdbuff, 3);
		if (err < 0)
			ret_err = err;

		rst_cmdbuff[0] = MOTION_DUR;
		rst_cmdbuff[1] = motosh_g_motion_dur;
		err = motosh_i2c_write_no_reset(motosh_misc_data,
						rst_cmdbuff, 2);
		if (err < 0)
			ret_err = err;

		rst_cmdbuff[0] = ZRMOTION_DUR;
		rst_cmdbuff[1] = motosh_g_zmotion_dur;
		err = motosh_i2c_write_no_reset(motosh_misc_data,
						rst_cmdbuff, 2);
		if (err < 0)
			ret_err = err;

		for (i = 0; i < MOTOSH_NUM_ALGOS; i++) {
			if (motosh_g_algo_requst[i].size > 0) {
				rst_cmdbuff[0] =
					motosh_algo_info[i].req_register;
				memcpy(&rst_cmdbuff[1],
					   motosh_g_algo_requst[i].data,
					   motosh_g_algo_requst[i].size);
				err = motosh_i2c_write_no_reset(
						motosh_misc_data,
						rst_cmdbuff,
						motosh_g_algo_requst[i].size
								+ 1);
				if (err < 0)
					ret_err = err;
			}
		}

		rst_cmdbuff[0] = PROX_SETTINGS;
		rst_cmdbuff[1]
			= (pdata->ct406_detect_threshold >> 8) & 0xff;
		rst_cmdbuff[2]
			= pdata->ct406_detect_threshold & 0xff;
		rst_cmdbuff[3] = (pdata->ct406_undetect_threshold >> 8) & 0xff;
		rst_cmdbuff[4] = pdata->ct406_undetect_threshold & 0xff;
		rst_cmdbuff[5]
			= (pdata->ct406_recalibrate_threshold >> 8) & 0xff;
		rst_cmdbuff[6] = pdata->ct406_recalibrate_threshold & 0xff;
		rst_cmdbuff[7] = pdata->ct406_pulse_count & 0xff;
		err = motosh_i2c_write_no_reset(motosh_misc_data,
						rst_cmdbuff, 8);
		if (err < 0) {
			dev_err(&motosh_misc_data->client->dev,
				"unable to write proximity settings %d\n", err);
			ret_err = err;
		}

		err = motosh_load_brightness_table(motosh_misc_data,
						   rst_cmdbuff);
		if (err < 0)
			ret_err = err;

		getnstimeofday(&current_time);
		current_time.tv_sec += motosh_time_delta;

		rst_cmdbuff[0] = AP_POSIX_TIME;
		rst_cmdbuff[1] = (unsigned char)(current_time.tv_sec >> 24);
		rst_cmdbuff[2] = (unsigned char)((current_time.tv_sec >> 16)
						 & 0xff);
		rst_cmdbuff[3] = (unsigned char)((current_time.tv_sec >> 8)
						 & 0xff);
		rst_cmdbuff[4] = (unsigned char)((current_time.tv_sec)
						 & 0xff);
		err = motosh_i2c_write_no_reset(motosh_misc_data,
						rst_cmdbuff, 5);
		if (err < 0)
			ret_err = err;

		rst_cmdbuff[0] = MAG_CAL;
		memcpy(&rst_cmdbuff[1], motosh_g_mag_cal,
		       MOTOSH_MAG_CAL_SIZE);
		err = motosh_i2c_write_no_reset(motosh_misc_data, rst_cmdbuff,
						MOTOSH_MAG_CAL_SIZE);
		if (err < 0)
			ret_err = err;

		if (motosh_g_ir_config_reg_restore) {
			rst_cmdbuff[0] = IR_CONFIG;
			memcpy(&rst_cmdbuff[1], motosh_g_ir_config_reg,
				   motosh_g_ir_config_reg[0]);
			err = motosh_i2c_write_no_reset(motosh_misc_data,
						rst_cmdbuff,
						motosh_g_ir_config_reg[0] + 1);
			if (err < 0)
				ret_err = err;
		}

		/* sending reset to slpc hal */
		motosh_ms_data_buffer_write(motosh_misc_data, DT_RESET,
					    NULL, 0);
/* TODO: wleh01, remove this neuter of reset */
#endif
	}
	mutex_locked = mutex_trylock(&motosh_misc_data->lock);
	motosh_quickpeek_reset_locked(motosh_misc_data);
	if (mutex_locked)
		mutex_unlock(&motosh_misc_data->lock);

	kfree(rst_cmdbuff);
	motosh_sleep(motosh_misc_data);
	motosh_misc_data->in_reset_and_init = false;
	wake_unlock(&motosh_misc_data->reset_wakelock);

	return ret_err;
}