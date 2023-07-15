#!/usr/bin/env python3

import argparse
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns

parser = argparse.ArgumentParser()
parser.add_argument("csv_path")

args = parser.parse_args()

df = pd.read_csv(args.csv_path)

df = (
    df.groupby(["benchmark", "workers"])
    .agg(
        {
            "commited operations": np.mean,
            "throughput (ops/s)": np.mean,
            "abort rate": np.mean,
        }
    )
    .reset_index()
)

marker = ["o", "v", "^", "<", ">", "8", "s", "p", "*", "h", "H", "D", "d", "P", "X"]
markers = [marker[i] for i in range(len(df["benchmark"].unique()))]

# sns.set_theme()
plt.xscale("log")

ticks = df[df["benchmark"] == "single"]["workers"]
plt.xticks(ticks, ticks)

ax1 = plt.subplot()
ax2 = plt.twinx()

sns.lineplot(
    ax=ax2,
    data=df,
    x="workers",
    y="abort rate",
    hue="benchmark",
    style="benchmark",
    markers=markers,
    legend=None,
    alpha=0.35,
)
plt.ylim([0, 1])

sns.lineplot(
    ax=ax1,
    data=df,
    x="workers",
    y="throughput (ops/s)",
    hue="benchmark",
    style="benchmark",
    markers=markers,
)

plt.savefig("test.pdf")
