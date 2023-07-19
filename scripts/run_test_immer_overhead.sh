#!/bin/bash

worker_list=(1 2 4 8 16 32 64 128)
type_list=(stl_vector stl_vector_ptrs immer_flex_vector)
# padding_list=(10 1000 100000 10000000)
padding_list=(100000)
seconds=60
runs=5

printf "benchmark,workers,execution time (s),padding,commited operations,throughput (ops/s)\n"

for type in ${type_list[@]}; do
    for workers in ${worker_list[@]}; do
        for padding in ${padding_list[@]}; do
            for _ in $(seq $runs); do
                ./build/bin/test_immer_overhead $type $workers $seconds $padding
            done
        done
    done
done
