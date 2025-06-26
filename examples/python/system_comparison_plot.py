import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns

BIG_SIZE = 11
FIGRATIO = 3 / 4
FIGWIDTH = 3.335 # inches
FIGHEIGHT = FIGWIDTH * FIGRATIO
FIGSIZE = (FIGWIDTH, FIGHEIGHT)

plt.rcParams.update(
{
"figure.figsize": FIGSIZE,
"figure.dpi": 300,
}
)

COLORS = sns.color_palette("Paired")
sns.set_style("ticks")
sns.set_palette(COLORS)

plt.rc("font", size=BIG_SIZE) # controls default text sizes
plt.rc("axes", titlesize=BIG_SIZE) # fontsize of the axes title
plt.rc("axes", labelsize=BIG_SIZE) # fontsize of the x and y labels
plt.rc("xtick", labelsize=BIG_SIZE) # fontsize of the tick labels
plt.rc("ytick", labelsize=BIG_SIZE) # fontsize of the tick labels
plt.rc("legend", fontsize=BIG_SIZE) # legend fontsize
plt.rc("figure", titlesize=BIG_SIZE) # fontsize of the figure title

# Message sizes in bytes
msg_sizes_bytes = [
    1024,       # 1 KB
    4096,       # 4 KB
    16 * 1024,  # 16 KB
    64 * 1024,  # 64 KB
    256 * 1024, # 256 KB
    1024 * 1024,  # 1 MB
    10 * 1024 * 1024,  # 10 MB
    100 * 1024 * 1024  # 100 MB
]

# Bandwidth data (GB/s)
nccl_sendrecv = [0.04, 0.16, 0.60, 1.96, 7.60, 18.63, 39.60, 42.50]
uccl_kvtrans   = [0.09, 0.36, 1.37, 5.10, 15.02, 27.76, 46.64, 49.11]
ucx_perftest   = [0.16, 0.62, 2.28, 7.57, 17.01, 31.29, 41.67, 42.60]
nixl_perftest = [0.00, 0.01, 0.05, 0.24, 0.87, 3.42, 21.33, 40.70
    
]
# Convert message sizes to MB for plotting
msg_sizes_mb = np.array(msg_sizes_bytes) / (1024 * 1024)

plt.semilogx(msg_sizes_mb, uccl_kvtrans, marker='s', label='UCCL')
plt.semilogx(msg_sizes_mb, nccl_sendrecv, marker='o', label='NCCL')
# plt.semilogx(msg_sizes_mb, ucx_perftest, marker='^', label='UCX_perftest')
plt.semilogx(msg_sizes_mb, nixl_perftest, marker='^', label='NIXL')

# Draw a red dotted curve at 50 GB/s
plt.axhline(y=50, color='red', linestyle='--', label='Line Rate')

plt.xlabel('Message Size (MB)')
plt.ylabel('Throughput (GB/s)')
# plt.title('Bandwidth vs Message Size')
# plt.grid(True, which="both", ls="--")
plt.legend()
plt.tight_layout()
plt.savefig('system-comparison.png')
plt.close()