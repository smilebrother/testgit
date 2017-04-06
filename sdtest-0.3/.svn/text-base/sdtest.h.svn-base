/* sdtest.h
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
 * 2008-02-22 redefine the sd_err, with -1 as return value
 * 2008-03-14 changed MAX_BLK_SIZE to 4096, added 'blk' to hold size in blocks
 *            'bsget' and 'blkget' hook for different io method
 * 2008-06-10 replace 'size_t' with 'off_t' to hold disk size.
 *
 */

#ifndef SDTEST_H
#define SDTEST_H

#include <sys/types.h>
#include <pthread.h>

#include "version.h"

/*
 * global definitaions
 */
#define NUM_TESTS	9
#define NUM_THREADS	16

/* physical device has 512-byte sector */
#define BASE_SEC_SIZE	512
#define MAX_BLK_SIZE	4096

/* block per transfer */
#define BPT_BLOCKS	128
#define MAX_BPT_SIZE	64 * 1024

/* disk types */
enum {
	SD_GENERIC,
	SD_SCSI_SG,
	SD_SCSI_CD,
	SD_BLOCK,
	SD_RAW,
	SD_OTHER,
};

#define GENERIC_DISK	"disk"
#define ATA_DISK	"hd"
#define SCSI_DISK	"sd"
#define SATA_DISK	"sd"
#define SAS_DISK	"sd"
#define CDROM_DISK	"cd"
#define DVDROM_DISK	"dvd"
#define CDRW_DISK	"sg"

/* test names */
#define SEQU_READ	"sread"
#define RAND_READ	"rread"
#define BUTT_READ	"bread"
#define SEQU_WRITE	"swrite"
#define RAND_WRITE	"rwrite"
#define BUTT_WRITE	"bwrite"
#define SEQU_WRC	"swrc"
#define RAND_WRC	"rwrc"
#define BUTT_WRC	"bwrc"
#define BUTT_SEEK	"bseek"
#define INTERFACE	"interf"
#define SELF_DIAG	"selfd"

/* 
 * test parameters
 */
struct test_parm {
	char *		device;	/* device name */
	char *		test;	/* test name */
	int		pass;	/* passes to test */
	int		thread;	/* threads to run concurrently */

	int		block;	/* block size in byte */
	int		blocks;	/* blocks of every transfer */
	off_t		size;	/* total byte or block size of test */
	//size_t		size;	/* total byte or block size of test */
	off_t		start;	/* start position in byte or block */
	off_t		end;	/* end position in byte or block */
	int		cover;	/* coverage of whole disk */

	int		full;	/* tag test on full disk */
	unsigned	pattern;/* pattern on writing test */
	int		backup; /* backup mode of test */
	int		sgio;	/* use sgio to access */
	int		direct;	/* use O_DIRECT I/O */
	int		nopro;  /* don't show process percentage */
};

/*
 * core error level
 */
enum sd_err {
	SD_ERR_SGIO	= 252,	/* sgio returned error */
	SD_ERR_TEST	= 253,	/* tests returned error */
	SD_ERR_SYS	= 254,	/* system returned error */
	SD_ERR_USR	= 255,	/* error caused by user */
	SD_ERR_NO	= 0,	/* no error occured */
	SD_ERR		= -1,	/* returned value when error occured */
};

/*
 * core test structure
 */
struct sd_test {
	const char *	name;	/* test name */

	enum sd_err	stat;	/* keep test status */

	int (*func)(struct test_parm *, void *);
};

/*
 * core scsi disk device
 */
struct sd_device {	
	const char *	name;	/* device name */

	int		type;	/* property: device type */

	int		bs;	/* property: device block size */
	off_t		blk;	/* property: device size in blocks */
	off_t		size;	/* property: device size in bytes */
	//size_t		blk;	/* property: device size in blocks */
	//size_t		size;	/* property: device size in bytes */

	int		fd;	/* keep device descriptor */
	off_t		pos;	/* keep current offset pointer */
	
	char *		buf;	/* buffer to hold data */

	int (*seek)(struct sd_device *, off_t);
	int (*read)(struct sd_device *, void *, size_t);	
	int (*write)(struct sd_device *, void *, size_t);
	int (*bsget)(struct sd_device *);
	int (*blkget)(struct sd_device *);
	
	enum sd_err	stat;	/* keep latest io status */

	struct test_parm *parm;	/* keep test_parm pointer */
	
	struct sd_test tests[NUM_TESTS];
};

/*
 * core partition structure
 */
struct sd_part {
	char *		name;	/* partition name */
	int		ind;	/* partition index */
	
	off_t		size;	/* property: byte size of the partition */
	//size_t		size;	/* property: byte size of the partition */
	off_t		start;	/* property: start position in scsi device */

	off_t		pos;	/* keep current offset pointer */
	
	char *		buf;	/* buffer to hold data */
	
	struct sd_device *sd;	/* the parent sd_device of partition */
	
	struct test_parm *parm; /* local test_parm structure of partition */
	
	struct sd_test *test;	/* the test pointer of this partition */
};

/*
 * core thread structure
 */
struct sd_thread {
	pthread_t	self;	/* this thread */
	pthread_attr_t  *attr;  /* thread attr */
	pthread_mutex_t	*lock;  /* thread lock */

	pid_t		pid;	/* pid of thread */

	int		ind;	/* thread index */

	enum sd_err	res;	/* keep return value */
	char *		test;	/* test name */

	struct sd_device *dev;  /* host scsi device */
	struct sd_part	*part;  /* one partition per thread */

	struct test_parm parm;	/* a copy of test_parm per thread */
};

/*
 * partition functions
 */

#endif /* SDTEST_H */
