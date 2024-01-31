**Cyber RT 协程的调度延时测试代码**

 

涉及文件：

- CyberRT/conf/test_choreography_latency.conf
- CyberRT/conf/test_classic_latency.conf
- CybertRT/scheduler/test_utils.h
- CyberRT/scheduler/croutine_latency.cc

编译及启动命令：

```shell
# 编译
bazel build :croutine_latency_test

# 启动
./croutine_latency_test [scheduler_conf_file] [is_classic_mode] [resources_num] [time_to_run]
./croutine_latency_test test_classic_latency 1 3 60
```

Cyber RT协程的优先级为0-19（MAX_PRIO为20），所以本测试代码为每一个资源组创建20个协程（一个资源组对应classic模式下的一个group或者choreography模式下的一个processor）。

每一个资源组的配置元信息如下：

```C
// croutine_latency.cc 42

/***
 * @brief: runner的配置信息，一个Runner类代表一个任务组
 * @note： 注意这里的顺序与Runner类成员变量的声明顺序不一样，但是切记C++成员变量的初始化顺序应该与声明顺序一致
 */
struct runner_conf {
  int croutine_num;                                                         // 创建协程的数量
  float ratio;                                                              // CPU密集型任务比例
  std::string process_group;                                                // 所属资源组名字
  DistributionType type;                                                    // 数据分布类型
  double a = 0;                                                             // 采样左区间
  double b = 20;                                                            // 采样右区间
    // 将概率值映射到task名字的函数指针
  std::string (*classic_mapping_ptr)(const std::string&, float ) = nullptr; 
    // 将概率值映射到task名字的函数指针
  std::string (*chography_mapping_ptr)(int, float ) = nullptr;     
    // choreography模式下的processor_id
  int processor_id = -1;                                      
  bool is_classic = true;                                                   // 是否是classic模式
};

```

整个调度器通过runner_conf数组配置：

```c
// croutine_latency.cc 92


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
```

