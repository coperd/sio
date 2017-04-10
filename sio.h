#ifndef __SIO_H
#define __SIO_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/time.h>


#define KB              (1024L)
#define MB              (KB*KB)
#define GB              (KB*MB)
#define CHUNK_SZ        (4*KB)
#define STRIPE_SZ       (3*CHUNK_SZ)
#define ALIGNMENT       4096 // O_DIRECT

/* Global Variables Definition */
extern long NB_READ;            /* # of total reads in blocks (4K) */
extern long NB_WRITE;           /* # of total writes in blocks (4K) */
extern long NB_WARMUP;          /* # of writes for warmup in blocks (4K) */
extern long NB_RTHRD;           /* # of read threads */
extern long NB_WTHRD;           /* # of write threads */
extern long DSK_SZ;             /* disk size in bytes */
extern long BLK_SZ;     /* block size used to do r/w */
extern long TRIPE_SZ;     /* block size used to do r/w */
extern long BLK_RANGE;          /* block range */
extern long RBLK_RANGE;         /* read block range */
extern long WBLK_RANGE;         /* range range */
extern int FD;                 /* dev fd */
extern int FD_WRITE;                 /* dev fd */
extern char dev[32];       /* device name */
extern char rstfile[64];   /* output file name */

extern int vflag;
extern int sflag;
/* counter */
extern int write_cnt;
extern int read_cnt;

struct thread_info
{
    int fd;
    pthread_t tid;
    int iosize;         /* 4k, 16k, etc. */
    FILE *fp;
    int nb_rw;          /* number of reads/writes */
    bool is_read;
    int *ret;           /* record the ret value each of read/write */
    int *errlist;
    int *latencylist;   /* record the latency info of each read request */
    off_t *oftlist;
};


#define handle_error_en(en, msg) \
    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
    do { perror(msg);/* exit(EXIT_FAILURE); */} while (0)


#endif
