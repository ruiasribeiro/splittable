#!/bin/bash

worker_list=(1 2 4 8 16 32 64 128)
type_list=(single mrv-array)
seconds=60
runs=1

for type in ${type_list[@]}; do
    for workers in ${worker_list[@]}; do
        for _ in $(seq $runs); do
            ./a.out $type $workers $seconds
        done
    done
done
