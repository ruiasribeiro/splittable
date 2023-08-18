#!/usr/bin/env python3

import argparse
import math
import os

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import scipy.stats as ss
import seaborn as sns

from matplotlib import rcParams
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("csv_path", index_col=False)

args = parser.parse_args()

# figure size in inches
rcParams["figure.figsize"] = 4.5, 3
rcParams["font.family"] = "Inter"
rcParams["font.size"] = 14

df = pd.read_csv(args.csv_path).rename(columns={"benchmark": "Type"})


def occurrences_to_list(input: str) -> list[int]:
    values = input.split(";")
    return list(map(lambda value: int(value), values))


sns.set_style("whitegrid")

weights = occurrences_to_list(df.iloc[0]["occurrences"])
plot_data = list(range(len(weights)))
hist, bin_edges = np.histogram(plot_data, weights=weights, bins=len(weights))
plt.plot(bin_edges[:-1], hist, label="yay")

weights = occurrences_to_list(df.iloc[1]["occurrences"])
hist, bin_edges = np.histogram(plot_data, weights=weights, bins=len(weights))
plt.plot(bin_edges[:-1], hist)

weights = occurrences_to_list(df.iloc[2]["occurrences"])
hist, bin_edges = np.histogram(plot_data, weights=weights, bins=len(weights))
plt.plot(bin_edges[:-1], hist)

weights = occurrences_to_list(df.iloc[3]["occurrences"])
hist, bin_edges = np.histogram(plot_data, weights=weights, bins=len(weights))
plt.plot(bin_edges[:-1], hist)
# chart.get_legend().set_title(None)


def custom_tick(value: int) -> str:
    match value:
        case v if v >= 1_000_000:
            return "{:,.0f}M".format(v / 1_000_000)
        case v if v >= 1_000:
            return "{:,.0f}k".format(v / 1_000)
        case other:
            return "{:,.0f}".format(other)


# ylabels = [custom_tick(y) for y in chart.get_yticks()]
# chart.set_yticklabels(ylabels)

plt.xlabel("Number")
plt.ylabel("Occurrences")


result_dir = os.path.dirname(args.csv_path)
Path(os.path.join(result_dir, "graphs")).mkdir(exist_ok=True)

plt.tight_layout()  # avoids cropping the labels
file_name = Path(args.csv_path).stem
plt.savefig(
    os.path.join(result_dir, "graphs", f"{file_name}-rng-dist.pdf"),
    bbox_inches="tight",
    pad_inches=0.0,
)
