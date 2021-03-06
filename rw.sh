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
    sudo chown sim:sim /mnt/tmpfs -R
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
for disk in "md0"; do #"sdb" "sdc" "sdd"; do
    ./sio \
        --device /dev/$disk \
        --read_threads 10 \
        --read_nb_blocks 400000 \
        --write_threads 1 \
        --write_nb_blocks 30000 \
        --sort \
        --verbose \
        --output /mnt/tmpfs/vssim-md0.raw
done
