#include <iostream>
#include <mpi.h>

#include "Master.h"
#include "Worker.h"

using namespace std;

int main()
{
  MPI_Init(NULL, NULL); // 初始化MPI环境
  int nodeRank, nodeTotal; // 节点编号，节点总数
  MPI_Comm_rank(MPI_COMM_WORLD, &nodeRank); // 获取节点编号
  MPI_Comm_size(MPI_COMM_WORLD, &nodeTotal); // 获取节点总数

  if (nodeRank == 0) { // 如果是主节点
    Master masterNode(nodeRank, nodeTotal); // 创建主节点
    masterNode.run();
  }
  else {
    Worker workerNode(nodeRank);
    workerNode.run();
  }

  MPI_Finalize();

  return 0;
}
