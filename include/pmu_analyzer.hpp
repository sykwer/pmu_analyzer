#pragma once

namespace pmu_analyzer {
  void PMU_INIT();
  void PMU_TRACE_START(int trace_id);
  void PMU_TRACE_END(int trace_id);
  void PMU_CLOSE();
}
