#!/bin/bash

type_list=(single mrv-flex-vector pr-array)
worker_list=(8)
read_per_list=(5)
padding_list=(100000)
seconds=5
runs=5
scale=1000

printf "benchmark,workers,execution time (s),read percentage,writes,reads,write throughput (ops/s),read throughput (ops/s),abort rate,avg adjust interval (ms), avg balance interval (ms), avg phase interval (ms)\n"

for type in ${type_list[@]}; do
    for workers in ${worker_list[@]}; do
        for read_per in ${read_per_list[@]}; do
            for padding in ${padding_list[@]}; do
                for _ in $(seq $runs); do
                    ./build/bin/microbenchmark -b ${type} -w ${workers} -r ${read_per} -d ${seconds} -p ${padding} -s ${scale}
                done
            done
        done
    done
done
