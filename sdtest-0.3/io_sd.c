/* io_sd.c
 *
 * Author: Xiang-Yu Wang <rain_wang@jabil.com>
 *
 * Copyright (c) 2008 Jabil, Inc.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 2008-01-04 made initial version
 * 2008-03-14 added 'bsget_sd' and 'blkget_sd' in
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#include "sdtest.h"
#include "utils.h"

static inline int lseek_sd(struct sd_device *disk, off_t offset)
{
	return lseek(disk->fd, offset, SEEK_SET);
}

static inline int read_sd(struct sd_device *disk, void *buf, size_t size)
{
	return read(disk->fd, buf, size);
}

static inline int write_sd(struct sd_device *disk, void *buf, size_t size)
{
	return write(disk->fd, buf, size);
}

static inline int bsget_sd(struct sd_device *disk)
{
#ifdef BSZGET
	return ioctl(disk->fd, BLKBSZGET, &disk->bs);
#else
	return ioctl(disk->fd, BLKSSZGET, &disk->bs);
#endif
}

static inline int blkget_sd(struct sd_device *disk)
{
	return ioctl(disk->fd, BLKGETSIZE, &disk->blk);
}

struct sd_device gen_disk = {
	.name	= GENERIC_DISK,
	.seek	= lseek_sd,
	.read	= read_sd,
	.write	= write_sd,
	.bsget	= bsget_sd,
	.blkget	= blkget_sd,
	.tests	= {
		{ SEQU_READ, },
		{ RAND_READ, },
		{ BUTT_READ, },
		{ SEQU_WRITE, },
		{ RAND_WRITE, },
		{ BUTT_WRITE, },
		{ SEQU_WRC, },
		{ RAND_WRC, },
		{ BUTT_WRC, },
	}
};
