#!/bin/bash

splittable_list=(single mrv-flex-vector pr-array)
client_list=(1 2 4 8 16 32 64 128)
runs=5
mrv_balances=(none)

# low contention
# vacation_params=(-n2 -q90 -u98 -r1048576 -T4194304)
# high contention
# vacation_params=(-n4 -q60 -u90 -r1048576 -T4194304)

# We use 1000x less relations; the default is too much for TM.
vacation_params=(-n4 -q60 -u90 -r1048 -T4194304)

echo "benchmark,workers,execution time (s),abort rate,avg adjust interval (ms),avg balance interval (ms),avg phase interval (ms)"

for type in ${splittable_list[@]}; do
    if [ "$type" == "mrv-flex-vector" ]; then
        for balance_type in ${mrv_balances[@]}; do
            for client in ${client_list[@]}; do
                for _ in $(seq $runs); do
                    ./build/bin/vacation ${vacation_params[@]} -t${client} -s${type} -b${balance_type}
                done
            done
        done
    else 
        for client in ${client_list[@]}; do
            for _ in $(seq $runs); do
                ./build/bin/vacation ${vacation_params[@]} -t${client} -s${type}
            done
        done
    fi
done