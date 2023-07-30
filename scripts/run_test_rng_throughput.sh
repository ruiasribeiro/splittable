#!/bin/bash

type_list=(mt minstd ranlux24 xoshiro128)
worker_list=(1 2 4 8 16 32 64 128)
seconds=60
runs=5

printf "benchmark,workers,execution time (s),operations,throughput (ops/s)\n"

for type in ${type_list[@]}; do
    for workers in ${worker_list[@]}; do
        for _ in $(seq $runs); do
            ./build/bin/test_rng_throughput -b $type -w $workers -d $seconds
        done
    done
done
