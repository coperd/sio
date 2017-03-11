#define _GNU_SOURCE

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

//#define DEBUG

#define KB              (1024L)
#define MB              (KB*KB)
#define CHUNK_SZ        (4*KB)
#define STRIPE_SZ       (3*CHUNK_SZ)
#define ALIGNMENT       4096 // O_DIRECT

#define handle_error_en(en, msg) \
    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
    do { perror(msg);/* exit(EXIT_FAILURE); */} while (0)


static struct option long_options[] = 
{
    { "help",            no_argument,       0, 'h' },
    { "device",          required_argument, 0, 'd' },
    { "block_size",      required_argument, 0, 'b' },
    { "read_threads",    required_argument, 0, 'r' },
    { "read_nb_blocks",  required_argument, 0, 'p' },
    { "write_threads",   required_argument, 0, 'w' },
    { "write_nb_blocks", required_argument, 0, 'q' },
    { "warmup",          required_argument, 0, 'm' },
    { "output",          required_argument, 0, 'o' },
    { "verbose",         no_argument,       0, 'v' },
    { "sort",            no_argument,       0, 's' },
    { 0,                 0,                 0, 0   }
};

char * const short_options = "hvsb:d:r:p:w:q:m:o:";

/* Global Variables Definition */
long NB_READ = 0;            /* # of total reads in blocks (4K) */
long NB_WRITE = 0;           /* # of total writes in blocks (4K) */
long NB_WARMUP = 0;          /* # of writes for warmup in blocks (4K) */
long NB_RTHRD = 0;           /* # of read threads */
long NB_WTHRD = 0;           /* # of write threads */
long DSK_SZ = 0;             /* disk size in bytes */
long BLK_SZ = STRIPE_SZ;     /* block size used to do r/w */
long BLK_RANGE = 0;          /* block range */
long RBLK_RANGE = 0;         /* read block range */
long WBLK_RANGE = 0;         /* range range */
int FD = -1;                 /* dev fd */
int FD_WRITE = -1;                 /* dev fd */
char dev[32] = {'\0'};       /* device name */
char rstfile[64] = {'\0'};   /* output file name */

int vflag = 0;
int sflag = 0;
/* counter */
int write_cnt = 0;
int read_cnt = 0;

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

void usage()
{
    fprintf(stderr, "Usage: ./sio\n");
    fprintf(stderr, "\t -h, --help,             print help information\n");
    fprintf(stderr, "\t -d, --device,           device to operate on\n");
    fprintf(stderr, "\t -b, --block_size,       block size in 4KB unit\n");
    fprintf(stderr, "\t -m, --warmup,           # of writes to warmup in 4K blocks\n");
    fprintf(stderr, "\t -r, --read_threads,     # of read threads\n");
    fprintf(stderr, "\t -p, --read_nb_blocks,   # of reads in 4K blocks\n");
    fprintf(stderr, "\t -w, --write_threads,    # of write threads\n");
    fprintf(stderr, "\t -q, --write_nb_blocks,  # of writes in 4K blocks\n");
    fprintf(stderr, "\t -s, --sort,             sort according to latency\n");
    fprintf(stderr, "\t -v, --verbose,          whether to output to stdout\n");
    fprintf(stderr, "\t -o, --output,           output file\n");
}

uint64_t get_disk_sz_in_bytes(int fd)
{
    off_t size = lseek(fd, 0, SEEK_END);
    if (size == -1) {
        handle_error("lseek");
    }

    return (int64_t)size;
}

void sio_memalign(void **memptr, size_t alignment, size_t size)
{
    int ret = posix_memalign(memptr, alignment, size);
    if (ret == EINVAL) {
        fprintf(stderr, "Error: the alignment argument was not a power of two, \
                or was not a multile of sizeof(void *)\n");
        exit(EXIT_FAILURE);
    } else if (ret == ENOMEM) {
        fprintf(stderr, "Error: Insufficient memory for allocation\n");
        exit(EXIT_FAILURE);
    }
}

void parse_param(int argc, char **argv)
{
    int c;
    int option_index = 0;

    while (1) {
        c = getopt_long(argc, argv, short_options, long_options, &option_index);
        if (c == -1) break;

        switch (c) {
        case 'd':
            strncpy(dev, optarg, 32); 
            printf("DISK: %s\n", dev);
            break;
        case 'b':
            BLK_SZ = atol(optarg);
            BLK_SZ *= 4*KB;
            break;
        case 'r':
            NB_RTHRD = atol(optarg);
            printf("NB_RTHRD: %ld\n", NB_RTHRD);
            break;
        case 'p':
            NB_READ = atol(optarg);
            printf("NB_READ: %ld\n", NB_READ);
            break;
        case 'w':
            NB_WTHRD = atol(optarg);
            printf("NB_WTHRD: %ld\n", NB_WTHRD);
            break;
        case 'q':
            NB_WRITE = atol(optarg);
            printf("NB_WRITE: %ld\n", NB_WRITE);
            break;
        case 'm':
            NB_WARMUP = atol(optarg);
            printf("NB_WARMUP: %ld\n", NB_WARMUP);
            break;
        case 'o':
            strncpy(rstfile, optarg, 64);
            break;
        case 'v':
            vflag = 1;
            break;
        case 's':
            sflag = 1;
            break;
        case 'h':
        case '?':
        default:
            usage();
            exit(EXIT_FAILURE);
        }
    }

    FD = open(dev, O_RDWR | O_DIRECT | O_SYNC);
    if (FD == -1)  {
        handle_error("open");
        usage();
        exit(EXIT_FAILURE);
    }

    DSK_SZ = get_disk_sz_in_bytes(FD);
    printf("Disk Size: %ld MB\n", DSK_SZ/(MB));
    printf("BLK_SZ: %ld KB\n", BLK_SZ/(KB));

    BLK_RANGE = DSK_SZ / BLK_SZ;
    RBLK_RANGE = BLK_RANGE;
    WBLK_RANGE = RBLK_RANGE;

    if (vflag)
        printf("====================Begin TRACE===================\n");
}

void check_all_var()
{
    int err = 0;

    if (dev[0] == '\0') {
        printf("ERROR: no disk provided\n");
        err++;
    }

    if (DSK_SZ <= 0) {
        printf("ERROR: disk size should be larger than 0!\n");
        err++;
    }

    if ((NB_WARMUP < 0) || (NB_RTHRD < 0) || (NB_READ < 0) || (NB_WTHRD < 0) || 
            (NB_WRITE < 0)) {
        printf("ERROR: illeagal variables");
        err++;
    }

    if (NB_RTHRD > 0) {
        NB_READ = NB_READ / NB_RTHRD * NB_RTHRD;
    }

    if (NB_WTHRD > 0) {
        NB_WRITE = NB_WRITE / NB_WTHRD * NB_WTHRD;
    }

    if (err > 0) {
        usage();
        exit(EXIT_FAILURE);
    }
}

int cmp(const void *a, const void *b)
{
    return *((int *)a) >= *((int *)b);
}

int64_t calc_latency(struct timespec, struct timespec);

/* return latency in us */
int64_t calc_latency(struct timespec start, struct timespec end)
{
    return (end.tv_sec - start.tv_sec) * 1000000 + \
        (end.tv_nsec - start.tv_nsec) / 1000;
}


void *warmup_thread(void *args)
{
#ifdef DEUBG
    printf("warmup_thread starts\n");
#endif

    void *buf;
    struct thread_info *tinfo = (struct thread_info *)args;
    int fd = tinfo->fd;
    int iosize = tinfo->iosize;
    printf("iosize=%d, BLK_SZ = %ld\n", iosize, BLK_SZ);
    int nb_thread_rw = tinfo->nb_rw;
    sio_memalign(&buf, ALIGNMENT, iosize);

    memset(buf, 1, iosize);

    int i;
    off_t offset = 0;

    printf("warmup thread # writes: %d, fd=%d\n", nb_thread_rw, fd);

    for (i = 0; i < nb_thread_rw; i++) {
        int ret = pwrite(fd, buf, iosize, offset);
        //int ret = pread(fd, buf, iosize, offset);
        if (ret == -1) {
            handle_error("warmup pwrite");
            exit(0);
        }
        printf("warmup write: %ld success\n", offset);
        offset += BLK_SZ; 
    }

#ifdef DEBUG
    printf("warmup write finished ...\n");
#endif
    return NULL;
}


void ssleep(struct timespec *ts)
{
    struct timespec rem;
    while (nanosleep(ts, &rem) == -1) {
        ts = &rem;
    }
}

/* create a read/write thread according to arg->is_read */
void *rw_iothread(void *arg)
{
    sleep(5);
    int i;
    void *buf;
    struct timespec rstart, rend;

    struct thread_info *tinfo = (struct thread_info *)arg;
    int fd = tinfo->fd;
    int iosize = tinfo->iosize;
    int *retlist = tinfo->ret;
    int *latencylist = tinfo->latencylist;
    int *errlist = tinfo->errlist;
    off_t *oftlist = tinfo->oftlist;

    sio_memalign(&buf, ALIGNMENT, iosize); /* STRIPE size alignment */
    memset(buf, 0, iosize);

    srand(time(NULL));

    int nb_thread_rw = tinfo->nb_rw;

#if 0
    struct timespec rts, wts;
    rts.tv_sec = 0;
    rts.tv_nsec = 10000000;

    wts.tv_sec = 0;
    wts.tv_nsec = 1500000; // 40ms
#endif

    if (tinfo->is_read) {   /* create read threads */
        for (i = 0; i < nb_thread_rw; i++) {
            off_t randoffset = rand() % RBLK_RANGE;
            clock_gettime(CLOCK_REALTIME, &rstart);
            int ret = pread(fd, buf, iosize, randoffset*BLK_SZ);
            clock_gettime(CLOCK_REALTIME, &rend);
            if (ret == -1) {
                errlist[i] = errno;
                handle_error("pread");
            } else {
                errlist[i] = 0;
            }
            oftlist[i] = randoffset*CHUNK_SZ/512;
            retlist[i] = ret;
            latencylist[i] = calc_latency(rstart, rend);

            if (vflag) {
                printf("pread  %-6d ret: %-6d errno: %-2d offset: %-8ld\n", 
                        ++read_cnt, ret, errlist[i], randoffset*CHUNK_SZ/512);
            }
            //ssleep(&rts);
        }
    } else {                /* create write threads */
        for (i = 0; i < nb_thread_rw; i++) {
            off_t randoffset = rand() % WBLK_RANGE;
            clock_gettime(CLOCK_REALTIME, &rstart);
            int ret = pwrite(fd, buf, iosize, randoffset*BLK_SZ);
            clock_gettime(CLOCK_REALTIME, &rend);
            if (ret == -1) {
                handle_error("pwrite");
                errlist[i] = errno;
            } else {
                errlist[i] = 0;
            }
            oftlist[i] = randoffset*CHUNK_SZ/512;
            retlist[i] = ret;
            latencylist[i] = calc_latency(rstart, rend);

            if (vflag) {
                printf("pwrite %-6d ret: %-6d errno: %-2d offset: %-8ld\n", 
                        ++write_cnt, ret, errlist[i], randoffset*CHUNK_SZ/512);
            }

            //ssleep(&wts);

        }
    }

    return NULL;
}

void rw_thrd_main(int argc, char **argv)
{

    struct timespec start, end;
    struct thread_info *warmup_args = malloc(sizeof(struct thread_info));
    struct thread_info *wargs = malloc(sizeof(struct thread_info)*NB_WTHRD);
    struct thread_info *rargs = malloc(sizeof(struct thread_info)*NB_RTHRD);
    int *tr_latencylist = calloc(NB_READ, sizeof(int));
    int *tr_retlist = calloc(NB_READ, sizeof(int));
    int *tr_errlist = calloc(NB_READ, sizeof(int));
    off_t *tr_oftlist = calloc(NB_READ, sizeof(off_t));

    int *tw_latencylist = calloc(NB_WRITE, sizeof(int));
    int *tw_retlist = calloc(NB_WRITE, sizeof(int));
    int *tw_errlist = calloc(NB_WRITE, sizeof(int));
    off_t *tw_oftlist = calloc(NB_WRITE, sizeof(off_t));

    int i, j, ret;

#ifdef DEBUG
    printf("dev: %s\n", dev);
#endif

    if (NB_WARMUP > 0) {

        warmup_args->fd = FD;
        warmup_args->iosize = CHUNK_SZ;
        warmup_args->is_read = false;
        warmup_args->nb_rw = NB_WARMUP;

        printf("creating warmup thread..\n");
        ret = pthread_create(&warmup_args->tid, NULL, warmup_thread, 
                (void *)warmup_args);

        if (ret != 0) {
            handle_error("pthread_create");
        }

        printf("joining warmup thread.., warmup_args->tid=%ld\n", warmup_args->tid);
        ret = pthread_join(warmup_args->tid, NULL);
        if (ret != 0) {
            handle_error("pthread_join");
        }


        printf("sleeping here .. \n");
    }

    clock_gettime(CLOCK_REALTIME, &start);

    /* init write threads */
    if (NB_WTHRD > 0 && NB_WRITE > 0) {
#ifdef DEBUG
        printf("creating write threads...\n");
#endif

        for (i = 0; i < NB_WTHRD; i++) {
            wargs[i].fd = FD;
            wargs[i].iosize = CHUNK_SZ*2;
            wargs[i].nb_rw = NB_WRITE/NB_WTHRD;
            wargs[i].is_read = false;
            wargs[i].ret = calloc(wargs[i].nb_rw, sizeof(int));
            wargs[i].latencylist = calloc(wargs[i].nb_rw, sizeof(int));
            wargs[i].errlist = calloc(wargs[i].nb_rw, sizeof(int));
            wargs[i].oftlist = calloc(wargs[i].nb_rw, sizeof(off_t));

            ret = pthread_create(&wargs[i].tid, NULL, rw_iothread, 
                    (void *)&wargs[i]);

            if (ret != 0) {
                handle_error("pthread_create");
                exit(1);
            }

        }
    }

    /* init read threads */
    if (NB_RTHRD > 0 && NB_READ > 0) {
#ifdef DEBUG
        printf("crating read threads...\n");
#endif

        for (i = 0; i < NB_RTHRD; i++) {
            rargs[i].fd = FD;
            rargs[i].iosize = STRIPE_SZ;
            rargs[i].nb_rw = NB_READ/NB_RTHRD;
            rargs[i].is_read = true; /* read thread */
            rargs[i].ret = calloc(rargs[i].nb_rw, sizeof(int));
            rargs[i].latencylist = calloc(rargs[i].nb_rw, sizeof(int));
            rargs[i].errlist = calloc(rargs[i].nb_rw, sizeof(int));
            rargs[i].oftlist = calloc(rargs[i].nb_rw, sizeof(off_t));

            ret = pthread_create(&rargs[i].tid, NULL, rw_iothread, 
                    (void *)&rargs[i]);

            if (ret != 0) {
                handle_error("pthread_create");
                exit(1);
            }
        }

        /* wait for all read threads to finish */
        for (i = 0; i < NB_RTHRD; i++) {
            pthread_join(rargs[i].tid, NULL);
        }
    }
    
    if (NB_WTHRD > 0 && NB_WRITE > 0) {
        /* wait for all write thread to finish, if any */
        for (i = 0; i < NB_WTHRD; i++) {
            pthread_join(wargs[i].tid, NULL);
        }
    }

    if (vflag)
        printf("=====================End TRACE====================\n");

    clock_gettime(CLOCK_REALTIME, &end);

    int cnt = 0;
    for (i = 0; i < NB_RTHRD; i++) {
        for (j = 0; j < NB_READ/NB_RTHRD; j++) {
            tr_latencylist[cnt] = rargs[i].latencylist[j];
            tr_retlist[cnt] = rargs[i].ret[j];
            tr_errlist[cnt] = rargs[i].errlist[j];
            tr_oftlist[cnt] = rargs[i].oftlist[j];
            cnt++;
        }
    }

    cnt = 0;
    for (i = 0; i < NB_WTHRD; i++) {
        for (j = 0; j < NB_WRITE/NB_WTHRD; j++) {
            tw_latencylist[cnt] = wargs[i].latencylist[j];
            tw_retlist[cnt] = wargs[i].ret[j];
            tw_errlist[cnt] = wargs[i].errlist[j];
            tw_oftlist[cnt] = wargs[i].oftlist[j];
            cnt++;
        }
    }

    if (sflag) {
        qsort(tr_latencylist, NB_READ, sizeof(int), cmp);
        qsort(tw_latencylist, NB_WRITE, sizeof(int), cmp);
    }

    FILE *fp = NULL;
    if (rstfile[0] == '\0') {
        fp = stdout;
    } else {
        fp = fopen(rstfile, "w");
        if (fp == NULL)
            fp = stdout;
    }

    if (NB_RTHRD > 0) {
        for (i = 0; i < NB_READ; i++) {
            fprintf(fp, "%ld, %d, %d, %d\n", tr_oftlist[i], tr_latencylist[i], 
                    tr_retlist[i], tr_errlist[i]);
        }
    }

    if (NB_WTHRD > 0) {
        for (i = 0; i < NB_WRITE; i++) {
            fprintf(fp, "%ld,%d,%d,%d\n", tw_oftlist[i], tw_latencylist[i], 
                    tw_retlist[i], tw_errlist[i]);
        }
    }

    if ((NB_RTHRD > 0 && NB_READ > 0) || (NB_WTHRD > 0 && NB_WRITE > 0)) {
        printf("Total Latency: %" PRId64 " us\n", calc_latency(start, end));
    }
}

int main(int argc, char **argv)
{
    parse_param(argc, argv);

    check_all_var();

    rw_thrd_main(argc, argv);

    printf("end of main\n");

    return 0;
}
