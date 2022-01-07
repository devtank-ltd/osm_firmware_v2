#! /usr/bin/python3

from scipy.signal import find_peaks
import numpy as np
import matplotlib.pyplot as plt
import csv


NUM_ADCS = 4


all_data = []

with open("data.dat", "r") as f:
    csvf = csv.reader(f)
    for line in csvf:
        for datum in line:
            if datum != '':
                all_data.append(int(datum))

all_data_reordered = [[], [], [], []]

for i in range(0, len(all_data)):
    all_data_reordered[i%NUM_ADCS].append(all_data[i])

cc_data,_,_,_ = all_data_reordered

peak_index,_ = find_peaks(-np.array(cc_data), height=-4095)

peak_vals = []
for index in peak_index:
    peak_vals.append(cc_data[index])

# plt.plot(all_data_reordered[1])
# plt.plot(all_data_reordered[2])
# plt.plot(all_data_reordered[3])

plt.plot(peak_index, peak_vals)
plt.plot(np.linspace(0, len(cc_data), len(cc_data)), cc_data, marker='h')

midpoint = 2044

max_ = np.max(cc_data)
min_ = np.min(cc_data)
rms = midpoint - np.sqrt(np.mean(np.square(np.array(cc_data) - midpoint)))
rms_2 = np.sqrt(np.mean(np.square(np.array(cc_data))))
pk = midpoint -np.mean(np.array(peak_vals) - midpoint)
rms_f = midpoint - (pk-midpoint) * (2 ** (-0.5))

plt.plot(np.full(shape=len(cc_data), fill_value=midpoint, dtype=np.int), "--", color="gray")
plt.plot(np.full(shape=len(cc_data), fill_value=rms, dtype=np.int), "--", color="red")
plt.plot(np.full(shape=len(cc_data), fill_value=rms_f, dtype=np.int), "--", color="blue")

print("max = %u"% max_)
print("min = %u"% min_)
print("rms = %u"% rms)
print("rms_2 = %u"% rms_2)
print("pk = %u"% pk)
print("rms_f = %u"% rms_f)

plt.show()
