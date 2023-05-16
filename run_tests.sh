#!/bin/bash

WORKERS=(1 2 4 8 16 32 64 128)
TYPES=(single mrv-array)
RUNS=5

for type in ${TYPES[@]}; do
    for workers in ${WORKERS[@]}; do
        for _ in $(seq $RUNS); do
            ./a.out $type $workers
            echo "finished!"
        done
    done
done
