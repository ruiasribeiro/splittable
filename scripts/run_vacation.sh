#!/bin/bash

splittable_list=(single mrv-flex-vector pr-array)
client_list=(1 2 4 8 16 32 64 128)
runs=5
num_threads=4

# original high contention
# vacation_params=(-n4 -q60 -u90 -r1048576 -T4194304)
vacation_params=(-n4 -q60 -u90 -r104857 -T4194304)

echo "benchmark,workers,execution time (s),abort rate"
for type in ${splittable_list[@]}; do
    make clean >/dev/null 2>&1
    SPLITTABLE_NUM_THREADS=${num_threads} make >/dev/null 2>&1

    for client in ${client_list[@]}; do
        for _ in $(seq $runs); do
            ./build/bin/vacation ${vacation_params[@]} -t${client} -s${type}
        done
    done
done