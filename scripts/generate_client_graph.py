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

# df["workers"] = df["workers"].astype(str)

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


# print(df)

# single_df = df[df['benchmark'] == 'single']
# mrv_flex_vector_df = df[df['benchmark'] == 'mrv-flex-vector']
# pr_array_df = df[df['benchmark'] == 'pr-array']

# sns.set_theme()
plt.xscale("log")

ticks = df[df["benchmark"] == "single"]["workers"]
plt.xticks(ticks, ticks)

sns.set_style("white", {"axes.linewidth": 0.5})
sns.lineplot(
    data=df,
    x="workers",
    y="abort rate",
    hue="benchmark",
    marker="o",
    linestyle='--',
    legend=None,
    palette="pastel"
    # order=["1", "2", "4", "8", "16", "32", "64", "128"],
    # sort=False,
)
ax2 = plt.twinx()
sns.lineplot(
    ax=ax2,
    data=df,
    x="workers",
    y="throughput (ops/s)",
    hue="benchmark",
    marker="o",
    palette="bright"
    # order=["1", "2", "4", "8", "16", "32", "64", "128"],
    # sort=False,
)
# sns.lineplot(data=mrv_flex_vector_df, kind="line", x="workers", y="throughput (ops/s)")
# sns.lineplot(data=pr_array_df, kind="line", x="workers", y="throughput (ops/s)")

plt.savefig("test.png")
