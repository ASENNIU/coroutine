//
// Created by dell on 2024/1/30.
//

#include "cyber/common/global_data.h"
#include "cyber/cyber.h"
#include "cyber/scheduler/scheduler.h"
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

using apollo::cyber::scheduler::DistributionType;
using apollo::cyber::scheduler::Runner;

/**
 * @brief：启动参数对应的类，见parse_argv
 */
struct execute_env {
  bool is_classic;
  struct runner_conf* conf_ptr;
  int conf_length = 1;
  int time_to_run = 60;
  const char* conf;
};


//const char* classic_conf_file = "test_classic_latency";
//const char* chography_conf_file = "";

/***
 * @brief: runner的配置信息，一个Runner类代表一个任务组
 * @note： 注意这里的顺序与Runner类成员变量的声明顺序不一样，但是切记C++成员变量的初始化顺序应该与声明顺序一致
 */
struct runner_conf {
  int croutine_num;                                                                   // 创建协程的数量
  float ratio;                                                                        // CPU密集型任务的比例
  std::string process_group;                                                          // 所属资源组名字
  DistributionType type;                                                              // 数据分布类型
  double a = 0;                                                                       // 采样左区间
  double b = 20;                                                                      // 采样右区间
  std::string (*classic_mapping_ptr)(const std::string&, float ) = nullptr;           // 将概率值映射到task名字的函数指针
  std::string (*chography_mapping_ptr)(int, float ) = nullptr;                        // 将概率值映射到task名字的函数指针
  int processor_id = -1;                                                              // choreography模式下的processor_id
  bool is_classic = true;                                                             // 是否是classic模式
};



/**
 * @brief: 解析启动参数
 * @param argv: 启动命令行参数
 * @details:
 *   argv[0]: 程序文件名
 *   argv[1]: scheduler配制文件
 *   argv[2]: 是否是classic模式, 0-1
 *   argv[3]: resources的数量
 *   argv[4]: 运行时长
 * @return
 */
bool parse_argv(char** argv, struct execute_env& env);

void show_info(struct execute_env& env);



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
std::string chography_mapping(int, float prob);


/***
 * @brief：所有资源组的配置信息
 */
struct runner_conf classic_conf[]{
    {20, 0.2, "group1", DistributionType::UNIFORM, 0, 20, classic_mapping, nullptr, -1, true},
    {20, 0.5, "group2", DistributionType::UNIFORM, 0, 20, classic_mapping, nullptr, -1, true},
    {20, 0.8, "group3", DistributionType::UNIFORM, 0, 20, classic_mapping, nullptr, -1, true}
};

struct runner_conf chography_conf[]{
    {20, 0.2, "", DistributionType::NORMAL, 0, 20, nullptr, chography_mapping, 0, false},
    {20, 0.4, "", DistributionType::NORMAL, 0, 20, nullptr, chography_mapping, 1, false},
    {20, 0.6, "", DistributionType::NORMAL, 0, 20, nullptr, chography_mapping, 2, false},
    {20, 0.8, "", DistributionType::NORMAL, 0, 20, nullptr, chography_mapping, 3, false},
    {20, 0.5, "", DistributionType::NORMAL, 0, 20, nullptr, chography_mapping, 4, false}

};

namespace apollo{
namespace cyber{
namespace scheduler{

void execute(struct execute_env& env) {
  Scheduler* sched;
  GlobalData::Instance()->SetProcessGroup(env.conf);
  if (env.is_classic) {
    sched = dynamic_cast<SchedulerClassic*>(scheduler::Instance());
    env.conf_ptr = classic_conf;
  } else {
    sched = dynamic_cast<SchedulerChoreography*>(scheduler::Instance());
    env.conf_ptr = chography_conf;
  }

  std::vector<std::shared_ptr<Runner>> runner_vec;

  for (int i = 0; i < env.conf_length; ++i) {
    runner_vec.emplace_back(std::make_shared<Runner>(sched,
                                                     env.conf_ptr[i].process_group,
                                                     env.conf_ptr[i].croutine_num,
                                                     env.conf_ptr[i].ratio,
                                                     env.conf_ptr[i].classic_mapping_ptr,
                                                     env.conf_ptr[i].chography_mapping_ptr,
                                                     env.conf_ptr[i].processor_id,
                                                     env.conf_ptr[i].is_classic,
                                                     env.conf_ptr[i].type,
                                                     env.conf_ptr[i].a,
                                                     env.conf_ptr[i].b));
  }

  std::vector<std::thread> thread_vec;
  for (int i = 0; i < env.conf_length; ++i) {
    thread_vec.emplace_back(&Runner::run, runner_vec[i].get());
//    std::this_thread::sleep_for(std::chrono::seconds(2));
  }

  for (int i = 0; i < env.conf_length; ++i) {
    if (thread_vec[i].joinable()) {
      thread_vec[i].join();
    }
  }

  std::this_thread::sleep_for(std::chrono::seconds(env.time_to_run));

  sched->Shutdown();
}
}
}
}

int main(int argc, char** argv) {
  if (argc < 5) {
    std::cout << "Usage: argc must equal to 5." << std::endl;
    exit(0);
  }
  struct execute_env env;
  if (!parse_argv(argv, env)) {
    std::cout << "Usage: argv[2] must be greater than zero." << std::endl;
    exit(0);
  }

  show_info(env);

  apollo::cyber::scheduler::execute(env);

  return 0;

}

bool parse_argv(char** argv, struct execute_env& env) {
  env.conf = argv[1];
  env.is_classic = atoi(argv[2]);
  env.conf_length = atoi(argv[3]);
  env.time_to_run = atoi(argv[4]);
  return env.conf_length > 0;
}

void show_info(struct execute_env& env) {
  printf("------------ EXECUTE ENVIRONMENT --------------\n");
  printf("-----------> Scheduler Config file: %s\n", env.conf);
  printf("-----------> Is Classic mode: %d\n", env.is_classic);
  printf("-----------> Resources length: %d\n", env.conf_length);
  printf("-----------> Time to run(seconds): %d\n", env.time_to_run);
}

std::string classic_mapping(const std::string& group, float prob) {
  std::string task_name = group + "_task_" + std::to_string(static_cast<int>(prob));
  return task_name;
}

std::string chography_mapping(int processor_id, float prob) {
  std::string task_name = "processor_" + std::to_string(processor_id) + "_task_" + std::to_string(static_cast<int>(prob));
  return task_name;
}