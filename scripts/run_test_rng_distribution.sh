#!/bin/bash

type_list=(mt minstd ranlux24 xoshiro128)
seconds=60
runs=1

printf "benchmark,execution time (s),occurrences\n"

for type in ${type_list[@]}; do
    for _ in $(seq $runs); do
        ./build/bin/test_rng_distribution $type $seconds
    done
done
