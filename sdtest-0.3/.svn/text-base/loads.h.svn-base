/* loads.h
 *
 * Author: Xiang-Yu Wang <rain_wang@jabil.com>
 *
 * Copyright (c) 2008 Jabil, Inc.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 2008-01-25 made initial version
 *
 */

#ifndef LOADS_H
#define LOADS_H

#include <sys/types.h>
#include <sys/time.h>

#include "sdtest.h"

/*
 * the loads structures
 */
struct sd_time {
	struct timeval start;		/* start timeval */
	struct timeval elapse;		/* elapsed timeval */
};

struct sd_load {
	struct sd_device * sd;		/* the tested device */

	size_t		blk_total;	/* total blocks to read/write */
	size_t		blk_read;	/* blocks have been read */
	size_t		blk_wrtn;	/* blocks have been written */
	size_t		tsf_read;	/* transfers have been read */
	size_t		tsf_wrtn;	/* transfers have been written */

	struct sd_time *tm;		/* hold the test time */

	double		blk_read_s;	/* blocks read per sec */
	double		blk_wrtn_s;	/* blocks written per sec */
	double		tsf_read_s;	/* transfers read per sec */
	double		tsf_wrtn_s;	/* transfers written per sec */
	double		MB_s;		/* total MB per sec */
};

/*
 * the loads functions
 */
extern void sd_initime(struct sd_time *);
extern void sd_gettime(struct sd_time *);
extern void sd_caloads(struct sd_load *);
extern void sd_prloads(struct sd_load *);
extern struct sd_load *sd_initload(struct sd_device *, struct sd_time *);
extern void sd_exitload(struct sd_load *);

#endif /* LOADS_H */
