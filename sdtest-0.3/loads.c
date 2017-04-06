/* loads.c
 *
 * Author: Xiang-Yu Wang <rain_wang@jabil.com>
 *
 * Copyright (c) 2008 Jabil, Inc.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 2008-03-07 made initial version
 *
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>

#include "sdtest.h"
#include "loads.h"
#include "utils.h"

void sd_initime(struct sd_time *tm)
{
	tm->start.tv_sec = 0;
	tm->start.tv_usec = 0;
	tm->elapse.tv_sec = 0;
	tm->elapse.tv_usec = 0;

	gettimeofday(&tm->start, NULL);
	gettimeofday(&tm->elapse, NULL);
}

void sd_gettime(struct sd_time *tm)
{
	gettimeofday(&tm->elapse, NULL);
}

struct sd_load *sd_initload(struct sd_device *sd, struct sd_time *tm)
{
	struct sd_load *load = NULL;
	load = malloc(sizeof(struct sd_load));
	if (!load)
		return NULL;
	load->sd	= sd;
	load->blk_total = 0;
	load->blk_read	= 0;
	load->blk_wrtn	= 0;
	load->tsf_read	= 0;
	load->tsf_wrtn	= 0;
	load->tm 	= tm;
	sd_initime(load->tm);
	return load;
}

void sd_exitload(struct sd_load *load)
{
	if (load)
		free(load);
}

void sd_caloads(struct sd_load *load)
{
	sd_gettime(load->tm);
	sd_debug("\n blk %ld, blk_rd %ld, blk_wr %d, tsf_rd %d, tsf_wr %d", 
		load->blk_total, load->blk_read, load->blk_wrtn, 
		load->tsf_read, load->tsf_wrtn);
        if (load->tm->start.tv_sec || load->tm->start.tv_usec) {
		struct timeval it;
		double t;

		it.tv_sec = load->tm->elapse.tv_sec - load->tm->start.tv_sec;
		it.tv_usec = load->tm->elapse.tv_usec - load->tm->start.tv_usec;
		if (it.tv_usec < 0) {
                	--it.tv_sec;
                	it.tv_usec += 1000000;
            	}
		t = it.tv_sec;
		t += (0.000001 * it.tv_usec);
		sd_debug("\n t %.2f", t);

		load->blk_read_s = (double)load->blk_read / (t * 1.0);
		load->blk_wrtn_s = (double)load->blk_wrtn / (t * 1.0);
		load->tsf_read_s = (double)load->tsf_read / (t * 1.0);
		load->tsf_wrtn_s = (double)load->tsf_wrtn / (t * 1.0);
		load->MB_s	 = (double)(load->blk_total * load->sd->bs) / (t * 1000000.0);
		sd_debug("\n elapsed time %d.%06d secs", 
			(int)it.tv_sec, (int)it.tv_usec);
	}
}

void sd_prloads(struct sd_load *load)
{
	tpout("\n blk_rd/s  blk_wr/s  tps_rd  tps_wr        total");
	tpout("\n %8.2f  %8.2f  %6.2f  %6.2f    %9.2f MB/s\n",
		load->blk_read_s, load->blk_wrtn_s, 
		load->tsf_read_s, load->tsf_wrtn_s,
		load->MB_s);
}
