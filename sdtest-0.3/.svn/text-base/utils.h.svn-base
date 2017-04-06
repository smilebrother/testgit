/* utils.h
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

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

/* get random number */
extern int sd_randinit(void);
extern off_t sd_randget(off_t);

/* get size shift bit */
extern int sd_bitss(int);

/* get byte value from input */
extern size_t sd_bytebox(char *, int);

/* print debug messages */
extern void sd_debug(const char *, ...);

/* use tpout & tperr to print messages */
extern void tprintf(FILE *, const char *, ...);
extern void tprintt(FILE *, const char *, ...);

#define tpout(fmt, arg...)	tprintf(stdout, fmt, ## arg)
#define tperr(fmt, arg...)	tprintf(stderr, fmt, ## arg)
#define tptout(fmt, arg...)	tprintt(stdout, fmt, ## arg)
#define tpterr(fmt, arg...)	tprintt(stderr, fmt, ## arg)

#endif /* UTILS_H */
