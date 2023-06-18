# pmu_analyzer
This tool allows you to insert tracepoints to measure any performance counter at any section in your C++ application.
Additionally, various functionalities are provided to aid your performance analysis.

## Features
- Provides a library for repeatedly measuring an arbitrary performance counter at any section in a C++ application. The measured performance counter can be specified using string data from the `perf list` command.
- Offers a supplementary library for repeatedly measuring elapsed time at any section in a C++ application.
- Includes a default parser (visualizer) for the above two features.

## Environment
This tool only works on Linux and depends on the `perf_event_open(2)` syscall and `libpfm`.
Supported processors are listed on the [perfmon2 documentation page](https://perfmon2.sourceforge.net/hw.html).


## How to use
### Measure performance counter
The API for counting performance counters in a specific section consists of four functions: `PMU_INIT`, `PMU_TRACE_START`, `PMU_TRACE_END`, and `PMU_CLOSE`.
Call `PMU_INIT` and `PMU_CLOSE` once in the target application, and enclose a measurement section with `PMU_TRACE_START` and `PMU_TRACE_END`.

```cpp
#include <pmu_analyzer.hpp>

void compute() {
  ...
}

int main() {
  pmu_analyzer::PMU_INIT();
  int trace_id = 0;

  while (running()) {
    pmu_analyzer::PMU_TRACE_START(trace_id);

    compute();

    pmu_analyzer::PMU_TRACE_END(trace_id);
    trace_id++;
  }

  pmu_analyzer::PMU_CLOSE();
}
```

After building the target application with `libpmuanalyzer` linked, you can measure the performance counters by executing it with this tool's config file specified in the `PMU_ANALYZER_CONFIG_FILE` environment variable.

```shell
PMU_ANALYZER_CONFIG_FILE=/path/to/config.yaml ./app
```

In the config file, specify the target event names, the maximum number of log entries, and the path for log output.

```yaml
events:
  - instructions
  - bus-cycles
  - LLC-store-misses
  - LLC-load-misses
  - minor-faults
max_logs_num:
  pmu: 400
log_path: /path/to/log_directory
```

After `PMU_CLOSE` is called, a log file named `${log_path}/pmu_log_${pid}` is created, with the following format:

```text
<trace_id> <event0> <event1> ...
```

You can create your own log parser or use the provided one.

```shell
PMU_ANALYZER_CONFIG_FILE=/path/to/config.yaml python3 scripts/pmu_parser.py `${log_path}/pmu_log_${pid}`
```

### Measure turn-around time
The API for turn-around time in specific sections consists of three functions: `ELAPSED_TIME_INIT`, `ELAPSED_TIME_TIMESTAMP`, and `ELAPSED_TIME_CLOSE`.
Call `ELAPSED_TIME_INIT` and `ELAPSED_TIME_CLOSE` once per session, and enclose a measurement section with `ELAPSED_TIME_TIMESTAMP`.
It is recommended to perform the measurement in an isolated environment to minimize the effects of OS scheduling and interrupts, as explained in the “Prepare separated cores” section of the [Performance analysis - Autoware Documentation](https://autowarefoundation.github.io/autoware-documentation/main/how-to-guides/performance_analysis/).

```cpp
#include <pmu_analyzer.hpp>

void f0() {
  ...
}

void f1() {
  ...
}

void f2() {
  ...
}

int main() {
  std::string session_name = "session0";
  pmu_analyzer::ELAPSED_TIME_INIT(session_name);

  while (running()) {
    pmu_analyzer::ELAPSED_TIME_TIMESTAMP(session_name, 0 /* part index */,
       true /* is first in this loop? */, 0 /* data (any data you like) */);

    f0();

    pmu_analyzer::ELAPSED_TIME_TIMESTAMP(session_name, 1, false, 0);

    f1();

    pmu_analyzer::ELAPSED_TIME_TIMESTAMP(session_name, 2, false, 0);

    f2();

    pmu_analyzer::ELAPSED_TIME_TIMESTAMP(session_name, 3, false, 0);
  }

  pmu_analyzer::ELAPSED_TIME_CLOSE(session_name);
}
```

After building the target application with `libpmuanalyzer` linked, you can measure the turn-around times by executing it with this tool's config file specified in the `PMU_ANALYZER_CONFIG_FILE` environment variable.

```shell
PMU_ANALYZER_CONFIG_FILE=/path/to/config.yaml ./app
```

In the config file, specify the maximum number of log entries and the path for log output.
For each call of `ELAPSED_TIME_TIMESTAMP`, the number of log entries increases by one, so set a value for `max_logs_num` that is equal to or greater than the number of loops times the number of `ELAPSED_TIME_TIMESTAMP` insertions.

```yaml
max_logs_num:
  elapsed_time: 1000
log_path: /path/to/logdir
```

After `ELAPSED_TIME_CLOSE` is called, a log file named `${log_path}/elapsed_time_log_${pid}_${local_session_idx}` is created, with the following format:

```text
<session_name> <part_idx> <loop_idx> <timestamp> <data>
```

You can create your own log parser or use the provided one.

```shell
PMU_ANALYZER_CONFIG_FILE=/path/to/config.yaml python3 scripts/elapsed_time_parser.py `${log_path}/elapsed_time_log_${pid}_${local_session_idx}`
```

### Relationship between performance counter and time-around time
TODO

## API
### Measure performance counter
As mentioned in the above section, there are four functions for repeatedly measuring performance counter at an arbitrary section in a C++ application.


#### PMU_INIT
Creates file descriptors that observe performance counters specified in the YAML config file.
This init function emits the `perf_event_open(2)` syscall.

```cpp
void pmu_analyzer::PMU_INIT();
```

#### PMU_TRACE_START
Start counting the performance counters specified in the YAML config file.
This start function emits ioctl syscall that operates the file descriptor created in the `PMU_INIT` function.
You are supposed to pass the same `trace_id` for `PMU_TRACE_START` and `PMU_TRACE_END` in one measurement.

```cpp
void pmu_analyzer::PMU_TRACE_START(int trace_id);
```

#### PMU_TRACE_END
Stops counting the performance counters.
This end function emits ioctl syscall that operates on the file descriptor created in the `PMU_INIT` function.
The results are written into a log file later in the `PMU_CLOSE` function.
You are supposed to pass the same `trace_id` for `PMU_TRACE_START` and `PMU_TRACE_END` in one measurement.

```cpp
void pmu_analyzer::PMU_TRACE_END(int trace_id);
```

#### PMU_CLOSE
Closes the file descriptors created in the `PMU_INIT` function and write log data into the file located at `${log_path}/pmu_log_${pid}`.

```cpp
void pmu_analyzer::PMU_CLOSE();
```

### Measure turn-around time
As mentioned in the above section, there are three functions for repeatedly measuring turn-around time at an arbitrary section in a C++ application.


#### ELAPSED_TIME_INIT
Starts a session to measure the turn-around time of the target.
There can be several sessions in one application at the same time.

```cpp
void ELAPSED_TIME_INIT(std::string &session_name);
```

#### ELAPSED_TIME_TIMESTAMP
This function is placed at the boundary of the measurement interval.
For the second argument `part_idx`, pass an incrementing number from 0, increasing by 1 each time.
Several measurement sections in one loop can be measured simultaneously.

For the third argument `new_loop`, pass false by default, and only pass true for the first `PMU_ELAPSED_TIME_TIMESTAMP` call in one loop.
This allows the tool to determine the start of one measurement cycle.

For the fourth argument data, any number can be passed.
It is assumed that the data size being processed or the number of loops in a for-loop during the processing interval is passed.
This data is written directly to the log file and is useful for visualizing the relationship between the measured elapsed time and other factors when visualizing the results.

```cpp
void ELAPSED_TIME_TIMESTAMP(std::string &session_name, int part_idx, bool new_loop, long long data);
```

#### ELAPSED_TIME_CLOSE
Ends a session to measure the turn-around time of the target and write log data into the file located at `${log_path}/elapsed_time_log_${pid}_${local_session_idx}`.

```cpp
void pmu_analyzer::ELAPSED_TIME_CLOSE(std::string &session_name);
```
