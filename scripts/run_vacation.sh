#!/bin/bash

splittable_list=(SINGLE MRV_FLEX_VECTOR PR_ARRAY)
client_list=(1 2 4 8 16 32 64 128)
runs=1

vacation_params=(-n4 -q60 -u90 -r1048576 -T4194304)

for type in ${splittable_list[@]}; do
    make clean >/dev/null 2>&1
    SPLITTABLE_TYPE="${type}" make >/dev/null 2>&1

    echo "=========================="
    echo "Running $type..."
    echo "=========================="
    for client in ${client_list[@]}; do
        for _ in $(seq $runs); do
            ./build/bin/vacation ${vacation_params[@]} -t${client}
        done
    done
done