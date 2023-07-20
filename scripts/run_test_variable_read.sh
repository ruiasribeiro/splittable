#!/bin/bash

type_list=(single mrv-flex-vector pr-array)
worker_list=(8)
read_per_list=(0 20 40 60 80 100)
seconds=60
runs=5

printf "benchmark,workers,execution time (s),read percentage,writes,reads,write throughput (ops/s),read throughput (ops/s),abort rate\n"

for type in ${type_list[@]}; do
    for workers in ${worker_list[@]}; do
        for read_per in ${read_per_list[@]}; do
            for _ in $(seq $runs); do
                ./build/bin/test_variable_read $type $workers $read_per $seconds
            done
        done
    done
done