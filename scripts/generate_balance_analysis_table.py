#!/usr/bin/env python3

import argparse
import numpy as np
import pandas as pd

from tabulate import tabulate

parser = argparse.ArgumentParser()
parser.add_argument("csv_path")

args = parser.parse_args()

df = pd.read_csv(args.csv_path, index_col=False)

if "read percentage" in df:
    result_type = "write throughput (ops/s)"
else:
    result_type = "execution time (s)"

df = (
    df[
        [
            "benchmark",
            result_type,
            "abort rate",
            "avg balance interval (ms)",
        ]
    ]
    .groupby(["benchmark"])
    .agg(
        {
            result_type: np.mean,
            "abort rate": np.mean,
            "avg balance interval (ms)": np.mean,
        }
    )
)

print(
    tabulate(
        df,
        tablefmt="latex_booktabs",
        floatfmt=(".0f", ".1f", ".2%", ".2f"),
    )
)
