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
rcParams["figure.figsize"] = 5, 3.5
# rcParams["font.family"] = "Helvetica"

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

plt.xscale("log")

ticks = df[df["benchmark"] == "single"]["workers"]
plt.xticks(ticks, ticks)

sns.lineplot(
    data=df,
    x="workers",
    y="throughput (ops/s)",
    hue="benchmark",
    style="benchmark",
    markers=markers,
)

result_dir = os.path.dirname(args.csv_path)
Path(os.path.join(result_dir, "graphs")).mkdir(exist_ok=True)

plt.tight_layout()  # avoids cropping the labels
file_name = Path(args.csv_path).stem
plt.savefig(os.path.join(result_dir, "graphs", f"{file_name}-throughput.pdf"))
