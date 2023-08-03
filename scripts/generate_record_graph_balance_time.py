#!/usr/bin/env python3

import argparse
import os

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns

from matplotlib import rcParams, ticker
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("csv_path")

args = parser.parse_args()

# figure size in inches
rcParams["figure.figsize"] = 5.25, 3
rcParams["font.family"] = "Inter"
rcParams["font.size"] = 14

df = (
    pd.read_csv(args.csv_path, index_col=False)
    .groupby(["benchmark", "records"])
    .agg({"avg balance interval (ms)": np.mean})
    .reset_index()
    .rename(columns={"benchmark": "Type"})
)

df.loc[df["Type"] == "mrv-flex-vector.balance-none", "Type"] = "None"
df.loc[df["Type"] == "mrv-flex-vector.balance-random", "Type"] = "Random"
df.loc[df["Type"] == "mrv-flex-vector.balance-minmax", "Type"] = "Min-max"
df.loc[df["Type"] == "mrv-flex-vector.balance-all", "Type"] = "All"

df = df[df["Type"] != "None"]
# df = df[df["avg balance interval (ms)"] < 29000]

df = df.sort_values(
    by=["Type"], key=lambda x: x.map({"All": 0, "Min-max": 1, "Random": 2, "None": 3})
)

marker = ["o", "v", "^", "<", ">", "8", "s", "p", "*", "h", "H", "D", "d", "P", "X"]
markers = [marker[i] for i in range(len(df["Type"].unique()))]

plt.xscale("log")
plt.yscale("log")
plt.gca().yaxis.set_minor_locator(ticker.LogLocator(base=10.0, subs=(0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9), numticks=9))

ticks = df["records"].unique()
plt.xticks(ticks, ticks, rotation=90)
plt.gca().xaxis.set_tick_params(which="minor", bottom=False)

chart = sns.lineplot(
    data=df,
    x="records",
    y="avg balance interval (ms)",
    hue="Type",
    style="Type",
    markers=markers,
)

plt.ylim(top=3000) # a hack
chart.get_legend().set_title(None)

plt.xlabel("Records")
plt.ylabel("Balance time (ms)")

plt.axhline(y=100, color="r", linestyle="--")

# plt.ylim(top=20000)

result_dir = os.path.dirname(args.csv_path)
Path(os.path.join(result_dir, "graphs")).mkdir(exist_ok=True)

plt.tight_layout()  # avoids cropping the labels
file_name = Path(args.csv_path).stem
plt.savefig(
    os.path.join(result_dir, "graphs", f"{file_name}-record-balance-time.pdf"),
    bbox_inches="tight",
    pad_inches=0.0,
)
