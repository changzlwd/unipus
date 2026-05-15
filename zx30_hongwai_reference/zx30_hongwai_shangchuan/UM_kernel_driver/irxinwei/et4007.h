/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#ifndef _ET4007_H_
#define _ET4007_H_

//#define MAX_SIZE 1024
//#define MAX_INDEX 64
//#define MAX_SAMPLE 16
//#define MAX_SAMPLE_INDEX 32
//#define MAX_DATA 310
//#define MAX_ORIGINAL_DATA 620
//#define MAX_SEND_DATA 380
//#define MAX_CONSUMERIR_COUNT 8192
//#define COMPARE_OFFSET 2

struct ir_remocon_data {
	struct mutex			mutex;
	//char consumerir[MAX_CONSUMERIR_COUNT];
	uint32_t consumer_size;

	uint8_t send_flag;
};

struct remote_data {
	uint16_t high_level;
	uint16_t low_level;
};

struct remote_desc {
	int gpio;
	char *name;
	struct timer_list timer;
};
void gpio_init(void);
void gpio_learn(void);
void gpio_open(void);
void gpio_close(void);
void gpio_start(void);
void gpio_stop(void);
void gpio_send_ack(void);
int gpio_check_ack(void);
int gpio_write_data(char data);
int gpio_read_data(char* data);
//for dat pin
void gpio_dat_input(void);
void gpio_dat_output(int value);
void gpio_dat_low(void);
void gpio_dat_hight(void);
int gpio_dat_status(void);
//for clk pin
void gpio_clk_input(void);
void gpio_clk_output(int value);
void gpio_clk_low(void);
void gpio_clk_hight(void);
int gpio_clk_status(void);
//for csn pin
void gpio_csn_input(void);
void gpio_csn_output(void);
void gpio_csn_low(void);
void gpio_csn_hight(void);
int gpio_csn_status(void);

#endif /* _IR_REMOTE_ET4007_H_ */
