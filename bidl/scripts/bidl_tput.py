import numpy as np
import sys

bidl_tps_raw = []
bidl_tps_raw1 = []
bidl_mean_tps = 0
lines = []
lines = sys.stdin.readlines()
for line in lines:
        if "Inf" not in line:
                tput,tput1 = line.split()[6],line.split()[11]
                bidl_tps_raw.append(float(tput))
                bidl_tps_raw1.append(float(tput1))
                
num = len(bidl_tps_raw)
bidl_tps_raw.sort()
print(bidl_tps_raw)
bidl_mean_tps =  np.mean(bidl_tps_raw[int(num*0.2):int(num*0.8)])

bidl_mean_tps1 = bidl_tps_raw1[-1]
print(bidl_mean_tps, "kTxns/s")
print(bidl_tps_raw1)
print(bidl_mean_tps1, "kTxns/s")

