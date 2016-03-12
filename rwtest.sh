#!/bin/bash

if [[ $UID != 0 ]]; then
    echo "root privilege needed"
    exit
fi

if [[ ! -d /mnt/tmpfs ]]; then
    mkdir -p /mnt/tmpfs
fi

isMounted=$(mount | grep "/mnt/tmpfs")

if [[ "X" == "X"$isMounted ]]; then
    echo ""
    #sudo mount -t tmpfs -o size=32m tmpfs /mnt/tmpfs
fi


# ./rwtest NB_RTHRD NB_READ NB_WTHRD NB_WRITE rstfile

#dd if=/dev/urandom of=/dev/sda bs=1M count=490
#echo "first 450MB writes finishes.."

#sleep 15 

# each 4k, 65536 <=> 256MB
# each 4k, 131072 <=> 512MB
#./rwtest 65536 1 1000 1 5120  m-rw1.rst

    #--warmup 65536 \
    #--write_threads 4 \
    #--write_nb_blocks 5120 \
    #--verbose \

    #--read_threads 1 \
    #--read_nb_blocks 1000 \
    #--output m-ro-wo-21.rst

./sio \
    --device /dev/sda \
    --read_threads 1 \
    --read_nb_blocks 1 \
    --verbose \
    --output /mnt/tmpfs/rwtest.rst

