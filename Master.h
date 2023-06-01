#ifndef _MR_MASTER
#define _MR_MASTER

#include "Configuration.h"

class Master
{
 private:
  Configuration conf;  // 不是指针类型的常量
  unsigned int rank;  // 节点编号
  unsigned int totalNode; // 节点总数

 public:
 /*
 初始化主节点的属性。在代码中，rank( _rank ) 表示将传入的 _rank 参数值赋值给 rank 属性；totalNode( _totalNode ) 表示将传入的 _totalNode 参数值赋值给 totalNode 属性。
 */
 Master( unsigned int _rank, unsigned int _totalNode ): rank( _rank ), totalNode( _totalNode ) {}; 
  ~Master() {};

  void run(); // 运行主节点
};

#endif
