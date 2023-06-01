#ifndef _MR_PARTITIONSAMPLING
#define _MR_PARTITIONSAMPLING

#include <vector>
#include "Configuration.h"
#include "Common.h"

using namespace std;
/*
  在Hadoop中，分区列表是指将大量数据划分为若干个小数据块的过程.
  这些小数据块通常具有相同的大小，并被存储到不同的物理位置上，以便在并行计算中使用。
  分区列表可以是根据特定准则划分数据，如哈希码或某种排序规则。
  在 MapReduce 计算模型中，数据被分配到不同的分区中，并分别处理每个分区中的数据。
  PartitionSampling 类提供了一个创建分区列表的方法，该方法利用配置文件中的参数来决定如何随机地分配键到分区中。
*/
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
