#!/bin/bash

type_list=(mt minstd ranlux24)
worker_list=(1 2 4)
seconds=1
runs=1

printf "benchmark,workers,execution time (s),occurrences\n"

for type in ${type_list[@]}; do
    for workers in ${worker_list[@]}; do
        for _ in $(seq $runs); do
            ./build/bin/test_rng_distribution $type $workers $seconds
        done
    done
done
