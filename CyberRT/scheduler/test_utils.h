//
// Created by dell on 2024/1/29.
//

#ifndef COROUTINE_CYBERRT_SCHEDULER_TEST_UTILS_H_
#define COROUTINE_CYBERRT_SCHEDULER_TEST_UTILS_H_


#include "cyber/scheduler/policy/scheduler_classic.h"

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

/***
 * @brief: 代表运行时的虚基类，将上层业务和scheduler绑定
 */
class BaseRunner {

 public:
  BaseRunner(int croutine_num, float ratio) :
    croutine_num_(croutine_num), ratio_(ratio) {}
  virtual ~BaseRunner();
  virtual void run() = 0;
 protected:
  int croutine_num_;                          // 创建协程的数量
  float ratio_;                               // CPU密集型任务的比例
};

/***
 * @brief：运行时的实现类
 * @tparam T：scheduler的类型，SchedulerClassic或 SchedulerChoreography
 */
template<typename T>
class Runner final : public BaseRunner {

 public:
  Runner(T* sched, const std::string& process_group,
         int croutine_num = 1000, float ratio = 0.5,
         std::string (*p_classic)(const std::string& , float ) = nullptr,
         std::string (*p_chography)(float ) = nullptr,
         int processor_id = -1, bool is_classic = true,
         distribution_type type = NORMAL,
         double a = 0, double b = 0) :
  BaseRunner(croutine_num, ratio), sched_(dynamic_cast<T*>(sched)),
  process_group_(process_group), processor_id_(processor_id),
  classic_mapping_ptr_(p_classic), chography_mapping_ptr_(p_chography),
  is_classic_(is_classic), type_(type), a_(a), b_(b){}

  Runner(const Runner&) = delete;
  Runner(const Runner&&) = delete;

  Runner& operator=(const Runner&) = delete;
  Runner& operator=(const Runner&&) = delete;

  /***
   * @brief: 执行函数，根据需创建协程的协程和任务比例，创建协程由scheduler分发
   */
  void run() {
    for (int i = 0; i < this->croutine_num_; ++i) {
      std::shared_ptr<CRoutine>  c_ptr = get_croutine(this->ratio_);

      float prob = distribution_sampler(type_, a_, b_);
      std::string task_name;
      if (is_classic_) {
        task_name = classic_mapping_ptr_(process_group_, prob);
      } else {
        task_name = chography_mapping_ptr_(prob);
      }

      auto task_id = GlobalData::RegisterTaskName(task_name);
      c_ptr->set_id(task_id);
      c_ptr->set_name(task_name);

      if (!is_classic_) {
        c_ptr->set_processor_id(processor_id_);
      }

      sched_->DispatchTask(c_ptr);
    }
  }


 private:
  T* sched_;                                                                // 调度器类指针
  std::string process_group_;                                               // 所属资源组名字
  distribution_type type_;                                                  // 数据分布类型
  double a_ = 0;                                                            // 采样左区间
  double b_ = 20;                                                                   // 采样右区间
  std::string (*classic_mapping_ptr_)(const std::string&, float ) = nullptr;       // 将概率值映射到task名字的函数指针
  std::string (*chography_mapping_ptr_)(float ) = nullptr;                  // 将概率值映射到task名字的函数指针
  int processor_id_ = -1;                                                   // choreography模式下的processor_id
  bool is_classic_ = true;                                                  // 是否是classic模式

  // bool is_choreography_;
};

}
}
}





#endif //COROUTINE_CYBERRT_SCHEDULER_TEST_UTILS_H_
