#ifndef __UTIL_H
#define __UTIL_H

#include "sio.h"

int cmp(const void *a, const void *b);

void sio_memalign(void **memptr, size_t alignment, size_t size);

uint64_t get_disk_sz_in_bytes(int fd);

int64_t calc_latency(struct timespec, struct timespec);

void ssleep(struct timespec *ts);

#endif
