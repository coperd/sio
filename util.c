#include "sio.h"


int cmp(const void *a, const void *b)
{
    return *((int *)a) >= *((int *)b);
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

uint64_t get_disk_sz_in_bytes(int fd)
{
    off_t size = lseek(fd, 0, SEEK_END);
    if (size == -1) {
        handle_error("lseek");
    }

    return (int64_t)size;
}

int64_t calc_latency(struct timespec, struct timespec);

/* return latency in us */
int64_t calc_latency(struct timespec start, struct timespec end)
{
    return (end.tv_sec - start.tv_sec) * 1000000 + \
        (end.tv_nsec - start.tv_nsec) / 1000;
}

void ssleep(struct timespec *ts)
{
    struct timespec rem;
    while (nanosleep(ts, &rem) == -1) {
        ts = &rem;
    }
}

