/* al_par.c
 *
 * Author: Xiang-Yu Wang <rain_wang@jabil.com>
 *
 * Copyright (c) 2008 Jabil, Inc.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 2008-01-24 made initial version
 * 2008-02-22 accommodate the sd_err, with -1 as return value
 * 2008-03-05 added codes calculating 'diskstats'
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>

#include "sdtest.h"
#include "algos.h"
#include "utils.h"
#include "loads.h"

typedef enum {
	/* type0 */	
	READ,
	WRITE,
	WRC,
	SEEK,
} par_ot;

typedef enum {
	/* type1 */
	SEQUENTIAL,
	RANDOM,
	BUTTERFLY,
} par_pt;

static int al_par_rws(struct test_parm *p, void *d, par_ot type0, par_pt type1)
{
	struct sd_time time;
	struct sd_load *load = NULL;
	struct sd_part *par = (struct sd_part *)d;
	struct sd_device *dsk = NULL;
	off_t offset = p->start, op = p->start;
	off_t count = p->size / (p->block * p->blocks) * p->cover / 100;
	off_t total = count + 1;
	char *bak = NULL;
	int i = 0, ret = 0, res = 0;
	int seed = 1;
	
	/* map a private copy of sd_device per sd_part */
	dsk = (struct sd_device *)malloc(sizeof(struct sd_device));
	if (!dsk) {
		dsk->stat = SD_ERR_SYS;
		return SD_ERR;
	}
	memcpy(dsk, par->sd, sizeof(struct sd_device));
	dsk->name = par->name;
	dsk->size = par->size;
	dsk->pos  = par->pos;
	dsk->buf  = par->buf;
	dsk->parm = par->parm;

	if (type1 == RANDOM)
		if (sd_randinit()) {
			dsk->stat = SD_ERR_SYS;
			return SD_ERR;
		}

	if (type0 == WRITE || type0 == WRC)
		if (p->backup) {
			bak = (char *)malloc(p->block * p->blocks);
			if (!bak) {
				dsk->stat = SD_ERR_SYS;
				return SD_ERR;
			}
		}

	load = sd_initload(dsk, &time);
	if (!load) {
		dsk->stat = SD_ERR_SYS;
		return SD_ERR;
	}

	do {
		if (type1 == BUTTERFLY) {
			if (op < 0)
				offset = op * (-1);
			else
				offset = op;
		}

		if (offset >= par->size)
			offset = 0;
		
		if (type0 == WRITE || type0 == WRC)
			if (p->backup) {
				dsk->seek(dsk, offset);
				dsk->read(dsk, bak, p->block * p->blocks);
			}

		if (type0 == WRC)
			for (i = 0; i < p->block * p->blocks; i += 4) {
				par->buf[i + 0] = (char)(p->pattern >> 24);
				par->buf[i + 1] = (char)(p->pattern >> 16);
				par->buf[i + 2] = (char)(p->pattern >> 8);
				par->buf[i + 3] = (char)p->pattern;
			}

		dsk->seek(dsk, offset);
		if (type0 == READ)
			ret = dsk->read(dsk, par->buf, p->block * p->blocks);
		else if (type0 == WRITE)
			ret = dsk->write(dsk, par->buf, p->block * p->blocks);
		else if (type0 == WRC) {
			res = dsk->write(dsk, par->buf, p->block * p->blocks);
			if (res < 0) {
				if (dsk->stat != SD_ERR_SGIO) {
					/* update the stat in parent device */
					if (dsk->stat == SD_ERR_NO)
						par->sd->stat = dsk->stat = SD_ERR_TEST;
				}
				ret = SD_ERR;
				break;
			}
			dsk->seek(dsk, offset);
			res = dsk->read(dsk, par->buf, res);
			if (res < 0) {
				if (p->backup) {
					dsk->seek(dsk, offset);
					dsk->write(dsk, bak, p->block * p->blocks);
				}
				if (dsk->stat != SD_ERR_SGIO) {
					/* update the stat in parent device */
					if (dsk->stat == SD_ERR_NO)
						par->sd->stat = dsk->stat = SD_ERR_TEST;
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
		} else if (type0 == SEEK)
			;
		else
			return SD_ERR_NO;
		
		if (type0 == WRITE || type0 == WRC)
			if (p->backup) {
				dsk->seek(dsk, offset);
				dsk->write(dsk, bak, p->block * p->blocks);
			}
		
		if (type0 == WRC) {
			if (dsk->stat == SD_ERR_SGIO 
				|| dsk->stat == SD_ERR_TEST || errno == EIO)
				break;
		} else {
			if (ret < 0) {
				sd_debug("par->pos: %ld\n", par->pos);
				return ret;
			}
		}
		
		if (type1 == SEQUENTIAL)
			offset += p->block * p->blocks;
		else if (type1 == RANDOM)
			offset += sd_randget(p->size - p->start);
		else if (type1 == BUTTERFLY)
			op = p->size - ((p->block * p->blocks) * (seed++)) - op;
		else
			return SD_ERR_NO;

		if (!p->nopro)
			tpout("%3ld%%\b\b\b\b", (total - count) * 100 / total);
	} while (count--);

	if (type0 == READ) {
		load->blk_total = p->blocks * total;
		load->blk_read  = p->blocks * total;
		load->blk_wrtn  = 0;
		load->tsf_read  = total;
		load->tsf_wrtn  = 0;
	} else if (type0 == WRITE) {
		load->blk_total = p->blocks * total;
		load->blk_read  = 0;
		load->blk_wrtn  = p->blocks * total;
		load->tsf_read  = 0;
		load->tsf_wrtn  = total;
	} else if (type0 == WRC) {
		load->blk_total = p->blocks * total * 2;
		load->blk_read  = p->blocks * total;
		load->blk_wrtn  = p->blocks * total;
		load->tsf_read  = total;
		load->tsf_wrtn  = total;
	} else {
		load->blk_total = 0;
		load->blk_read  = 0;
		load->blk_wrtn  = 0;
		load->tsf_read  = 0;
		load->tsf_wrtn  = 0;
	}
	sd_caloads(load);

	if (p->nopro < 2)
		sd_prloads(load);

	sd_exitload(load);

	if (type0 == WRITE || type0 == WRC)
		if (p->backup)
			free(bak);

	/* ensure one device status */
	par->sd->stat = dsk->stat;

	/* free this private copy of sd_device */
	if (dsk)
		free(dsk);

	return SD_ERR_NO;
}

static inline int al_sequential_read(struct test_parm *p, void *d)	
{
	return al_par_rws(p, d, READ, SEQUENTIAL);
}

static inline int al_random_read(struct test_parm *p, void *d)
{
	return al_par_rws(p, d, READ, RANDOM);
}

static inline int al_butterfly_read(struct test_parm *p, void *d)
{
	return al_par_rws(p, d, READ, BUTTERFLY);
}

static inline int al_sequential_write(struct test_parm *p, void *d)
{
	return al_par_rws(p, d, WRITE, SEQUENTIAL);
}

static inline int al_random_write(struct test_parm *p, void *d)
{
	return al_par_rws(p, d, WRITE, RANDOM);
}

static inline int al_butterfly_write(struct test_parm *p, void *d)
{
	return al_par_rws(p, d, WRITE, BUTTERFLY);
}

static inline int al_sequential_wrc(struct test_parm *p, void *d)
{
	return al_par_rws(p, d, WRC, SEQUENTIAL);
}

static inline int al_random_wrc(struct test_parm *p, void *d)
{
	return al_par_rws(p, d, WRC, RANDOM);
}

static inline int al_butterfly_wrc(struct test_parm *p, void *d)
{
	return al_par_rws(p, d, WRC, BUTTERFLY);
}

static inline int al_butterfly_seek(struct test_parm *p, void *d)
{
	return al_par_rws(p, d, SEEK, BUTTERFLY);
}

struct test_algo algo_par[] = {
	{ AL_PAR_SR, 	al_sequential_read },
	{ AL_PAR_RR, 	al_random_read },
	{ AL_PAR_BR, 	al_butterfly_read },
	{ AL_PAR_SW, 	al_sequential_write },
	{ AL_PAR_RW, 	al_random_write },
	{ AL_PAR_BW, 	al_butterfly_write },
	{ AL_PAR_SWRC, 	al_sequential_wrc },
	{ AL_PAR_RWRC, 	al_random_wrc },
	{ AL_PAR_BWRC, 	al_butterfly_wrc },
	{ AL_PAR_BS, 	al_butterfly_seek },
	{ NULL, }
};
