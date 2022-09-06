#include <chrono>
#include <unordered_map>
#include <atomic>
#include <string>
#include <vector>
#include <mutex>
#include <yaml-cpp/yaml.h>
#include <sys/time.h>
#include <unistd.h>

#include <pmu_analyzer.hpp>

namespace pmu_analyzer {

struct LogEntry {
  int session_idx;
  int part_idx;
  int loop_idx;
  unsigned long long timestamp;
  long long data;
};

static std::mutex mtx;

// Guarded - from here
static std::vector<std::string> session_names;
static std::unordered_map<std::string, int> session2idx;

static std::vector<std::vector<LogEntry>> logs;
static std::vector<int> logs_num;
static std::vector<int> loop_idxs;
// Guarded - to here

static std::string log_path;
static int max_logs_num;


void ELAPSED_TIME_INIT(std::string &session_name) {
  static bool first(true); // Guarded
  static std::atomic<int> session_idx(0);

  int local_session_idx = session_idx++;

  {
    std::lock_guard<std::mutex> lock(mtx);

    if (first) {
      first = false;
      YAML::Node config = YAML::LoadFile(std::getenv("PMU_ANALYZER_CONFIG_FILE"));
      log_path = config["log_path"].as<std::string>();
      max_logs_num = config["max_logs_num"]["elapsed_time"].as<int>();
    }

    session2idx[session_name] = local_session_idx;
    session_names.push_back(session_name);

    logs.push_back(std::vector<LogEntry>());
    logs[logs.size() - 1].resize(max_logs_num);
    for (int i = 0; i < max_logs_num; i++) { // memory touch
      volatile LogEntry tmp = logs[logs.size() - 1][i];
    }

    logs_num.push_back(0);
    loop_idxs.push_back(0);
  }

}


void ELAPSED_TIME_TIMESTAMP(std::string &session_name, int part_idx, bool new_loop, long long data) {
  int local_session_idx = session2idx[session_name];
  if (new_loop) loop_idxs[local_session_idx]++;

  LogEntry &e = logs[local_session_idx][logs_num[local_session_idx]];
  e.session_idx = local_session_idx;
  e.part_idx = part_idx;
  e.loop_idx = loop_idxs[local_session_idx];
  e.data = data;

  struct timeval tv;
  gettimeofday(&tv, NULL);
  e.timestamp = 1000 * 1000 * tv.tv_sec + tv.tv_usec;

  logs_num[local_session_idx]++;
}


void ELAPSED_TIME_CLOSE(std::string &session_name) {
  int local_session_idx = session2idx[session_name];

  char logfile_name[100];
  sprintf(logfile_name, "%s/elapsed_time_log_%d_%d", log_path.c_str(), getpid(), local_session_idx);

  FILE *f = fopen(logfile_name, "a+");

  for (int i = 0; i < logs_num[local_session_idx]; i++) {
    LogEntry &e = logs[local_session_idx][i];
    fprintf(f, "%s %d %d %lld %lld\n", session_name.c_str(), e.part_idx, e.loop_idx, e.timestamp, e.data);
  }

  fclose(f);
}

} // namespace pmu_analyzer
