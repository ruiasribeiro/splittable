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
rcParams["figure.figsize"] = 4.5, 3
rcParams["font.family"] = "Inter"
rcParams["font.size"] = 14

df = pd.read_csv(args.csv_path, index_col=False)

df = (
    df.groupby(["benchmark", "type"])
    .agg(
        {
            "energy consumption (J)": np.mean,
            "execution time (s)": np.mean,
        }
    )
    .reset_index()
)

df["efficiency"] = 4194304 / df["energy consumption (J)"]

df.loc[df["benchmark"] == "single", "benchmark"] = "Single"
df.loc[df["benchmark"] == "mrv-flex-vector", "benchmark"] = "MRV"
df.loc[df["benchmark"] == "pr-array", "benchmark"] = "PR"

chart = sns.barplot(
    data=df,
    x="type",
    y="efficiency",
    hue="benchmark",
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

plt.xlabel("Type")
plt.ylabel("Efficiency (ops/J)")

result_dir = os.path.dirname(args.csv_path)
Path(os.path.join(result_dir, "graphs")).mkdir(exist_ok=True)

plt.tight_layout()  # avoids cropping the labels
file_name = Path(args.csv_path).stem
plt.savefig(
    os.path.join(result_dir, "graphs", f"{file_name}-power-execution-time.pdf"),
    bbox_inches="tight",
    pad_inches=0.0,
)
