/* io_sg.c
 *
 * Author: Xiang-Yu Wang <rain_wang@jabil.com>
 *
 * Copyright (c) 2008 Jabil, Inc.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 2008-01-17 derived from the 'sg3_utils', made initial version
 * 2008-02-22 accommodate the sd_err, with -1 as return value
 * 2008-03-12 fixed a bug in doing checksum of interf buffer test, found
 *            the read buffer command will fail frequently if we use a buffer 
 *            size larger than 512k but less than the full capacity of device 
 *            buffer size (e.g., 16m). so we set the test buffer size as
 *            512k simply
 * 2008-03-14 added 'bsget_sg' and 'blkget_sg' in
 * 2008-06-11 tuned the buffer size in 'interf' test to 128k to fit various
 *            types of hard drives we tested.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>

#include <scsi/sg_lib.h>
#include <scsi/sg_io_linux.h>
#include <scsi/sg_cmds_basic.h>
#include <scsi/sg_cmds_extra.h>

#include "sdtest.h"
#include "utils.h"

#define DEF_SCSI_CDBSZ 10
#define MAX_SCSI_CDBSZ 16

#ifndef SG_FLAG_MMAP_IO
#define SG_FLAG_MMAP_IO 4
#endif

#define SENSE_BUFF_LEN 32       /* Arbitrary, could be larger */
#define DEF_TIMEOUT 40000       /* 40,000 millisecs == 40 seconds */

#define MIN_RESERVED_SIZE 8192

static int verbose = 0;
static int pack_id_count = 0;
static int sum_of_resids = 0;

#define BPI (signed)(sizeof(int))

#define RB_MODE_DESC 3
#define RWB_MODE_DATA 2 
#define RB_DESC_LEN 4

static int base = 0x12345678;
static int buf_capacity = 0;
static int buf_granul = 255;
static unsigned char *cmpbuf = NULL;

/*
 * don't use scsi command seek for it's been obsolete in T10 and T13
 */
static const int cmdseek = 0;

static int sg_build_scsi_cdb(unsigned char *cdbp, int cdb_sz,
                             unsigned int blocks, long long start_block,
                             int write_true, int fua, int dpo)
{
	int rd_opcode[] = {0x8, 0x28, 0xa8, 0x88};
	int wr_opcode[] = {0xa, 0x2a, 0xaa, 0x8a};
	int sz_ind;

	memset(cdbp, 0, cdb_sz);
	if (dpo)
               	cdbp[1] |= 0x10;
	if (fua)
               	cdbp[1] |= 0x8;
	switch (cdb_sz) {
	case 6:
               	sz_ind = 0;
               	cdbp[0] = (unsigned char)(write_true ? wr_opcode[sz_ind] :
                                          rd_opcode[sz_ind]);
               	cdbp[1] = (unsigned char)((start_block >> 16) & 0x1f);
               	cdbp[2] = (unsigned char)((start_block >> 8) & 0xff);
               	cdbp[3] = (unsigned char)(start_block & 0xff);
               	cdbp[4] = (256 == blocks) ? 0 : (unsigned char)blocks;
               	if (blocks > 256) {
               		tperr("for 6 byte commands, maximum number of blocks is 256\n");
                	return 1;
               	}
               	if ((start_block + blocks - 1) & (~0x1fffff)) {
               		tperr("for 6 byte commands, can't address blocks beyond %d\n", 0x1fffff);
               		return 1;
               	}
               	if (dpo || fua) {
               		tperr("for 6 byte commands, neither dpo nor fua bits supported\n");
               		return 1;
               	}
		break;
	case 10:
               	sz_ind = 1;
               	cdbp[0] = (unsigned char)(write_true ? wr_opcode[sz_ind] :
                                          rd_opcode[sz_ind]);
               	cdbp[2] = (unsigned char)((start_block >> 24) & 0xff);
               	cdbp[3] = (unsigned char)((start_block >> 16) & 0xff);
               	cdbp[4] = (unsigned char)((start_block >> 8) & 0xff);
               	cdbp[5] = (unsigned char)(start_block & 0xff);
               	cdbp[7] = (unsigned char)((blocks >> 8) & 0xff);
               	cdbp[8] = (unsigned char)(blocks & 0xff);
               	if (blocks & (~0xffff)) {
               		tperr("for 10 byte commands, maximum number of blocks is %d\n", 0xffff);
		        return 1;
               	}
               	break;
	case 12:
               	sz_ind = 2;
               	cdbp[0] = (unsigned char)(write_true ? wr_opcode[sz_ind] :
                                          rd_opcode[sz_ind]);
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
               	sz_ind = 3;
               	cdbp[0] = (unsigned char)(write_true ? wr_opcode[sz_ind] :
                                          rd_opcode[sz_ind]);
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
		tperr("expected cdb size of 6, 10, 12, or 16 but got %d\n", cdb_sz);
               	return 1;
	}
	return 0;
}

/* -3 medium/hardware error, -2 -> not ready, 0 -> successful,
   1 -> recoverable (ENOMEM), 2 -> try again (e.g. unit attention),
   3 -> try again (e.g. aborted command), -1 -> other unrecoverable error */
static int sg_bread(int sg_fd, unsigned char *buff, int blocks,
                    long long from_block, int bs, int cdbsz,
                    int fua, int dpo, int *diop, int do_mmap,
                    int no_dxfer)
{
	int k;
	unsigned char rdCmd[MAX_SCSI_CDBSZ];
	unsigned char senseBuff[SENSE_BUFF_LEN];
	struct sg_io_hdr io_hdr;

	if (sg_build_scsi_cdb(rdCmd, cdbsz, blocks, from_block, 0, fua, dpo)) {
		tperr("bad cdb build, from_block=%lld, blocks=%d\n", from_block, blocks);
		return -1;
	}
        memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
        io_hdr.interface_id = 'S';
        io_hdr.cmd_len = cdbsz;
        io_hdr.cmdp = rdCmd;
        if (blocks > 0) {
                io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
                io_hdr.dxfer_len = bs * blocks;
                if (! do_mmap) /* not required: shows dxferp unused during mmap-ed IO */
                	io_hdr.dxferp = buff;
                if (diop && *diop)
                	io_hdr.flags |= SG_FLAG_DIRECT_IO;
                else if (do_mmap)
                	io_hdr.flags |= SG_FLAG_MMAP_IO;
                else if (no_dxfer)
                	io_hdr.flags |= SG_FLAG_NO_DXFER;
        } else
                io_hdr.dxfer_direction = SG_DXFER_NONE;
        io_hdr.mx_sb_len = SENSE_BUFF_LEN;
        io_hdr.sbp = senseBuff;
        io_hdr.timeout = DEF_TIMEOUT;
        io_hdr.pack_id = pack_id_count++;
        if (verbose > 1) {
                tperr("    read cdb: ");
                for (k = 0; k < cdbsz; ++k)
	                tperr("%02x ", rdCmd[k]);
                tperr("\n");
        }

        if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
                if (ENOMEM == errno)
	                return 1;
                perror("reading (SG_IO) on sg device, error");
                return -1;
        }

        if (verbose > 2)
                tperr("      duration=%u ms\n", io_hdr.duration);
        switch (sg_err_category3(&io_hdr)) {
        case SG_LIB_CAT_RECOVERED:
                if (verbose > 1)
                        sg_chk_n_print3("reading, continue", &io_hdr, 1);
                /* fall through */
        case SG_LIB_CAT_CLEAN:
                break;
        case SG_LIB_CAT_UNIT_ATTENTION:
                if (verbose)
                	sg_chk_n_print3("reading", &io_hdr, verbose - 1);
                return 2;
        case SG_LIB_CAT_ABORTED_COMMAND:
                if (verbose)
                	sg_chk_n_print3("reading", &io_hdr, verbose - 1);
                return 3;
        case SG_LIB_CAT_NOT_READY:
                if (verbose)
                	sg_chk_n_print3("reading", &io_hdr, verbose - 1);
                return -2;
        case SG_LIB_CAT_MEDIUM_HARD:
                if (verbose)
                	sg_chk_n_print3("reading", &io_hdr, verbose - 1);
                return -3;
        default:
                sg_chk_n_print3("reading", &io_hdr, verbose);
                return -1;
        }
        if (blocks > 0) {
                if (diop && *diop && 
                  ((io_hdr.info & SG_INFO_DIRECT_IO_MASK) != SG_INFO_DIRECT_IO))
	                *diop = 0; /* flag that dio not done (completely) */
                sum_of_resids += io_hdr.resid;
        }
        return 0;
}

/* -3 medium/hardware error, -2 -> not ready, 0 -> successful,
   1 -> recoverable (ENOMEM), 2 -> try again (e.g. unit attention),
   3 -> try again (e.g. aborted command), -1 -> other unrecoverable error */
static int sg_bwrite(int sg_fd, unsigned char *buff, int blocks,
                     long long from_block, int bs, int cdbsz,
                     int fua, int dpo, int *diop, int do_mmap,
                     int no_dxfer)
{
        int k;
        unsigned char wrCmd[MAX_SCSI_CDBSZ];
        unsigned char senseBuff[SENSE_BUFF_LEN];
        struct sg_io_hdr io_hdr;

        if (sg_build_scsi_cdb(wrCmd, cdbsz, blocks, from_block, 1, fua, dpo)) {
	        tperr("bad cdb build, from_block=%lld, blocks=%d\n", from_block, blocks);
                return -1;
        }
        memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
        io_hdr.interface_id = 'S';
        io_hdr.cmd_len = cdbsz;
        io_hdr.cmdp = wrCmd;
        if (blocks > 0) {
                io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
                io_hdr.dxfer_len = bs * blocks;
                if (! do_mmap) /* not required: shows dxferp unused during mmap-ed IO */
                	io_hdr.dxferp = buff;
                if (diop && *diop)
                	io_hdr.flags |= SG_FLAG_DIRECT_IO;
                else if (do_mmap)
                	io_hdr.flags |= SG_FLAG_MMAP_IO;
                else if (no_dxfer)
                	io_hdr.flags |= SG_FLAG_NO_DXFER;
        } else
                io_hdr.dxfer_direction = SG_DXFER_NONE;
        io_hdr.mx_sb_len = SENSE_BUFF_LEN;
        io_hdr.sbp = senseBuff;
        io_hdr.timeout = DEF_TIMEOUT;
        io_hdr.pack_id = pack_id_count++;
        if (verbose > 1) {
                tperr("    write cdb: ");
                for (k = 0; k < cdbsz; ++k)
	                tperr("%02x ", wrCmd[k]);
                tperr("\n");
        }

        if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
                if (ENOMEM == errno)
	                return 1;
                perror("writing (SG_IO) on sg device, error");
                return -1;
        }

        if (verbose > 2)
                tperr("      duration=%u ms\n", io_hdr.duration);
        switch (sg_err_category3(&io_hdr)) {
        case SG_LIB_CAT_RECOVERED:
                if (verbose > 1)
                        sg_chk_n_print3("writing, continue", &io_hdr, 1);
                /* fall through */
        case SG_LIB_CAT_CLEAN:
                break;
        case SG_LIB_CAT_UNIT_ATTENTION:
                if (verbose)
                	sg_chk_n_print3("writing", &io_hdr, verbose - 1);
                return 2;
        case SG_LIB_CAT_ABORTED_COMMAND:
                if (verbose)
                	sg_chk_n_print3("writing", &io_hdr, verbose - 1);
                return 3;
        case SG_LIB_CAT_NOT_READY:
                if (verbose)
                	sg_chk_n_print3("writing", &io_hdr, verbose - 1);
                return -2;
        case SG_LIB_CAT_MEDIUM_HARD:
                if (verbose)
                	sg_chk_n_print3("writing", &io_hdr, verbose - 1);
                return -3;
        default:
                sg_chk_n_print3("writing", &io_hdr, verbose);
                return -1;
        }
        if (blocks > 0) {
                if (diop && *diop && 
                 ((io_hdr.info & SG_INFO_DIRECT_IO_MASK) != SG_INFO_DIRECT_IO))
                	*diop = 0; /* flag that dio not done (completely) */
                sum_of_resids += io_hdr.resid;
        }
        return 0;
}

static int read_sg(struct sd_device *sd, void *buf, size_t size)
{
        int do_dio = 0;
        int do_mmap = 0;
        int no_dxfer = 0;
        int fua = 0;
        int dpo = 0;
        int scsi_cdbsz = DEF_SCSI_CDBSZ;
        int dio_incomplete = 0;
        int res, buf_sz, dio_tmp, i;
        int n_blocks, blocks, blocks_per;
	int ret = SD_ERR_NO;

		n_blocks = size / sd->bs + ((size % sd->bs) ? 1 : 0);
        blocks_per = sd->parm->blocks;

        for (i = 0; n_blocks != 0; ++i) {
                if (n_blocks < 0)
                	blocks = 0;
                else
                	blocks = (n_blocks > blocks_per) ? blocks_per : n_blocks;
				
                dio_tmp = do_dio;
                res = sg_bread(sd->fd, buf, blocks, sd->pos, sd->bs, scsi_cdbsz, fua, dpo, &dio_tmp, do_mmap, no_dxfer);
                if (1 == res) {     /* ENOMEM, find what's available+try that */
                       	if (ioctl(sd->fd, SG_GET_RESERVED_SIZE, &buf_sz) < 0) {
                       		perror("RESERVED_SIZE ioctls failed");
                       		break;
                	}
                	if (buf_sz < MIN_RESERVED_SIZE)
                       		buf_sz = MIN_RESERVED_SIZE;
                	blocks_per = (buf_sz + sd->bs - 1) / sd->bs;
                	blocks = blocks_per;
                	tperr("Reducing read to %d blocks per loop\n", blocks_per);
                	res = sg_bread(sd->fd, buf, blocks, sd->pos, sd->bs, scsi_cdbsz, fua, dpo, &dio_tmp, do_mmap, no_dxfer);
                } else if (2 == res) {
                	tperr("Unit attention, try again (r)\n");
                       	res = sg_bread(sd->fd, buf, blocks, sd->pos, sd->bs, scsi_cdbsz, fua, dpo, &dio_tmp, do_mmap, no_dxfer);
               	}
                if (0 != res) {
						ret = SD_ERR;
                       	switch (res) {
                       	case -3:
                       		sd->stat = SG_LIB_CAT_MEDIUM_HARD;
                       		tperr("SCSI READ medium/hardware error\n");
                       		break;
                       	case -2:
                       		sd->stat = SG_LIB_CAT_NOT_READY;
                       		tperr("device not ready\n");
                       		break;
                       	case 2:
                       		sd->stat = SG_LIB_CAT_UNIT_ATTENTION;
                       		tperr("SCSI READ unit attention\n");
                       		break;
                       	case 3:
                       		sd->stat = SG_LIB_CAT_ABORTED_COMMAND;
                       		tperr("SCSI READ aborted command\n");
                       		break;
                       	default:
                       		sd->stat = SG_LIB_CAT_OTHER;
                       		tperr("SCSI READ failed\n");
							sd_debug("sd->pos %ld\n", sd->pos);
                       		break;
                       	}
						sd->stat = SD_ERR_SGIO;
                	break;
                } else {
                       	if (do_dio && (0 == dio_tmp))
                       		dio_incomplete++;
                }
				sd->pos += blocks;
				
                if (n_blocks > 0)
                	n_blocks -= blocks;
                else if (n_blocks < 0)
                	++n_blocks;
        }
	return ret;
}

static int write_sg(struct sd_device *sd, void *buf, size_t size)
{
        int do_dio = 0;
        int do_mmap = 0;
        int no_dxfer = 0;
        int fua = 0;
        int dpo = 0;
        int scsi_cdbsz = DEF_SCSI_CDBSZ;
        int dio_incomplete = 0;
        int res, buf_sz, dio_tmp, i;
        int n_blocks, blocks, blocks_per;
	int ret = SD_ERR_NO;

	n_blocks = size / sd->bs + ((size % sd->bs) ? 1 : 0);
        blocks_per = sd->parm->blocks;

        for (i = 0; n_blocks != 0; ++i) {
                if (n_blocks < 0)
                	blocks = 0;
                else
                	blocks = (n_blocks > blocks_per) ? blocks_per : n_blocks;
                dio_tmp = do_dio;
                res = sg_bwrite(sd->fd, buf, blocks, sd->pos, sd->bs, scsi_cdbsz, fua, dpo, &dio_tmp, do_mmap, no_dxfer);
                if (1 == res) {     /* ENOMEM, find what's available+try that */
                       	if (ioctl(sd->fd, SG_GET_RESERVED_SIZE, &buf_sz) < 0) {
                       		perror("RESERVED_SIZE ioctls failed");
                       		break;
                       	}
                       	if (buf_sz < MIN_RESERVED_SIZE)
                       		buf_sz = MIN_RESERVED_SIZE;
                       	blocks_per = (buf_sz + sd->bs - 1) / sd->bs;
                       	blocks = blocks_per;
                       	tperr("Reducing write to %d blocks per loop\n", blocks_per);
                	res = sg_bwrite(sd->fd, buf, blocks, sd->pos, sd->bs, scsi_cdbsz, fua, dpo, &dio_tmp, do_mmap, no_dxfer);
                } else if (2 == res) {
                       	tperr("Unit attention, try again (w)\n");
                       	res = sg_bwrite(sd->fd, buf, blocks, sd->pos, sd->bs, scsi_cdbsz, fua, dpo, &dio_tmp, do_mmap, no_dxfer);
                }
                if (0 != res) {
			ret = SD_ERR;
                       	switch (res) {
                       	case -3:
                       		sd->stat = SG_LIB_CAT_MEDIUM_HARD;
                       		tperr("SCSI WRITE medium/hardware error\n");
                       		break;
                       	case -2:
                      		sd->stat = SG_LIB_CAT_NOT_READY;
                       		tperr("device not ready\n");
                       		break;
                       	case 2:
                       		sd->stat = SG_LIB_CAT_UNIT_ATTENTION;
                       		tperr("SCSI WRITE unit attention\n");
                       		break;
                       	case 3:
                       		sd->stat = SG_LIB_CAT_ABORTED_COMMAND;
                       		tperr("SCSI WRITE aborted command\n");
                       		break;
                       	default:
                       		sd->stat = SG_LIB_CAT_OTHER;
                       		tperr("SCSI WRITE failed\n");
				sd_debug("sd->pos %ld\n", sd->pos);
                       		break;
                       	}
			sd->stat = SD_ERR_SGIO;
                       	break;
                } else {
                       	if (do_dio && (0 == dio_tmp))
                       		dio_incomplete++;
                }
		sd->pos += blocks;
                if (n_blocks > 0)
                	n_blocks -= blocks;
                else if (n_blocks < 0)
                	++n_blocks;
        }
	return ret;
}

static int sg_build_seek_cdb(unsigned char *cdbp, int cdb_sz, 
				long long start_block)
{
	int s_opcode[] = {0xb, 0x2b};
	int sz_ind;

	memset(cdbp, 0, cdb_sz);
	switch (cdb_sz) {
	case 6:
               	sz_ind = 0;
               	cdbp[0] = (unsigned char)(s_opcode[sz_ind]);
               	cdbp[1] = (unsigned char)((start_block >> 16) & 0x1f);
               	cdbp[2] = (unsigned char)((start_block >> 8) & 0xff);
               	cdbp[3] = (unsigned char)(start_block & 0xff);
		break;
	case 10:
               	sz_ind = 1;
               	cdbp[0] = (unsigned char)(s_opcode[sz_ind]);
               	cdbp[2] = (unsigned char)((start_block >> 24) & 0xff);
               	cdbp[3] = (unsigned char)((start_block >> 16) & 0xff);
               	cdbp[4] = (unsigned char)((start_block >> 8) & 0xff);
               	cdbp[5] = (unsigned char)(start_block & 0xff);
               	break;
	default:
		tperr("expected cdb size of 6, 10 but got %d\n", cdb_sz);
               	return 1;
	}
	return 0;
}

/* -3 medium/hardware error, -2 -> not ready, 0 -> successful,
   1 -> recoverable (ENOMEM), 2 -> try again (e.g. unit attention),
   3 -> try again (e.g. aborted command), -1 -> other unrecoverable error */
static int sg_bseek(int sg_fd, long long from_block, int bs, int cdbsz)
{
	int k;
	unsigned char sCmd[MAX_SCSI_CDBSZ];
	unsigned char senseBuff[SENSE_BUFF_LEN];
	struct sg_io_hdr io_hdr;

	if (sg_build_seek_cdb(sCmd, cdbsz, from_block)) {
		tperr("bad cdb build, from_block=%lld\n", from_block);
		return -1;
	}
        memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
        io_hdr.interface_id = 'S';
        io_hdr.cmd_len = cdbsz;
        io_hdr.cmdp = sCmd;
        io_hdr.dxfer_direction = SG_DXFER_NONE;
        io_hdr.mx_sb_len = SENSE_BUFF_LEN;
        io_hdr.sbp = senseBuff;
        io_hdr.timeout = DEF_TIMEOUT;
        io_hdr.pack_id = pack_id_count++;
        if (verbose > 1) {
                tperr("    seek cdb: ");
                for (k = 0; k < cdbsz; ++k)
	                tperr("%02x ", sCmd[k]);
                tperr("\n");
        }

        if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
                if (ENOMEM == errno)
	                return 1;
                perror("seeking (SG_IO) on sg device, error");
                return -1;
        }

        if (verbose > 2)
                tperr("      duration=%u ms\n", io_hdr.duration);
        switch (sg_err_category3(&io_hdr)) {
        case SG_LIB_CAT_RECOVERED:
                if (verbose > 1)
                        sg_chk_n_print3("seeking, continue", &io_hdr, 1);
                /* fall through */
        case SG_LIB_CAT_CLEAN:
                break;
        case SG_LIB_CAT_UNIT_ATTENTION:
                if (verbose)
                	sg_chk_n_print3("seeking", &io_hdr, verbose - 1);
                return 2;
        case SG_LIB_CAT_ABORTED_COMMAND:
                if (verbose)
                	sg_chk_n_print3("seeking", &io_hdr, verbose - 1);
                return 3;
        case SG_LIB_CAT_NOT_READY:
                if (verbose)
                	sg_chk_n_print3("seeking", &io_hdr, verbose - 1);
                return -2;
        case SG_LIB_CAT_MEDIUM_HARD:
                if (verbose)
                	sg_chk_n_print3("seeking", &io_hdr, verbose - 1);
                return -3;
        default:
                sg_chk_n_print3("seeking", &io_hdr, verbose);
                return -1;
        }
        return 0;
}

static int cmd_seek_sg(struct sd_device *sd, off_t offset)
{
	//long long block = offset & ~((1 << sd_bitss(sd->bs)) - 1);
	//long long block = offset >> (sd_bitss(sd->bs));
        int scsi_cdbsz = DEF_SCSI_CDBSZ;
        int res, ret = SD_ERR_NO;

        res = sg_bseek(sd->fd, (long long)offset, sd->bs, scsi_cdbsz);
        if (0 != res) {
               	switch (res) {
		ret = SD_ERR;
               	case -3:
               		sd->stat = SG_LIB_CAT_MEDIUM_HARD;
               		tperr("SCSI SEEK medium/hardware error\n");
               		break;
               	case -2:
               		sd->stat = SG_LIB_CAT_NOT_READY;
               		tperr("device not ready\n");
               		break;
               	case 2:
               		sd->stat = SG_LIB_CAT_UNIT_ATTENTION;
               		tperr("SCSI SEEK unit attention\n");
               		break;
               	case 3:
               		sd->stat = SG_LIB_CAT_ABORTED_COMMAND;
               		tperr("SCSI SEEK aborted command\n");
               		break;
               	default:
               		tperr("SCSI SEEK failed\n");
               		break;
               	}
		sd->stat = SD_ERR_SGIO;
	}

        return (res == 0) ? SD_ERR_NO : ret;
}

static int seek_sg(struct sd_device *sd, off_t offset)
{
	if ((offset + sd->bs * sd->parm->blocks) > sd->size)
		offset = sd->size - sd->bs * sd->parm->blocks;
	/* >> 9 for 512-byte sector */
	sd->pos = offset >> (sd_bitss(sd->bs));

	if (cmdseek)
		return cmd_seek_sg(sd, sd->pos);
	/* 
	 * do nothing for the seek command has been made obsolete 
	 * for years, in T13 and T10 standards.
	 */
	else
		return 0;
}

static int find_out_about_buffer(int sg_fd)
{
        unsigned char rbCmdBlk[] = {READ_BUFFER, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        unsigned char rbBuff[RB_DESC_LEN];
        unsigned char sense_buffer[32];
        struct sg_io_hdr io_hdr;
        int res;

        rbCmdBlk[1] = RB_MODE_DESC;
        rbCmdBlk[8] = RB_DESC_LEN;
        memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
        io_hdr.interface_id = 'S';
        io_hdr.cmd_len = sizeof(rbCmdBlk);
        io_hdr.mx_sb_len = sizeof(sense_buffer);
        io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
        io_hdr.dxfer_len = RB_DESC_LEN;
        io_hdr.dxferp = rbBuff;
        io_hdr.cmdp = rbCmdBlk;
        io_hdr.sbp = sense_buffer;
        io_hdr.timeout = 60000;     /* 60000 millisecs == 60 seconds */

        if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
                perror("SG_IO READ BUFFER descriptor error");
                return -1;
        }
        /* now for the error processing */
        res = sg_err_category3(&io_hdr);
        switch (res) {
        case SG_LIB_CAT_RECOVERED:
                sg_chk_n_print3("READ BUFFER descriptor, continuing",
                                &io_hdr, 1);
                /* fall through */
        case SG_LIB_CAT_CLEAN:
                break;
        default: /* won't bother decoding other categories */
                sg_chk_n_print3("READ BUFFER descriptor error", &io_hdr, 1);
                return res;
        }
    
        buf_capacity = ((rbBuff[1] << 16) | (rbBuff[2] << 8) | rbBuff[3]);
        buf_granul = (unsigned char)rbBuff[0];
        if (verbose)
        	tpout("READ BUFFER reports: buffer capacity=%d, offset "
               		"boundary=%d\n", buf_capacity, buf_granul);
        return 0;
}

static int mymemcmp(unsigned char *bf1, unsigned char *bf2, int len)
{
        int df;
        for (df = 0; df < len; df++)
                if (bf1[df] != bf2[df]) return df;
        return 0;
}

/* return 0 if good, else 2222 */
static int do_checksum(int *buf, int len, int quiet)
{
        int sum = base;
        int i; int rln = len;
        for (i = 0; i < len/BPI; i++)
                sum += buf[i];
        while (rln%BPI) sum += ((char*)buf)[--rln];
        if (sum != 0x12345678) {
                if (!quiet) tpout("sg_test_rwbuf: Checksum error (sz=%i):"
                                    " %08x\n", len, sum);
                if (cmpbuf && !quiet) {
                        int diff = mymemcmp (cmpbuf, (unsigned char*)buf,
                                             len);
                        tpout("Differ at pos %i/%i:\n", diff, len);
                        for (i = 0; i < 24 && i+diff < len; i++)
                                tpout(" %02x", cmpbuf[i+diff]);
                        tpout("\n");
                        for (i = 0; i < 24 && i+diff < len; i++)
                                tpout(" %02x",
                                        ((unsigned char*)buf)[i+diff]);
                        tpout("\n");
                }
                return 2222;
        }
        else {
		if (verbose)
                	tpout("Checksum value: 0x%x\n", sum);
                return 0;
        }
}

static void do_fill_buffer(int *buf, int len)
{
        int sum; 
        int i; int rln = len;
        srand(time(0));
    retry:
        if (len >= BPI) 
                base = 0x12345678 + rand();
        else 
                base = 0x12345678 + (char)rand();
        sum = base;
        for (i = 0; i < len/BPI - 1; i++)
        {
                /* we rely on rand() giving full range of int */
                buf[i] = rand(); 
                sum += buf[i];
        }
        while (rln%BPI) 
        {
                ((char*)buf)[--rln] = rand();
                sum += ((char*)buf)[rln];
        }
        if (len >= BPI) buf[len/BPI - 1] = 0x12345678 - sum;
        else ((char*)buf)[0] = 0x12345678 + ((char*)buf)[0] - sum;
        if (do_checksum(buf, len, 1)) {
                if (len < BPI) goto retry;
                tpout("sg_test_rwbuf: Memory corruption?\n");
                exit (1);
        }
        if (cmpbuf) memcpy(cmpbuf, (char*)buf, len);
}

static int read_buffer(int sg_fd, unsigned size)
{
        int res;
        unsigned char rbCmdBlk[] = {READ_BUFFER, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        int bufSize = size + 0;
        unsigned char * rbBuff = (unsigned char *)malloc(bufSize);
        unsigned char sense_buffer[32];
        struct sg_io_hdr io_hdr;

        if (NULL == rbBuff)
                return -1;
        rbCmdBlk[1] = RWB_MODE_DATA;
        rbCmdBlk[6] = 0xff & (bufSize >> 16);
        rbCmdBlk[7] = 0xff & (bufSize >> 8);
        rbCmdBlk[8] = 0xff & bufSize;
        memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
        io_hdr.interface_id = 'S';
        io_hdr.cmd_len = sizeof(rbCmdBlk);
        io_hdr.mx_sb_len = sizeof(sense_buffer);
        io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
        io_hdr.dxfer_len = bufSize;
        io_hdr.dxferp = rbBuff;
        io_hdr.cmdp = rbCmdBlk;
        io_hdr.sbp = sense_buffer;
        io_hdr.pack_id = 2;
        io_hdr.timeout = 60000;     /* 60000 millisecs == 60 seconds */

        if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
                perror("SG_IO READ BUFFER data error");
                free(rbBuff);
                return -1;
        }
        /* now for the error processing */
        res = sg_err_category3(&io_hdr);
        switch (res) {
        case SG_LIB_CAT_RECOVERED:
            sg_chk_n_print3("READ BUFFER data, continuing", &io_hdr, 1);
            /* fall through */
        case SG_LIB_CAT_CLEAN:
                break;
        default: /* won't bother decoding other categories */
                sg_chk_n_print3("READ BUFFER data error", &io_hdr, 1);
                free(rbBuff);
                return res;
        }

        res = do_checksum ((int*)rbBuff, size, 0);
        free(rbBuff);
        return res;
}

static int write_buffer(int sg_fd, unsigned size)
{
        unsigned char wbCmdBlk[] = {WRITE_BUFFER, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        int bufSize = size + 0;
        unsigned char * wbBuff = (unsigned char *)malloc(bufSize);
        unsigned char sense_buffer[32];
        struct sg_io_hdr io_hdr;
        int res;

        if (NULL == wbBuff)
                return -1;
        memset(wbBuff, 0, bufSize);
        do_fill_buffer ((int*)wbBuff, size);
        wbCmdBlk[1] = RWB_MODE_DATA;
        wbCmdBlk[6] = 0xff & (bufSize >> 16);
        wbCmdBlk[7] = 0xff & (bufSize >> 8);
        wbCmdBlk[8] = 0xff & bufSize;
        memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
        io_hdr.interface_id = 'S';
        io_hdr.cmd_len = sizeof(wbCmdBlk);
        io_hdr.mx_sb_len = sizeof(sense_buffer);
        io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
        io_hdr.dxfer_len = bufSize;
        io_hdr.dxferp = wbBuff;
        io_hdr.cmdp = wbCmdBlk;
        io_hdr.sbp = sense_buffer;
        io_hdr.pack_id = 1;
        io_hdr.timeout = 60000;     /* 60000 millisecs == 60 seconds */

        if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
                perror("SG_IO WRITE BUFFER data error");
                free(wbBuff);
                return -1;
        }
        /* now for the error processing */
        res = sg_err_category3(&io_hdr);
        switch (res) {
        case SG_LIB_CAT_RECOVERED:
            sg_chk_n_print3("WRITE BUFFER data, continuing", &io_hdr, 1);
            /* fall through */
        case SG_LIB_CAT_CLEAN:
                break;
        default: /* won't bother decoding other categories */
                sg_chk_n_print3("WRITE BUFFER data error", &io_hdr, 1);
                free(wbBuff);
                return res;
        }
        free(wbBuff);
        return res;
}

static int scsi_interf_test(struct test_parm *p, void *d)
{	
	struct sd_device *sd = d;
        int ret = SD_ERR_NO;
	int size = 128 * 1024; 

        ret = find_out_about_buffer(sd->fd);
        if (ret)
                goto err_out;
	/*
	 * we tuned test size now as 128k, for our case we found
	 * the read buffer command will frequently fail if we use
	 * a buffer size greater than 128k.
	 */
	if (buf_capacity < size)
		size = buf_capacity;
        
        cmpbuf = (unsigned char *)malloc(size);
        ret = write_buffer(sd->fd, size);
        if (ret) {
                goto err_out;
        }
        ret = read_buffer(sd->fd, size);
        if (ret) {
                if (2222 == ret)
			ret = SD_ERR;
                sd->stat = SG_LIB_CAT_MALFORMED;
        }
err_out:
	if (sd->stat == SD_ERR_NO)
        	sd->stat = (ret >= 0) ? ret : SG_LIB_CAT_OTHER;
        if (cmpbuf)
                free(cmpbuf);
        return (sd->stat == SD_ERR_NO) ? ret : SD_ERR;
}

/* Return of 0 -> success, otherwise see sg_ll_send_diag() */
static int do_senddiag(int sg_fd, int sf_code, int pf_bit, int sf_bit,
                       int devofl_bit, int unitofl_bit, void * outgoing_pg, 
                       int outgoing_len, int noisy, int verbose)
{
	int long_duration = 0;

	if ((0 == sf_bit) && ((5 == sf_code) || (6 == sf_code)))
		long_duration = 1;      /* foreground self-tests */
	return sg_ll_send_diag(sg_fd, sf_code, pf_bit, sf_bit, devofl_bit,
                           unitofl_bit, long_duration, outgoing_pg,
                           outgoing_len, noisy, verbose);
}

static int scsi_selfd_test(struct test_parm *p, void *d)
{
	struct sd_device *sd = d;
	int do_selftest = 0;
	int do_pf = 0;
	int do_deftest = 1;
	int do_doff = 0;
	int do_uoff = 0;
	int do_verbose = 0;
	int ret = SD_ERR_NO;

        ret = do_senddiag(sd->fd, do_selftest, do_pf,
                          do_deftest, do_doff, do_uoff, NULL,
                          0, 1, do_verbose);
        if (0 == ret) {
            if ((5 == do_selftest) || (6 == do_selftest))
                sd_debug("foreground self-test returned GOOD status\n");
            else if (do_deftest && (! do_doff) && (! do_uoff))
                sd_debug("default self-test returned GOOD status\n");
        } else
            goto err_out;
	if (sd->stat == SD_ERR_NO)
        	sd->stat = (ret >= 0) ? ret : SG_LIB_CAT_OTHER;
        return (sd->stat == SD_ERR_NO) ? ret : SD_ERR;
err_out:
	if (SG_LIB_CAT_UNIT_ATTENTION == ret)
        	tpout("SEND DIAGNOSTIC, unit attention\n");
	else if (SG_LIB_CAT_ABORTED_COMMAND == ret)
        	tpout("SEND DIAGNOSTIC, aborted command\n");
	else if (SG_LIB_CAT_NOT_READY == ret)
        	tpout("SEND DIAGNOSTIC, device not ready\n");
	else
        	tpout("SEND DIAGNOSTIC command, failed\n");
        return (sd->stat == SD_ERR_NO) ? ret : SD_ERR;
}

static int bsget_sg(struct sd_device *sd)
{
	/*
	 * the real bsget will be put in the following blkget_sg()
	 * for simplicity, just do nothing here.
	 */
	return SD_ERR_NO;
}

static int blkget_sg(struct sd_device *sd)
{
	struct sg_io_hdr *hdr = malloc(sizeof(*hdr));
	unsigned char buf[8], cdb[16];
	size_t eb = 0;
	int bs = 0;

	memset(hdr, 0, sizeof(*hdr));
	memset(cdb, 0, sizeof(cdb));
	memset(buf, 0, sizeof(buf));

	hdr->interface_id = 'S';
	hdr->cmd_len = sizeof(cdb);
	hdr->dxfer_direction = SG_DXFER_FROM_DEV;
	hdr->dxferp = buf;
	hdr->dxfer_len = sizeof(buf);

	hdr->cmdp = cdb;
	hdr->cmdp[0] = GPCMD_READ_CDVD_CAPACITY;

	if (ioctl(sd->fd, SG_IO, hdr) < 0) {
		tperr("read capacity: %s\n", strerror(errno));
		tperr("sense key: %x\n", hdr->status);
		sd->stat = SD_ERR_SYS;
		return SD_ERR;
	}

	eb = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
	if (!eb)
		eb = ~0UL;

	bs = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
	
	/* the total blocks is endblock + 1 */
	sd->bs = bs;
	sd->blk = eb + 1;
	return SD_ERR_NO;
}

struct sd_device sd_disk = {
	.name	= SCSI_DISK,
	.seek	= seek_sg,
	.read	= read_sg,
	.write	= write_sg,
	.bsget	= bsget_sg,
	.blkget	= blkget_sg,
	.tests	= {
		{ SEQU_WRC, },
		{ RAND_WRC, },
		{ RAND_READ, },
		{ RAND_WRITE, },
		{ SEQU_READ, },
		{ SEQU_WRITE, },
		{ BUTT_SEEK, },
		{ INTERFACE, 0, scsi_interf_test },
		{ SELF_DIAG, 0, scsi_selfd_test }
	}
};
