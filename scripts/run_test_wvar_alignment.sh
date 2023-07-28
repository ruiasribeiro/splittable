#!/bin/bash

type_list=(immer-wvar immer-aligned-wvar std-wvar std-aligned-wvar)
worker_list=(1 2 4 8 16 32 64 128)
seconds=60
runs=5
padding=100000

printf "benchmark,workers,execution time (s),padding,operations,throughput (ops/s)\n"

for type in ${type_list[@]}; do
    for workers in ${worker_list[@]}; do
        for _ in $(seq $runs); do
            ./build/bin/test_wvar_alignment -b ${type} -w ${workers} -d ${seconds} -p ${padding}
        done
    done
done
