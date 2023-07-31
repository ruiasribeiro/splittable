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
parser.add_argument("throughput_type", choices=["write", "read"])
parser.add_argument("csv_path")

args = parser.parse_args()

# figure size in inches
rcParams["figure.figsize"] = 3, 3
rcParams["font.family"] = "Inter"
rcParams["font.size"] = 14


df = (
    pd.read_csv(args.csv_path, index_col=False)
    .groupby(["benchmark", "read percentage"])
    .agg(
        {
            "writes": np.mean,
            "reads": np.mean,
            "write throughput (ops/s)": np.mean,
            "read throughput (ops/s)": np.mean,
            "abort rate": np.mean,
        }
    )
    .reset_index()
    .rename(columns={"benchmark": "Type"})
)

df.loc[df["Type"] == "single", "Type"] = "Single"
df.loc[df["Type"] == "mrv-flex-vector", "Type"] = "MRV"
df.loc[df["Type"] == "pr-array", "Type"] = "PR"

throughput_type = args.throughput_type

if throughput_type == "write":
    df = df[df["read percentage"] < 100]
else:
    df = df[df["read percentage"] > 0]

marker = ["o", "v", "^", "<", ">", "8", "s", "p", "*", "h", "H", "D", "d", "P", "X"]
markers = [marker[i] for i in range(len(df["Type"].unique()))]

# plt.xscale("log")

# ticks = df[df["Type"] == "Single"]["read percentage"]
if throughput_type == "write":
    ticks = [0, 25, 50, 75, 95]
else:
    ticks = [5, 25, 50, 75, 100]
plt.xticks(ticks, ticks)
# plt.xticks(rotation=60)

chart = sns.lineplot(
    data=df,
    x="read percentage",
    y=f"{throughput_type} throughput (ops/s)",
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

plt.xlabel("Read percentage (%)")
plt.ylabel(f"{throughput_type.capitalize()} throughput (ops/s)")

result_dir = os.path.dirname(args.csv_path)
Path(os.path.join(result_dir, "graphs")).mkdir(exist_ok=True)

# plt.tight_layout()  # avoids cropping the labels
file_name = Path(args.csv_path).stem
plt.savefig(
    os.path.join(
        result_dir,
        "graphs",
        f"{file_name}-read-percentage-{throughput_type}-throughput.pdf",
    ),
    bbox_inches="tight",
    pad_inches=0.0,
)
