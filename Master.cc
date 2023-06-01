#include <iostream>
#include <cassert>
#include <mpi.h>
#include <iomanip>

#include "Master.h"
#include "Common.h"
#include "Configuration.h"
#include "PartitionSampling.h"

using namespace std;

void Master::run()
{
  if (totalNode != 1 + conf.getNumReducer()) { // 1主节点+n个工作节点
    cout << "The number of workers mismatches the number of processes.\n";
    assert(false); // 断言
    
    //assert(false); 是一个断言语句。断言是一种用于在程序中检查条件是否满足的机制。
    //如果断言的条件为 false,也就是条件不满足那么程序会终止并输出错误信息。
  }

  // GENERATE LIST OF PARTITIONS. // 生成分区列表
  PartitionSampling partitioner;  // 分区采样
  partitioner.setConfiguration(&conf); // 设置配置
  PartitionList* partitionList = partitioner.createPartitions();    // 创建分区
 
  // BROADCAST CONFIGURATION TO WORKERS 广播配置到工作节点
  /*
    MPI_Bcast 函数的参数依次为要发送/接收的数据指针、数据长度、数据类型、消息源、通信域。
    第一个参数是指向配置信息结构体 conf 的指针
    第二个参数是 sizeof(Configuration) 表示 conf 的大小
    第三个参数是广播源的进程编号 0，表示将从进程 0 中读取广播的数据，并将其发送给其它所有工作节点。
    最后一个参数是通信域，通常将这个域设置为 MPI_COMM_WORLD，表示将所有进程归为同一通信域中。
  */
  MPI_Bcast(&conf, sizeof(Configuration), MPI_CHAR, 0, MPI_COMM_WORLD);

  // BROADCAST PARTITIONS TO WORKERS 
  /*
    auto 关键字是 C++11 引入的一种类型推导机制，它可以让编译器自动推断某个变量的类型。
    使用了一个迭代器（或者说指针） it 遍历分区列表 partitionList 中的每一个分区。对于每个分区，都将它的内容广播给所有的工作节点
    第一个参数是指向分区内容的指针
    第二个参数是分区大小，由于键值对的键长度是不确定的（由配置信息中的 getKeySize() 方法获取,加一是为了将结尾符 '\0' 也包含在内）。
    第三个参数表示广播的数据类型，本例中为 MPI_UNSIGNED_CHAR，表示使用无符号字符类型进行广播。
    第四个和第五个参数表示广播源以及接收方的进程编号（或通信域），和前面的代码一样，这里将广播源设置为进程 0，接收方的进程编号使用 MPI_COMM_WORLD 表示所有工作节点。
  */
  for (auto it = partitionList->begin(); it != partitionList->end(); it++) {
    unsigned char* partition = *it;
    MPI_Bcast(partition, conf.getKeySize() + 1, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
  }

  // TIME BUFFER // 时间缓冲区
  int numWorker = conf.getNumReducer(); // 获取工作节点数
  double rcvTime[numWorker + 1]; //  定义了一个数组，用于记录每个 reducer 节点处理 Map 阶段任务所耗费的时间，数组长度为 numWorker + 1，其中 +1 是为了防止数组越界。
  double rTime = 0; // rTime 用于记录当前 reducer 节点处理数据所花费的时间，初始值为 0。
  double avgTime; // avgTime 用于记录所有 reducer 节点处理数据的平均时间。
  double maxTime; // maxTime 用于记录所有 reducer 节点处理数据的最大时间。

  // COMPUTE MAP TIME // 计算 Map 阶段时间
  /*
    第一个参数 &rTime 表示发送的数据缓冲区地址，即本节点的 Map 处理时间。
    第二个参数 1 该程序中每个节点只需要发送本节点的 Shuffle 阶段时间给根节点 0 即可，不需要发送其他数据
    第三个参数 MPI_DOUBLE 表示发送数据缓冲区中元素的数据类型，即一个双精度浮点数。
    第四个参数 rcvTime 表示接收数据缓冲区的地址，即所有 reducer 节点 Map 的处理时间
    第五个参数 1 表示接收数据缓冲区中元素的数量，与发送一样也是只有一个元素，因为每个 reducer 节点只统计了自己的 Map 处理时间。
    第六个参数 MPI_DOUBLE 表示接收数据缓冲区中元素的数据类型。
    第七个参数 0 表示接收数据的进程号，这里是进程 0。
    第八个参数 MPI_COMM_WORLD 表示通信域，这里是全局通信域。
    用于测量整个阶段或整个程序的执行时间 （所有节点的性能）
  */
  MPI_Gather(&rTime, 1, MPI_DOUBLE, rcvTime, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD); // MPI_Gather() 函数用于收集所有工作节点的数据，将它们汇总到根节点 0 中。
  avgTime = 0; 
  maxTime = 0;
  for (int i = 1; i <= numWorker; i++) { // 遍历所有 reducer 节点的 Map 处理时间，计算平均时间和最大时间。
    avgTime += rcvTime[i];
    maxTime = max(maxTime, rcvTime[i]);
  }
  cout << rank << ": MAP     | Avg = " << setw(10) << avgTime / numWorker // setw() 函数用于设置输出的宽度，这里设置为 10。
       << "   Max = " << setw(10) << maxTime << endl;

  // COMPUTE PACKING TIME 计算 Map 阶段后数据打包的时间，即将 Map 阶段输出的键值对打包成分区的时间,与计算 Map 阶段时间的代码类似
  /*
    可以看到，在 MPI_Gather() 函数中，第一个参数都是 &rTime，即本节点的 Map 阶段时间或数据打包时间。那么程序如何区分这两种不同的时间呢？
    在执行 Map 阶段时，变量 rTime 存储的是本节点执行 Map 函数所需的时间；在执行数据打包时，变量 rTime 存储的是本节点进行数据打包所需的时间。
    由于 Map 阶段和数据打包是顺序执行的，且本节点只能执行其中的一种操作，因此可以通过变量名称和代码逻辑来区分这两种时间。
  */
  MPI_Gather(&rTime, 1, MPI_DOUBLE, rcvTime, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD); //收集所有节点的数据打包时间
  avgTime = 0;
  maxTime = 0;
  for (int i = 1; i <= numWorker; i++) {
    avgTime += rcvTime[i];
    maxTime = max(maxTime, rcvTime[i]);
  }
  cout << rank << ": PACK    | Avg = " << setw(10) << avgTime / numWorker
       << "   Max = " << setw(10) << maxTime << endl;

  // COMPUTE SHUFFLE TIME 这段代码用于收集 Shuffle 阶段的时间和数据传输速率，并计算它们的平均值
  /*
    MPI_Barrier(MPI_COMM_WORLD) 函数的作用是等待所有进程到达同一点，然后再继续执行后面的指令。
    MPI_RECV() 函数的第一个参数是接受缓存区，也就是用于存储接收到的数据的变量 rTime 和 txRate；
    第二个参数是接收数据的数量，这里都是 1；
    第三个参数是接收数据的数据类型，这里都是 MPI_DOUBLE；
    第四个参数是发送节点的标识符，即当前处理的 Reducer 节点 i
    第五个参数是消息标记，这里是0
    第六个参数是通信域，这里是全局通信域 MPI_COMM_WORLD；
    最后一个参数是用于获取状态信息的变量，这里是 MPI_STATUS_IGNORE，表示不需要获取状态信息
  */
  double txRate = 0; // txRate 用于记录当前 reducer 节点的数据传输速率，初始值为 0。
  double avgRate = 0; // avgRate 用于记录所有 reducer 节点的数据传输速率的平均值。
  for (unsigned int i = 1; i <= conf.getNumReducer(); i++) { // 遍历所有 reducer 节点
    MPI_Barrier(MPI_COMM_WORLD); // 第一次 MPI_Barrier(MPI_COMM_WORLD) 函数确保了所有节点都已完成 Map 阶段
    MPI_Barrier(MPI_COMM_WORLD); // 第二次 MPI_Barrier(MPI_COMM_WORLD) 函数则确保了所有节点都已经完成了数据打包阶段。
    MPI_Recv(&rTime, 1, MPI_DOUBLE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // 接收 reducer 节点 i 的 Shuffle 阶段时间
    avgTime += rTime; // 计算所有 reducer 节点的 Shuffle 阶段时间的总和
    MPI_Recv(&txRate, 1, MPI_DOUBLE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // 接收 reducer 节点 i 的数据传输速率
    avgRate += txRate; // 计算所有 reducer 节点的数据传输速率的总和
  }
  cout << rank << ": SHUFFLE | Sum = " << setw(10) << avgTime
       << "   Rate = " << setw(10) << avgRate / numWorker << " Mbps" << endl;

  // COMPUTE UNPACK TIME 评估数据解包操作的性能表现，包括平均解包时间和最大解包时间
  /*
    不再赘述，与计算 Map 阶段时间的代码类似
  */
  MPI_Gather(&rTime, 1, MPI_DOUBLE, rcvTime, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD); // 收集所有 reducer 节点的解包时间
  avgTime = 0;
  maxTime = 0;
  for (int i = 1; i <= numWorker; i++) {
    avgTime += rcvTime[i];
    maxTime = max(maxTime, rcvTime[i]);
  }
  cout << rank << ": UNPACK  | Avg = " << setw(10) << avgTime / numWorker
       << "   Max = " << setw(10) << maxTime << endl;

  // COMPUTE REDUCE TIME 评估 Reduce 阶段的性能表现，包括平均 Reduce 时间和最大 Reduce 时间
  MPI_Gather(&rTime, 1, MPI_DOUBLE, rcvTime, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD); // 收集所有 reducer 节点的 Reduce 时间
  avgTime = 0;
  maxTime = 0;
  for (int i = 1; i <= numWorker; i++) {
    avgTime += rcvTime[i];
    maxTime = max(maxTime, rcvTime[i]);
  }
  cout << rank << ": REDUCE  | Avg = " << setw(10) << avgTime / numWorker
       << "   Max = " << setw(10) << maxTime << endl;

  // CLEAN UP MEMORY 释放内存
  for (auto it = partitionList->begin(); it != partitionList->end(); it++) { // 遍历 partitionList
    delete[] *it; // 释放内存
  }
}
