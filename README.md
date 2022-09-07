# pmu_analyzer
You can insert tracepoints that enable to measure any performance counter at any section in your C++ application.
Additionally, various functionalities are provided to aid your performance analysis.

## Features
- Provide a library that enables to repeatedly measure an arbitrary performance counter at an arbitrary section in a C++ application. The measured performance counter can be specified in string data which shown in `perf list` command.
- Provide a supplementary library that enables to repeatedly measure elapsed time at an arbitrary section in a C++ application.
- Provide a default parser (visualizer) for the above two features.

## Environment
WIP

## Quick start
We need `libpfm` to convert event names (string data) into code that `perf_event_open(2)` can understand.
```
$ git clone https://git.code.sf.net/p/perfmon2/libpfm4 perfmon2-libpfm4
$ cd perfmon2-libpfm4
$ make
$ make install
```

You can get a shared library `libpmuanalyzer.so` under the local `lib/` directory by running `make`.
```
$ make
```
The `pmu_analizer` API can be utilized in your application by linking this shared library and add `pmu_analyzer/include` to the include directories.

The API for counting performance counters in a specific section is composed of four functions: `PMU_INIT`, `PMU_TRACE_START`, `PMU_TRACE_END` and `PMU_CLOSE`.
`PMU_INIT` and `PMU_ÇLOSE` should be called once in the target application, and a measurement section should be embraced with `PMU_TRACE_START` and `PMU_TRACE_END`.
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
```
PMU_ANALYZER_CONFIG_FILE=/path/to/config.yaml ./app
```

In the config file, the target event names, the maximum number of log entries, and the path for log output should be specified.
```yaml
events:
  - instructions
  - bus-cycles
max_logs_num:
  pmu: 400
log_path: /path/to/log_directory
```

After `PMU_CLOSE` called, a log file named `${log_path}/pmu_log_${pid}` is created, whose format is shown below.
```
<trace_id> <event0> <event1> ...
```

You can prepare a original log parser, or utilize the given parser.
```
PMU_ANALYZER_CONFIG_FILE=/path/to/config.yaml python3 scripts/pmu_original_parser.py `${log_path}/pmu_log_${pid}`
```



## API
### libpmuanalyzer: Measure Performance Counter
As mentioned above, we have four functions to repeatedly measure an arbitrary performance counter at an arbitrary section in a C++ application.

- `void pmu_analyzer::PMU_INIT()`: Create file descripters that observe performance counters specified in the yaml config file. This init function emits `perf_event_open(2)` syscall.
- `void pmu_analyzer::PMU_TRACE_START(int trace_id)`: Start counting the performance counters specified in the yaml config file. This start function emits ioctl syscall that operates the file descripter created in the `PMU_INIT` function. You are supposed to pass the same `trace_id` for `PMU_TRACE_START` and `PMU_TRACE_END` in one measurement.
- `void pmu_analyzer::PMU_TRACE_END(int trace_id)`: End counting the performance counters. This end function emits ioctl syscall that operates the file descripter created in `PMU_INIT` the funciton. The results are written into a log file later in the `PMU_CLOSE` function. You are supposed to pass the same `trace_id` for `PMU_TRACE_START` and `PMU_TRACE_END` in one measurement.
- `void pmu_analyzer::PMU_CLOSE()`: Close the file descripters created in the `PMU_INIT` function and write log data into the file located at `${log_path}/pmu_log_${pid}`.

All these four kinds of functions must be called in the same thread to satisfy the constraint of the `perf_event_open(2)`.

### libpmuanalyzer: Measure Elapsed Time
WIP
