/*
 * Copyright (c) 2013 HON HAI PRECISION IND.CO.,LTD. (FOXCONN)
 *
 */
#ifndef __DISKIO_H__
#define __DISKIO_H__

#define BANNER    "Copyright (c) 2013 HON HAI PRECISION IND.CO.,LTD. (FOXCONN)"

#define DEFAULT_ITERATIONS    1
#define DEFAULT_XFER_SIZE    512

#define STR_SZ    1024

typedef enum {
	READ_OP = 1,
	WRITE_OP,
	WRITE_READ_OP,
} TEST_MODE;

#endif
