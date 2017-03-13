#define _GNU_SOURCE

#include "sio.h"
#include "util.h"
#include "thread.h"

//#define DEBUG

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


int main(int argc, char **argv)
{
    parse_param(argc, argv);

    check_all_var();

    rw_thrd_main(argc, argv);

    printf("end of main\n");

    return 0;
}
