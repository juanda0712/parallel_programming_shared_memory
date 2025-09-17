import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

# --------------------------
# Carpetas de entrada/salida
# --------------------------
RESULTS_FILE = "outputs/results.csv"
GRAPH_DIR = "graficas"
os.makedirs(GRAPH_DIR, exist_ok=True)

# --------------------------
# Leer CSV
# --------------------------
df = pd.read_csv(RESULTS_FILE)

# --------------------------
# Limpiar columnas numéricas
# --------------------------
num_cols = ["t_gen","t_count","t_merge","total","real","user","sys","cpu%",
            "cycles","instructions","cache_ref","cache_miss"]

for col in num_cols:
    df[col] = df[col].astype(str).str.replace(r"[^0-9eE\.\-]", "", regex=True)
    df[col] = pd.to_numeric(df[col], errors='coerce')

# --------------------------
# Promediar por método, variante y threads
# --------------------------
df_avg = df.groupby(["method","variant","threads"]).mean(numeric_only=True).reset_index()

# --------------------------
# Calcular speedup y eficiencia
# --------------------------
df_avg["speedup"] = np.nan
df_avg["efficiency"] = np.nan
for method in df_avg["method"].unique():
    for variant in df_avg["variant"].unique():
        base_time_row = df_avg[(df_avg["method"]==method) & 
                               (df_avg["variant"]==variant) & 
                               (df_avg["threads"]==1)]
        if not base_time_row.empty:
            base_time = base_time_row["total"].values[0]
            idx = df_avg[(df_avg["method"]==method) & (df_avg["variant"]==variant)].index
            df_avg.loc[idx, "speedup"] = base_time / df_avg.loc[idx, "total"]
            df_avg.loc[idx, "efficiency"] = df_avg.loc[idx, "speedup"] / df_avg.loc[idx, "threads"]

# --------------------------
# Función para graficar métricas
# --------------------------
def plot_metric(metric, ylabel=None, filename=None, logy=False):
    methods = df_avg["method"].unique()
    fig, axes = plt.subplots(1, len(methods), figsize=(6*len(methods),5), sharey=not logy)
    
    if len(methods) == 1:
        axes = [axes]
    
    for ax, method in zip(axes, methods):
        for variant in df_avg["variant"].unique():
            sub = df_avg[(df_avg["method"]==method) & (df_avg["variant"]==variant)]
            ax.plot(sub["threads"], sub[metric], marker="o", label=variant)
        ax.set_title(method.upper())
        ax.set_xlabel("Threads")
        ax.set_ylabel(ylabel if ylabel else metric.capitalize())
        ax.grid(True)
        ax.legend()
        if logy:
            ax.set_yscale("log")
    
    plt.tight_layout()
    if filename:
        plt.savefig(os.path.join(GRAPH_DIR, filename))
    plt.close()

# --------------------------
# Graficar métricas
# --------------------------
plot_metric("total", ylabel="Execution time (s)", filename="execution_time.png", logy=False)
plot_metric("speedup", ylabel="Speedup", filename="speedup.png")
plot_metric("efficiency", ylabel="Efficiency", filename="efficiency.png")
plot_metric("cpu%", ylabel="%CPU used", filename="cpu_usage.png")
plot_metric("cache_miss", ylabel="Cache misses", filename="cache_misses.png", logy=True)

print(f"Plots saved in '{GRAPH_DIR}/': execution_time.png, speedup.png, efficiency.png, cpu_usage.png, cache_misses.png")
