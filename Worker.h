#ifndef _MR_WORKER
#define _MR_WORKER

#include <unordered_map>

#include "Configuration.h"
#include "Common.h"
#include "Utility.h"
#include "Trie.h"

class Worker
{
 public:
 /*
  在 typedef 中，尖括号 "<" 和 ">" 用于指定一个模板类型的参数。在这个例子中，尖括号 "<" 和 ">" 的作用是为 unordered_map 容器类指定键类型。
  别名可以方便地用于定义和声明 定义的类型的变量 例如：PartitionCollection myMap;
 */
  typedef unordered_map< unsigned int, LineList* > PartitionCollection;   // 第一个 typedef 定义了一个名为 PartitionCollection 的类型别名，表示一个键为 unsigned int，值为指向 LineList 对象的指针的 unordered_map 容器。该别名可以方便地用于定义和声明 PartitionCollection 类型的变量
  typedef struct _TxData { // 第二个 typedef 定义了一个名为 TxData 的结构体，其定义了两个属性 data 和 numLine。
    unsigned char* data; // 表示所有中间结果数据的一个拼接字符串(数据内容)
    unsigned long long numLine; // 表示中间结果的数量
  } TxData;
  typedef unordered_map< unsigned int, TxData > PartitionPackData;  // 第三个 typedef 定义了一个名为 PartitionPackData 的类型别名，表示一个键为 unsigned int，值为 TxData 结构体类型的 unordered_map 容器。

 private:
  const Configuration* conf; //指针类型的常量
  unsigned int rank;
  PartitionList partitionList; // 分区列表 用于存储分区数据
  PartitionCollection partitionCollection; // partitionCollection[i] 表示第 i 个分区，它对应的值是一个指向行数据列表的指针。而该列表的大小就是分区中行的数量
  PartitionPackData partitionTxData; // 存储发送者节点的中间结果数据 数组的下标从 0 开始，对应的节点编号则从 1 开始
  PartitionPackData partitionRxData; // 存储接收者节点的中间结果数据 数组的下标从 0 开始，对应的节点编号则从 1 开始
  LineList localList; // 本地列表
  TrieNode* trie;// 前缀树

 public:
 Worker( unsigned int _rank ): rank( _rank ) {} // 构造函数,与Master 构造函数不同,这里只有一个参数,但语法上是一样的
  ~Worker();
  void run();

 private:
  void execMap(); // 执行映射
  void execReduce();// 执行归约
  void printLocalList(); // 打印本地列表
  void printPartitionCollection();// 打印分区集合
  void outputLocalList();// 输出本地列表
  TrieNode* buildTrie( PartitionList* partitionList, int lower, int upper, unsigned char* prefix, int prefixSize, int maxDepth );// 构建前缀树
};


#endif
