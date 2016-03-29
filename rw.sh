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
    sudo mount -t tmpfs -o size=32m tmpfs /mnt/tmpfs
fi


echo "BEGIN RW TEST"
# run rw second
#--write_nb_blocks 86320 \
        #--read_threads 1 \
        #--read_nb_blocks 1000 \
        #--warmup 76800 \
        #--warmup 78000 \

#for i in $(seq 1); do
#    echo "start warmup [$i]"
#    ./sio \
#        --device /dev/sda \
#        --warmup 7800
#done
#
#exit
        #--warmup 76800 \
        #--warmup 76800 \
        # 8400
        #--write_threads 1 \
        #--write_nb_blocks 2000 \

# all write threads are targeting /dev/sda implicitly
for disk in "md127"; do #"sdb" "sdc" "sdd"; do
    if [[ "X""$1" == "X" ]]; then
        for i in $(seq 1); do
            ./sio \
                --device /dev/$disk \
                --warmup 78500 
        done
    fi

    ./sio \
        --device /dev/$disk \
        --read_threads 4 \
        --read_nb_blocks 8000 \
        --write_threads 1 \
        --write_nb_blocks 2000 \
        --verbose \
        --sort \
        --output /mnt/tmpfs/gc-blocking-d100-${disk}.rst 
done
