#!/bin/bash

type_list=(single mrv-flex-vector pr-array)
worker_list=(8)
read_per_list=(0 5 10 15 20 25 30 35 40 45 50 55 60 65 70 75 80 85 90 95 100)
padding_list=(100000)
seconds=60
runs=5
scale=1000
mrv_balances=(none)

printf "benchmark,workers,execution time (s),padding,read percentage,writes,reads,write throughput (ops/s),read throughput (ops/s),abort rate,avg adjust interval (ms),avg balance interval (ms),avg phase interval (ms)\n"

for type in ${type_list[@]}; do
    if [ "$type" == "mrv-flex-vector" ]; then
        for balance_type in ${mrv_balances[@]}; do
            for workers in ${worker_list[@]}; do
                for read_per in ${read_per_list[@]}; do
                    for padding in ${padding_list[@]}; do
                        for _ in $(seq $runs); do
                            ./build/bin/microbenchmark -b ${type} -w ${workers} -r ${read_per} -d ${seconds} -p ${padding} -s ${scale} -m ${balance_type}
                        done
                    done
                done
            done
        done
    else 
        for workers in ${worker_list[@]}; do
            for read_per in ${read_per_list[@]}; do
                for padding in ${padding_list[@]}; do
                    for _ in $(seq $runs); do
                        ./build/bin/microbenchmark -b ${type} -w ${workers} -r ${read_per} -d ${seconds} -p ${padding} -s ${scale}
                    done
                done
            done
        done
    fi
done
