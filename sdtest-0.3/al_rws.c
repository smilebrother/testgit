/* al_rws.c
 *
 * Author: Xiang-Yu Wang <rain_wang@jabil.com>
 *
 * Copyright (c) 2008 Jabil, Inc.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 2008-01-18 made initial version
 * 2008-02-22 accommodate the sd_err, with -1 as return value
 * 2008-03-05 added codes calculating 'diskstats'
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>

#include "sdtest.h"
#include "algos.h"
#include "utils.h"

enum rws_type {
	SEQUENTIAL,
	RANDOM,
	BUTTERFLY,
};

static int al_rws_read(struct test_parm *p, void *d, enum rws_type type)
{
	struct timeval start_tm, end_tm;
	struct sd_device *dsk = d;
	off_t offset = p->start, op = p->start;
	off_t count = p->size / (p->block * p->blocks) * p->cover / 100;
	off_t total = count + 1;
	int ret = 0;
	int seed = 1;
	
	if (type == RANDOM)
		if (sd_randinit()) {
			dsk->stat = SD_ERR_SYS;
			return SD_ERR;
		}

	start_tm.tv_sec = 0;
	start_tm.tv_usec = 0;

	gettimeofday(&start_tm, NULL);

	do {
		if (type == BUTTERFLY) {
			if (op < 0)
				offset = op * (-1);
			else
				offset = op;
		}

		if (offset >= dsk->size)
			offset = 0;
		
		dsk->seek(dsk, offset);
		ret = dsk->read(dsk, dsk->buf, p->block * p->blocks);
		
		if (ret < 0) {
			sd_debug("dsk->pos: %ld\n", dsk->pos);
			return ret;
		}
		
		switch (type) {
		case SEQUENTIAL:
			offset += p->block * p->blocks;
			break;
		case RANDOM:
			offset += sd_randget(p->size - p->start);
			break;
		case BUTTERFLY:
			op = p->size - ((p->block * p->blocks) * (seed++)) - op;
			break;
		}

		if (!p->nopro)
			tpout("%3ld%%\b\b\b\b", (total - count) * 100 / total);
	} while (count--);

	gettimeofday(&end_tm, NULL);

        if (start_tm.tv_sec || start_tm.tv_usec) {
		struct timeval res_tm;
		double a, b, c;

		res_tm.tv_sec = end_tm.tv_sec - start_tm.tv_sec;
		res_tm.tv_usec = end_tm.tv_usec - start_tm.tv_usec;
		if (res_tm.tv_usec < 0) {
                	--res_tm.tv_sec;
                	res_tm.tv_usec += 1000000;
            	}
		a = res_tm.tv_sec;
		a += (0.000001 * res_tm.tv_usec);
		b = (double)(p->block * p->blocks) * total;
		c = b / (a * 1000000.0);
    
		if (p->nopro < 2) {
			tpout("\n elapsed time %d.%06d secs", 
				(int)res_tm.tv_sec, (int)res_tm.tv_usec);
			if ((a > 0.00001) && (b > 511))
				tpout(", %.2f MB/sec\n", c);
			else
				tpout("\n");
		}
	}
	return SD_ERR_NO;
}

static int al_rws_write(struct test_parm *p, void *d, enum rws_type type)
{
	struct timeval start_tm, end_tm;
	struct sd_device *dsk = d;
	off_t offset = p->start, op = p->start;
	off_t count = p->size / (p->block * p->blocks) * p->cover / 100;
	off_t total = count + 1;
	char *bak = NULL;
	int ret = 0;
	int seed = 1;
	
	if (type == RANDOM)
		if (sd_randinit()) {
			dsk->stat = SD_ERR_SYS;
			return SD_ERR;
		}

	if (p->backup) {
		bak = (char *)malloc(p->block * p->blocks);
		if (!bak) {
			dsk->stat = SD_ERR_SYS;
			return SD_ERR;
		}
	}

	start_tm.tv_sec = 0;
	start_tm.tv_usec = 0;

	gettimeofday(&start_tm, NULL);

	do {
		if (type == BUTTERFLY) {
			if (op < 0)
				offset = op * (-1);
			else
				offset = op;
		}

		if (offset >= dsk->size)
			offset = 0;
		
		if (p->backup) {
			dsk->seek(dsk, offset);
			dsk->read(dsk, bak, p->block * p->blocks);
		}

		dsk->seek(dsk, offset);
		ret = dsk->write(dsk, dsk->buf, p->block * p->blocks);
		
		if (p->backup) {
			dsk->seek(dsk, offset);
			dsk->write(dsk, bak, p->block * p->blocks);
		}
		
		if (ret < 0) {
			sd_debug("dsk->pos: %ld\n", dsk->pos);
			return ret;
		}

		switch (type) {
		case SEQUENTIAL:
			offset += p->block * p->blocks;
			break;
		case RANDOM:
			offset += sd_randget(p->size - p->start);
			break;
		case BUTTERFLY:
			op = p->size - ((p->block * p->blocks) * (seed++)) - op;
			break;
		}
		
		if (!p->nopro)
			tpout("%3ld%%\b\b\b\b", (total - count) * 100 / total);
	} while (count--);

	gettimeofday(&end_tm, NULL);

        if (start_tm.tv_sec || start_tm.tv_usec) {
		struct timeval res_tm;
		double a, b, c;

		res_tm.tv_sec = end_tm.tv_sec - start_tm.tv_sec;
		res_tm.tv_usec = end_tm.tv_usec - start_tm.tv_usec;
		if (res_tm.tv_usec < 0) {
                	--res_tm.tv_sec;
                	res_tm.tv_usec += 1000000;
            	}
		a = res_tm.tv_sec;
		a += (0.000001 * res_tm.tv_usec);
		b = (double)(p->block * p->blocks) * total;
		c = b / (a * 1000000.0);
    
		if (p->nopro < 2) {
			tpout("\n elapsed time %d.%06d secs", 
				(int)res_tm.tv_sec, (int)res_tm.tv_usec);
			if ((a > 0.00001) && (b > 511))
				tpout(", %.2f MB/sec\n", c);
			else
				tpout("\n");
		}
	}

	if (p->backup)
		free(bak);

	return SD_ERR_NO;
}

static int al_rws_wrc(struct test_parm *p, void *d, enum rws_type type)
{
	struct timeval start_tm, end_tm;
	struct sd_device *dsk = d;
	off_t offset = p->start, op = p->start;
	off_t count = p->size / (p->block * p->blocks) * p->cover / 100;
	off_t total = count + 1;
	char *bak = NULL;
	int i = 0, ret = 0, res = 0;
	int seed = 1;
	
	if (type == RANDOM)
		if (sd_randinit()) {
			dsk->stat = SD_ERR_SYS;
			return SD_ERR;
		}

	if (p->backup) {
		bak = (char *)malloc(p->block * p->blocks);
		if (!bak) {
			dsk->stat = SD_ERR_SYS;
			return SD_ERR;
		}
	}

	start_tm.tv_sec = 0;
	start_tm.tv_usec = 0;

	gettimeofday(&start_tm, NULL);

	do {
		if (type == BUTTERFLY) {
			if (op < 0)
				offset = op * (-1);
			else
				offset = op;
		}

		if (offset >= dsk->size)
			offset = 0;
		
		if (p->backup) {
			dsk->seek(dsk, offset);
			dsk->read(dsk, bak, p->block * p->blocks);
		}

		for (i = 0; i < p->block * p->blocks; i += 4) {
			dsk->buf[i + 0] = (char)(p->pattern >> 24);
			dsk->buf[i + 1] = (char)(p->pattern >> 16);
			dsk->buf[i + 2] = (char)(p->pattern >> 8);
			dsk->buf[i + 3] = (char)p->pattern;
		}
		dsk->seek(dsk, offset);
		res = dsk->write(dsk, dsk->buf, p->block * p->blocks);
		if (res < 0) {
			if (dsk->stat != SD_ERR_SGIO) {
				if (dsk->stat == SD_ERR_NO)
					dsk->stat = SD_ERR_TEST;
			}
			ret = SD_ERR;
			break;
		}
		dsk->seek(dsk, offset);
		res = dsk->read(dsk, dsk->buf, res);
		if (res < 0) {
			if (p->backup) {
				dsk->seek(dsk, offset);
				dsk->write(dsk, bak, p->block * p->blocks);
			}
			if (dsk->stat != SD_ERR_SGIO) {
				if (dsk->stat == SD_ERR_NO)
					dsk->stat = SD_ERR_TEST;
			}
			ret = SD_ERR;
			break;
		}
		for (i = 0; i < p->block * p->blocks; i += 4) {
			if (i + 4 > res)
				break;
			if ((dsk->buf[i] == (char)(p->pattern >> 24)) && 
				(dsk->buf[i + 1] == (char)(p->pattern >> 16)) &&
				(dsk->buf[i + 2] == (char)(p->pattern >> 8)) &&
				(dsk->buf[i + 3] == (char)p->pattern))
				continue;
			dsk->stat = SD_ERR_TEST;
			ret = SD_ERR;
			break;
		}

		if (p->backup) {
			dsk->seek(dsk, offset);
			dsk->write(dsk, bak, p->block * p->blocks);
		}
		
		if (dsk->stat == SD_ERR_SGIO || dsk->stat == SD_ERR_TEST 
				|| errno == EIO)
			break;
		
		switch (type) {
		case SEQUENTIAL:
			offset += p->block * p->blocks;
			break;
		case RANDOM:
			offset += sd_randget(p->size - p->start);
			break;
		case BUTTERFLY:
			op = p->size - ((p->block * p->blocks) * (seed++)) - op;
			break;
		}
		
		if (!p->nopro)
			tpout("%3ld%%\b\b\b\b", (total - count) * 100 / total);
	} while (count--);

	gettimeofday(&end_tm, NULL);

        if (start_tm.tv_sec || start_tm.tv_usec) {
		struct timeval res_tm;
		double a, b, c;

		res_tm.tv_sec = end_tm.tv_sec - start_tm.tv_sec;
		res_tm.tv_usec = end_tm.tv_usec - start_tm.tv_usec;
		if (res_tm.tv_usec < 0) {
                	--res_tm.tv_sec;
                	res_tm.tv_usec += 1000000;
            	}
		a = res_tm.tv_sec;
		a += (0.000001 * res_tm.tv_usec);
		b = (double)(p->block * p->blocks) * total;
		c = b / (a * 1000000.0);
    
		if (p->nopro < 2) {
			tpout("\n elapsed time %d.%06d secs", 
				(int)res_tm.tv_sec, (int)res_tm.tv_usec);
			if ((a > 0.00001) && (b > 511))
				tpout(", %.2f MB/sec\n", c);
			else
				tpout("\n");
		}
	}

	if (p->backup)
		free(bak);

	return ret;
}

static int al_rws_seek(struct test_parm *p, void *d, enum rws_type type)
{
	struct timeval start_tm, end_tm;
	struct sd_device *dsk = d;
	off_t offset = p->start, op = p->start;
	off_t count = p->size / (p->block * p->blocks) * p->cover / 100;
	off_t total = count + 1;
	int ret = 0;
	int seed = 1;
	
	if (type == RANDOM)
		if (sd_randinit()) {
			dsk->stat = SD_ERR_SYS;
			return SD_ERR;
		}

	start_tm.tv_sec = 0;
	start_tm.tv_usec = 0;

	gettimeofday(&start_tm, NULL);

	do {
		if (type == BUTTERFLY) {
			if (op < 0)
				offset = op * (-1);
			else
				offset = op;
		}

		if (offset >= dsk->size) 
			offset = 0;
		
		dsk->seek(dsk, offset);
		
		if (ret < 0)
			return ret;

		switch (type) {
		case SEQUENTIAL:
			offset += p->block * p->blocks;
			break;
		case RANDOM:
			offset += sd_randget(p->size - p->start);
			break;
		case BUTTERFLY:
			op = p->size - ((p->block * p->blocks) * (seed++)) - op;
			break;
		}
		
		if (!p->nopro)
			tpout("%3ld%%\b\b\b\b", (total - count) * 100 / total);
	} while (count--);

	gettimeofday(&end_tm, NULL);

        if (start_tm.tv_sec || start_tm.tv_usec) {
		struct timeval res_tm;
		double a, b, c;

		res_tm.tv_sec = end_tm.tv_sec - start_tm.tv_sec;
		res_tm.tv_usec = end_tm.tv_usec - start_tm.tv_usec;
		if (res_tm.tv_usec < 0) {
                	--res_tm.tv_sec;
                	res_tm.tv_usec += 1000000;
            	}
		a = res_tm.tv_sec;
		a += (0.000001 * res_tm.tv_usec);
		b = (double)(p->block * p->blocks) * total;
		c = b / (a * 1000000.0);
    
		if (p->nopro < 2) {
			tpout("\n elapsed time %d.%06d secs", 
				(int)res_tm.tv_sec, (int)res_tm.tv_usec);
			if ((a > 0.00001) && (b > 511))
				tpout(", %.2f MB/sec\n", c);
			else
				tpout("\n");
		}
	}

	return SD_ERR_NO;
}

static inline int al_sequential_read(struct test_parm *p, void *d)	
{
	return al_rws_read(p, d, SEQUENTIAL);
}

static inline int al_random_read(struct test_parm *p, void *d)
{
	return al_rws_read(p, d, RANDOM);
}

static inline int al_butterfly_read(struct test_parm *p, void *d)
{
	return al_rws_read(p, d, BUTTERFLY);
}

static inline int al_sequential_write(struct test_parm *p, void *d)
{
	return al_rws_write(p, d, SEQUENTIAL);
}

static inline int al_random_write(struct test_parm *p, void *d)
{
	return al_rws_write(p, d, RANDOM);
}

static inline int al_butterfly_write(struct test_parm *p, void *d)
{
	return al_rws_write(p, d, BUTTERFLY);
}

static inline int al_sequential_wrc(struct test_parm *p, void *d)
{
	return al_rws_wrc(p, d, SEQUENTIAL);
}

static inline int al_random_wrc(struct test_parm *p, void *d)
{
	return al_rws_wrc(p, d, RANDOM);
}

static inline int al_butterfly_wrc(struct test_parm *p, void *d)
{
	return al_rws_wrc(p, d, BUTTERFLY);
}

static inline int al_butterfly_seek(struct test_parm *p, void *d)
{
	return al_rws_seek(p, d, BUTTERFLY);
}

struct test_algo algo_rws[] = {
	{ AL_RWS_SR, 	al_sequential_read },
	{ AL_RWS_RR, 	al_random_read },
	{ AL_RWS_BR, 	al_butterfly_read },
	{ AL_RWS_SW, 	al_sequential_write },
	{ AL_RWS_RW, 	al_random_write },
	{ AL_RWS_BW, 	al_butterfly_write },
	{ AL_RWS_SWRC, 	al_sequential_wrc },
	{ AL_RWS_RWRC, 	al_random_wrc },
	{ AL_RWS_BWRC, 	al_butterfly_wrc },
	{ AL_RWS_BS, 	al_butterfly_seek },
	{ NULL, }
};
