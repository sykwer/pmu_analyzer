#pragma once

#include <string>
#include <vector>

namespace pmu_analyzer {
  void PMU_INIT();
  void PMU_TRACE_START(int trace_id);
  void PMU_TRACE_END(int trace_id);
  void PMU_CLOSE();

  void ELAPSED_TIME_INIT(std::string &session_name);
  void ELAPSED_TIME_TIMESTAMP(std::string &session_name, int part_idx, bool new_loop, long long data);
  void ELAPSED_TIME_CLOSE(std::string &session_name);
  void OUTPUT_VARIABLE(std::string &session_name, std::string &variable_name, std::vector<double> &data);
}
