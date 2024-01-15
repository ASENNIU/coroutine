

# Cyber RT源码分析

本篇博客参考很多开源的优秀的资料和博客，如下。

```https
Apollo cyber的设计 https://zhuanlan.zhihu.com/p/62259081
万字长文深入浅出 Golang Runtime https://zhuanlan.zhihu.com/p/95056679
```

Apollo Cyber RT 是一个开源、高性能的运行时框架，专为自动驾驶场景而设计。针对自动驾驶的高并发、低延迟、高吞吐量进行了大幅优化。

从设计上看，cyber需要具备以下功能。

![](./assert/CyberRT系统功能需求图.jpg)



本篇博客主要分析Cyber RT的协程模型，聚焦其设计与实现以及针对自动驾驶的高并发、低延迟、高吞吐量所做的优化。

具体来说，重点考虑以下几个点：

- **协程的基本设计模型，有栈协程还是无栈协程**
- **协程的调度策略以及分发到执行线程上的策略，是否可以跨线程，针对面向自动驾驶这样强的实时性场景对线程调度所做的优化**
- **执行栈和共享栈以及上下文信息的存储上的设计，在内存拷贝和CPU Cache Missing上的权衡**

# 基本概念

Apollo Cyber RT协程模型的设计吸取了很多[Golang runtime](https://zhuanlan.zhihu.com/p/95056679)的思想，这里先介绍goroutine

**1.GPM模型**

GPM模型是go语言中的概念，**G代表协程，P代表执行器，M代表线程**。P代表协程执行的环境，一个P绑定一个线程M，协程G根据不同的上下文环境在P中执行，上述的设计解耦了协程和线程，线程和协程都只知道P中，在go语言中还有一个调度器，把具体的协程分配到不同的P中，这样就可以在用户态实现协程的调度，同时执行多个任务。

在go语言早期，只有一个全局的协程队列，每个P都会从全局队列中取任务，然后执行。因为多个P都会去全局队列中取任务，因此会带来并发访问，需要在全局队列中加锁。全局队列调度模型如图。

![img](./assert/go早期GM模型.jpg)

​                                                                                全局队列模型

上述方法虽然实现了多个任务的调度，但是带来的问题是多个P都会去竞争全局锁，导致效率低下，之后go语言对调度模型做了改善，改善之后每个P都会拥有一个本地队列，P优先从本地队列中取任务执行，如果P中没有任务了，那么P会从全局或者相邻的任务中偷取一半的任务执行，这样带来的好处是不需要全局锁了，每个P都优先执行本地队列。另外调度器还会监视P中的协程，如果协程过长时间阻塞，则会把协程移动到全局队列，以示惩罚。本地队列加全局队列如图。

![img](./assert/GPM模型.jpg)

​                                                                             本地队列加全局队列模型

# Cyber RT协程模型 

理解了GPM模型，下面我们接着看Cyber中的调度器，SchedulerClassic代表着全局队列模型，SchedulerChoreography则代表着本地队列加全局队列模型。SchedulerChoreography模型和go语言的模型稍微有点区别，全局队列和本地队列是隔离的，也就是说本地队列不会去全局队列中取任务。

![](./assert/CyberRT协程调度示意图.png)

​                                                                              CyberRT调度协程示意图

 如上图所示，CyberRT协程二级调度分为两种策略：choreography和classic。其中每个scheduler和processor是一个task的抽象化类，该task在choreography策略下,对其调度的croutine采用依据优先级排序的FIFO队列调度策略。而在classic模式下，将对其croutine采用完全公平的轮转策略。

# CyberRT调度初始化流程

首先对Cyber RT协程有一个总体的认识。从初始化到创建CRoutine并执行，流程如下：

![](./assert/schedule初始化流程.png)

​                                                                            schedule初始化流程图

实例代码如下：

```c++
// 一个总体性的示例
TEST(SchedulerTest, create_task) {
  GlobalData::Instance()->SetProcessGroup("example_sched_classic");
  auto sched = Instance();
  cyber::Init("scheduler_test");
  // read example_sched_classic.conf task 'ABC' prio
  std::string croutine_name = "ABC";

  EXPECT_TRUE(sched->CreateTask(&proc, croutine_name));
  // create a croutine with the same name
  EXPECT_FALSE(sched->CreateTask(&proc, croutine_name));

  auto task_id = GlobalData::RegisterTaskName(croutine_name);
  EXPECT_TRUE(sched->NotifyTask(task_id));
  EXPECT_TRUE(sched->RemoveTask(croutine_name));
  // remove the same task twice
  EXPECT_FALSE(sched->RemoveTask(croutine_name));
  // remove a not exist task
  EXPECT_FALSE(sched->RemoveTask("driver"));
}

// SchedulerClassicTest
TEST(SchedulerClassicTest, sched_classic) {
  // read example_sched_classic.conf 
  GlobalData::Instance()->SetProcessGroup("example_sched_classic");
  //  创建scheduler实例
  auto sched1 = dynamic_cast<SchedulerClassic*>(scheduler::Instance());
  // 创建CRoutine并注册
  std::shared_ptr<CRoutine> cr = std::make_shared<CRoutine>(func);
  auto task_id = GlobalData::RegisterTaskName("ABC");
  // 配置CRoutine
  cr->set_id(task_id);
  cr->set_name("ABC");
  EXPECT_TRUE(sched1->DispatchTask(cr));
  // dispatch the same task
  EXPECT_FALSE(sched1->DispatchTask(cr));
  EXPECT_TRUE(sched1->RemoveTask("ABC"));
    
  sched1->Shutdown();
}

TEST(SchedulerChoreoTest, sched_choreo) {
  GlobalData::Instance()->SetProcessGroup("example_sched_choreography");
  auto sched = dynamic_cast<SchedulerChoreography*>(scheduler::Instance());
  cyber::Init("SchedulerChoreoTest");
  std::shared_ptr<CRoutine> cr = std::make_shared<CRoutine>(func);
  cr->set_id(GlobalData::RegisterTaskName("sched_choreo"));
  cr->set_name("sched_choreo");
  EXPECT_TRUE(sched->DispatchTask(cr));

  std::shared_ptr<CRoutine> cr1 = std::make_shared<CRoutine>(func);
  cr1->set_id(GlobalData::RegisterTaskName("sched_choreo1"));
  cr1->set_name("sched_choreo1");
  // 指定processor
  cr1->set_processor_id(0);
  EXPECT_TRUE(sched->DispatchTask(cr1));

  auto& croutines =
      ClassicContext::cr_group_[DEFAULT_GROUP_NAME].at(cr->priority());
  std::vector<std::string> cr_names;
  for (auto& croutine : croutines) {
    cr_names.emplace_back(croutine->name());
  }
  auto itr = std::find(cr_names.begin(), cr_names.end(), cr->name());
  EXPECT_NE(itr, cr_names.end());

  itr = std::find(cr_names.begin(), cr_names.end(), cr1->name());
  EXPECT_EQ(itr, cr_names.end());

  sched->RemoveTask(cr->name());
  croutines = ClassicContext::cr_group_[DEFAULT_GROUP_NAME].at(cr->priority());
  cr_names.clear();
  for (auto& croutine : croutines) {
    cr_names.emplace_back(croutine->name());
  }
  itr = std::find(cr_names.begin(), cr_names.end(), cr->name());
  EXPECT_EQ(itr, cr_names.end());
}

```

在Cyber Init之后，通过GlobalData::Instance()->SetProcessGroup读取对应conf文件；

创建一个Scheduler实例，在创建该实例时，会通过配置文件获取对应processor配置并创建对应processor；

Scheduler实例创建完毕后，通过CreateTask接口进行任务创建和CRoutine创建，其中包括：

- ①通过RegisterTaskName接口设置TaskName并返回task_id；
- ②创建CRoutine实例;
- ③将TaskName和task_id等任务信息配置给CRoutine；
- ④调用DispatchTask接口，将创建的CRoutine与processor进行绑定，如果在classic模式则将其加入对应group，并调用Notify进行任务唤醒，如果在choreography模式则调用Enqueue将该CRoutine入队；
- ⑤调用RegisterNotifyCallback注册回调函数。
- ⑥调用NotifyProcessor底层通过notify_one唤醒阻塞的线程继续执行。
- ⑦可通过调用NotifyTask手动唤醒阻塞线程。

# CyberRT调度各子模块

![](./assert/调度各子模块函数结构图.png)

​                                                                              调度各子模块结构图

（Context才是真正存储协程的地方，Context还有一个重要的成员函数Context::NextRoutine()，用此函数将Processor和Scheduler解耦）

## Processor执行器

Processor执行器是协程的载体，对协程来说Processor就是逻辑的CPU。Processor中有协程执行的上下文信息，还有绑定的线程信息。先看下BindContext的实现。

```c++
void Processor::BindContext(const std::shared_ptr<ProcessorContext>& context) {
  // 1. 协程执行的上下文信息，会随着协程切换而切换
  context_ = context;
  // 2. 绑定Processor到具体的线程
  std::call_once(thread_flag_,
                 [this]() { thread_ = std::thread(&Processor::Run, this); });
}
```

 - 使用 `std::call_once` 确保 `Processor` 类的 `Run` 函数仅在第一次调用时执行。在这里，它创建一个新的线程 `thread_`，并在该线程上运行 `Processor` 类的 `Run` 函数。通过这种方式，将 `Processor` 绑定到具体的线程上，确保 `Run` 函数在独立的线程中执行。

也就是说协程运行在Processor之上，切换协程的时候传入对应的上下文到Processor，然后Processor开始执行协程任务。下面看Processor是如何执行的。

```c++
void Processor::Run() {
  // 1. 获取线程的PID，系统内唯一
  tid_.store(static_cast<int>(syscall(SYS_gettid)));
  
  snap_shot_->processor_id.store(tid_);

  while (cyber_likely(running_.load())) {
    if (cyber_likely(context_ != nullptr)) {
      // 2. 获取优先级最高并且准备就绪的协程
      auto croutine = context_->NextRoutine();
      if (croutine) {
        snap_shot_->execute_start_time.store(cyber::Time::Now().ToNanosecond());
        snap_shot_->routine_name = croutine->name();
        // 3. 执行协程任务，完成后释放协程
        croutine->Resume();
        croutine->Release();
      } else {
        snap_shot_->execute_start_time.store(0);
        // 4. 如果协程组中没有空闲的协程，则等待
        context_->Wait();
      }
    } else {
      // 5. 如果上下文为空，则线程阻塞10毫秒
      std::unique_lock<std::mutex> lk(mtx_ctx_);
      cv_ctx_.wait_for(lk, std::chrono::milliseconds(10));
    }
  }
}
```

**疑问:**

1. 因为Processor每次都会从高优先级的队列开始取任务，假设Processor的数量不够，可能会出现低优先级的协程永远得不到调度的情况。
2. 协程的调度没有抢占，也就是说一个协程在执行的过程中，除非主动让出，否则会一直占用Processor，如果Processor在执行低优先级的任务，来了一个高优先级的任务并不能抢占执行。调度器的抢占还是交给线程模型去实现了，cyber中通过调节cgroup来调节，保证优先级高的任务优先执行。

一共有2种ProcessorContext上下文"ClassicContext"和"ChoreographyContext"上下文，分别对应不同的调度策略。后面分析Scheduler对象的时候，我们会接着分析。

## conf配置文件

Scheduler调度的配置文件在"conf"目录中，配置文件中可以设置线程线程的CPU亲和性以及调度测量，也可以设置cgroup，还可以设置协程的优先级。下面以"example_sched_classic.conf"文件来举例子。

```json
scheduler_conf {
    // 1. 设置调度器策略
    policy: "classic"
    // 2. 设置cpu set
    process_level_cpuset: "0-7,16-23" # all threads in the process are on the cpuset
    // 3. 设置线程的cpuset，调度策略和优先级
    threads: [
        {
            name: "async_log"
            cpuset: "1"
            policy: "SCHED_OTHER"   # policy: SCHED_OTHER,SCHED_RR,SCHED_FIFO
            prio: 0
        }, {
            name: "shm"
            cpuset: "2"
            policy: "SCHED_FIFO"
            prio: 10
        }
    ]
    classic_conf {
        // 4. 设置分组，线程组的cpuset，cpu亲和性，调度测量和优先级。设置调度器创建"processor"对象的个数，以及协程的优先级。  
        groups: [
            {
                name: "group1"
                processor_num: 16
                affinity: "range"
                cpuset: "0-7,16-23"
                processor_policy: "SCHED_OTHER"  # policy: SCHED_OTHER,SCHED_RR,SCHED_FIFO
                processor_prio: 0
                tasks: [
                    {
                        name: "E"
                        prio: 0
                    }
                ]
            },{
                name: "group2"
                processor_num: 16
                affinity: "1to1"
                cpuset: "8-15,24-31"
                processor_policy: "SCHED_OTHER"
                processor_prio: 0
                tasks: [
                    {
                        name: "A"
                        prio: 0
                    },{
                        name: "B"
                        prio: 1
                    },{
                        name: "C"
                        prio: 2
                    },{
                        name: "D"
                        prio: 3
                    }
                ]
            }
        ]
    }
}
```

**配置属性说明**

每个配置属性的含义、影响：

- groups：如example_sched_classic.conf的配置示例，classic策略可以配置多个group，主要为了实现资源隔离、跨numa问题，比如一个进程产生的所有task在0-31号cpu上执行，内核的调度会将线程在0-31号cpu上切换，跨numa节点会给系统带来额外的开销，这里可以通过group将numa节点进行隔离，一个numa节点下的0-7,16-23号cpu划分到一个group中，另外一个numa节点下的8-15,24-31号cpu划分到另一个group，这样既保证了资源利用，也能避免跨numa节点带来的开销。
- process_level_cpuset: 控制mainboard进程使用哪些cpu
- affinity： 取值为range或者1to1，如第一个group，创建16个线程，在0-7,16-23号cpu上设置亲和性，每个线程都可以在0-7，16-23号核上执行。第二个group中，affinity为1to1，表示16个线程对应8-15,24-31号cpu，每个线程和一个cpu进行亲和性设置，能减少线程在cpu之间切换带来的开销，但是前提是开启的线程数和cpu数必须一致。
- processor_policy和processor_prio: 这两个一般成对出现，processor_policy指设置线程的调度策略，取值为SCHED_FIFO（实时调度策略，先到先服务）, SCHED_RR（实时调度策略，时间片轮转）, SCHED_OTHER（分时调度策略，为默认策略），对于设置了SCHED_FIFO或者SCHED_RR的线程会更优先的得到cpu执行， 调度模型中设置processor_policy背景：为了保证主链路的任务或者其他一些实时task的优先执行。如果processor_policy设置为SCHED_FIFO/SCHED_RR，processor_prio取值为(1-99)，值越大，表明优先级越高，抢到cpu概率越大。如果processor_policy设置为SCHED_OTHER，processor_prio取值为（-20-19，0为默认值），这里为nice值，nice值不影响分配到cpu的优先级，但是影响分到cpu时间片的大小，如果nice值越小，分到的时间片越多。
- tasks：这里是对task任务进行配置，name表示task的名字，prio表示任务的优先级，为了提高性能，减小任务队列锁的粒度，调度模型中采用的是多优先级队列，也就是同一优先级的task在同一个队列里面，系统调度时会优先执行优先级高的任务。

下面我们开始看调度器，上述已经简单的介绍了2种调度器，SchedulerClassic是基于全局优先队列的调度器。

## SchedulerClassic调度器

我们知道协程实际上建立在线程之上，线程分时执行多个协程，看上去多个协程就是一起工作的。假如让你设计一个协程池，首先需要设置协程池中协程的个数，当协程超过协程池个数的时候需要把协程放入一个阻塞队列中，如果队列满了，还有协程到来，那么丢弃到来的协程，并且报错。（上述设计借鉴了线程池的思路）

**1.创建Processor**

调度器中首先会创建Processor，并且绑定到线程。调度器根据"conf"目录中的cgroup配置初始化线程，根据"processor_num"的个数创建多个Processor，并且绑定到线程。

```c++
void SchedulerClassic::CreateProcessor() {
  //  读取调度配置文件，参照conf目录
  for (auto& group : classic_conf_.groups()) {
    // 1. 分组名称 
    auto& group_name = group.name();
    // 2. 分组执行器(线程)数量 等于协程池大小
    auto proc_num = group.processor_num();
    if (task_pool_size_ == 0) {
      task_pool_size_ = proc_num;
    }
    // 3. 分组CPU亲和性
    auto& affinity = group.affinity();
    // 4. 分组线程调度策略
    auto& processor_policy = group.processor_policy();
    // 5. 分组优先级
    auto processor_prio = group.processor_prio();
    // 7. 分组cpu set
    std::vector<int> cpuset;
    ParseCpuset(group.cpuset(), &cpuset);

    for (uint32_t i = 0; i < proc_num; i++) {
      auto ctx = std::make_shared<ClassicContext>(group_name);
      pctxs_.emplace_back(ctx);

      auto proc = std::make_shared<Processor>();
      // 8. 绑定上下文
      proc->BindContext(ctx);
      // 9. 设置线程的cpuset和cpu亲和性
      SetSchedAffinity(proc->Thread(), cpuset, affinity, i);
      // 10. 设置线程调度策略和优先级 (proc->Tid()为线程pid)
      SetSchedPolicy(proc->Thread(), processor_policy, processor_prio,
                     proc->Tid());
      processors_.emplace_back(proc);
    }
  }
}
```

`SchedulerClassic`是`Scheduler`的子类，`Scheduler`中实现了`CreateTask`和`NotifyTask`接口，用于创建任务和唤醒任务。对用户来说只需要关心任务，`Scheduler`为我们屏蔽了`Processor`对象的操作。对应的子类中实现了`DispatchTask`，`RemoveTask`和`NotifyProcessor`的操作。

我们先看Scheduler如何创建任务。

```c++
bool Scheduler::CreateTask(std::function<void()>&& func,
                           const std::string& name,
                           std::shared_ptr<DataVisitorBase> visitor) {

  auto task_id = GlobalData::RegisterTaskName(name);
  // 1. 创建协程，绑定func函数
  auto cr = std::make_shared<CRoutine>(func);
  cr->set_id(task_id);
  cr->set_name(name);
  AINFO << "create croutine: " << name;
  
  // 2. 分发任务
  if (!DispatchTask(cr)) {
    return false;
  }

  // 3. 注册回调，visitor参数为可选的。
  if (visitor != nullptr) {
    visitor->RegisterNotifyCallback([this, task_id]() {
      if (cyber_unlikely(stop_.load())) {
        return;
      }
      this->NotifyProcessor(task_id);
    });
  }
  return true;
}
```

**总结**：

1. 创建任务的时候只有对应数据访问的`DataVisitorBase`注册了回调，其它的任务要自己触发回调。
2. `DispatchTask`中调用子类中的不同策略进行任务分发。

接着我们看`SchedulerClassic`的`DispatchTask`调度策略。

```c++
bool SchedulerClassic::DispatchTask(const std::shared_ptr<CRoutine>& cr) {
  // 1. 根据协程id，获取协程的锁
  ...

  // 2. 将协程放入协程map
  {
    WriteLockGuard<AtomicRWLock> lk(id_cr_lock_);
    if (id_cr_.find(cr->id()) != id_cr_.end()) {
      return false;
    }
    id_cr_[cr->id()] = cr;
  }

  // 3. 设置协程的优先级和group
  if (cr_confs_.find(cr->name()) != cr_confs_.end()) {
    ClassicTask task = cr_confs_[cr->name()];
    cr->set_priority(task.prio());
    cr->set_group_name(task.group_name());
  } else {
    // croutine that not exist in conf
    cr->set_group_name(classic_conf_.groups(0).name());
  }

  // 4. 将协程放入对应的优先级队列
  // Enqueue task.
  {
    WriteLockGuard<AtomicRWLock> lk(
        ClassicContext::rq_locks_[cr->group_name()].at(cr->priority()));
    ClassicContext::cr_group_[cr->group_name()]
        .at(cr->priority())
        .emplace_back(cr);
  }

  // 5. 唤醒协程组
  ClassicContext::Notify(cr->group_name());
  return true;
}
```

这里`NotifyProcessor`实际上就是唤醒对应`Processor`的上下文执行环境。

```c++
bool SchedulerClassic::NotifyProcessor(uint64_t crid) {
  if (cyber_unlikely(stop_)) {
    return true;
  }

  {
    ReadLockGuard<AtomicRWLock> lk(id_cr_lock_);
    if (id_cr_.find(crid) != id_cr_.end()) {
      auto cr = id_cr_[crid];
      if (cr->state() == RoutineState::DATA_WAIT ||
          cr->state() == RoutineState::IO_WAIT) {
        cr->SetUpdateFlag();
      }

      ClassicContext::Notify(cr->group_name());
      return true;
    }
  }
  return false;
}
```

**疑问**:

1. 如果协程在等待IO，系统知道协程在等待io，但还是触发对应的协程组去工作。并没有让协程继续阻塞？？？

再看如何从`ClassicContext`获取`Processor`执行的协程。下图是全局协程队列的数据结构，为一个2级数组，第一级数组为优先级数组，第二级数组为一个队列。

再看如何从`ClassicContext`获取`Processor`执行的协程。下图是全局协程队列的数据结构，为一个2级数组，第一级数组为优先级数组，第二级数组为一个队列。

![img](./assert/classicschedule.jpg)



数据结构

```c++
std::shared_ptr<CRoutine> ClassicContext::NextRoutine() {
  if (cyber_unlikely(stop_.load())) {
    return nullptr;
  }

  // 1. 从优先级最高的队列开始遍历
  for (int i = MAX_PRIO - 1; i >= 0; --i) {
    // 2. 获取当前优先级队列的锁
    ReadLockGuard<AtomicRWLock> lk(lq_->at(i));
    for (auto& cr : multi_pri_rq_->at(i)) {
      if (!cr->Acquire()) {
        continue;
      }
      // 3. 返回状态就绪的协程
      if (cr->UpdateState() == RoutineState::READY) {
        return cr;
      }

      cr->Release();
    }
  }

  return nullptr;
}
```

我们知道线程阻塞的条件有4种：

1. 通过调用sleep(millseconds)使任务进入休眠状态
2. 通过调用wait（）使线程挂起
3. 等待某个输入、输出流
4. 等待锁

而Processor绑定的线程阻塞是通过上下文的等待实现的。在ClassicContext中等待条件（1s的超时时间），等待的时候设置notify_grp_减1。经典PV操作。

```c++
void ClassicContext::Wait() {
  // 1. 获取锁
  std::unique_lock<std::mutex> lk(mtx_wrapper_->Mutex());
  // 2. 等待条件大于0
  cw_->Cv().wait_for(lk, std::chrono::milliseconds(1000),
                     [&]() { return notify_grp_[current_grp] > 0; });
  // 3. 对应协程组的唤醒条件减1
  if (notify_grp_[current_grp] > 0) {
    notify_grp_[current_grp]--;
  }
}
```

Processor的唤醒也是通过上下文实现的。

```c++
void ClassicContext::Notify(const std::string& group_name) {
  // 1. 加锁
  (&mtx_wq_[group_name])->Mutex().lock();
  // 2. 协程唤醒条件加1
  notify_grp_[group_name]++;
  (&mtx_wq_[group_name])->Mutex().unlock();
  // 3. 唤醒线程
  cv_wq_[group_name].Cv().notify_one();
}
```

关于SchedulerClassic我们就先介绍到这里，下面我们开始介绍另外一种调度器SchedulerChoreography。

## SchedulerChoreography调度器

SchedulerChoreography创建Processor，会分2部分创建，一部分是有本地队列的Processor，一部分是协程池的Processor。

```c++
void SchedulerChoreography::CreateProcessor() {
  for (uint32_t i = 0; i < proc_num_; i++) {
    auto proc = std::make_shared<Processor>();
    // 1. 绑定ChoreographyContext
    auto ctx = std::make_shared<ChoreographyContext>();

    proc->BindContext(ctx);
    SetSchedAffinity(proc->Thread(), choreography_cpuset_,
                     choreography_affinity_, i);
    SetSchedPolicy(proc->Thread(), choreography_processor_policy_,
                   choreography_processor_prio_, proc->Tid());
    pctxs_.emplace_back(ctx);
    processors_.emplace_back(proc);
  }

  for (uint32_t i = 0; i < task_pool_size_; i++) {
    auto proc = std::make_shared<Processor>();
    // 2. 绑定ClassicContext
    auto ctx = std::make_shared<ClassicContext>();

    proc->BindContext(ctx);
    SetSchedAffinity(proc->Thread(), pool_cpuset_, pool_affinity_, i);
    SetSchedPolicy(proc->Thread(), pool_processor_policy_, pool_processor_prio_,
                   proc->Tid());
    pctxs_.emplace_back(ctx);
    processors_.emplace_back(proc);
  }
}
```

再看SchedulerChoreography如何分发任务。

```c++
bool SchedulerChoreography::DispatchTask(const std::shared_ptr<CRoutine>& cr) {
  // 1. 根据协程id，获取协程的锁
  ...

  // 2. 设置优先级和协程绑定的Processor Id
  if (cr_confs_.find(cr->name()) != cr_confs_.end()) {
    ChoreographyTask taskconf = cr_confs_[cr->name()];
    cr->set_priority(taskconf.prio());

    if (taskconf.has_processor()) {
      cr->set_processor_id(taskconf.processor());
    }
  }

  ...

  uint32_t pid = cr->processor_id();
  // 3. 如果Processor Id小于proc_num_，默认Processor Id为-1
  if (pid < proc_num_) {
    // 4. 协程放入上下文本地队列中
    static_cast<ChoreographyContext*>(pctxs_[pid].get())->Enqueue(cr);
  } else {

    cr->set_group_name(DEFAULT_GROUP_NAME);

    // 5. 协程放入ClassicContext协程池队列中
    {
      WriteLockGuard<AtomicRWLock> lk(
          ClassicContext::rq_locks_[DEFAULT_GROUP_NAME].at(cr->priority()));
      ClassicContext::cr_group_[DEFAULT_GROUP_NAME]
          .at(cr->priority())
          .emplace_back(cr);
    }
  }
  return true;
}
```

**疑问**：

1. cr->processor_id()的默认值为"-1"，而vector访问越界的时候不会报错，本来应该放入全局队列中的？？？

## ChoreographyContext上下文

ChoreographyContext中的调度就非常简单了。

```c++
std::shared_ptr<CRoutine> ChoreographyContext::NextRoutine() {
  // 1. 从本地队列中取出协程
  ReadLockGuard<AtomicRWLock> lock(rq_lk_);
  for (auto it : cr_queue_) {
    auto cr = it.second;
    if (!cr->Acquire()) {
      continue;
    }

    if (cr->UpdateState() == RoutineState::READY) {
      return cr;
    }
    cr->Release();
  }
  return nullptr;
}
```

ChoreographyContext中"Wait"和"Notify"方法与ClassicContext类似，这里就不展开了。

## 总结

1. SchedulerClassic 采用了协程池的概念，协程没有绑定到具体的Processor，所有的协程放在全局的优先级队列中，每次从最高优先级的任务开始执行。
2. SchedulerChoreography 采用了本地队列和全局队列相结合的方式，通过"ChoreographyContext"运行本地队列的线程，通过"ClassicContext"来运行全局队列。
3. Processor对协程来说是一个逻辑上的cpu，ProcessorContext实现Processor的运行上下文，通过ProcessorContext来获取协程，休眠或者唤醒，Scheduler调度器实现了协程调度算法。

