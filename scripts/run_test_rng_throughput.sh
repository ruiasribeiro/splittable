#!/bin/bash

type_list=(mt minstd ranlux24)
worker_list=(1 2 4)
seconds=60
runs=1

printf "benchmark,workers,execution time (s),operations,throughput (ops/s)\n"

for type in ${type_list[@]}; do
    for workers in ${worker_list[@]}; do
        for _ in $(seq $runs); do
            ./build/bin/test_rng_throughput $type $workers $seconds
        done
    done
done
