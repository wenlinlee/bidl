import numpy as np
import sys

bidl_latency_raw = []
total_latency_raw = []
txnumlist = []
bidl_mean_latency,bidl_mean_latency1 = 0,0
lines = []
lines = sys.stdin.readlines()
t,txnum,total = 0,0,0
for line in lines:
        if line.split()[3] == "Transaction":
                total_latency,ave_latency = int(line.split()[19]),float(line.split()[15])
                i = line.split()[12]
                j = i.split(",")[0]
                txnumlist.append(j)
                txnum += int(j)
                total_latency_raw.append(total_latency)
                bidl_latency_raw.append(float(ave_latency))
                t += float(ave_latency)
                total += total_latency
        if line.split()[3] == "persisted":
                persisted_latency = line.split()[14]
                txnumlist.append(1)
                txnum += 1
                total_latency_raw.append(persisted_latency)
                total += float(persisted_latency)
len = len(bidl_latency_raw)
print(bidl_latency_raw)
print(total_latency_raw)
print(txnumlist)
bidl_latency_raw = bidl_latency_raw.sort()
if len == 0 and txnum != 0:
   bidl_mean_latency1 =  total/txnum
elif txnum == 0 and len != 0:
   bidl_mean_latency =  t/len
elif len!=0 and txnum !=0:
   bidl_mean_latency1 =  total/txnum
   bidl_mean_latency =  t/len
else:
   print("no tx commit!")
#bidl_mean_latency1 =  np.mean(bidl_latency_raw[int(num*0.3):int(num*0.6)])
print(bidl_mean_latency, "ms")
print(bidl_mean_latency1, "ms")
