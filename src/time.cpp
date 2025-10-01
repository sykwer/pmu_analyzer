#include <chrono>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstdio>
#include <yaml-cpp/yaml.h>
#include <sys/time.h>
#include <unistd.h>

#include <pmu_analyzer.hpp>

namespace pmu_analyzer {

struct ElapsedLogEntry {
  int session_idx;
  int part_idx;
  int loop_idx;
  unsigned long long timestamp;
  long long data;
};

struct VariableLogEntry {
  int session_idx;
  int loop_idx;
  std::string variable_names;
  std::vector<double> data;
};

static std::mutex mtx;

// Guarded - from here
static std::vector<std::string> session_names;
static std::unordered_map<std::string, int> session2idx;

static std::vector<std::vector<ElapsedLogEntry>> elapsed_logs;
static std::vector<std::vector<VariableLogEntry>> var_logs;
static std::vector<int> elapsed_logs_num;
static std::vector<int> var_logs_num;
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

    elapsed_logs.push_back(std::vector<ElapsedLogEntry>());
    elapsed_logs[elapsed_logs.size() - 1].resize(max_logs_num);
    var_logs.push_back(std::vector<VariableLogEntry>());
    var_logs[var_logs.size() - 1].resize(max_logs_num);
    for (int i = 0; i < max_logs_num; i++) { // memory touch
      volatile ElapsedLogEntry tmp_elapsed = elapsed_logs[elapsed_logs.size() - 1][i];
      volatile VariableLogEntry tmp_var = var_logs[var_logs.size() - 1][i];
    }

    elapsed_logs_num.push_back(0);
    var_logs_num.push_back(0);
    loop_idxs.push_back(0);
  }

}


void ELAPSED_TIME_TIMESTAMP(std::string &session_name, int part_idx, bool new_loop, long long data) {
  int local_session_idx = session2idx[session_name];
  if (new_loop) loop_idxs[local_session_idx]++;

  ElapsedLogEntry &e = elapsed_logs[local_session_idx][elapsed_logs_num[local_session_idx]];
  e.session_idx = local_session_idx;
  e.part_idx = part_idx;
  e.loop_idx = loop_idxs[local_session_idx];
  e.data = data;

  struct timeval tv;
  gettimeofday(&tv, NULL);
  e.timestamp = 1000 * 1000 * tv.tv_sec + tv.tv_usec;

  elapsed_logs_num[local_session_idx]++;
}


void OUTPUT_VARIABLE(std::string &session_name, std::string &variable_name, std::vector<double> &data) {
  int local_session_idx = session2idx[session_name];
  
  VariableLogEntry &v = var_logs[local_session_idx][var_logs_num[local_session_idx]];
  v.session_idx = local_session_idx;
  v.loop_idx = loop_idxs[local_session_idx];
  v.variable_names = variable_name;
  v.data = data;
  
  var_logs_num[local_session_idx]++;
}


void ELAPSED_TIME_CLOSE(std::string &session_name) {
  int local_session_idx = session2idx[session_name];

  // Output elapsed time logs
  char logfile_name[100];
  sprintf(logfile_name, "%s/elapsed_time_log_%d_%d", log_path.c_str(), getpid(), local_session_idx);

  FILE *f = fopen(logfile_name, "a+");

  for (int i = 0; i < elapsed_logs_num[local_session_idx]; i++) {
    ElapsedLogEntry &e = elapsed_logs[local_session_idx][i];
    fprintf(f, "%s %d %d %lld %lld\n", session_name.c_str(), e.part_idx, e.loop_idx, e.timestamp, e.data);
  }

  fclose(f);

  // Output variable logs
  char var_logfile_name[100];
  sprintf(var_logfile_name, "%s/variable_log_%d_%d", log_path.c_str(), getpid(), local_session_idx);

  FILE *vf = fopen(var_logfile_name, "a+");

  for (int i = 0; i < var_logs_num[local_session_idx]; i++) {
    VariableLogEntry &v = var_logs[local_session_idx][i];
    fprintf(vf, "%s %d %s ", session_name.c_str(), v.loop_idx, v.variable_names.c_str());
    for (const double &value : v.data) {
      fprintf(vf, "%lf ", value);
    }
    fprintf(vf, "\n");
  }

  fclose(vf);
}

} // namespace pmu_analyzer
