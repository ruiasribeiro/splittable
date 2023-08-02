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
parser.add_argument("throughput_type", choices=["generic", "write", "read"])
parser.add_argument("csv_path")

args = parser.parse_args()

# figure size in inches
rcParams["figure.figsize"] = 4, 4.4
rcParams["font.family"] = "Inter"
rcParams["font.size"] = 14

df = (
    pd.read_csv(args.csv_path, index_col=False)
    .groupby(["benchmark", "records"])
    .agg(
        {
            "write throughput (ops/s)": np.mean,
            "read throughput (ops/s)": np.mean,
        }
    )
    .reset_index()
    .rename(columns={"benchmark": "Type"})
)

df.loc[df["Type"] == "mrv-flex-vector.balance-none", "Type"] = "None"
df.loc[df["Type"] == "mrv-flex-vector.balance-random", "Type"] = "Random"
df.loc[df["Type"] == "mrv-flex-vector.balance-minmax", "Type"] = "Min-max"
df.loc[df["Type"] == "mrv-flex-vector.balance-all", "Type"] = "All"

throughput_type = args.throughput_type

df = df.sort_values(
    by=["Type"], key=lambda x: x.map({"All": 0, "Min-max": 1, "Random": 2, "None": 3})
)

marker = ["o", "v", "^", "<", ">", "8", "s", "p", "*", "h", "H", "D", "d", "P", "X"]
markers = [marker[i] for i in range(len(df["Type"].unique()))]

plt.xscale("log")

ticks = df["records"].unique()
plt.xticks(ticks, ticks, rotation=90)
plt.gca().xaxis.set_tick_params(which="minor", bottom=False)

chart = sns.lineplot(
    data=df,
    x="records",
    y=f"{throughput_type} throughput (ops/s)"
    if throughput_type != "generic"
    else "throughput (ops/s)",
    hue="Type",
    style="Type",
    markers=markers,
)

chart.get_legend().set_title(None)
# plt.legend(frameon=False)


def custom_tick(value: int) -> str:
    match value:
        case v if v >= 1_000_000:
            return "{:,.0f}M".format(v / 1_000_000)
        case v if v >= 1_000:
            return "{:,.0f}k".format(v / 1_000)
        case other:
            return "{:,.0f}".format(other)


plt.ylim(bottom=0)

ylabels = [custom_tick(y) for y in chart.get_yticks()]
chart.set_yticklabels(ylabels)

plt.xlabel("Records")
plt.ylabel(
    f"{throughput_type.capitalize()} throughput (ops/s)"
    if throughput_type != "generic"
    else "Throughput (ops/s)"
)

result_dir = os.path.dirname(args.csv_path)
Path(os.path.join(result_dir, "graphs")).mkdir(exist_ok=True)

plt.tight_layout()  # avoids cropping the labels
file_name = Path(args.csv_path).stem
plt.savefig(
    os.path.join(result_dir, "graphs", f"{file_name}-record-{throughput_type}-throughput.pdf"),
    bbox_inches="tight",
    pad_inches=0.0,
)
