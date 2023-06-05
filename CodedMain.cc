#include <iostream>
#include <mpi.h>

#include "CodedMaster.h"
#include "CodedWorker.h"

using namespace std;

int main()
{
  MPI_Init(NULL, NULL);
  int nodeRank, nodeTotal; // 节点编号，节点总数
  MPI_Comm_rank(MPI_COMM_WORLD, &nodeRank); // 获取节点编号
  MPI_Comm_size(MPI_COMM_WORLD, &nodeTotal); // 获取节点总数

  if ( nodeRank == 0 ) {
    CodedMaster masterNode( nodeRank, nodeTotal );
    MPI_Comm_split( MPI_COMM_WORLD, 0, nodeRank, &masterNode );
    masterNode.run();
}
else {
    CodedWorker workerNode( nodeRank );
    MPI_Comm workerComm;
    MPI_Comm_split( MPI_COMM_WORLD, 1, nodeRank, &workerComm );
    workerNode.setWorkerComm( workerComm );
    workerNode.run();
}


  MPI_Finalize();
  
  return 0;
}
