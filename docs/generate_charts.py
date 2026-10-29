import pandas as pd
import matplotlib.pyplot as plt
import os

# Load CSV data
win_csv = "win32_benchmark.csv"
lin_csv = "linux_benchmark.csv"

if not os.path.exists(win_csv) or not os.path.exists(lin_csv):
    print("Error: Ensure both win32_benchmark.csv and linux_benchmark.csv are in docs/")
    exit(1)

df_win = pd.read_csv(win_csv)
df_lin = pd.read_csv(lin_csv)

# Merge datasets on Category and index
df_win['OS'] = 'Windows 11 (Win32)'
df_lin['OS'] = 'CachyOS (POSIX)'

df_combined = pd.concat([df_win, df_lin], ignore_index=True)

# 1. Plot System Call Latency Comparison
plt.figure(figsize=(12, 6))
categories = df_win['Category'].unique()

for idx, cat in enumerate(categories, 1):
    sub_win = df_win[df_win['Category'] == cat]
    sub_lin = df_lin[df_lin['Category'] == cat]
    
    plt.subplot(2, 3, idx)
    x_win = sub_win['System_Call']
    x_lin = sub_lin['System_Call']
    
    plt.bar(sub_win['System_Call'], sub_win['Latency_Microseconds'], alpha=0.7, label='Windows 11', color='skyblue')
    plt.bar(sub_lin['System_Call'], sub_lin['Latency_Microseconds'], alpha=0.7, label='CachyOS', color='orange')
    
    plt.title(f"{cat} Calls")
    plt.ylabel("Latency (us)")
    plt.xticks(rotation=30, ha='right')
    plt.legend()

plt.tight_layout()
plt.savefig("syscall_latency_comparison.png", dpi=300)
print("[SUCCESS] Chart saved to docs/syscall_latency_comparison.png")

# 2. Print Summary Performance Table for Final Report
print("\n=== SYSTEM CALL LATENCY COMPARISON TABLE (Microseconds) ===")
print(f"{'Category':<15} | {'Win32 Call':<22} | {'Win32 (us)':<12} | {'POSIX Call':<15} | {'Linux (us)':<12}")
print("-" * 85)

for i in range(min(len(df_win), len(df_lin))):
    w = df_win.iloc[i]
    l = df_lin.iloc[i]
    print(f"{w['Category']:<15} | {w['System_Call']:<22} | {w['Latency_Microseconds']:<12.2f} | {l['System_Call']:<15} | {l['Latency_Microseconds']:<12.2f}")