#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>

#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

#include <pmu_analyzer.hpp>

namespace pmu_analyzer {

static std::vector<std::string> monitored_event_strings;

struct log_entry {
  int trace_id;
  std::vector<uint64_t> values;
};

struct read_format {
  uint64_t nr;            /* The number of events */
  uint64_t  time_enabled; /* if PERF_FORMAT_TOTAL_TIME_ENABLED */
  uint64_t time_running;  /* if PERF_FORMAT_TOTAL_TIME_RUNNING */
  struct {
    uint64_t value;       /* The value of the event */
    uint64_t id;          /* if PERF_FORMAT_ID */
  } values[];
};

struct event_info {
  std::string event_string;
  char is_leader;
  uint32_t event_type;
  uint64_t event_config;
  struct perf_event_attr event_attr;
  int fd;
  uint64_t id;
  uint64_t measured_value;
};

static std::vector<log_entry> logs;
static int logs_num = 0;
static int max_logs_num;

static std::vector<event_info> event_infos;
static char buffer[1000];
static int global_leader_fd;


static void encode_event_string(event_info *event_info) {
  pfm_perf_encode_arg_t arg;
 	struct perf_event_attr attr;
 	char *fstr = NULL; // Get event string in [pmu::][event_name][:unit_mask][:modifier|:modifier=val]

 	memset(&arg, 0, sizeof(arg));
 	arg.size = sizeof(arg);
 	arg.attr = &attr;
 	arg.fstr = &fstr;

 	int ret = pfm_get_os_event_encoding(event_info->event_string.c_str(), PFM_PLM0, PFM_OS_PERF_EVENT_EXT, &arg);
 	if (ret != PFM_SUCCESS) {
 		perror("pfm_get_os_event_encoding error");
 		exit(EXIT_FAILURE);
 	}

  event_info->event_type = attr.type;
  event_info->event_config = attr.config;

  // The returned `fstr` value of "l1d_pend_miss.pending" is
 	// skl::L1D_PEND_MISS:PENDING:e=0:i=0:c=0:t=0:intx=0:intxcp=0:u=0:k=1:period=34:freq=34:excl=0:mg=0:mh=1
 	free(fstr);
}


static void setup_perf_event_attr_grouped(event_info *event_info) {
  struct perf_event_attr *attr = &event_info->event_attr;

  memset(attr, 0, sizeof(*attr));
  attr->type = event_info->event_type;
  attr->size = sizeof(*attr);
  attr->config = event_info->event_config;
  attr->disabled = 1;
  attr->exclude_kernel = 1;
  attr->exclude_hv = 1;
  //attr->pinned = event_info->is_leader; // experimental
  attr->read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING |
                      PERF_FORMAT_ID | PERF_FORMAT_GROUP;
}


static int prepare_event_infos() {
  int leader_fd = -1;
  // First element has to be the leader for this impelentation
  event_infos[0].is_leader = 1;

  // for (int i = 0; i < MONITORED_EVENTS_NUM; i++) {
  for (int i = 0; i < monitored_event_strings.size(); i++) {
    event_infos[i].event_string = monitored_event_strings[i];
    encode_event_string(&event_infos[i]);
    setup_perf_event_attr_grouped(&event_infos[i]);

    int fd = syscall(__NR_perf_event_open, &event_infos[i].event_attr,
        0, -1/*cpu*/ , event_infos[i].is_leader ? -1 : leader_fd, 0/*flag*/);

    if (fd == -1) {
      perror("perf_event_open error");
      fprintf(stderr, "is_leader=%d", event_infos[i].is_leader);
      exit(EXIT_FAILURE);
    }

    if (event_infos[i].is_leader) leader_fd = fd;

    event_infos[i].fd = fd;

    ioctl(event_infos[i].fd, PERF_EVENT_IOC_ID, &event_infos[i].id);
  }

  return leader_fd;
}


void PMU_INIT() {
  if (pfm_initialize() != PFM_SUCCESS) {
    perror("pfm_initialize error");
    exit(EXIT_FAILURE);
  }

  YAML::Node config = YAML::LoadFile(std::getenv("PMU_ANALYZER_CONFIG_FILE"));
  auto events = config["events"];
  max_logs_num = config["max_logs_num"].as<int>();

  for (YAML::const_iterator it = events.begin(); it != events.end(); it++) {
    std::string event_name = it->as<std::string>();
    monitored_event_strings.push_back(event_name);
  }

  int events_num = monitored_event_strings.size();
  event_infos.resize(events_num);
  logs.resize(max_logs_num);
  for (log_entry &e : logs) {
    e.values.resize(events_num);
  }

  global_leader_fd = prepare_event_infos();
  if (global_leader_fd < 0) {
    fprintf(stderr, "prepare_event_info() returns invalid leader_fd\n");
    exit(EXIT_FAILURE);
  }
}

void PMU_TRACE_START(int trace_id) {
  (void) trace_id;
  int ret;

  ret = ioctl(global_leader_fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
  if (ret < 0) {
    perror("ioctl perf_event_ioc_reset error");
    exit(EXIT_FAILURE);
  }

  ret = ioctl(global_leader_fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
  if (ret < 0) {
    perror("ioctl perf_event_ioc_enable error");
    exit(EXIT_FAILURE);
  }
}


void PMU_TRACE_END(int trace_id) {
  int ret = ioctl(global_leader_fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
  if (ret < 0) {
    perror("ioctl perf_event_ioc_disable error");
    exit(EXIT_FAILURE);
  }

  struct read_format *rf = (struct read_format*) buffer;
  int sz = read(global_leader_fd, buffer, sizeof(buffer));

  if (sz < 0) {
    perror("read error");
    exit(EXIT_FAILURE);
  }

  if (rf->nr != monitored_event_strings.size()) {
    fprintf(stderr, "nr field is invalid value: %ld\n", rf->nr);
    exit(EXIT_FAILURE);
  }

  log_entry &e = logs[logs_num];
  e.trace_id = trace_id;
  int values_num_read = 0;

  for (size_t i = 0; i < rf->nr; i++) {
    for (size_t j = 0; j < monitored_event_strings.size(); j++) {
      if (rf->values[i].id == event_infos[j].id) {
        e.values[j] = rf->values[i].value;
        values_num_read++;
      }
    }
  }

  if (values_num_read != monitored_event_strings.size()) {
    fprintf(stderr, "values_num_read is invalid: %d\n", values_num_read);
    exit(EXIT_FAILURE);
  }

  logs_num++;
}

void PMU_CLOSE() {
  for (int i = 0; i < monitored_event_strings.size(); i++) {
    close(event_infos[i].fd);
  }

  char logfile_name[100];
  sprintf(logfile_name, "/home/sykwer/work/autoware_tutorial/log/pmu_log_%d", getpid());

  FILE *f = fopen(logfile_name, "a+");
  if (f == NULL) {
    perror("fopen error");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < logs_num; i++) {
    fprintf(f, "%d ", logs[i].trace_id);
    for (int j = 0; j < monitored_event_strings.size(); j++) {
      fprintf(f, "%ld ", logs[i].values[j]);
    }
    fprintf(f, "\n");
  }

  fclose(f);
}

} // namespace t4trace
