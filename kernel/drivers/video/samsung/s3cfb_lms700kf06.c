/* linux/drivers/video/samsung/s3cfb_lms700kf06.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Lms700kf06 7.0" WVGA(800*480) Landscape LCD module driver for the SMDK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "s3cfb.h"
static struct s3cfb_lcd lms700kf06 = {
	.width	= 800,
	.height	= 480,
	.bpp	= 24,
	.freq	= 120,

	.timing = {
		.h_fp	= 8,
		.h_bp	= 13,
		.h_sw	= 3,
		.v_fp	= 5,
		.v_fpe	= 1,
		.v_bp	= 7,
		.v_bpe	= 1,
		.v_sw	= 1,
	},

	.polarity = {
		.rise_vclk	= 0,
		.inv_hsync	= 1,
		.inv_vsync	= 1,
		.inv_vden	= 0,
	},
};	

/* name should be fixed as 's3cfb_set_lcd_info' */
void s3cfb_set_lcd_info(struct s3cfb_global *ctrl)
{
	lms700kf06.init_ldi = NULL;
	ctrl->lcd = &lms700kf06;
}
