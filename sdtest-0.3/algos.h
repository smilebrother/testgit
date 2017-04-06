/* algos.h
 *
 * Author: Xiang-Yu Wang <rain_wang@jabil.com>
 *
 * Copyright (c) 2008 Jabil, Inc.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 2008-01-06 made initial version
 * 2008-03-06 removed algo_srb
 *
 */

#ifndef ALGOS_H
#define ALGOS_H

#include "sdtest.h"

/* algorithm names */
#define AL_SRB_SR	SEQU_READ
#define AL_SRB_RR	RAND_READ
#define AL_SRB_BR	BUTT_READ
#define AL_SRB_SW	SEQU_WRITE
#define AL_SRB_RW	RAND_WRITE
#define AL_SRB_BW	BUTT_WRITE
#define AL_SRB_SWRC	SEQU_WRC
#define AL_SRB_RWRC	RAND_WRC
#define AL_SRB_BWRC	BUTT_WRC
#define AL_SRB_BS	BUTT_SEEK

#define AL_RWS_SR	SEQU_READ
#define AL_RWS_RR	RAND_READ
#define AL_RWS_BR	BUTT_READ
#define AL_RWS_SW	SEQU_WRITE
#define AL_RWS_RW	RAND_WRITE
#define AL_RWS_BW	BUTT_WRITE
#define AL_RWS_SWRC	SEQU_WRC
#define AL_RWS_RWRC	RAND_WRC
#define AL_RWS_BWRC	BUTT_WRC
#define AL_RWS_BS	BUTT_SEEK

#define AL_ONE_SR	SEQU_READ
#define AL_ONE_RR	RAND_READ
#define AL_ONE_BR	BUTT_READ
#define AL_ONE_SW	SEQU_WRITE
#define AL_ONE_RW	RAND_WRITE
#define AL_ONE_BW	BUTT_WRITE
#define AL_ONE_SWRC	SEQU_WRC
#define AL_ONE_RWRC	RAND_WRC
#define AL_ONE_BWRC	BUTT_WRC
#define AL_ONE_BS	BUTT_SEEK

#define AL_PAR_SR	SEQU_READ
#define AL_PAR_RR	RAND_READ
#define AL_PAR_BR	BUTT_READ
#define AL_PAR_SW	SEQU_WRITE
#define AL_PAR_RW	RAND_WRITE
#define AL_PAR_BW	BUTT_WRITE
#define AL_PAR_SWRC	SEQU_WRC
#define AL_PAR_RWRC	RAND_WRC
#define AL_PAR_BWRC	BUTT_WRC
#define AL_PAR_BS	BUTT_SEEK

/*
 * algorithm structure
 */
struct test_algo {
	const char *	name;	/* algorithm name */

	int (*func)(struct test_parm *, void *);
};

/* 
 * declaration of algorithms
 */
extern struct test_algo algo_rws[];
extern struct test_algo algo_one[];
extern struct test_algo algo_par[];

#endif /* ALGOS_H */
