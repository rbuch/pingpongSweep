#! /usr/bin/env python

import sys
import statistics

entries = {}

for filename in sys.argv[1:]:
    with open(filename) as f:
        lines = f.readlines()
        for line in lines:
            if 'roundtrip time' in line:
                parts = line.split()
                time = float(parts[4])
                size = int(parts[7])
                if size not in entries:
                    entries[size] = []
                entries[size].append(time)

for time in entries:
    sample = sorted(entries[time])[1:-1]
    print(time, statistics.mean(sample) / 2)