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
#include <sys/time.h>

//#define DEBUG

#define KB              1024
#define MB              (KB*KB)
//#define DSK_SZ          (512*MB)
#define BLK_SZ          (4*KB)
//#define BLK_RANGE       (DSK_SZ/BLK_SZ)

//#define RBLK_RANGE      (256*MB/BLK_SZ)
//#define WBLK_RANGE      (BLK_RANGE-RBLK_RANGE)

//#define DSKDEV          "/dev/sda"

#define handle_error_en(en, msg) \
    do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)


static struct option long_options[] = 
{
    { "help",            no_argument,       0, 'h' },
    { "device",          required_argument, 0, 'd' },
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

char * const short_options = "hvsd:r:p:w:q:m:o:";

long NB_READ = 0;            /* # of total reads in blocks (4K) */
long NB_WRITE = 0;           /* # of total writes in blocks (4K) */
long NB_WARMUP = 0;          /* # of writes for warmup in blocks (4K) */
long NB_RTHRD = 0;           /* # of read threads */
long NB_WTHRD = 0;           /* # of write threads */
long DSK_SZ = 0;             /* disk size in bytes */
long BLK_RANGE = 0;          /* block range */
long RBLK_RANGE = 0;         /* read block range */
long WBLK_RANGE = 0;         /* range range */
int FD = -1;                 /* dev fd */
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
};

void usage()
{
    fprintf(stderr, "Usage: ./sio\n");
    fprintf(stderr, "\t -h, --help,             print help information\n");
    fprintf(stderr, "\t -d, --device,           device to operate on\n");
    fprintf(stderr, "\t -m, --warmup,           # of writes to warmup in 4K blocks\n");
    fprintf(stderr, "\t -r, --read_threads,     # of read threads\n");
    fprintf(stderr, "\t -p, --read_nb_blocks,   # of reads in 4K blocks\n");
    fprintf(stderr, "\t -w, --write_threads,    # of write threads\n");
    fprintf(stderr, "\t -q, --write_nb_blocks,  # of writes in 4K blocks\n");
    fprintf(stderr, "\t -s, --sort,             sort according to latency\n");
    fprintf(stderr, "\t -v, --verbose,          whether to output to stdout\n");
    fprintf(stderr, "\t -o, --output,           output file\n");
}

int64_t get_disk_sz_in_bytes(int fd)
{
    /*off_t size = lseek(fd, 0, SEEK_END);
    if (size == -1)
        handle_error("lseek");

    return (int64_t)size;
    */

    return 512*MB;
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
                strcpy(dev, optarg); 
                printf("DISK: %s\n", dev);
                FD = open(dev, O_RDWR | O_DIRECT | O_SYNC);
                if (FD == -1) 
                    handle_error("open");
                DSK_SZ = get_disk_sz_in_bytes(FD);
                printf("Disk Size: %ld MB\n", DSK_SZ/MB);
                BLK_RANGE = DSK_SZ / BLK_SZ;
                RBLK_RANGE = BLK_RANGE / 2;
                WBLK_RANGE = BLK_RANGE - RBLK_RANGE;
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
                strcpy(rstfile, optarg);
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
}

void check_all_var()
{
    int err = 0;

    if (dev[0] == '\0') {
        printf("ERROR: no disk provided\n");
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

int calc_latency(struct timespec, struct timespec);

/* return latency in us */
int calc_latency(struct timespec start, struct timespec end)
{
    return (end.tv_sec - start.tv_sec) * 1000000 + \
        (end.tv_nsec - start.tv_nsec) / 1000;
}


void *warmup_thread(void *args)
{
#ifdef DEBUG
    printf("warmup_thread starts\n");
#endif
    void *buf;
    struct thread_info *tinfo = (struct thread_info *)args;
    int fd = tinfo->fd;
    int iosize = tinfo->iosize;
    int nb_thread_rw = tinfo->nb_rw;

    posix_memalign(&buf, BLK_SZ, iosize);
    memset(buf, 0, iosize);

    int i;
    off_t offset = 0;

    for (i = 0; i < nb_thread_rw; i++) {
        assert(iosize % BLK_SZ == 0);
        int ret = pwrite(fd, buf, iosize, offset);
        if (ret == -1) handle_error("pwrite");
        //printf("write: %ld success\n", offset);
        offset += iosize;
    }

    printf("warmup write finished ...\n");
    return NULL;
}

/* create a read/write thread according to arg->is_read */
void *rw_iothread(void *arg)
{
    int i;
    void *buf;
    struct timespec rstart, rend;

    struct thread_info *tinfo = (struct thread_info *)arg;
    int fd = tinfo->fd;
    int iosize = tinfo->iosize;
    int *retlist = tinfo->ret;
    int *latencylist = tinfo->latencylist;
    int *errlist = tinfo->errlist;

    posix_memalign(&buf, BLK_SZ, iosize); /* BLOCK size alignment */
    memset(buf, 0, iosize);

    srand(time(NULL));

    int nb_thread_rw = tinfo->nb_rw;

    if (tinfo->is_read) {   /* create read threads */
        for (i = 0; i < nb_thread_rw; i++) {
            off_t randoffset = rand() % RBLK_RANGE;
            clock_gettime(CLOCK_REALTIME, &rstart);
            sleep(5);
            printf("Begin sleepp....\n");
            int ret = pread(fd, buf, iosize, randoffset*BLK_SZ);
            errlist[i] = errno;
            clock_gettime(CLOCK_REALTIME, &rend);
            if (vflag) {
                printf("pread count: [%d], retval: [%d], errno: [%d], %ld\n", 
                        ++read_cnt, ret, errlist[i], randoffset*BLK_SZ/512);
            }
            if (ret == -1) {
                perror("pread");
            }

            printf("pread OVER\n");
            sleep(5);
            retlist[i] = ret;
            latencylist[i] = calc_latency(rstart, rend);
        }
    } else {                /* create write threads */
        for (i = 0; i < nb_thread_rw; i++) {
            off_t randoffset = RBLK_RANGE + rand() % WBLK_RANGE;
            int ret = pwrite(fd, buf, iosize, randoffset*BLK_SZ);
            //printf("tid[%ld] write count: [%d]\n", pthread_self(), ++write_cnt);
            if (vflag) {
                printf("pwrite count: [%d], retval: [%d], %ld\n", 
                        ++write_cnt, ret, randoffset*BLK_SZ);
            }
            if (ret == -1) handle_error("pwrite");
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
    int *tlatencylist = malloc(sizeof(int)*NB_READ);
    int *tretlist = malloc(sizeof(int)*NB_READ);
    int *terrlist = malloc(sizeof(int)*NB_READ);

    int i, j, ret;

#ifdef DEBUG
    printf("dev: %s\n", dev);
#endif

    if (NB_WARMUP > 0) {

        warmup_args->fd = FD;
        warmup_args->iosize = BLK_SZ;
        warmup_args->is_read = false;
        warmup_args->nb_rw = NB_WARMUP;

        ret = pthread_create(&warmup_args->tid, NULL, warmup_thread, 
                (void *)warmup_args);

        if (ret != 0) {
            handle_error("pthread_create");
        }

        pthread_join(warmup_args->tid, NULL);

        sleep(5);
        printf("warmup writes are finished .. \n");
    }

    clock_gettime(CLOCK_REALTIME, &start);

    /* init write threads */
    if (NB_WTHRD > 0 && NB_WRITE > 0) {
#ifdef DEBUG
        printf("crating write threads...\n");
#endif

        for (i = 0; i < NB_WTHRD; i++) {
            wargs[i].fd = FD;
            wargs[i].iosize = BLK_SZ;
            assert(NB_WTHRD > 0);
            wargs[i].nb_rw = NB_WRITE/NB_WTHRD;
            wargs[i].is_read = false;
            wargs[i].ret = 0;

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
            rargs[i].iosize = BLK_SZ;
            rargs[i].nb_rw = NB_READ/NB_RTHRD;
            rargs[i].is_read = true; /* read thread */
            rargs[i].ret = malloc(sizeof(int)*rargs[i].nb_rw);
            rargs[i].latencylist = malloc(sizeof(int)*rargs[i].nb_rw);
            rargs[i].errlist = malloc(sizeof(int)*rargs[i].nb_rw);

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

    clock_gettime(CLOCK_REALTIME, &end);

    int cnt = 0;
    for (i = 0; i < NB_RTHRD; i++) {
        for (j = 0; j < NB_READ/NB_RTHRD; j++) {
            tlatencylist[cnt] = rargs[i].latencylist[j];
            tretlist[cnt] = rargs[i].ret[j];
            terrlist[cnt] = rargs[i].errlist[j];
            cnt++;
        }
    }

    if (sflag) {
        qsort(tlatencylist, NB_READ, sizeof(int), cmp);
    }

    FILE *fp = NULL;
    if (rstfile[0] == '\0') {
        fp = stdout;
    } else {
        fp = fopen(rstfile, "w+");
    }

    if (NB_RTHRD > 0) {
        for (i = 0; i < NB_READ; i++) {
            fprintf(fp, "%d\t%d\t%d\n", tretlist[i], terrlist[i], tlatencylist[i]);
        }
    }

    if (NB_RTHRD > 0 && NB_READ > 0) {
        printf("Total Latency: %d us\n", calc_latency(start, end));
    }
}

int main(int argc, char **argv)
{
    parse_param(argc, argv);

    check_all_var();

    rw_thrd_main(argc, argv);

    printf("the end\n");
    return 0;
}
