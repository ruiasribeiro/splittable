#!/usr/bin/env python3

import argparse
import os

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns

from matplotlib import rcParams
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("csv_path")

args = parser.parse_args()

# figure size in inches
rcParams["figure.figsize"] = 8.5, 3
rcParams["font.family"] = "Inter"
rcParams["font.size"] = 14


df = (
    pd.read_csv(args.csv_path, index_col=False)
    .groupby(["benchmark", "workers"])
    .agg({"throughput (ops/s)": np.mean})
    .reset_index()
    .rename(columns={"benchmark": "Type"})
)

df.loc[df["Type"] == "single", "Type"] = "Single"
df.loc[df["Type"] == "mrv-flex-vector", "Type"] = "MRV"
df.loc[df["Type"] == "pr-array", "Type"] = "PR"

df.loc[df["Type"] == "immer_flex_vector", "Type"] = "immer::flex_vector"
df.loc[df["Type"] == "stl_vector", "Type"] = "std::vector"
df.loc[df["Type"] == "stl_vector_ptrs", "Type"] = "std::vector (w/ pointers)"

marker = ["o", "v", "^", "<", ">", "8", "s", "p", "*", "h", "H", "D", "d", "P", "X"]
markers = [marker[i] for i in range(len(df["Type"].unique()))]

plt.xscale("log")

ticks = df["workers"].unique()
plt.xticks(ticks, ticks)
plt.gca().xaxis.set_tick_params(which="minor", bottom=False)

chart = sns.lineplot(
    data=df,
    x="workers",
    y="throughput (ops/s)",
    hue="Type",
    style="Type",
    markers=markers,
)

chart.get_legend().set_title(None)


def custom_tick(value: int) -> str:
    match value:
        case v if v >= 1_000_000:
            return "{:,.0f}M".format(v / 1_000_000)
        case v if v >= 1_000:
            return "{:,.0f}k".format(v / 1_000)
        case other:
            return "{:,.0f}".format(other)


ylabels = [custom_tick(y) for y in chart.get_yticks()]
chart.set_yticklabels(ylabels)

plt.xlabel("Clients")
plt.ylabel("Throughput (ops/s)")

result_dir = os.path.dirname(args.csv_path)
Path(os.path.join(result_dir, "graphs")).mkdir(exist_ok=True)

plt.tight_layout()  # avoids cropping the labels
file_name = Path(args.csv_path).stem
plt.savefig(
    os.path.join(result_dir, "graphs", f"{file_name}-client-throughput.pdf"),
    bbox_inches="tight",
    pad_inches=0.0,
)
