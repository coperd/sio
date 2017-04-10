#!/bin/bash

if [[ $UID != 0 ]]; then
    echo "root privilege needed"
    exit
fi

PIDS=""
RESULT=0

TGTDIR="chip40-ro-lat-sio"
mkdir -p $TGTDIR

{

echo "Benchmark start at $(date)"
for bs in 64 128 256; do #1 2 4 8 16 32; do
    for nt in 1 2 4 8 16 32; do
        ((tbs = bs * 4))

        echo ""
        echo "Start[$(date)] testing: $nt threads, with $tbs KB read I/O"
        echo ""

        for disk in "tgt0"; do
            ./sio \
                -d /dev/$disk \
                -b $bs \
                -r $nt \
                -p 20000 \
                -o $TGTDIR/chip40_2_${disk}-ro-${nt}t-${tbs}k-1_lat.log
        done

        echo "End[$(date)] testing ..."
        echo ""
        echo ""

        sleep 5

    done
done


#sleep 2
#trap 'kill $(jobs -p)' EXIT

chown -R huaicheng:huaicheng $TGTDIR

echo "Benchmark end at $(date)"

} | tee $TGTDIR/running.log
