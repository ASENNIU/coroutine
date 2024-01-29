//
// Created by dell on 2024/1/29.
//

#ifndef COROUTINE_CYBERRT_SCHEDULER_TEST_UTILS_H_
#define COROUTINE_CYBERRT_SCHEDULER_TEST_UTILS_H_


#include "cyber/scheduler/policy/scheduler_classic.h"

#include "gtest/gtest.h"

#include "cyber/base/for_each.h"
#include "cyber/common/global_data.h"
#include "cyber/cyber.h"
#include "cyber/scheduler/policy/classic_context.h"
#include "cyber/scheduler/processor.h"
#include "cyber/scheduler/scheduler_factory.h"
#include "cyber/task/task.h"
#include <iostream>
#include <cstdlib>
#include <random>
#include <memory>


#define CPU_LOAD 100000
#define IO_LOAD 50000

namespace apollo {
namespace cyber{
namespace scheduler {

using apollo::cyber::croutine::RoutineState;

/***
 * @brief 模拟CPU密集型任务
 * @param
 *   CPU_LOAD: 表示任务负载，一直占有CPU
 */
void cpu_task() {
  int i = 0;
  while (i < CPU_LOAD) ++i;
}

/***
 * @brief 模拟IO密集型任务，模式为: 计算 -> 等待IO -> 计算
 * @param:
 *   IO_LOAD: 表示IO密集型任务中的CPU负载
 */
void io_task() {
  int i = 0;
  while (i < IO_LOAD) ++i;

  CRoutine::yield(RoutineState::IO_WAIT);
  i = 0;
  while (i <  IO_LOAD) ++i;
}

/**
 * @brief 支持的数据分布类型
 * @details
 *   UNIFORM: 均匀分布
 *   NORMAL: 正态分布
 */
enum distribution_type{
  UNIFORM,
  NORMAL
};

/***
 * @brief 从某个数据分布中采样，支持均匀分布和正态分布
 * @param type 数据分布类型
 * @param a 采样左区间
 * @param b 采样右区间
 * @return 采样值（默认值为10.0）
 */
double distribution_sampler(enum distribution_type type, double a, double b) {
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
      res = std::max(a, std::min(b, res)); // 限制在 [a, b] 区间内
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
std::shared_ptr<CRoutine>  get_croutine(float ratio) {
  bool flag = (static_cast<float>(std::rand()) / RAND_MAX) < ratio;
  if (flag) {
    return std::make_shared<CRoutine>(cpu_task);
  } else {
    return std::make_shared<CRoutine>(io_task);
  }
};


class BaseRunner {

 public:
  BaseRunner(int croutine_num, float ratio) :
    croutine_num_(croutine_num), ratio_(ratio) {}
  virtual ~BaseRunner();
  virtual void run() = 0;
 protected:
  int croutine_num_;
  float ratio_;
};

class ChographyRunner : public BaseRunner {

 public:
  ChographyRunner(const std::string& process_group, SchedulerChoreography* sched ,
           int croutine_num, float ratio, distribution_type type,
           double a, double b) :
  BaseRunner(croutine_num, ratio), sched_(dynamic_cast<SchedulerChoreography*>(sched)),
  process_group_(process_group), type_(type),
  a_(a), b_(b){}

  void run() {
    for (int i = 0; i < this->croutine_num_; ++i) {
      std::shared_ptr<CRoutine>  c_ptr = get_croutine(this->ratio_);
      std::string task_name = Mapping2CName(distribution_sampler(type_, a_, b_));
      auto task_id = GlobalData::RegisterTaskName(task_name);
      c_ptr->set_id(task_id);
      c_ptr->set_name(task_name);
      sched_->DispatchTask(c_ptr);
    }
  }

 private:
  std::string Mapping2CName(float prob) {

  }

 private:
  SchedulerChoreography* sched_;
  std::string process_group_;
  distribution_type type_;
  double a_;
  double b_;
};
}
}
}





#endif //COROUTINE_CYBERRT_SCHEDULER_TEST_UTILS_H_
