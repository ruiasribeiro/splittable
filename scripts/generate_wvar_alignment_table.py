#!/usr/bin/env python3

import argparse
import pandas as pd

from tabulate import tabulate

parser = argparse.ArgumentParser()
parser.add_argument("csv_path")

args = parser.parse_args()

df = pd.read_csv(args.csv_path)

pivot_df = pd.pivot_table(
    df,
    values="throughput (ops/s)",
    index="workers",
    columns="benchmark",
    aggfunc="mean",
)

sorted_df = pivot_df.sort_index()

cols = sorted_df.columns.tolist()

# Swap column names in list
cols[0], cols[1] = cols[1], cols[0]  # Swap 1st and 2nd
cols[2], cols[3] = cols[3], cols[2]  # Swap 3rd and 4th

# Re-index DataFrame with new column list
sorted_df = sorted_df[cols]

# Calculate percentage difference for first two columns
sorted_df["diff_perc_1_2"] = (
    (sorted_df[cols[1]] - sorted_df[cols[0]]) / sorted_df[cols[0]] * 100
)

# Calculate percentage difference for third and fourth column
sorted_df["diff_perc_3_4"] = (
    (sorted_df[cols[3]] - sorted_df[cols[2]]) / sorted_df[cols[2]] * 100
)

sorted_df.insert(2, "diff_perc_1_2_temp", sorted_df["diff_perc_1_2"])
del sorted_df["diff_perc_1_2"]

sorted_df.insert(5, "diff_perc_3_4_temp", sorted_df["diff_perc_3_4"])
del sorted_df["diff_perc_3_4"]

# Round percentage columns to 2 decimal places
sorted_df["diff_perc_1_2_temp"] = sorted_df["diff_perc_1_2_temp"].round(2)
sorted_df["diff_perc_3_4_temp"] = sorted_df["diff_perc_3_4_temp"].round(2)

print(
    tabulate(
        sorted_df,
        tablefmt="latex_booktabs",
        floatfmt=(".0f", ".1f", ".1f", ".2f", ".1f", ".1f", ".2f"),
    )
)
