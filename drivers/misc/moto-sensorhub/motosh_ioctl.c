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
#include <linux/gfp.h>
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

long motosh_misc_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	static int brightness_table_loaded;
	static int lowpower_mode = 1;
	int err = 0;
	unsigned int addr = 0;
	unsigned int data_size = 0;
	unsigned char rw_bytes[4];
	struct motosh_data *ps_motosh = file->private_data;
	unsigned char byte;
	unsigned char bytes[3];
	unsigned short delay;
	unsigned long current_posix_time;
	unsigned int handle;
	struct timespec current_time;

	if (mutex_lock_interruptible(&ps_motosh->lock) != 0)
		return -EINTR;

	motosh_wake(ps_motosh);

	switch (cmd) {
	case MOTOSH_IOCTL_BOOTLOADERMODE:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_BOOTLOADERMODE");
		err = switch_motosh_mode(BOOTMODE);
		break;
	case MOTOSH_IOCTL_NORMALMODE:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_NORMALMODE");
		err = switch_motosh_mode(NORMALMODE);
		break;
	case MOTOSH_IOCTL_MASSERASE:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_MASSERASE");
		err = motosh_boot_flash_erase();
		break;
	case MOTOSH_IOCTL_SETSTARTADDR:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SETSTARTADDR");
		if (copy_from_user(&addr, argp, sizeof(addr))) {
			dev_err(&ps_motosh->client->dev,
				"Copy start address returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_misc_data->current_addr = addr;
		err = 0;
		break;
	case MOTOSH_IOCTL_SET_FACTORY_MODE:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_FACTORY_MODE");
		err = switch_motosh_mode(FACTORYMODE);
		break;
	case MOTOSH_IOCTL_TEST_BOOTMODE:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_TEST_BOOTMODE");
		err = switch_motosh_mode(BOOTMODE);
		break;
	case MOTOSH_IOCTL_SET_DEBUG:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_DEBUG");
		err = 0;
		break;
	case MOTOSH_IOCTL_GET_VERNAME:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_VERNAME");
		if (copy_to_user(argp, &(ps_motosh->pdata->fw_version),
				FW_VERSION_SIZE))
			err = -EFAULT;
		else
			err = 0;
		break;
	case MOTOSH_IOCTL_GET_BOOTED:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_BOOTED");
		byte = motosh_g_booted;
		if (copy_to_user(argp, &byte, 1))
			err = -EFAULT;
		else
			err = 0;
		break;
	case MOTOSH_IOCTL_GET_VERSION:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_VERSION");
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_get_version(ps_motosh);
		else
			err = -EBUSY;
		break;
	case MOTOSH_IOCTL_SET_ACC_DELAY:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_ACC_DELAY");
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy acc delay returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_cmdbuff[0] = ACCEL_UPDATE_RATE;
		motosh_cmdbuff[1] = delay;
		motosh_g_acc_delay = delay;
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, motosh_cmdbuff, 2);
		break;

	case MOTOSH_IOCTL_SET_MAG_DELAY:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_MAG_DELAY");
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy mag delay returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_cmdbuff[0] = MAG_UPDATE_RATE;
		motosh_cmdbuff[1] = delay;
		motosh_g_mag_delay = delay;
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, motosh_cmdbuff, 2);
		break;
	case MOTOSH_IOCTL_SET_GYRO_DELAY:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_GYRO_DELAY");
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy gyro delay returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_cmdbuff[0] = GYRO_UPDATE_RATE;
		motosh_cmdbuff[1] = delay;
		motosh_g_gyro_delay = delay;
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, motosh_cmdbuff, 2);
		break;
	case MOTOSH_IOCTL_SET_STEP_COUNTER_DELAY:
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy step counter delay returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_cmdbuff[0] = STEP_COUNTER_UPDATE_RATE;
		motosh_cmdbuff[1] = (delay>>8);
		motosh_cmdbuff[2] = delay;
		motosh_g_step_counter_delay = delay;
		err = motosh_i2c_write(ps_motosh, motosh_cmdbuff, 3);
		break;
	case MOTOSH_IOCTL_SET_PRES_DELAY:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_PRES_DELAY");
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy pres delay returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_cmdbuff[0] = PRESSURE_UPDATE_RATE;
		motosh_cmdbuff[1] = delay;
		motosh_g_baro_delay = delay;
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, motosh_cmdbuff, 2);
		break;
	case MOTOSH_IOCTL_SET_SENSORS:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_SENSORS");
		if (copy_from_user(bytes, argp, 3 * sizeof(unsigned char))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy set sensors returned error\n");
			err = -EFAULT;
			break;
		}

		if ((brightness_table_loaded == 0)
				&& (bytes[1] & (M_DISP_BRIGHTNESS >> 8))) {
			err = motosh_load_brightness_table(ps_motosh,
					motosh_cmdbuff);
			if (err) {
				dev_err(&ps_motosh->client->dev,
					"Loading brightness failed\n");
				break;
			}
			brightness_table_loaded = 1;
		}

		motosh_cmdbuff[0] = NONWAKESENSOR_CONFIG;
		motosh_cmdbuff[1] = bytes[0];
		motosh_cmdbuff[2] = bytes[1];
		motosh_cmdbuff[3] = bytes[2];
		motosh_g_nonwake_sensor_state = (motosh_cmdbuff[3] << 16)
			| (motosh_cmdbuff[2] << 8) | motosh_cmdbuff[1];
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, motosh_cmdbuff, 4);
		dev_dbg(&ps_motosh->client->dev, "Sensor enable = 0x%lx\n",
			motosh_g_nonwake_sensor_state);
		break;
	case MOTOSH_IOCTL_GET_SENSORS:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_SENSORS");
		if (ps_motosh->mode > BOOTMODE) {
			motosh_cmdbuff[0] = NONWAKESENSOR_CONFIG;
			err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff,
				1, 3);
			if (err < 0) {
				dev_err(&ps_motosh->client->dev,
					"Reading get sensors failed\n");
				break;
			}
			bytes[0] = motosh_readbuff[0];
			bytes[1] = motosh_readbuff[1];
			bytes[2] = motosh_readbuff[2];
		} else {
			bytes[0] = motosh_g_nonwake_sensor_state & 0xFF;
			bytes[1] = (motosh_g_nonwake_sensor_state >> 8) & 0xFF;
			bytes[2] = (motosh_g_nonwake_sensor_state >> 16) & 0xFF;
		}
		if (copy_to_user(argp, bytes, 3 * sizeof(unsigned char)))
			err = -EFAULT;
		break;
	case MOTOSH_IOCTL_SET_WAKESENSORS:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_WAKESENSORS");
		if (copy_from_user(bytes, argp, 2 * sizeof(unsigned char))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy set sensors returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_cmdbuff[0] = WAKESENSOR_CONFIG;
		motosh_cmdbuff[1] = bytes[0];
		motosh_cmdbuff[2] = bytes[1];
		motosh_g_wake_sensor_state =  (motosh_cmdbuff[2] << 8)
			| motosh_cmdbuff[1];
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, motosh_cmdbuff, 3);
		dev_dbg(&ps_motosh->client->dev, "Sensor enable = 0x%02X\n",
			motosh_g_wake_sensor_state);
		break;
	case MOTOSH_IOCTL_GET_WAKESENSORS:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_GET_WAKESENSORS");
		if (ps_motosh->mode > BOOTMODE) {
			motosh_cmdbuff[0] = WAKESENSOR_CONFIG;
			err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff,
				1, 2);
			if (err < 0) {
				dev_err(&ps_motosh->client->dev,
					"Reading get sensors failed\n");
				break;
			}
			bytes[0] = motosh_readbuff[0];
			bytes[1] = motosh_readbuff[1];
		} else {
			bytes[0] = motosh_g_wake_sensor_state & 0xFF;
			bytes[1] = (motosh_g_wake_sensor_state >> 8) & 0xFF;
		}
		if (copy_to_user(argp, bytes, 2 * sizeof(unsigned char)))
			err = -EFAULT;
		break;
	case MOTOSH_IOCTL_SET_ALGOS:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_ALGOS");
		if (copy_from_user(&bytes, argp, 2 * sizeof(unsigned char))) {
			dev_err(&ps_motosh->client->dev,
				"Copy set algos returned error\n");
			err = -EFAULT;
			break;
		}
		dev_dbg(&ps_motosh->client->dev,
			"Set algos config: 0x%x", (bytes[1] << 8) | bytes[0]);
		motosh_cmdbuff[0] = ALGO_CONFIG;
		motosh_cmdbuff[1] = bytes[0];
		motosh_cmdbuff[2] = bytes[1];
		motosh_g_algo_state = (motosh_cmdbuff[2] << 8)
			| motosh_cmdbuff[1];
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, motosh_cmdbuff, 3);
		break;
	case MOTOSH_IOCTL_GET_ALGOS:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_ALGOS");
		if (ps_motosh->mode > BOOTMODE) {
			motosh_cmdbuff[0] = ALGO_CONFIG;
			err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff,
				1, 2);
			if (err < 0) {
				dev_err(&ps_motosh->client->dev,
					"Reading get algos failed\n");
				break;
			}
			bytes[0] = motosh_readbuff[0];
			bytes[1] = motosh_readbuff[1];
		} else {
			bytes[0] = motosh_g_algo_state & 0xFF;
			bytes[1] = (motosh_g_algo_state >> 8) & 0xFF;
		}
		dev_info(&ps_motosh->client->dev,
			"Get algos config: 0x%x", (bytes[1] << 8) | bytes[0]);
		if (copy_to_user(argp, bytes, 2 * sizeof(unsigned char)))
			err = -EFAULT;
		break;
	case MOTOSH_IOCTL_SET_MAG_CAL:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_MAG_CAL");
		if (copy_from_user(&motosh_cmdbuff[1], argp,
			MOTOSH_MAG_CAL_SIZE)) {
			dev_err(&ps_motosh->client->dev,
				"Copy set mag cal returned error\n");
			err = -EFAULT;
			break;
		}
		memcpy(motosh_g_mag_cal, &motosh_cmdbuff[1],
			MOTOSH_MAG_CAL_SIZE);
		motosh_cmdbuff[0] = MAG_CAL;
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, motosh_cmdbuff,
				(MOTOSH_MAG_CAL_SIZE + 1));
		break;
	case MOTOSH_IOCTL_GET_MAG_CAL:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_MAG_CAL");
		if (ps_motosh->mode > BOOTMODE) {
			motosh_cmdbuff[0] = MAG_CAL;
			err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff,
				1, MOTOSH_MAG_CAL_SIZE);
			if (err < 0) {
				dev_err(&ps_motosh->client->dev,
					"Reading get mag cal failed\n");
				break;
			}
		} else {
			memcpy(&motosh_readbuff[0], motosh_g_mag_cal,
				MOTOSH_MAG_CAL_SIZE);
		}
		if (copy_to_user(argp, &motosh_readbuff[0],
				MOTOSH_MAG_CAL_SIZE))
			err = -EFAULT;
		break;
	case MOTOSH_IOCTL_SET_MOTION_DUR:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_MOTION_DUR");
		if (copy_from_user(&addr, argp, sizeof(addr))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy set motion dur returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_cmdbuff[0] = MOTION_DUR;
		motosh_cmdbuff[1] = addr & 0xFF;
		motosh_g_motion_dur =  motosh_cmdbuff[1];
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, motosh_cmdbuff, 2);
		break;
	case MOTOSH_IOCTL_SET_ZRMOTION_DUR:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_ZRMOTION_DUR");
		if (copy_from_user(&addr, argp, sizeof(addr))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy zmotion dur returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_cmdbuff[0] = ZRMOTION_DUR;
		motosh_cmdbuff[1] = addr & 0xFF;
		motosh_g_zmotion_dur =  motosh_cmdbuff[1];
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, motosh_cmdbuff, 2);
		break;
	case MOTOSH_IOCTL_GET_DOCK_STATUS:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_GET_DOCK_STATUS");
		if (ps_motosh->mode > BOOTMODE) {
			err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff,
				1, 1);
			byte = motosh_readbuff[0];
		} else
			byte = 0;
		if (copy_to_user(argp, &byte, sizeof(byte)))
			err = -EFAULT;
		break;
	case MOTOSH_IOCTL_TEST_READ:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_TEST_READ");
		if (ps_motosh->mode > BOOTMODE) {
			err = motosh_i2c_read(ps_motosh, &byte, 1);
			/* motosh will return num of bytes read or error */
			if (err > 0)
				err = byte;
		}
		break;
	case MOTOSH_IOCTL_TEST_WRITE:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_TEST_WRITE");
		if (ps_motosh->mode == BOOTMODE)
			break;
		if (copy_from_user(&byte, argp, sizeof(unsigned char))) {
			dev_err(&ps_motosh->client->dev,
				"Copy test write returned error\n");
			err = -EFAULT;
			break;
		}
		err = motosh_i2c_write(ps_motosh, &byte, 1);
		break;
	case MOTOSH_IOCTL_SET_POSIX_TIME:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_POSIX_TIME");
		if (ps_motosh->mode == BOOTMODE)
			break;
		if (copy_from_user(&current_posix_time, argp,
			 sizeof(current_posix_time))) {
			dev_err(&ps_motosh->client->dev,
				"Copy from user returned error\n");
			err = -EFAULT;
			break;
		}
		getnstimeofday(&current_time);
		motosh_time_delta = current_posix_time - current_time.tv_sec;
		motosh_cmdbuff[0] = AP_POSIX_TIME;
		motosh_cmdbuff[1] = (unsigned char)(current_posix_time >> 24);
		motosh_cmdbuff[2] = (unsigned char)((current_posix_time >> 16)
				& 0xff);
		motosh_cmdbuff[3] = (unsigned char)((current_posix_time >> 8)
				& 0xff);
		motosh_cmdbuff[4] = (unsigned char)((current_posix_time)
			& 0xff);
		err = motosh_i2c_write(ps_motosh, motosh_cmdbuff, 5);
		break;
	case MOTOSH_IOCTL_SET_ALGO_REQ:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_ALGO_REQ");
		/* copy algo into bytes[2] */
		if (copy_from_user(&bytes, argp, 2 * sizeof(unsigned char))) {
			dev_err(&ps_motosh->client->dev,
				"Set algo req copy bytes returned error\n");
			err = -EFAULT;
			break;
		}
		addr = (bytes[1] << 8) | bytes[0];
		/* copy len into byte */
		if (copy_from_user(&byte, argp + 2 * sizeof(unsigned char),
				sizeof(byte))) {
			dev_err(&ps_motosh->client->dev,
				"Set algo req copy byte returned error\n");
			err = -EFAULT;
			break;
		}
		/* algo req register */
		dev_dbg(&ps_motosh->client->dev,
			"Set algo req, algo idx: %d, len: %u\n", addr, byte);
		if (addr < MOTOSH_NUM_ALGOS) {
			motosh_cmdbuff[0] = motosh_algo_info[addr].req_register;
			dev_dbg(&ps_motosh->client->dev,
				"Register: 0x%x", motosh_cmdbuff[0]);
		} else {
			dev_err(&ps_motosh->client->dev,
				"Set algo req invalid arg\n");
			err = -EFAULT;
			break;
		}
		if (byte > ALGO_RQST_DATA_SIZE) {
			dev_err(&ps_motosh->client->dev,
				"Set algo req invalid size arg\n");
			err = -EFAULT;
			break;
		}
		if (copy_from_user(&motosh_cmdbuff[1],
			argp + 2 * sizeof(unsigned char)
			+ sizeof(byte), byte)) {
			dev_err(&ps_motosh->client->dev,
				"Set algo req copy req info returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_g_algo_requst[addr].size = byte;
		memcpy(motosh_g_algo_requst[addr].data,
			&motosh_cmdbuff[1], byte);
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, motosh_cmdbuff,
				1 + byte);
		break;
	case MOTOSH_IOCTL_GET_ALGO_EVT:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_ALGO_EVT");
		if (ps_motosh->mode == BOOTMODE) {
			err = -EFAULT;
			break;
		}
		/* copy algo into bytes[2] */
		if (copy_from_user(&bytes, argp, 2 * sizeof(unsigned char))) {
			dev_err(&ps_motosh->client->dev,
				"Get algo evt copy bytes returned error\n");
			err = -EFAULT;
			break;
		}
		addr = (bytes[1] << 8) | bytes[0];
		/* algo evt register */
		dev_dbg(&ps_motosh->client->dev,
			"Get algo evt, algo idx: %d\n", addr);
		if (addr < MOTOSH_NUM_ALGOS) {
			motosh_cmdbuff[0] = motosh_algo_info[addr].evt_register;
			dev_dbg(&ps_motosh->client->dev,
				"Register: 0x%x", motosh_cmdbuff[0]);
		} else {
			dev_err(&ps_motosh->client->dev,
				"Get algo evt invalid arg\n");
			err = -EFAULT;
			break;
		}
		err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff, 1,
			motosh_algo_info[addr].evt_size);
		if (err < 0) {
			dev_err(&ps_motosh->client->dev,
				"Get algo evt failed\n");
			break;
		}
		if (copy_to_user(argp + 2 * sizeof(unsigned char),
			motosh_readbuff, motosh_algo_info[addr].evt_size))
			err = -EFAULT;
		break;
	case MOTOSH_IOCTL_WRITE_REG:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_WRITE_REG");
		if (ps_motosh->mode == BOOTMODE) {
			err = -EFAULT;
			break;
		}
		/* copy addr and size */
		if (copy_from_user(&rw_bytes, argp, sizeof(rw_bytes))) {
			dev_err(&ps_motosh->client->dev,
				"Write Reg, copy bytes returned error\n");
			err = -EFAULT;
			break;
		}
		addr = (rw_bytes[0] << 8) | rw_bytes[1];
		data_size = (rw_bytes[2] << 8) | rw_bytes[3];

		/* fail if the write size is too large */
		if (data_size > 512 - 1) {
			err = -EFAULT;
			dev_err(&ps_motosh->client->dev,
				"Write Reg, data_size > %d\n",
				512 - 1);
			break;
		}

		/* copy in the data */
		if (copy_from_user(&motosh_cmdbuff[1], argp +
			sizeof(rw_bytes), data_size)) {
			dev_err(&ps_motosh->client->dev,
				"Write Reg copy from user returned error\n");
			err = -EFAULT;
			break;
		}

		/* setup the address */
		motosh_cmdbuff[0] = addr;

		/* + 1 for the address in [0] */
		err = motosh_i2c_write(ps_motosh, motosh_cmdbuff,
			data_size + 1);

		if (err < 0)
			dev_err(&motosh_misc_data->client->dev,
				"Write Reg unable to write to direct reg %d\n",
				err);
		break;
	case MOTOSH_IOCTL_READ_REG:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_READ_REG");
		if (ps_motosh->mode == BOOTMODE) {
			err = -EFAULT;
			break;
		}
		/* copy addr and size */
		if (copy_from_user(&rw_bytes, argp, sizeof(rw_bytes))) {
			dev_err(&ps_motosh->client->dev,
			    "Read Reg, copy bytes returned error\n");
			err = -EFAULT;
			break;
		}
		addr = (rw_bytes[0] << 8) | rw_bytes[1];
		data_size = (rw_bytes[2] << 8) | rw_bytes[3];

		if (data_size > READ_CMDBUFF_SIZE) {
			dev_err(&ps_motosh->client->dev,
				"Read Reg error, size too large\n");
			err = -EFAULT;
			break;
		}

		/* setup the address */
		motosh_cmdbuff[0] = addr;
		err = motosh_i2c_write_read(ps_motosh,
			motosh_cmdbuff, 1, data_size);

		if (err < 0) {
			dev_err(&motosh_misc_data->client->dev,
				"Read Reg, unable to read from direct reg %d\n",
				err);
			break;
		}

		if (copy_to_user(argp, motosh_readbuff, data_size)) {
			dev_err(&ps_motosh->client->dev,
				"Read Reg error copying to user\n");
			err = -EFAULT;
			break;
		}
		break;
	case MOTOSH_IOCTL_SET_IR_CONFIG:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_IR_CONFIG");
		motosh_cmdbuff[0] = IR_CONFIG;
		if (copy_from_user(&motosh_cmdbuff[1], argp, 1)) {
			dev_err(&ps_motosh->client->dev,
				"Copy size from user returned error\n");
			err = -EFAULT;
			break;
		}
		if (motosh_cmdbuff[1] > sizeof(motosh_g_ir_config_reg)) {
			dev_err(&ps_motosh->client->dev,
				"IR Config too big: %d > %zu\n",
				motosh_cmdbuff[1],
				sizeof(motosh_g_ir_config_reg));
			err = -EINVAL;
			break;
		}
		if (copy_from_user(&motosh_cmdbuff[2], argp + 1,
					motosh_cmdbuff[1] - 1)) {
			dev_err(&ps_motosh->client->dev,
				"Copy data from user returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_g_ir_config_reg_restore = 1;
		memcpy(motosh_g_ir_config_reg, &motosh_cmdbuff[1],
			motosh_cmdbuff[1]);

		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, motosh_cmdbuff,
					motosh_cmdbuff[1] + 1);
		dev_dbg(&motosh_misc_data->client->dev,
			"SET_IR_CONFIG: Writing %d bytes (err=%d)\n",
			motosh_cmdbuff[1] + 1, err);
		if (err < 0)
			dev_err(&motosh_misc_data->client->dev,
				"Unable to write IR config reg %d\n", err);
		break;
	case MOTOSH_IOCTL_GET_IR_CONFIG:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_GET_IR_CONFIG");
		if (copy_from_user(&byte, argp, 1)) {
			dev_err(&ps_motosh->client->dev,
				"Copy size from user returned error\n");
			err = -EFAULT;
			break;
		}
		if (byte > sizeof(motosh_g_ir_config_reg)) {
			dev_err(&ps_motosh->client->dev,
				"IR Config too big: %d > %d\n", byte,
				(int)sizeof(motosh_g_ir_config_reg));
			err = -EINVAL;
			break;
		}

		if (ps_motosh->mode > BOOTMODE) {
			motosh_cmdbuff[0] = IR_CONFIG;
			err = motosh_i2c_write_read(ps_motosh, motosh_cmdbuff,
				1, byte);
			if (err < 0) {
				dev_err(&ps_motosh->client->dev,
					"Get IR config failed: %d\n", err);
				break;
			}
		} else {
			memcpy(motosh_readbuff, motosh_g_ir_config_reg, byte);
		}
		if (copy_to_user(argp, motosh_readbuff, byte))
			err = -EFAULT;

		break;
	case MOTOSH_IOCTL_SET_IR_GESTURE_DELAY:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_IR_GESTURE_DELAY");
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy IR gesture delay returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_cmdbuff[0] = IR_GESTURE_RATE;
		motosh_cmdbuff[1] = delay;
		motosh_g_ir_gesture_delay = delay;
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, motosh_cmdbuff, 2);
		break;
	case MOTOSH_IOCTL_SET_IR_RAW_DELAY:
		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH_IOCTL_SET_IR_RAW_DELAY");
		delay = 0;
		if (copy_from_user(&delay, argp, sizeof(delay))) {
			dev_dbg(&ps_motosh->client->dev,
				"Copy IR raw delay returned error\n");
			err = -EFAULT;
			break;
		}
		motosh_cmdbuff[0] = IR_RAW_RATE;
		motosh_cmdbuff[1] = delay;
		motosh_g_ir_raw_delay = delay;
		if (ps_motosh->mode > BOOTMODE)
			err = motosh_i2c_write(ps_motosh, motosh_cmdbuff, 2);
		break;
	case MOTOSH_IOCTL_ENABLE_BREATHING:
		if (ps_motosh->mode == BOOTMODE) {
			err = -EBUSY;
			break;
		}
		if (copy_from_user(&byte, argp, sizeof(byte))) {
			dev_err(&ps_motosh->client->dev,
				"Enable Breathing, copy byte returned error\n");
			err = -EFAULT;
			break;
		}

		if (byte)
			motosh_vote_aod_enabled_locked(ps_motosh,
				AOD_QP_ENABLED_VOTE_USER, true);
		else
			motosh_vote_aod_enabled_locked(ps_motosh,
				AOD_QP_ENABLED_VOTE_USER, false);
		motosh_resolve_aod_enabled_locked(ps_motosh);
		/* the user's vote can not fail */
		err = 0;
		break;
	case MOTOSH_IOCTL_SET_LOWPOWER_MODE:
		if (ps_motosh->mode == BOOTMODE) {
			err = -EBUSY;
			break;
		}
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_LOWPOWER_MODE");
		if (copy_from_user(&motosh_cmdbuff[0], argp, 1)) {
			dev_err(&ps_motosh->client->dev,
				"Copy size from user returned error\n");
			err = -EFAULT;
			break;
		}

		err = 0;
		if (motosh_cmdbuff[0] != 0 && lowpower_mode == 0) {
			/* allow sensorhub to sleep */
			motosh_sleep(ps_motosh);
			lowpower_mode = motosh_cmdbuff[0];
		} else if (motosh_cmdbuff[0] == 0 && lowpower_mode == 1) {
			/* keep sensorhub awake */
			motosh_wake(ps_motosh);
			lowpower_mode = motosh_cmdbuff[0];
		}
		break;
	case MOTOSH_IOCTL_SET_FLUSH:
		dev_dbg(&ps_motosh->client->dev, "MOTOSH_IOCTL_SET_FLUSH");
		if (ps_motosh->mode == BOOTMODE)
			break;
		if (copy_from_user(&handle, argp, sizeof(unsigned int))) {
			dev_err(&ps_motosh->client->dev,
				"Copy flush handle returned error\n");
			err = -EFAULT;
			break;
		}
		handle = cpu_to_be32(handle);
		motosh_as_data_buffer_write(ps_motosh, DT_FLUSH,
				(char *)&handle, 4, 0);
		break;
	}

	motosh_sleep(ps_motosh);
	mutex_unlock(&ps_motosh->lock);
	return err;
}