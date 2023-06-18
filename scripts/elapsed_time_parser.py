import sys
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator
import math

# Updated after parse_file() called
logdata = {}
part_idx_mx = 0
turnaround_times = {}

def parse_file(filename):
    global logdata, part_idx_mx, turnaround_times

    logdata = {}
    part_idx_mx = 0
    turnaround_times = {}

    with open(filename, mode="r", encoding="utf-8") as f:
        for line in f:
            ret = line.rstrip().split()
            session_name, part_idx, loop_idx, timestamp, data = ret
            part_idx = int(part_idx)
            loop_idx = int(loop_idx)
            timestamp = int(timestamp)

            if not part_idx in logdata:
                logdata[part_idx] = []
                part_idx_mx = max(part_idx_mx, part_idx)

            logdata[part_idx].append(timestamp)

    for i in range(0, part_idx_mx):
        turnaround_times[i] = []
        for j in range(len(logdata[0])):
            turnaround_times[i].append(logdata[i+1][j] - logdata[i][j])

    for i in range(0, part_idx_mx):
        sorted_times = sorted(turnaround_times[i])
        length = len(sorted_times)
        print("{}: part index = {}".format(session_name, i + 1))
        print("50%tile {}us".format(sorted_times[math.ceil(length * 0.5 - 1)]));
        print("90%tile {}us".format(sorted_times[math.ceil(length * 0.9 - 1)]));
        print("99%tile {}us".format(sorted_times[math.ceil(length * 0.99 - 1)]));
        print("------------------------")

    return session_name

def visualize(session_name, bins=50):
    for i in range(part_idx_mx):
        fig = plt.figure(figsize=(16, 16))
        max_value = max(turnaround_times[i])

        ax0 = fig.add_subplot(2, 1, 1)
        ax0.set_title("{}: part {} - elapsed time time-series".format(session_name, i))
        ax0.set_xlabel("sample index")
        ax0.set_ylabel("turn-around time (us)")
        ax0.set_ylim([0, max_value])
        ax0.plot(turnaround_times[i])

        ax1 = fig.add_subplot(2, 1, 2)
        ax1.set_title("{}: part {} - elapsed time histgram".format(session_name, i))
        ax1.set_xlabel("turn-around time (us)")
        ax1.set_ylabel("the number of samples")
        ax1.set_xlim([0, max_value])
        ax1.hist(turnaround_times[i], bins=bins)

        plt.savefig("{}.part{}_histgram.pdf".format(session_name, i))

if __name__ == "__main__":
    session_name = parse_file(sys.argv[1])
    visualize(session_name)

