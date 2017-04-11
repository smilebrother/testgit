/*
 * Copyright (c) 2013 HON HAI PRECISION IND.CO.,LTD. (FOXCONN)
 *
 */

#define _GNU_SOURCE /* O_DIRECT */
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>

#include "sg_lib.h"
#include "sg_cmds_basic.h"
#include "sg_io_linux.h"

#include "diskio.h"

#define RCAP16_REPLY_LEN 32
#define READ_CAP_REPLY_LEN 8
#define MAX_SCSI_CDBSZ 16
#define SENSE_BUFF_LEN 32

#define DEF_TIMEOUT 60000       /* 60,000 millisecs == 60 seconds */

#define ME "diskio: "

static int sum_of_resids = 0;
static int recovered_errs = 0;
static int unrecovered_errs = 0;
static int64_t in_full = 0;
static int64_t out_full = 0;

static int verbose = 0;
static int do_time = 0;
static int use_random_pattern = 1;
static int user_pattern;
static int start_tm_valid = 0;
static struct timeval start_tm;

static int64_t num_sect = -1;
static int sect_sz;

static char log_path[128] = { 0 };
static FILE *log_fp = NULL;

static void usage(void)
{
    fprintf(stderr, "\n\t%s\n", BANNER);

    fprintf(stderr, "\nDisk I/O stress program\n");
    fprintf(stderr, "Usage: "
            "diskio <device path> <mode> <parameters ...>\n"
            "\ndevice path: device file path\n"
            "\nmode:\n"
            "\tr    read test\n"
            "\tw    write test\n"
            "\twr   write/read test with data compare\n"
            "\nparameters:\n"
            "\t-b    transfer size per read/write transaction (default: 512 bytes)\n"
            "\t-i    iterations (default = 1, 0 = infinite)\n"
            "\t-d    duration in seconds (if iteration is 0)\n"
            "\t-m    test range in bytes (default: entire device)\n"
            "\t-p    user specified test pattern (default: random)\n"
            "\t-s    starting logical block address (default: 0)\n"
            "\t-t    calculate time and throughput\n"
            "\t-l    specify log file path (default: none)\n"
            "\t-v    verbose\n"
            "\n"
            );
    fprintf(stderr, "ex:\n"
            "\tdiskio /dev/sda wr -b=128k -m=32M -p=0xaaaa5555\n"
            "\tdiskio /dev/sdb w -b=256k -s=1000 -l=/tmp/diskio_log.txt\n");
}

static void open_log(void)
{
    if (log_path[0] != '\0') {
        log_fp = fopen(log_path, "w+");
        if (log_fp == NULL)
            fprintf(stderr, "Can not open log file %s\n", log_path);
    }
}

static void close_log(void)
{
    if (log_fp)
        fclose(log_fp);
}

static void log_message(const char *message)
{
    if (log_fp)
        fprintf(log_fp, "%s\n", message);
}

static void calc_duration_throughput(int contin)
{
    struct timeval end_tm, res_tm;
    double a, b;
    int64_t blks;

    if (start_tm_valid && (start_tm.tv_sec || start_tm.tv_usec)) {
        blks = (in_full > out_full) ? in_full : out_full;
        gettimeofday(&end_tm, NULL);
        res_tm.tv_sec = end_tm.tv_sec - start_tm.tv_sec;
        res_tm.tv_usec = end_tm.tv_usec - start_tm.tv_usec;
        if (res_tm.tv_usec < 0) {
            --res_tm.tv_sec;
            res_tm.tv_usec += 1000000;
        }
        a = res_tm.tv_sec;
        a += (0.000001 * res_tm.tv_usec);
        b = (double)sect_sz * blks;
        fprintf(stderr, "time to transfer data%s: %d.%06d secs",
                (contin ? " so far" : ""), (int)res_tm.tv_sec,
                (int)res_tm.tv_usec);
        if ((a > 0.00001) && (b > 511))
            fprintf(stderr, " at %.2f MB/sec\n", b / (a * 1000000.0));
        else
            fprintf(stderr, "\n");
    }
}

static int open_dev(const char *path, int verbose)
{
    int fd;

    fd = open(path, O_NONBLOCK | O_DIRECT | O_SYNC);

    return fd;
}

/* Return of 0 -> success, see sg_ll_read_capacity*() otherwise */
static int scsi_read_capacity(int fd, int64_t *num_sect, int *sect_sz)
{
    int k, res;
    unsigned int ui;
    unsigned char rcBuff[RCAP16_REPLY_LEN];

    res = sg_ll_readcap_10(fd, 0, 0, rcBuff, READ_CAP_REPLY_LEN, 0, verbose);
    if (0 != res)
        return res;

    if ((0xff == rcBuff[0]) && (0xff == rcBuff[1]) && (0xff == rcBuff[2]) &&
        (0xff == rcBuff[3])) {
        int64_t ls;

        res = sg_ll_readcap_16(fd, 0, 0, rcBuff, RCAP16_REPLY_LEN, 0,
                               verbose);
        if (0 != res)
            return res;
        for (k = 0, ls = 0; k < 8; ++k) {
            ls <<= 8;
            ls |= rcBuff[k];
        }
        *num_sect = ls + 1;
        *sect_sz = (rcBuff[8] << 24) | (rcBuff[9] << 16) |
                   (rcBuff[10] << 8) | rcBuff[11];
    } else {
        ui = ((rcBuff[0] << 24) | (rcBuff[1] << 16) | (rcBuff[2] << 8) |
              rcBuff[3]);
        /* take care not to sign extend values > 0x7fffffff */
        *num_sect = (int64_t)ui + 1;
        *sect_sz = (rcBuff[4] << 24) | (rcBuff[5] << 16) |
                   (rcBuff[6] << 8) | rcBuff[7];
    }

    if (verbose)
        fprintf(stderr, "      number of blocks=%"PRId64" [0x%"PRIx64"], "
                "block size=%d\n", *num_sect, *num_sect, *sect_sz);

    return 0;
}

static int build_scsi_cdb(unsigned char * cdbp, int cdb_sz,
        unsigned int blocks, int64_t start_block)
{
    /* reset cdb except op code */
    memset(&cdbp[1], 0, cdb_sz - 1);

    switch (cdb_sz) {
    case 6:
        cdbp[1] = (unsigned char)((start_block >> 16) & 0x1f);
        cdbp[2] = (unsigned char)((start_block >> 8) & 0xff);
        cdbp[3] = (unsigned char)(start_block & 0xff);
        cdbp[4] = (256 == blocks) ? 0 : (unsigned char)blocks;
        if (blocks > 256) {
            fprintf(stderr, ME "for 6 byte commands, maximum number of "
                            "blocks is 256\n");
            return 1;
        }
        if ((start_block + blocks - 1) & (~0x1fffff)) {
            fprintf(stderr, ME "for 6 byte commands, can't address blocks"
                            " beyond %d\n", 0x1fffff);
            return 1;
        }
        break;
    case 10:
        cdbp[2] = (unsigned char)((start_block >> 24) & 0xff);
        cdbp[3] = (unsigned char)((start_block >> 16) & 0xff);
        cdbp[4] = (unsigned char)((start_block >> 8) & 0xff);
        cdbp[5] = (unsigned char)(start_block & 0xff);
        cdbp[7] = (unsigned char)((blocks >> 8) & 0xff);
        cdbp[8] = (unsigned char)(blocks & 0xff);
        if (blocks & (~0xffff)) {
            fprintf(stderr, ME "for 10 byte commands, maximum number of "
                            "blocks is %d\n", 0xffff);
            return 1;
        }
        break;
    case 12:
        cdbp[2] = (unsigned char)((start_block >> 24) & 0xff);
        cdbp[3] = (unsigned char)((start_block >> 16) & 0xff);
        cdbp[4] = (unsigned char)((start_block >> 8) & 0xff);
        cdbp[5] = (unsigned char)(start_block & 0xff);
        cdbp[6] = (unsigned char)((blocks >> 24) & 0xff);
        cdbp[7] = (unsigned char)((blocks >> 16) & 0xff);
        cdbp[8] = (unsigned char)((blocks >> 8) & 0xff);
        cdbp[9] = (unsigned char)(blocks & 0xff);
        break;
    case 16:
        cdbp[2] = (unsigned char)((start_block >> 56) & 0xff);
        cdbp[3] = (unsigned char)((start_block >> 48) & 0xff);
        cdbp[4] = (unsigned char)((start_block >> 40) & 0xff);
        cdbp[5] = (unsigned char)((start_block >> 32) & 0xff);
        cdbp[6] = (unsigned char)((start_block >> 24) & 0xff);
        cdbp[7] = (unsigned char)((start_block >> 16) & 0xff);
        cdbp[8] = (unsigned char)((start_block >> 8) & 0xff);
        cdbp[9] = (unsigned char)(start_block & 0xff);
        cdbp[10] = (unsigned char)((blocks >> 24) & 0xff);
        cdbp[11] = (unsigned char)((blocks >> 16) & 0xff);
        cdbp[12] = (unsigned char)((blocks >> 8) & 0xff);
        cdbp[13] = (unsigned char)(blocks & 0xff);
        break;
    default:
        fprintf(stderr, ME "expected cdb size of 6, 10, 12, or 16 but got"
                        " %d\n", cdb_sz);
        return 1;
    }
    return 0;
}

static int scsi_read(int fd, void *buff,
        int blocks, int64_t from_block, int bs)
{
    unsigned char rdCmd[MAX_SCSI_CDBSZ];
    unsigned char senseBuff[SENSE_BUFF_LEN];
    const unsigned char * sbp;
    struct sg_io_hdr io_hdr;
    int res, k, info_valid, slen, cdb_sz;
    uint64_t io_addr;

    if (from_block > 0xffffffff) {
        rdCmd[0] = 0x88; /* read_16 */
        cdb_sz = 16;
    } else {
        rdCmd[0] = 0x28; /* read_10 */
        cdb_sz = 10;
    }

    if (build_scsi_cdb(rdCmd, cdb_sz, blocks, from_block)) {
        fprintf(stderr, ME "bad rd cdb build, from_block=%" PRId64
                ", blocks=%d\n", from_block, blocks);
        return SG_LIB_SYNTAX_ERROR;
    }

    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = cdb_sz;
    io_hdr.cmdp = rdCmd;
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = bs * blocks;
    io_hdr.dxferp = buff;
    io_hdr.mx_sb_len = SENSE_BUFF_LEN;
    io_hdr.sbp = senseBuff;
    io_hdr.timeout = DEF_TIMEOUT;
    io_hdr.pack_id = (int)from_block;
    io_hdr.flags |= SG_FLAG_DIRECT_IO; /* direct I/O */

    if (verbose > 2) {
        fprintf(stderr, "    read cdb: ");
        for (k = 0; k < cdb_sz; ++k)
            fprintf(stderr, "%02x ", rdCmd[k]);
        fprintf(stderr, "\n");
    }
    while (((res = ioctl(fd, SG_IO, &io_hdr)) < 0) && (EINTR == errno));

    if (res < 0) {
        if (ENOMEM == errno)
            return -2;
        perror("reading (SG_IO) on sg device, error");
        return -1;
    }
    if (verbose > 2)
        fprintf(stderr, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    sbp = io_hdr.sbp;
    slen = io_hdr.sb_len_wr;
    switch (res) {
    case SG_LIB_CAT_CLEAN:
        break;
    case SG_LIB_CAT_RECOVERED:
        ++recovered_errs;
        info_valid = sg_get_sense_info_fld(sbp, slen, &io_addr);
        if (info_valid) {
            fprintf(stderr, "    lba of last recovered error in this "
                    "READ=0x%"PRIx64"\n", io_addr);
            if (verbose > 1)
                sg_chk_n_print3("reading", &io_hdr, 1);
        } else {
            fprintf(stderr, "Recovered error: [no info] reading from "
                    "block=0x%"PRIx64", num=%d\n", from_block, blocks);
            sg_chk_n_print3("reading", &io_hdr, verbose > 1);
        }
        break;
    case SG_LIB_CAT_ABORTED_COMMAND:
    case SG_LIB_CAT_UNIT_ATTENTION:
        sg_chk_n_print3("reading", &io_hdr, verbose > 1);
        return res;
    case SG_LIB_CAT_MEDIUM_HARD:
        if (verbose > 1)
            sg_chk_n_print3("reading", &io_hdr, verbose > 1);
        ++unrecovered_errs;
        info_valid = sg_get_sense_info_fld(sbp, slen, &io_addr);
        if ((info_valid) && (io_addr > 0))
            return SG_LIB_CAT_MEDIUM_HARD_WITH_INFO;
        else {
            fprintf(stderr, "Medium, hardware or blank check error but "
                    "no lba of failure in sense\n");
            return res;
        }
        break;
    case SG_LIB_CAT_NOT_READY:
        ++unrecovered_errs;
        if (verbose > 0)
            sg_chk_n_print3("reading", &io_hdr, verbose > 1);
        return res;
    case SG_LIB_CAT_ILLEGAL_REQ:
        /* drop through */
    default:
        ++unrecovered_errs;
        if (verbose > 0)
            sg_chk_n_print3("reading", &io_hdr, verbose > 1);
        return res;
    }

    sum_of_resids += io_hdr.resid;
    in_full += blocks;
    return 0;
}

static int scsi_write(int fd, void *buff,
        int blocks, int64_t to_block, int bs)
{
    unsigned char wrCmd[MAX_SCSI_CDBSZ];
    unsigned char senseBuff[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;
    int res, k, info_valid, cdb_sz;
    uint64_t io_addr = 0;

    if (to_block > 0xffffffff) {
        wrCmd[0] = 0x8a; /* write_16 */
        cdb_sz = 16;
    } else {
        wrCmd[0] = 0x2a; /* write_10 */
        cdb_sz = 10;
    }

    if (build_scsi_cdb(wrCmd, cdb_sz, blocks, to_block)) {
        fprintf(stderr, ME "bad wr cdb build, to_block=%"PRId64", blocks=%d\n",
                to_block, blocks);
        return SG_LIB_SYNTAX_ERROR;
    }

    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = cdb_sz;
    io_hdr.cmdp = wrCmd;
    io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    io_hdr.dxfer_len = bs * blocks;
    io_hdr.dxferp = buff;
    io_hdr.mx_sb_len = SENSE_BUFF_LEN;
    io_hdr.sbp = senseBuff;
    io_hdr.timeout = DEF_TIMEOUT;
    io_hdr.pack_id = (int)to_block;
    io_hdr.flags |= SG_FLAG_DIRECT_IO; /* direct I/O */

    if (verbose > 2) {
        fprintf(stderr, "    write cdb: ");
        for (k = 0; k < cdb_sz; ++k)
            fprintf(stderr, "%02x ", wrCmd[k]);
        fprintf(stderr, "\n");
    }
    while (((res = ioctl(fd, SG_IO, &io_hdr)) < 0) && (EINTR == errno))
        ;
    if (res < 0) {
        if (ENOMEM == errno)
            return -2;
        perror("writing (SG_IO) on sg device, error");
        return -1;
    }

    if (verbose > 2)
        fprintf(stderr, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_CLEAN:
        break;
    case SG_LIB_CAT_RECOVERED:
        ++recovered_errs;
        info_valid = sg_get_sense_info_fld(io_hdr.sbp, io_hdr.sb_len_wr,
                                           &io_addr);
        if (info_valid) {
            fprintf(stderr, "    lba of last recovered error in this "
                    "WRITE=0x%"PRIx64"\n", io_addr);
            if (verbose > 1)
                sg_chk_n_print3("writing", &io_hdr, 1);
        } else {
            fprintf(stderr, "Recovered error: [no info] writing to "
                    "block=0x%"PRIx64", num=%d\n", to_block, blocks);
            sg_chk_n_print3("writing", &io_hdr, verbose > 1);
        }
        break;
    case SG_LIB_CAT_ABORTED_COMMAND:
    case SG_LIB_CAT_UNIT_ATTENTION:
        sg_chk_n_print3("writing", &io_hdr, verbose > 1);
        return res;
    case SG_LIB_CAT_NOT_READY:
        ++unrecovered_errs;
        fprintf(stderr, "device not ready (w)\n");
        return res;
    case SG_LIB_CAT_MEDIUM_HARD:
    default:
        sg_chk_n_print3("writing", &io_hdr, verbose > 1);
        ++unrecovered_errs;
        
        return res;
    }

    out_full += blocks;
    return 0;
}

static int __read_op(int fd, void *buff,
        int64_t start_lba, int total_blk, int xfer_blk)
{
    int res;
    int blocks;

    while (total_blk) {
        if (total_blk - xfer_blk > 0)
            blocks = xfer_blk;
        else
            blocks = total_blk;

        res = scsi_read(fd, buff, blocks, start_lba, sect_sz);
        if (res == SG_LIB_CAT_UNIT_ATTENTION)
            res = scsi_read(fd, buff, blocks, start_lba, sect_sz); /* retry */

        if (res)
            break;

        total_blk -= blocks;
        start_lba += blocks;
    }

    return res;
}

static int read_op(int fd, int64_t start_lba,
        int64_t test_sz, int xfer_sz, int iter, int duration)
{
    int res;
    void *wrkBuff;
    void *wrkPos;
    int64_t total_blk;
    int64_t xfer_blk;

    wrkBuff = malloc(xfer_sz + sysconf(_SC_PAGESIZE));
    if (wrkBuff == NULL) {
        fprintf(stderr, "Not enough user memory\n");
        return -1;
    }
    wrkPos = (void*) (((unsigned long)wrkBuff + sysconf(_SC_PAGESIZE) - 1)
            & (~(sysconf(_SC_PAGESIZE) - 1)));

    total_blk = test_sz/sect_sz;
    xfer_blk = xfer_sz/sect_sz;

    if (iter == 0)
        iter = UINT_MAX;

    if (do_time) {
        start_tm.tv_sec = 0;
        start_tm.tv_usec = 0;
        gettimeofday(&start_tm, NULL);
        start_tm_valid = 1;
    }

    while (iter) {
        
        res = __read_op(fd, wrkPos, start_lba, total_blk, xfer_blk);
        if (res)
            break;

        iter--;
        
        if (do_time && iter)
            calc_duration_throughput(1);
    }

    if (do_time)
        calc_duration_throughput(0);

    free(wrkBuff);

    return res;
}

static int __write_op(int fd, void *buff,
        int64_t start_lba, int total_blk, int xfer_blk)
{
    int res;
    int blocks;

    while (total_blk) {
        if (total_blk - xfer_blk > 0)
            blocks = xfer_blk;
        else
            blocks = total_blk;

        res = scsi_write(fd, buff, blocks, start_lba, sect_sz);
        if (res == SG_LIB_CAT_UNIT_ATTENTION)
            res = scsi_write(fd, buff, blocks, start_lba, sect_sz); /* retry */

        if (res)
            break;

        total_blk -= blocks;
        start_lba += blocks;
    }

    return res;
}

static int write_op(int fd, int64_t start_lba,
        int64_t test_sz, int xfer_sz, int iter, int duration)
{
    int res, i;
    void *wrkBuff;
    void *wrkPos;
    int64_t total_blk;
    int64_t xfer_blk;
    int seed;

    wrkBuff = malloc(xfer_sz + sysconf(_SC_PAGESIZE));
    if (wrkBuff == NULL) {
        fprintf(stderr, "Not enough user memory\n");
        return -1;
    }
    wrkPos = (void*) (((unsigned long)wrkBuff + sysconf(_SC_PAGESIZE) - 1)
            & (~(sysconf(_SC_PAGESIZE) - 1)));

    /* fill in test pattern */
    seed = time(NULL);
    srand(seed);
    for (i = 0; i < (xfer_sz/4); i++) {
        if (use_random_pattern)
            *(((int*)wrkPos) + i) = rand(); 
        else
            *(((int*)wrkPos) + i) = user_pattern;
    }

    total_blk = test_sz/sect_sz;
    xfer_blk = xfer_sz/sect_sz;

    if (iter == 0)
        iter = UINT_MAX;

    if (do_time) {
        start_tm.tv_sec = 0;
        start_tm.tv_usec = 0;
        gettimeofday(&start_tm, NULL);
        start_tm_valid = 1;
    }

    while (iter) {
        
        res = __write_op(fd, wrkPos, start_lba, total_blk, xfer_blk);
        if (res)
            break;

        iter--;
        
        if (do_time && iter)
            calc_duration_throughput(1);
    }

    if (do_time)
        calc_duration_throughput(0);

    free(wrkBuff);

    return res;
}

static int read_and_compare(int fd, void *buff,
        int64_t start_lba, int total_blk, int xfer_blk, void *data)
{
    int res;
    int blocks;

    while (total_blk) {
        if (total_blk - xfer_blk > 0)
            blocks = xfer_blk;
        else
            blocks = total_blk;

        memset(buff, 0, (xfer_blk*sect_sz));
        res = scsi_read(fd, buff, blocks, start_lba, sect_sz);
        if (res == SG_LIB_CAT_UNIT_ATTENTION)
            res = scsi_read(fd, buff, blocks, start_lba, sect_sz); /* retry */

        if (res)
            break;

        if (memcmp(buff, data, (blocks*sect_sz))) {
            fprintf(stderr, "ERROR: data compare error, start LBA %"PRId64"\n", start_lba);
            res = -1;
            break;
        }

        total_blk -= blocks;
        start_lba += blocks;
    }

    return res;
}

static int write_read_op(int fd, int64_t start_lba,
        int64_t test_sz, int xfer_sz, int iter, int duration)
{
    int res, i;
    void *wrBuff;
    void *wrPos;
    void *rdBuff;
    void *rdPos;
    int64_t total_blk;
    int64_t xfer_blk;
    int seed;
    int start_time;
    int current_time;

	int64_t ptr_lba;
	int blocks;

    wrBuff = malloc(xfer_sz + sysconf(_SC_PAGESIZE));
    if (wrBuff == NULL) {
        fprintf(stderr, "Not enough user memory\n");
        return -1;
    }
    wrPos = (void*) (((unsigned long)wrBuff + sysconf(_SC_PAGESIZE) - 1)
            & (~(sysconf(_SC_PAGESIZE) - 1)));

    rdBuff = malloc(xfer_sz + sysconf(_SC_PAGESIZE));
    if (rdBuff == NULL) {
        fprintf(stderr, "Not enough user memory\n");
        free(wrBuff);
        return -1;
    }
    rdPos = (void*) (((unsigned long)rdBuff + sysconf(_SC_PAGESIZE) - 1)
            & (~(sysconf(_SC_PAGESIZE) - 1)));

    /* fill in test pattern */
    seed = time(NULL);
    srand(seed);
    for (i = 0; i < (xfer_sz/4); i++) {
        if (use_random_pattern)
            *(((int*)wrPos) + i) = rand(); 
        else
            *(((int*)wrPos) + i) = user_pattern;
    }

	start_time = time((time_t *)NULL);

	if (iter == 0)
        	iter = UINT_MAX;


	while (iter) {
		ptr_lba = start_lba;
		total_blk = test_sz/sect_sz;
		xfer_blk = xfer_sz/sect_sz;

		while(total_blk) {       
 			if (total_blk - xfer_blk > 0)
				blocks = xfer_blk;
			else
				blocks = total_blk;

	        	res = __write_op(fd, wrPos, ptr_lba, blocks, xfer_blk);
		        if (res) {
	        		fprintf(log_fp, "error: scsi_write() res=0x%x lba=0x%x\n", res, ptr_lba);
				goto error_out;
			}

		        res = read_and_compare(fd, rdPos, ptr_lba, blocks, xfer_blk, wrPos);
		        if (res) {
	        		fprintf(log_fp, "error: scsi_read() res=0x%x lba=0x%x\n", res, ptr_lba);
				goto error_out;
			}

			ptr_lba += xfer_blk;
			total_blk -= xfer_blk;
	
			if(-1 != duration) {
				current_time = time((time_t *) NULL);
				if(current_time > (start_time + duration)) {
					goto error_out;
				}
			}	
		}
	iter--;
	}

error_out:
    free(wrBuff);
    free(rdBuff);

    return res;
}

int main(int argc, char **argv)
{
    int i, res;
    int fd;
    char str[STR_SZ];
    char *key;
    char *buf;
    char dev_path[128];
    int mode;
    int xfer_sz = DEFAULT_XFER_SIZE;
    int64_t test_sz = 0;
    int iter = DEFAULT_ITERATIONS;
    int64_t lba = 0;
    int duration = -1;
    
    if (argc < 3) {
        usage();
        return 1;
    }
	
	pritnf("GitTest2\n");
	pritnf("GitTest2\n");
	pritnf("GitTest2\n");
	pritnf("GitTest2\n");

	printf("GitTest1\n");
	printf("GitTest1\n");
	printf("GitTest1\n");
	printf("GitTest1\n");

    strncpy(dev_path, argv[1], sizeof(dev_path));

    if (!strcmp(argv[2], "r")) {
        mode = READ_OP;
    } else if (!strcmp(argv[2], "w")) {
        mode = WRITE_OP;
    } else if (!strcmp(argv[2], "wr")) {
        mode = WRITE_READ_OP;
    } else {
        fprintf(stderr, "invalid test mode (%s)\n", argv[2]);
        return 1;
    }

    for (i = 3; i < argc; i++) {
        if (argv[i]) {
            strncpy(str, argv[i], STR_SZ);
            str[STR_SZ - 1] = '\0';
        } else
            continue;
		
        for (key = str, buf = key; *buf && *buf != '=';)
            buf++;
        if (*buf)
            *buf++ = '\0';
        if (!strcmp(key, "-b")) {
            xfer_sz = sg_get_num(buf);
            if (-1 == xfer_sz) {
                fprintf(stderr, "bad argument\n");
                return -1;
            }
        } else if (!strcmp(key, "-m")) {
            test_sz = sg_get_llnum(buf);
            if (-1 == test_sz) {
                fprintf(stderr, "bad argument\n");
                return -1;
            }
        } else if (!strcmp(key, "-i")) {
            iter = sg_get_num(buf);
            if (-1 == iter) {
                fprintf(stderr, "bad argument\n");
                return -1;
            }
        } else if (!strcmp(key, "-s")) {
            lba = sg_get_llnum(buf);
            if (-1LL == lba) {
                fprintf(stderr, "bad argument\n");
                return -1;
            }
        } else if (!strcmp(key, "-t")) {
            do_time = 1;
        } else if (!strcmp(key, "-p")) {
            user_pattern = sg_get_num(buf);
            use_random_pattern = 0;
        } else if (!strcmp(key, "-v")) {
            verbose = 1;
        } else if (!strcmp(key, "-l")) {
            strncpy(log_path, buf, sizeof(log_path));
        } else if (!strcmp(key, "debug")) {
            verbose = sg_get_llnum(buf);
        } else if (!strcmp(key, "-d")) {
            duration = sg_get_num(buf);
            if (-1 == duration) {
                fprintf(stderr, "bad argument\n");
                return -1;
            }
		}
    }


    fd = open_dev(dev_path, verbose);
    if (fd < 0) {
        fprintf(stderr, "Can not open device: %s\n", dev_path);
        return fd;
    }

    open_log();

    res = scsi_read_capacity(fd, &num_sect, &sect_sz);
    if (SG_LIB_CAT_UNIT_ATTENTION == res) {
        fprintf(stderr, "Unit attention, continuing\n");
        res = scsi_read_capacity(fd, &num_sect, &sect_sz);
    }

    if (!test_sz)
        test_sz = num_sect * sect_sz;
    else if (test_sz > (num_sect*sect_sz)) {
        fprintf(stderr, "Test range (%lld) out of disk size (%lld)\n",
                test_sz, num_sect*sect_sz);
        return -1;
    }
    if (verbose)
        fprintf(stderr, "Test range: %lld bytes\n", test_sz);


    switch (mode) {
    case READ_OP:

        res = read_op(fd, lba, test_sz, xfer_sz, iter, duration);
        break;
    case WRITE_OP:

        res = write_op(fd, lba, test_sz, xfer_sz, iter, duration);
        break;
    case WRITE_READ_OP:

        res = write_read_op(fd, lba, test_sz, xfer_sz, iter, duration);
        break;
    default:
        break;
    }

    close(fd);

    if (verbose) {
        fprintf(stderr, "%"PRId64" records in\n", in_full);
        fprintf(stderr, "%"PRId64" records out\n", out_full);
        fprintf(stderr, "%d recovered errors\n", recovered_errs);
        fprintf(stderr, "%d unrecovered errors\n", unrecovered_errs);
        fprintf(stderr, "%d sum_of_resids\n", sum_of_resids);
	if (log_fp) {
	        fprintf(log_fp, "%"PRId64" records in\n", in_full);
	        fprintf(log_fp, "%"PRId64" records out\n", out_full);
	        fprintf(log_fp, "%d recovered errors\n", recovered_errs);
	        fprintf(log_fp, "%d unrecovered errors\n", unrecovered_errs);
	        fprintf(log_fp, "%d sum_of_resids\n", sum_of_resids);

	}
    }

	if (!res) {
		log_message("********** PASS **********");
	}
	else {
		log_message("********* FAILED *********");
	}

    close_log();

    return 0;
}
