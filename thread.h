#ifndef __THREADS_H
#define __THREADS_H

void *warmup_thread(void *args);

void *rw_iothread(void *arg);

void rw_thrd_main(int argc, char **argv);

#endif
