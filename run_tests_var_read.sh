#!/bin/bash

type_list=(single mrv-array pr-array)
worker_list=(8)
read_per_list=(0 20 40 60 80 100)
seconds=5
runs=3

for type in ${type_list[@]}; do
    for workers in ${worker_list[@]}; do
        for read_per in ${read_per_list[@]}; do
            for _ in $(seq $runs); do
            ./a.out $type $workers $read_per $seconds
            done
        done
    done
done
