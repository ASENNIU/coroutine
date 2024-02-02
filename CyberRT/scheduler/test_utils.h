//
// Created by dell on 2024/1/29.
//

#ifndef COROUTINE_CYBERRT_SCHEDULER_TEST_UTILS_H_
#define COROUTINE_CYBERRT_SCHEDULER_TEST_UTILS_H_

#include "cyber/common/global_data.h"
#include "cyber/cyber.h"
#include "cyber/scheduler/processor.h"
#include "cyber/scheduler/scheduler.h"
#include "cyber/scheduler/scheduler_factory.h"
#include "cyber/task/task.h"

#include <iostream>
#include <cstdlib>
#include <random>
#include <memory>
#include <functional>
#include <thread>
#include <cstdio>


namespace apollo {
namespace cyber{
namespace scheduler {

#define CPU_LOAD 100000
#define IO_LOAD 50000

#define CR_execute 0
#define CR_yield 1

/***
 * 另一种写法：
 * const char* cr_runtime_state[] = {
 *   [CR_execute] = "execute",
 *   [CR_yield] = "yield"
 * };
 * 但这里会报错，编译器不支持非平凡（non-trivial）的指定初始化器（designated initializers）
 * sorry, unimplemented: non-trivial designated initializers not supported
 */

const char* cr_runtime_state[] = {
    "execute",                  // [CR_execute]
    "yield"                     // [CR_yield]
};

using apollo::cyber::croutine::RoutineState;

void print_info(const std::string& group, int processor_id, int id, const char* state) {
  if (processor_id != -1) {
    printf("Processor %d: cpu_task croutine %d %s...\n", processor_id, id, state);
  } else {
    printf("Group %s: cpu_task croutine %d %s...\n", group.c_str(), id, state);
  }
}

/***
 * @brief 模拟CPU密集型任务（一个CPU Task代表一次独占CPU的计算任务，while true循环模拟等待事件到来）
 * @param CPU_LOAD: 表示任务负载，一直占有CPU
 */
void cpu_task(const std::string& group, int processor_id, int id) {
  while (true) {
    print_info(group, processor_id, id, cr_runtime_state[CR_execute]);

    int i = 0;
    while (i < CPU_LOAD) ++i;

    // 让出CPU执行权
    CRoutine::GetCurrentRoutine()->SetUpdateFlag();
    CRoutine::Yield(RoutineState::IO_WAIT);
  }

}

/***
 * @brief 模拟IO密集型任务，一个IO任务模式为: 计算 -> 等待IO -> 计算
 * @param IO_LOAD: 表示IO密集型任务中的CPU负载
 */
void io_task(const std::string& group, int processor_id, int id) {
  while (true) {
    print_info(group, processor_id, id, cr_runtime_state[CR_execute]);

    int i = 0;
    while (i < IO_LOAD) ++i;

    // 等待IO，让出CPU
    CRoutine::GetCurrentRoutine()->SetUpdateFlag();
    CRoutine::Yield(RoutineState::IO_WAIT);

    i = 0;
    while (i <  IO_LOAD) ++i;
    print_info(group, processor_id, id, cr_runtime_state[CR_yield]);

    // 让出CPU，等待下一个事件
    CRoutine::GetCurrentRoutine()->SetUpdateFlag();
    CRoutine::Yield(RoutineState::IO_WAIT);
  }

}

/**
 * @brief 支持的数据分布类型
 * @details
 *   UNIFORM: 均匀分布
 *   NORMAL: 正态分布
 */
enum DistributionType{
  UNIFORM,
  NORMAL,
};

/***
 * @brief 从某个数据分布中采样，支持均匀分布和正态分布
 * @param type 数据分布类型
 * @param a 采样左区间
 * @param b 采样右区间
 * @return 采样值（默认值为10.0）
 */
double distribution_sampler(enum DistributionType type, double a, double b) {
  static std::default_random_engine generator;
  double res;
  switch (type) {
    case UNIFORM: {
      res = static_cast<float>(std::rand()) / RAND_MAX;
      res = a + res * (b -a);
      break;
    }
    case NORMAL: {
      double mean = (a + b) / 2;
      double stddev = (b - a) / 4;
      std::normal_distribution<double> distribution(mean, stddev);
//      std::default_random_engine generator;
      res = distribution(generator);
      res = std::max(a, std::min(b - 1, res)); // 限制在 [a, b] 区间内
      break;
    }

    default:
      res = 10.0;
  }
  return res;
};


/***
 * @brief: 协程的生成器，这里只是封装task，并不设置其余属性
 * @param: ratio：CPU密集型任务的占比
 */
std::shared_ptr<CRoutine>  get_croutine(float ratio, const std::string& group, int processor_id, int id) {
  bool flag = (static_cast<float>(std::rand()) / RAND_MAX) < ratio;
  if (flag) {
    auto func = [=]() {
      cpu_task(group, processor_id, id);
    };
    return std::make_shared<CRoutine>(func);
  } else {
    auto func = [=]() {
      io_task(group, processor_id, id);
    };
    return std::make_shared<CRoutine>(func);
  }
};

/***
 * @brief: 代表运行时的虚基类，将上层业务和scheduler绑定
 */
class BaseRunner {

 public:
  BaseRunner(int croutine_num, float ratio) :
    croutine_num_(croutine_num), ratio_(ratio) {}
  virtual ~BaseRunner() = default;
  virtual void run() = 0;
 protected:
  int croutine_num_;                          // 创建协程的数量
  float ratio_;                               // CPU密集型任务的比例
};

/***
 * @brief：运行时的实现类
 * @tparam T：scheduler的类型，SchedulerClassic或 SchedulerChoreography
 */
class Runner final : public BaseRunner {

 public:
  /***
   * @brief: Runner类的构造函数
   * @param sched：调度器类指针
   * @param process_group：classic模式下所属资源组名字
   * @param croutine_num：创建协程数量
   * @param ratio：CPU型任务所占比率
   * @param p_classic：classic模式的task_name映射函数
   * @param p_chography：choreography模式下的task_name映射函数
   * @param processor_id：choreography模式下，使用处理器的id
   * @param is_classic：是否是classic模式
   * @param type
   * @param a
   * @param b
   * @note: 成员变量的初始化顺序应当与类的声明顺序一致
   */
  Runner(Scheduler* sched, const std::string& process_group,
         int croutine_num = 1000, float ratio = 0.5,
         std::string (*p_classic)(const std::string& , float ) = nullptr,
         std::string (*p_chography)(int, float ) = nullptr,
         int processor_id = -1, bool is_classic = true,
         DistributionType type = NORMAL,
         double a = 0, double b = 0) :
  BaseRunner(croutine_num, ratio), sched_(sched),
  process_group_(process_group), processor_id_(processor_id),
  classic_mapping_ptr_(p_classic), chography_mapping_ptr_(p_chography),
  is_classic_(is_classic), type_(type), a_(a), b_(b) {}

  Runner(const Runner&) = delete;
  Runner(const Runner&&) = delete;

  Runner& operator=(const Runner&) = delete;
  Runner& operator=(const Runner&&) = delete;

  virtual ~Runner() = default;

  /***
   * @brief: 执行函数，根据需创建协程的协程和任务比例，创建协程由scheduler分发
   */
  void run() {

    int croutine_dispatch_num = 0;
    for (int i = 0; i < this->croutine_num_; ++i) {
      std::shared_ptr<CRoutine>  c_ptr = get_croutine(this->ratio_, process_group_, processor_id_, i);

      float prob = distribution_sampler(type_, a_, b_);
      prob = i < 20 ? i : prob;
      std::string task_name;
      if (is_classic_) {
        task_name = classic_mapping_ptr_(process_group_, prob);
      } else {
        task_name = chography_mapping_ptr_(processor_id_, prob);
      }

      auto task_id = GlobalData::RegisterTaskName(task_name);
      c_ptr->set_id(task_id);
      c_ptr->set_name(task_name);

      if (!is_classic_) {
        c_ptr->set_priority(static_cast<int>(prob));
        c_ptr->set_processor_id(processor_id_);
      }


      if (sched_->DispatchTask(c_ptr))
        ++croutine_dispatch_num;

      int delay = 0;
      while (delay < 5000) ++delay;
    }
    if (processor_id_ != -1) {
      printf("-------------------- Processor %d Finished DispatchTask, Num: %d ------------------\n",
             processor_id_, croutine_dispatch_num);
    } else {
      printf("-------------------- Group %s Finished DispatchTask, Num: %d ------------------\n",
             process_group_.c_str(), croutine_dispatch_num);
    }
  }

 private:

  Scheduler* sched_;                                                                // 调度器类指针
  std::string process_group_;                                                       // 所属资源组名字
  int processor_id_ = -1;                                                           // choreography模式下的processor_id
  std::string (*classic_mapping_ptr_)(const std::string&, float ) = nullptr;        // 将概率值映射到task名字的函数指针
  std::string (*chography_mapping_ptr_)(int, float ) = nullptr;                     // 将概率值映射到task名字的函数指针
  bool is_classic_ = true;                                                          // 是否是classic模式
  DistributionType type_;                                                           // 数据分布类型
  double a_ = 0;                                                                    // 采样左区间
  double b_ = 20;                                                                   // 采样右区间
};

}
}
}


#endif //COROUTINE_CYBERRT_SCHEDULER_TEST_UTILS_H_
