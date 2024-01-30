//
// Created by dell on 2024/1/30.
//

#include "cyber/common/global_data.h"
#include "cyber/cyber.h"
#include "cyber/scheduler/policy/classic_context.h"
#include "cyber/scheduler/processor.h"
#include "cyber/scheduler/scheduler_factory.h"
#include "cyber/scheduler/policy/scheduler_choreography.h"
#include "cyber/scheduler/policy/scheduler_classic.h"
#include "cyber/task/task.h"


#include "cyber/scheduler/test_utils.h"

#include <iostream>
#include <vector>
#include <memory>
#include <thread>

bool is_classic;
struct runner_conf* conf_ptr;
int conf_length;

const char* classic_conf_file = "";
const char* chography_conf_file = "";

/***
 * @brief: runner的配置信息，一个Runner类代表一个任务组
 */
struct runner_conf {
  int croutine_num;                                                       // 创建协程的数量
  float ratio;                                                            // CPU密集型任务的比例
  std::string process_group;                                               // 所属资源组名字
  distribution_type type;                                                  // 数据分布类型
  double a = 0;                                                            // 采样左区间
  double b = 20;                                                           // 采样右区间
  std::string (*classic_mapping_ptr)(std::string, float ) = nullptr;       // 将概率值映射到task名字的函数指针
  std::string (*chography_mapping_ptr)(float ) = nullptr;                  // 将概率值映射到task名字的函数指针
  int processor_id = -1;                                                   // choreography模式下的processor_id
  bool is_classic = true;                                                  // 是否是classic模式
};



/**
 * @brief: 解析启动参数
 * @param argv: 启动命令行参数
 * @details:
 *   argv[0]: 程序文件名
 *   argv[1]: 是否是classic模式, 0-1
 *   argv[2]: runner_conf的长度
 * @return
 */
bool parse_argv(char** argv);

/***
 * @brief: classic模式的映射函数，将采样所得概率值映射到task name（协程的配置信息）
 * @param group：资源组名字
 * @param prob：采样所得概率值（实际上代表的是优先级概率）
 * @return
 */
std::string classic_mapping(const std::string& group, float prob);

/***
 * @brief: choreography模式的映射函数，将采样所得概率值映射到task name（协程的配置信息）
 * @param prob：采样所得概率值（实际上代表的是优先级概率）
 * @return
 */
std::string chography_mapping(float prob);



struct runner_conf classic_conf[]{
    {1000, 0.2, "", NORMAL, 0, 20, classic_mapping, nullptr, -1, true},
    {1000, 0.5, "", NORMAL, 0, 20, classic_mapping, nullptr, -1, true},
    {1000, 0.8, "", NORMAL, 0, 20, classic_mapping, nullptr, -1, true}
};

struct runner_conf chography_conf[]{
    {1000, 0.2, "", NORMAL, 0, 20, classic_mapping, nullptr, -1, true},
    {1000, 0.5, "", NORMAL, 0, 20, classic_mapping, nullptr, -1, true},
    {1000, 0.8, "", NORMAL, 0, 20, classic_mapping, nullptr, -1, true}
};



int main(int argc, char** argv) {
  if (!parse_argv(argv)) {
    std::cerr << "Usage: argv[2] must be greater than zero." << std::endl;
    exit(0);
  }
  Scheduler* sched;

  if (is_classic) {
    GlobalData::Instance()->SetProcessGroup(classic_conf_file);
    sched = dynamic_cast<SchedulerClassic*>(scheduler::Instance());
    conf_ptr = classic_conf;
  } else {
    GlobalData::Instance()->SetProcessGroup(chography_conf_file);
    sched = dynamic_cast<SchedulerClassic*>(scheduler::Instance());
    conf_ptr = chography_conf;
  }

  std::vector<std::shared_ptr<Runner>> runner_vec;
  for (unsigned i = 0; i < conf_length; ++i) {
    runner_vec.emplace(std::make_shared<Runner>(sched, conf_ptr[i].process_group,
                                                conf_ptr[i].croutine_num, conf_ptr[i].ratio,
                                                conf_ptr[i].classic_mapping_ptr,
                                                conf_ptr[i].chography_mapping_ptr,
                                                conf_ptr[i].processor_id,
                                                conf_ptr[i].is_classic,
                                                conf_ptr[i].type,
                                                conf_ptr[i].a, conf_ptr[i].b));
  }

  std::vector<std::thread> thread_vec;
  for (unsigned i = 0; i < conf_length; ++i) {
    thread_vec.emplace(&Runner::run, runner_vec[i].get());
  }

  for (unsigned i = 0; i < conf_length; ++i) {
    if (thread_vec[i].joinable()) {
      thread_vec[i].join();
    }
  }

  sched->Shutdown();

  return 0;

}

bool parse_argv(char** argv) {
  is_classic = atoi(argv[1]);
  conf_length = atoi(argv[2]);
  return conf_length > 0;
}

std::string classic_mapping(const std::string& group, float prob) {
  return group + "_task_" + std::to_string(static_cast<int>(prob));
}

std::string chography_mapping(float prob) {
  return "task_" + std::to_string(static_cast<int>(prob));
}