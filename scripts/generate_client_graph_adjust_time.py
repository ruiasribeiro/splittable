#!/usr/bin/env python3

import argparse
import os

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns

from matplotlib import rcParams
from matplotlib.ticker import AutoMinorLocator
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("csv_path")

args = parser.parse_args()

# figure size in inches
rcParams["figure.figsize"] = 3.5, 3.5
rcParams["font.family"] = "Inter"
rcParams["font.size"] = 14


df = (
    pd.read_csv(args.csv_path, index_col=False)
    .groupby(["benchmark", "workers"])
    .agg({"avg adjust interval (ms)": np.mean})
    .reset_index()
    .rename(columns={"benchmark": "Type"})
)

df.loc[df["Type"] == "single", "Type"] = "Single"
df.loc[df["Type"] == "mrv-flex-vector", "Type"] = "MRV"
df.loc[df["Type"] == "pr-array", "Type"] = "PR"

marker = ["o", "v", "^", "<", ">", "8", "s", "p", "*", "h", "H", "D", "d", "P", "X"]
markers = [marker[i] for i in range(len(df["Type"].unique()))]

plt.xscale("log")
plt.yscale("log")
# plt.gca().yaxis.set_minor_locator(AutoMinorLocator())

ticks = df["workers"].unique()
plt.xticks(ticks, ticks, rotation=90)
plt.gca().xaxis.set_tick_params(which="minor", bottom=False)

chart = sns.lineplot(
    data=df,
    x="workers",
    y="avg adjust interval (ms)",
    hue="Type",
    style="Type",
    markers=markers,
)

chart.get_legend().set_title(None)

plt.xlabel("Clients")
plt.ylabel("Adjust time (ms)")

plt.axhline(y=1000, color="r", linestyle="--")

result_dir = os.path.dirname(args.csv_path)
Path(os.path.join(result_dir, "graphs")).mkdir(exist_ok=True)

plt.tight_layout()  # avoids cropping the labels
file_name = Path(args.csv_path).stem
plt.savefig(
    os.path.join(result_dir, "graphs", f"{file_name}-client-adjust-time.pdf"),
    bbox_inches="tight",
    pad_inches=0.0,
)
