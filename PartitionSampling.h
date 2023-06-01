#ifndef _MR_PARTITIONSAMPLING
#define _MR_PARTITIONSAMPLING

#include <vector>
#include "Configuration.h"
#include "Common.h"

using namespace std;
/*
  在 MapReduce 中，分区（Partition）是指对输入键值对进行哈希或范围划分的过程，
  其主要目的是将具有相同 key 的键值对映射到同一个 reduce task 进行处理。分区的定义和实现通常由 Partitioner 类完成。
*/s
class PartitionSampling {
 private:
  const Configuration* conf;  // 一个指向常量 Configuration 对象的指针
  
 public:
  PartitionSampling(); // 构造函数
  ~PartitionSampling();  // 析构函数

  void setConfiguration( const Configuration* configuration ) { conf = configuration; } 
  /*
  这段代码是一个函数，它接受一个指向 Configuration 类型对象的指针，并将其赋值给某个全局变量 conf。
  具体地，该函数的作用是设置 MapReduce 任务的配置信息。在执行 MapReduce 的过程中，一些参数如 reducer 节点数、输入路径、输出路径等需要在不同阶段进行传递和共享。
  因此，用户可以通过调用 setConfiguration() 函数来设置这些参数，在任务的执行过程中使用。
  */
  PartitionList* createPartitions(); // 创建分区

 private:
  static bool cmpKey( const unsigned char* keyl, const unsigned char* keyr ); // 比较两个键
  void printKeys( const PartitionList& keyList ) const; // 打印键
};

#endif
