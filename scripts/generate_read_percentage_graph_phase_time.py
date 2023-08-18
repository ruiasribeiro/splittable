#!/usr/bin/env python3

import argparse
import os

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns

from matplotlib import rcParams, ticker
# from matplotlibticker import FormatStrFormatter

from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("csv_path")

args = parser.parse_args()

# figure size in inches
rcParams["figure.figsize"] = 4.5, 3
rcParams["font.family"] = "Inter"
rcParams["font.size"] = 14

df = (
    pd.read_csv(args.csv_path, index_col=False)
    .groupby(["benchmark", "read percentage"])
    .agg({"avg phase interval (ms)": np.mean})
    .reset_index()
    .rename(columns={"benchmark": "Type"})
)

df.loc[df["Type"] == "pr-array", "Type"] = "PR"

marker = ["o", "v", "^", "<", ">", "8", "s", "p", "*", "h", "H", "D", "d", "P", "X"]
markers = [marker[i] for i in range(len(df["Type"].unique()))]

# plt.xscale("log")
plt.yscale("log")
plt.tick_params(axis='y', which='minor')
plt.gca().yaxis.set_minor_locator(ticker.LogLocator(base=10.0, subs=(0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9), numticks=9))

# ticks = df[df["Type"] == "Single"]["read percentage"]
ticks = [0, 25, 50, 75, 100]
plt.xticks(ticks, ticks)
# plt.xticks(rotation=60)

chart = sns.lineplot(
    data=df,
    x="read percentage",
    y="avg phase interval (ms)",
    hue="Type",
    style="Type",
    markers=markers,
)

# plt.ylim(top=4000) # a hack
chart.get_legend().remove()

plt.xlabel("Read percentage (%)")
plt.ylabel("Phase time (ms)")

plt.axhline(y=20, color="r", linestyle="--")

result_dir = os.path.dirname(args.csv_path)
Path(os.path.join(result_dir, "graphs")).mkdir(exist_ok=True)

plt.tight_layout()  # avoids cropping the labels
# plt.margins(tight=True)
file_name = Path(args.csv_path).stem
plt.savefig(
    os.path.join(result_dir, "graphs", f"{file_name}-read-percentage-phase-time.pdf"),
    bbox_inches="tight",
    pad_inches=0.0,
)
