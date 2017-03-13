#include "sio.h"
#include "util.h"
#include "thread.h"

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
