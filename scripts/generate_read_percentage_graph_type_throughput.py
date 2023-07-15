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
rcParams["figure.figsize"] = 5, 3.5
# rcParams["font.family"] = "Helvetica"


df = (
    pd.read_csv(args.csv_path)
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

ticks = df[df["Type"] == "Single"]["read percentage"]
plt.xticks(ticks, ticks)
plt.xticks(rotation=60)

sns.lineplot(
    data=df,
    x="read percentage",
    y=f"{throughput_type} throughput (ops/s)",
    hue="Type",
    style="Type",
    markers=markers,
)

plt.xlabel("Read percentage (%)")
plt.ylabel(f"{throughput_type.capitalize()} throughput (ops/s)")

result_dir = os.path.dirname(args.csv_path)
Path(os.path.join(result_dir, "graphs")).mkdir(exist_ok=True)

plt.tight_layout()  # avoids cropping the labels
file_name = Path(args.csv_path).stem
plt.savefig(
    os.path.join(result_dir, "graphs", f"{file_name}-read-percentage-{throughput_type}-throughput.pdf")
)
