/* utils.c
 *
 * Author: Xiang-Yu Wang <rain_wang@jabil.com>
 *
 * Copyright (c) 2008 Jabil, Inc.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 2008-01-17 made initial version
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>

#include "sdtest.h"
#include "utils.h"

static struct drand48_data random_state;

int sd_randinit(void)
{
	unsigned long seed;
	int fd;

	fd = open("/dev/random", O_RDONLY);
	if (fd < 0) {
		tperr("open random: %s\n", strerror(errno));
		return SD_ERR_SYS;
	}

	if (read(fd, &seed, sizeof(seed)) < (int)sizeof(seed)) {
		tperr("read random: %s\n", strerror(errno));
		return SD_ERR_SYS;
	}

	close(fd);
	
	sd_debug("randomizer seed: %lx\n", seed);
	srand48_r(seed, &random_state);

	return SD_ERR_NO;
}

off_t sd_randget(off_t range)
{
	long r;

	lrand48_r(&random_state, &r);
	return (1 + (double) (range - 1) * r / (RAND_MAX + 1.0));
}

int sd_bitss(int w)
{
	int b = 0;
#ifdef SDFFS
	int i;

	for (i = 0; i < sizeof(w) * 8; i++)
		if ((w & (1UL << i)) != 0) {
			b = i;
			break;
		}
	return b;
#else
	b = ffs(w);
	return (b == 0) ? 0 : (b - 1);
#endif
}

size_t sd_bytebox(char *input, int b)
{
	int res;
	off_t num;
	char c;

	res = sscanf(input, "%lld%c", &num, &c);
	if (res == 0) {
		return SD_ERR_USR;
	} else if (res == 1) {
		return num;
	} else {
		switch (c) {
		case 'c':
		case 'C':
			num = num;
			break;
		case 'b':
			num *= b;
			break;
		case 'B':
			num *= BASE_SEC_SIZE;
			break;
		case 'k':
			num *= 1024;
			break;
		case 'K':
			num *= 1000;
			break;
		case 'm':
			num *= 1024 * 1024;
			break;
		case 'M':
			num *= 1000000;
			break;
		case 'g':
			num *= 1024 * 1024 * 1024;
			break;
		case 'G':
			num *= 1000000000;
			break;
		default:
			return SD_ERR_USR;
		}
	}

	return num;
}

void sd_debug(const char *fmt, ...)
{
#ifdef DEBUG
	va_list arg;

	va_start(arg, fmt);
	vfprintf(stderr, fmt, arg);
	va_end(arg);
#else
	return;
#endif
}

void tprintf(FILE *fp, const char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	vfprintf(fp, fmt, arg);
	va_end(arg);
}

void tprintt(FILE *fp, const char *fmt, ...)
{
	va_list arg;
	time_t ts = time(NULL);
	struct tm *tm = localtime(&ts);

	/* the month ranges from 0 to 11 and year start from 0 */
	fprintf(fp, "%d/%d/%d-%d:%d:%d: ", 
			tm->tm_mon + 1, tm->tm_mday, tm->tm_year + 1900,
			tm->tm_hour, tm->tm_min, tm->tm_sec);
	va_start(arg, fmt);
	vfprintf(fp, fmt, arg);
	va_end(arg);
}
