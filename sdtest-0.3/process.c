/* process.c
 *
 * Author: Xiang-Yu Wang <rain_wang@jabil.com>
 *
 * Copyright (c) 2008 Jabil, Inc.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 2008-02-02 made initial version
 * 2008-03-03 don't use sgio, so that 'iostat' can get 'diskstats' data.
 * 2008-03-06 set part_test_device by default, use '-q 2' to call sdtest.
 * 2008-03-11 fix a bug to get device size before part_test_device.
 * 2008-03-13 move the 'interf' test out of main loop, for this test should
 *            only be issued once for every device.
 * 2008-06-10 move the 'selfd' test out of main loop, for this test should
 *            only be issued once for every device.
 * 2008-06-12 fixed a bug, test string in process should be preset.
 *
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#include "sdtest.h"
#include "utils.h"

enum test_item_error_code {
	SEQ_READ_ERR	= 0x01,
	SEQ_WRITE_ERR	= 0x02,
	SEQ_WRC_ERR	= 0x03,
	SEQ_SEEK_ERR	= 0x04,
	RAN_READ_ERR	= 0x11,
	RAN_WRITE_ERR	= 0x12,
	RAN_WRC_ERR	= 0x13,
	RAN_SEEK_ERR	= 0x14,
	BUTFLY_SEEK_ERR = 0x24,
	INTERFACE_ERR	= 0x30,
	SELFTEST_ERR	= 0x31,
};

//static const char *sdtest = "/usr/local/bin/sdtest";
static const char *sdtest = "./sdtest";

static const unsigned errors[] = {
	INTERFACE_ERR,
	SELFTEST_ERR,
	SEQ_WRC_ERR,
	RAN_WRC_ERR,
	SEQ_WRITE_ERR,
	RAN_WRITE_ERR,
	SEQ_READ_ERR,
	RAN_READ_ERR,
	BUTFLY_SEEK_ERR,
	0
};

static const char *item[] = {
	"interf",
	"selfd",
	"swrc",
	"rwrc",
	"swrite",
	"rwrite",
	"sread",
	"rread",
	"bseek",
	NULL
};

static const char *aitem = "auto";

struct process_parm {
	char *	device;
	char *	test;
	int 	thread;
	int 	sanity;
	char *	opts;
};

struct process_data {
	pthread_t thread;
	pthread_attr_t attr;
	int 	pid;
	int 	sigs;
	int 	res;
	struct process_parm *parm;
	const char *test[9];
	off_t 	start;
	off_t 	size;
};

static struct process_data process[NUM_THREADS];

static int part_test_device = 1;

static void psig(int sig)
{
	tperr("ptest interrupted (%s)\n", strsignal(sig));
	pthread_exit((void *)SD_ERR_USR);
}

static void *ptest(void *parm)
{	
	struct process_data *d = parm;
	struct process_parm *p = d->parm;
	int res = SD_ERR_NO;	
	
	d->pid = getpid();
	sd_debug("(pid %d)", d->pid);
	if (d->pid < 0)
		psig(0);

	while (1) {
#ifndef PRET
		int tok = '+', i;	
		char *in, *cr = p->test;

		if (!strcmp(p->test, aitem)) {
			/*
 			 * move 'interf' & 'selfd' test out of the loop
			 * for (i = 0; item[i]; i++) {
			 */
			for (i = 2; item[i]; i++) {
				char cmd[128];
				if (part_test_device) {
					if (p->opts)
						sprintf(cmd, 
						"%s -d %s -t %s %s -q 2 -s %lld -f %lld", 
						sdtest, p->device, item[i], p->opts, d->size, d->start);
					else
						sprintf(cmd, 
						"%s -d %s -t %s -q 2 -s %lld -f %lld", 
						sdtest, p->device, item[i], d->size, d->start);
				} else {
					if (p->opts)
						sprintf(cmd, 
						"%s -d %s -t %s %s -q 2", 
						sdtest, p->device, item[i], p->opts);
					else
						sprintf(cmd, 
						"%s -d %s -t %s -q 2", 
						sdtest, p->device, item[i]);
				}
				/* these tests must use sgio */
				if (i == 0 || i == 1 || i == 8)
					strcat(cmd, " -u");
				sd_debug("%s\n", cmd);
				res = system(cmd);
				if (res == -1)
					res = SD_ERR_SYS;
				else
					res = WEXITSTATUS(res);
				/* convert to error codes */
				if (res)
					res = errors[i]; 
				/* quit on interface and selfdiag error */
				if ((i == 0 || i == 1) && res != SD_ERR_NO)
					goto quit;
				if (p->sanity && res != SD_ERR_NO)
					break;
			}
		} else {
			for (i = 0; i < strlen(p->test); ) {
				char nm[8];
				int j = 0;

				in = strchr(cr, tok);
				if (in == NULL) {
					strcpy(nm, cr);
					i += strlen(cr);
				} else {
					strncpy(nm, cr, in - cr);
					if ((in - cr) > 0)
						nm[in - cr] = '\0';
					i += in - cr + 1;
					in += 1;
					cr = in;
				}
				sd_debug("get %s ", nm);
				/*
 				 * move 'interf' & 'selfd' test out of the loop
				 * for (j = 0; item[j]; j++) {
				 */
				for (j = 2; item[j]; j++) {
					if (strcmp(item[j], nm))
						continue;
					else
						break;
				}
				if (item[j]) {
					char cmd[128];
					if (part_test_device) {
						if (p->opts)
							sprintf(cmd, 
							"%s -d %s -t %s %s -q 2 -s %lld -f %lld", 
							sdtest, p->device, item[j], p->opts, d->size, d->start);
						else
							sprintf(cmd, 
							"%s -d %s -t %s -q 2 -s %lld -f %lld", 
							sdtest, p->device, item[j], d->size, d->start);
					} else {
						if (p->opts)
							sprintf(cmd, 
							"%s -d %s -t %s %s -q 2", 
							sdtest, p->device, item[j], p->opts);
						else
							sprintf(cmd, 
							"%s -d %s -t %s -q 2", 
							sdtest, p->device, item[j]);
					}
					/* these tests must use sgio */
					if (j == 0 || j == 1 || j == 8)
						strcat(cmd, " -u");
					sd_debug("%s\n", cmd);
					res = system(cmd);
					sd_debug("cmd res %d(%d)\n", res, (res & 0xff00) >> 8);
					if (res == -1)
						res = SD_ERR_SYS;
					else
						res = WEXITSTATUS(res);
				}
				/* convert to error codes */
				if (res)
					res = errors[j]; 
				/* quit on interface and selfdiag error */
				if ((j == 0 || j == 1) && res != SD_ERR_NO)
					goto quit;
				if (p->sanity && res != SD_ERR_NO)
					break;
			}
		}
#else
		int i;
		/*
 		 * move 'interf' & 'selfd' test out of the loop
		 * for (i = 0; i < 9; i++) {
		 */
		for (i = 2; i < 9; i++) {
			char cmd[128];
			if (!process[0].test[i])
				continue;
			if (part_test_device) {
				if (p->opts)
					sprintf(cmd, 
					"%s -d %s -t %s %s -q 2 -s %lld -f %lld", 
					sdtest, p->device, item[i], p->opts, d->size, d->start);
				else
					sprintf(cmd, 
					"%s -d %s -t %s -q 2 -s %lld -f %lld", 
					sdtest, p->device, item[i], d->size, d->start);
			} else {
				if (p->opts)
					sprintf(cmd, "%s -d %s -t %s %s -q 2", 
					sdtest, p->device, item[i], p->opts);
				else
					sprintf(cmd, "%s -d %s -t %s -q 2", 
					sdtest, p->device, item[i]);
			}
			/* these tests must use sgio */
			if (i == 0 || i == 1 || i == 8)
				strcat(cmd, " -u");
			sd_debug("%s\n", cmd);
			res = system(cmd);
			if (res == -1)
				res = SD_ERR_SYS;
			else
				res = WEXITSTATUS(res);
			/* convert to error codes */
			if (res)
				res = errors[i]; 
			/* quit on interface and selfdiag error */
			if ((i == 0 || i == 1) && res != SD_ERR_NO)
				goto quit;
			if (p->sanity && res != SD_ERR_NO)
				break;
		}
#endif
		if (p->sanity && res != SD_ERR_NO)
			break;
		if (d->sigs)
			break;
		d->res = res;

		usleep(1000);
	}
quit:
	pthread_exit((void *)(unsigned long)res);
}

static int init_thread(struct process_parm *p)
{	
	if (p->device && p->test && p->thread) {
		int tok = '+', i, j;
		char *in, *cr = p->test;

		/* important to preset the string */
		for (j = 0; item[j]; j++) 
			process[0].test[j] = "null";

		for (i = 0; i < strlen(p->test); ) {
			char nm[8];
			j = 0;

			in = strchr(cr, tok);
			if (in == NULL) {
				strcpy(nm, cr);
				i += strlen(cr);
			} else {
				strncpy(nm, cr, in - cr);
				if ((in - cr) > 0)
					nm[in - cr] = '\0';
				i += in - cr + 1;
				in += 1;
				cr = in;
			}
			sd_debug("get %s ", nm);
			for (j = 0; item[j]; j++) {
				if (strcmp(item[j], nm))
					continue;
				else
					break;
			}
			if (item[j])
				process[0].test[j] = item[j];
		}
		return 0;
	} else if (p->device && p->thread && !p->test) {
		int j;
		for (j = 0; item[j]; j++)
			process[0].test[j] = item[j];
		p->test = (char *)aitem;
		return 0;
	} else
		return 1;
}

static void exit_thread(struct process_parm *p)
{
	return;	
}

static void psighandler(int sig)
{
	int i;
	tperr("process interrupted (%s)\n", strsignal(sig));
	for (i = 0; process[i].parm; i++) {
		process[i].sigs++;
		pthread_join(process[i].thread, NULL);
	}
	exit(SD_ERR_USR);
}

int main(int argc, char **argv)
{
	FILE *fres = NULL;
	char fname[32], *na;
	const char *progname;
	struct process_parm parm;
	int i, ret = SD_ERR_NO;
	struct stat st;
	off_t size;

	if (stat(sdtest, &st) < 0) {
		tperr("please check %s\n", sdtest);
		exit(SD_ERR_USR);
	}

	progname = (const char *)strrchr(argv[0], '/');
	progname = progname ? (progname + 1) : argv[0];

	memset(&parm, 0, sizeof(struct process_parm));
	memset(process, 0, sizeof(struct process_data) * NUM_THREADS);

	while ((i = getopt(argc, argv, "d:t:r:spo:h")) != -1) {
		switch (i) {
		case 'd':
			parm.device = optarg;
			break;
		case 't':
			parm.test = optarg;
			break;
		case 'r':
			parm.thread = atoi(optarg);
			if (parm.thread < 0 || parm.thread > NUM_THREADS) {
				tperr("thread: bad value\n");
				exit(SD_ERR_USR);
			}
			break;
		case 's':
			parm.sanity = 1;
			break;
		case 'p':
			part_test_device = 1;
			break;
		case 'o':
			parm.opts = optarg;
			break;
		case 'h':
		default:
			tpout("usage: %s [-d device] [-t test] [-r thread] [-s] [-p] [-o optstring]\n", progname);
			exit(SD_ERR_NO);
		}
	}

	if (geteuid() != 0) {
		tperr("%s: must be run as root\n", progname);
		exit(SD_ERR_USR);
	}

	if (init_thread(&parm)) {
		tperr("init test thread failed\n");
		exit(SD_ERR_SYS);
	}

	if (part_test_device) {
		FILE *fp = NULL;
		char buf[80], *str;
		sprintf(buf, "%s -d %s -i", sdtest, parm.device);
		if (!(fp = popen(buf, "r"))) {
			tperr("get device info failed\n");
			exit(SD_ERR_SYS);
		}
		fgets(buf, sizeof(buf), fp);
		sd_debug("%s\n", buf);
		if (!(str = strstr(buf, "blocks:"))) {
			exit(SD_ERR_SYS);
		}
		if (sscanf(str, "blocks: %d size: %lld bytes", &i, &size) != 2) {
			exit(SD_ERR_SYS);
		}
		pclose(fp);
	}

        signal(SIGINT, psighandler);
        signal(SIGQUIT, psighandler);
        signal(SIGPIPE, psighandler);
        signal(SIGTERM, psighandler);

	tptout("%s: %s: %s: ...\n", progname, parm.device, parm.test);
	/*
	 * do 'interf' & 'selfd' test first before the main loop
	 */
	if (!strcmp(process[0].test[0], "interf")) {
		char cmd[128];

		sprintf(cmd, "%s -d %s -t %s -u", sdtest, parm.device, item[0]);
		ret = system(cmd);
		if (ret == -1)
			ret = SD_ERR_SYS;
		else
			ret = WEXITSTATUS(ret);
		if (ret) {
			ret = errors[0];
			tperr("%s: EXITED\n", progname);
			exit(ret);
		}
	}
	if (!strcmp(process[0].test[1], "selfd")) {
		char cmd[128];

		sprintf(cmd, "%s -d %s -t %s -u", sdtest, parm.device, item[1]);
		ret = system(cmd);
		if (ret == -1)
			ret = SD_ERR_SYS;
		else
			ret = WEXITSTATUS(ret);
		if (ret) {
			ret = errors[0];
			tperr("%s: EXITED\n", progname);
			exit(ret);
		}
	}

	for (i = 0; i < parm.thread; i++) {
		/* init thread */
		process[i].parm = &parm;
		if (part_test_device) {
			/* can't be larger than size of device */
			process[i].size  = size / parm.thread;
			process[i].start = 0 + (process[i].size * i);
		}
		pthread_attr_init(&process[i].attr);
		
		if (pthread_create(&process[i].thread, NULL, ptest, &process[i])) {
			tperr("pthread create: %s\n", strerror(errno));
			if (errno)
				ret = SD_ERR_SYS;
			break;
		}
	}
		
	usleep(1000);

	for (i = 0; i < parm.thread; i++) {
		if (pthread_join(process[i].thread, (void *)&ret)) {
			tperr("pthread join: %s\n", strerror(errno));
			if (errno)
				ret = SD_ERR_SYS;
			break;
		}
		
		/* exit thread */
		process[i].parm = NULL;
		pthread_attr_destroy(&process[i].attr);
	}
	tptout("%s: COMPLETED\n", progname);
	
	exit_thread(&parm);

	/* put ret value in .process-sdX before exit */
	na = (char *)strrchr(parm.device, '/');
	na = na ? (na + 1) : parm.device;
	sprintf(fname, ".process-%s", na);
	if (!(fres = fopen(fname, "w")))
		exit(ret);
	fputc(ret, fres);
	fclose(fres);
#if 0
	if (!(fres = fopen(fname, "r")))
		exit(ret);
	tpout("%d\n", fgetc(fres);
	fclose(fres);
	unlink(fname);
#endif

	exit(ret);
}
