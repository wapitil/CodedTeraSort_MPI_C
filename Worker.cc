#include <iostream>
#include <mpi.h>
#include <iomanip>
#include <fstream>
#include <cstdio>
#include <map>
#include <assert.h>
#include <algorithm>
#include <ctime>
#include <cstring>
#include "Worker.h"
#include "Configuration.h"
#include "Common.h"
#include "Utility.h"

using namespace std;

Worker::~Worker() // 析构函数
{
  delete conf; // 删除配置
  for ( auto it = partitionList.begin(); it != partitionList.end(); ++it ) { // 循环遍历 partitionList 容器中指针类型的元素，并释放掉对应的堆内存空间。
    delete [] *it;
  }

  LineList* ll = partitionCollection[ rank - 1 ];  // 在 partitionCollection 容器中取出“rank-1”对应的 LineList 指针 ll，然后循环遍历 ll 容器并释放其中的指针，最后释放 LineList 对象的堆内存。
  for( auto lit = ll->begin(); lit != ll->end(); lit++ ) {
    delete [] *lit;
  }
  delete ll;

  for ( auto it = localList.begin(); it != localList.end(); ++it ) { // 循环遍历 localList 容器中指针类型的元素，并释放掉对应的堆内存空间。
     delete [] *it;
  }

  delete trie; // 释放前缀树
}

void Worker::run()
{
  // RECEIVE CONFIGURATION FROM MASTER 接收主节点的配置
  conf = new Configuration; // 这一行代码创建了一个名为 conf 的指针对象，它指向在堆上动态分配的 Configuration 对象。这个对象用来存储来自主机的配置信息。
  /*
    在这段代码中，由于要发送的数据类型是 Configuration 对象的指针，而该类型不是 MPI 预定义的数据类型，因此需要将其转换为 MPI 能够接受的数据类型，这里使用的是 void* 类型。
    void* 类型可以表示任意类型的指针，包括指向 Configuration 对象的指针，因此将 Configuration 对象的指针强制转换为 void* 类型后才能作为 MPI_Bcast 函数的参数进行传递。
  */
  MPI_Bcast((void*)conf, sizeof(Configuration), MPI_CHAR, 0, MPI_COMM_WORLD); // 使用 MPI_Bcast 函数从主机广播 Configuration 对象，所有正在运行的进程都会接收该对象

  // RECEIVE PARTITIONS FROM MASTER 让所有的 reducer 节点都接收来自主节点的分区信息，并将这些分区信息保存在 partitionList 容器中
  /*
    for 循环遍历所有的 reducer 节点（从 1 开始，因为 0 表示主节点），然后使用 MPI_Bcast 函数从主节点广播当前分区的信息
    buff：在每次循环中，都会动态地为 buff 分配一块大小为 conf->getKeySize() + 1 字节的内存空间，用于存储从主节点接收到的分区信息。
          这样就能够保证接收到的分区信息被正确地存储在内存中，并且不会出现覆盖数据的情况。
    conf->getKeySize() + 1：conf->getKeySize() 表示分区的键（key）的长度，加 1 是为了也能接收到值（value）的长度；
          因为在这个系统中，分区的键（key）和值（value）是连在一起作为一个整体进行传输的。在主节点中，将键和值拼接在一起，并一起通过 MPI_Bcast 函数发送给所有的 reducer 节点。
          为了在 reducer 节点中分离出键和值，需要先知道键的长度和值的长度。而在主节点中，在将键和值拼接在一起发送给 reducer 节点之前，需要在键的末尾添加一个结束符（如 '\0'），以便在 reducer 节点中正确地分离出键和值。
          因此，接收数据的缓冲区 buff 的长度应该为键的长度加上值的长度再加 1（即结束符的长度），这样才能保证 reducer 节点正确地接收到分区的键和值。
    MPI_UNSIGNED_CHAR：接收数据类型，这里指定为 unsigned char 类型；
    0：广播的源节点，即主节点；
    MPI_COMM_WORLD：通信域，对于所有的节点来说，都是使用这个通信域进行通信。

     bug点: 循环次数不对
  */
  for (unsigned int i = 1; i < conf->getNumReducer(); i++) {  
    unsigned char* buff = new unsigned char[conf->getKeySize() + 1];
    MPI_Bcast(buff, conf->getKeySize() + 1, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
    partitionList.push_back(buff); // 将 buff 指针添加到 partitionList 容器中
  }

  // EXECUTE MAP PHASE
  clock_t time; // 用于记录程序开始执行 Map 阶段时的时间。
  // clock_t 是 C/C++ 标准库中提供的一种数据类型。它通常被用于记录程序执行的时间，其取值范围是 0 到 CLOCKS_PER_SEC-1，其中 CLOCKS_PER_SEC 是一个程序执行一秒钟所需要的 CPU 时钟数。
  double rTime; // 用于记录程序执行 Map 阶段所花费的时间。
  execMap(); // 执行 Map 阶段

  // SHUFFLING PHASE  
  /*
    Shuffle 阶段是将 Map 阶段产生的中间结果按照 Key 值进行分组并发送给不同的 Reducer 节点，然后将这些中间结果合并并交给 Reducer 进行 Reduce 阶段的计算
  */
  unsigned int lineSize = conf->getLineSize(); // 获取每行数据的长度
  for (unsigned int i = 1; i <= conf->getNumReducer(); i++) { // 遍历所有的 reducer 节点
    if (i == rank) { // 对于发送者节点（即 i == rank 的情况），会将本节点的中间结果消息发送给其他所有的 Reducer 节点，包括它自己
      clock_t txTime = 0; // 用于记录发送数据所花费的时间
      
      unsigned long long tolSize = 0; // 用于记录发送数据的总大小
      MPI_Barrier(MPI_COMM_WORLD); // 调用 MPI_Barrier 函数，等待所有的节点都执行到这里
      time = clock(); // 记录程序开始执行 Shuffle 阶段时的时间
      // Sending from node i
      for (unsigned int j = 1; j <= conf->getNumReducer(); j++) { 
        if (j == i) { // 通过循环遍历每个接收者节点 j，如果接收者节点和发送者节点编号相同，则跳过本次循环
          continue;
        }
        TxData& txData = partitionTxData[j - 1]; // 否则，从已经准备好的 partitionTxData[j - 1] 变量中获取需要发送的中间结果数据；
         /*
          在发送中间结果数据时，我们需要遍历所有的接收者节点，因此循环变量 j 的范围是从 1 到总节点数。
          由于 partitionTxData 数组的下标从 0 开始，因此在获取需要发送的中间结果数据时，我们需要将 j 的值减去 1，才能对应到 partitionTxData 数组中的正确位置。
        */
        txTime -= clock(); //记录的是当前 CPU 时间，即发送操作之前 CPU 执行程序的总时间
        /*
          具体来说，clock() 是一个计时器函数，会返回当前程序运行的 CPU 时间。
          在第一次 txTime -= clock() 时，发送操作还未完成，计时器计算出来的时间表示的是发送操作所耗费的 CPU 时间，而不是真实的发送时间；
        */
        MPI_Send(&(txData.numLine), 1, MPI_UNSIGNED_LONG_LONG, j, 0, MPI_COMM_WORLD); // 通过 MPI_Send 函数将中间结果数据的数量(行数)发送给接收者节点 j 
        /*
        &(txData.numLine)：待发送数据地址，使用取地址符 & 取得 txData.numLine 变量的内存地址，即 unsigned long long 类型值的首地址。
        1：待发送数据的数量，因为这里是发送单个 unsigned long long 类型的变量，所以数量为 1。
        MPI_UNSIGNED_LONG_LONG：待发送数据的类型，即 unsigned long long 类型，MPI 中对应的数据类型是 MPI_UNSIGNED_LONG_LONG。
        j：目标进程的标识符，即目标进程在通信域中的进程号。
        0：消息标签，用于区分不同类型的消息。发送方和接收方需要使用相同的 tag 来匹配消息。
        MPI_COMM_WORLD：通信域，它用于标识消息发送和接收的进程集合。
        */
        MPI_Send(txData.data, txData.numLine * lineSize, MPI_UNSIGNED_CHAR, j, 0, MPI_COMM_WORLD); // 通过 MPI_Send 函数将中间结果数据发送给接收者节点 j
        /*
          txData.data：待发送数据所在的内存地址，即数据块的起始地址，它是一个指向 char 类型的指针。
          txData.numLine * lineSize：待发送数据的数量，因为这里是发送一段数据块，所以数量为 txData.numLine * lineSize，其中 txData.numLine 表示数据块中所包含的行数，lineSize 表示每行数据的字节数。
          MPI_UNSIGNED_CHAR：待发送数据的类型，即无符号字符类型，MPI 中对应的数据类型是 MPI_UNSIGNED_CHAR。
          j：目标进程的标识符，即目标进程在通信域中的进程号。
          0：消息标签，用于区分不同类型的消息。发送方和接收方需要使用相同的 tag 来匹配消息。
          MPI_COMM_WORLD：通信域，它用于标识消息发送和接收的进程集合。
        */
        txTime += clock(); // 计算执行发送操作所花费的时间，并将该时间记录在 txTime 变量中。
        /*
          由于 MPI_Send 函数是一个阻塞函数，即发送方必须等待接收方收到消息后才能继续执行下一步操作
          因此，当 MPI_Send 函数返回时，发送方就可以认为消息已经发送成功了，这时就可以计算执行发送操作所花费的时间了。

          txTime 变量的运行流程和原理如下：
          首先，txTime 初始值为 0。对于每个接收者节点 j，代码会在执行 MPI_Send 函数之前调用 clock() 函数获取当前的 CPU 时间 time1，然后执行 txTime -= time1(txTime = txTime - time1)，以获取执行 MPI_Send 函数之前 CPU 执行程序的总时间 t1。
          接下来，MPI_Send 函数将 txData.numLine 或 txData.data 发送给接收者节点 j，这个过程所花费的时间为 t2。
          最后，代码会在 MPI_Send 函数返回之后再次调用 clock() 函数获取当前的 CPU 时间 time2，然后执行 txTime += time2，以获取 MPI_Send 函数执行完毕后 CPU 执行程序的总时间 t3。
          因此，txTime 变量存储的值是 t1-t2+t3。其中，t2 表示 MPI_Send 函数执行的真正发送时间，而 t1 和 t3 则表示 MPI_Send 函数执行前后程序所消耗的 CPU 时间。
          通过这种方式计算发送时间，可以准确地评估程序的性能。
        */
        tolSize += txData.numLine * lineSize + sizeof(unsigned long long); // 计算发送的数据总量
        delete[] txData.data; // 释放 txData.data 指向的内存空间
      }
      MPI_Barrier(MPI_COMM_WORLD); // 等待所有进程完成发送操作
      time = clock() - time; // 计算从该代码块开始到该语句执行时的时间差，即发送数据所消耗的时间，以便后续计算发送速率。
      rTime = double(time) / CLOCKS_PER_SEC; // 计算执行通信操作的时间 CLOCKS_PER_SEC 表示每秒钟 CPU 执行指令的次数
      double txRate = (tolSize * 8 * 1e-6) / (double(txTime) / CLOCKS_PER_SEC); // 计算发送速率，单位为 Mbps
      MPI_Send(&rTime, 1, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD); // 通过 MPI_Send 函数将执行通信操作的总时间发送给Master节点
      MPI_Send(&txRate, 1, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD); // 通过 MPI_Send 函数发送速率发送给Master节点 将这些计算结果发送给进程 0，进程 0 可以通过 MPI_Recv 函数接收这些计算结果，进行进一步统计和分析
    } else { // 对于接收者节点（即 i != rank 的情况），会接收发送者节点发送过来的中间结果消息，并保存在本地的内存中。
      MPI_Barrier(MPI_COMM_WORLD); // 等待发送者节点发送消息
      // Receiving from node i
      TxData& rxData = partitionRxData[i - 1]; // 获取接收者节点 i 的中间结果数据结构
      MPI_Recv(&(rxData.numLine), 1, MPI_UNSIGNED_LONG_LONG, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // 通过 MPI_Recv 函数接收发送者节点 i 发送过来的数据块的行数
      rxData.data = new unsigned char[rxData.numLine * lineSize]; // 为接收者节点 i 分配内存空间，用于存储发送者节点 i 发送过来的数据块
      MPI_Recv(rxData.data, rxData.numLine * lineSize, MPI_UNSIGNED_CHAR, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // 通过 MPI_Recv 函数接收发送者节点 i 发送过来的数据块
      MPI_Barrier(MPI_COMM_WORLD); // 等待所有接收者节点接收完毕
    }
  }

  // EXECUTE REDUCE PHASE
  execReduce();

  // OUTPUT RESULTS 当进程 rank 不等于 0 时，输出本地列表
  if (rank != 0) {
    outputLocalList();
  }
}


void Worker::execMap()
{
  clock_t time = 0; 
  double rTime = 0; 
  time -= clock(); 

  // READ INPUT FILE AND PARTITION DATA 
  char filePath[MAX_FILE_PATH]; // 用于存储输入文件的路径
  sprintf(filePath, "%s_%d", conf->getInputPath(), rank - 1); // 生成输入文件的路径
  ifstream inputFile(filePath, ios::in | ios::binary | ios::ate); // 打开输入文件
  if (!inputFile.is_open()) { 
    cout << rank << ": Cannot open input file " << conf->getInputPath() << endl;// 打开失败，输出错误信息
    assert(false);
  }

  int fileSize = inputFile.tellg(); // 获取输入文件的大小  
  /*
    函数 tellg() 返回文件指针相对于文件首的偏移量，即当前读取位置。
    由于在前面已经打开了文件，所以读取位置为文件尾，因此可以根据返回的值得到文件大小。
  */
  unsigned long int lineSize = conf->getLineSize();// 获取每行数据的大小
  unsigned long int numLine = fileSize / lineSize; // 计算输入文件的总行数
  inputFile.seekg(0, ios::beg); // 将文件指针重新移动到文件开头
  /*
    在计算文件大小之后文件指针已经移动到文件末尾，如果不将文件指针重新定位到文件开头，程序读取文件数据时就无法正确地从文件开头开始读取。
  */

  // Build trie 
  unsigned char prefix[conf->getKeySize()]; 
  trie = buildTrie(&partitionList, 0, partitionList.size(), prefix, 0, 2); 
  /*
    &partitionList：指向存储了输入数据的 vector 的指针。
    0：当前处理的 LineList 的起始位置为 0。
    partitionList.size()：当前处理的 LineList 的结束位置为 partitionList 的大小（即最后一个 LineList 的下标加一）。
    prefix：存储了已经处理过的键的前缀。
    0：根节点的深度为 0。
    2：Trie 树的最大深度为 2。
    在这里，将 Trie 树的最大深度设置为 2，即只考虑两个字符的前缀，可以将输入数据快速分成几个组，减少了计算分组的时间，同时也保证了最终效果的正确性。
  */


  // Create lists of lines 
  /*
    创建一个 partitionCollection 字典，用于存储每个分区所对应的行数据列表。
    循环 conf->getNumReducer() 次（也就是 Reduce 任务数），依次将分区编号作为键，新建一个行数据列表的指针作为值，插入到字典中。
    在 MapReduce 中，Reduce 阶段的输入是由多个分区（Partition）构成的。
    每个 Map 任务都将其处理后的结果输出到对应的分区中，而 Reduce 任务则需要按照分区来处理输入数据，不同的 Reduce 任务处理不同的分区数据。
    对于每个 Reduce 任务，在输入数据被划分为多个分区之后，该任务只需要处理对应的分区数据即可，不需要关心其它分区的数据。
    创建 partitionCollection 字典并将每个分区的数据存储在其中，可以让 Reduce 任务更加高效地获取到自己需要的数据
  */
  for (unsigned int i = 0; i < conf->getNumReducer(); i++) { 
    partitionCollection.insert(pair<unsigned int, LineList*>(i, new LineList));
  }

  // MAP  Map 阶段的主要实现代码，其目的是将输入文件按照键值分配到不同的分区中
  // Put each line to associated collection according to partition list
  for (unsigned long i = 0; i < numLine; i++) { 
    unsigned char* buff = new unsigned char[lineSize]; //在内存中开辟一个长度为 lineSize 的缓冲区，用于存储读取的一行数据
    inputFile.read((char*)buff, lineSize); // 读取一行数据
    unsigned int wid = trie->findPartition(buff); // 根据读取的数据计算分区编号
    partitionCollection.at(wid)->push_back(buff); // 将读取的数据插入到对应的分区中
  }
  inputFile.close(); // 关闭输入文件
  time += clock(); // 计算 Map 阶段的运行时间
  rTime = double(time) / CLOCKS_PER_SEC; // 将运行时间转换为秒
  MPI_Gather(&rTime, 1, MPI_DOUBLE, NULL, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);// 将运行时间发送给主进程

  time = -clock(); // 准备开始下一阶段的数据处理，并记录当前时间
  // Packet partitioned data to a chunk 将 Map 阶段生成的分区数据打包发送到不同的 Reduce 任务上进行处理
  for (unsigned int i = 0; i < conf->getNumReducer(); i++) { 
    if (i == rank - 1) { // Reduce 任务编号等于当前进程编号，则跳过该 Reduce 任务的处理,因为每个 Reduce 任务只需要处理自己的分区数据
      continue;
    }
    unsigned long long numLine = partitionCollection[i]->size(); // 获取当前分区的行数
    partitionTxData[i].data = new unsigned char[numLine * lineSize]; // 为第 i 个分区的数据打包对象 partitionTxData 分配内存
    partitionTxData[i].numLine = numLine;// 设置第 i 个分区的数据打包对象 partitionTxData 的行数
    auto lit = partitionCollection[i]->begin(); 
    for (unsigned long long j = 0; j < numLine * lineSize; j += lineSize) { //将该分区中的每行数据依次拷贝到 partitionTxData[i].data 中，并释放对应的动态内存
      memcpy(partitionTxData[i].data + j, *lit, lineSize);
      delete[] *lit;
      lit++;
    }
    delete partitionCollection[i];
  }
  time += clock();// 计算数据打包的时间
  rTime = double(time) / CLOCKS_PER_SEC; // 将运行时间转换为秒
  MPI_Gather(&rTime, 1, MPI_DOUBLE, NULL, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);// 将运行时间发送给主进程
}


/*
  在 Map 结束后，为了减少 Reduce 时间，我们会对本地数据进行排序。排序后，所有相同键值的数据将被排列在一起，
  Reduce 程序只需要遍历一次排序后的列表，即可快速处理所有相同键值的数据集合。这样能够显著提高 MapReduce 的性能。
*/
void Worker::execReduce()
{
  // if( rank == 1) {
  //   cout << rank << ":Sort " << localList.size() << " lines\n";
  // }
  // stable_sort( localList.begin(), localList.end(), Sorter( conf->getKeySize() ) );
  sort( localList.begin(), localList.end(), Sorter( conf->getKeySize() ) ); // 对本地数据进行排序
}

/*
  将本地的待处理数据（即经过 Map 阶段后由 Map 函数输出的键值对）打印到控制台上
*/
void Worker::printLocalList()
{
  unsigned long int i = 0;
  for ( auto it = localList.begin(); it != localList.end(); ++it ) { // 遍历本地数据
    cout << rank << ": " << i++ << "| "; // 打印当前行的序号
    printKey( *it, conf->getKeySize() ); // 打印当前行的键值
    cout << endl;
  }
}
/*
  这段代码中的 printKey(*it, conf->getKeySize()) 是一个函数调用，它将会输出列表中每个元素的键（key）值。这里我举一个例子来说明一下。
  假设我们有下面这个列表：
  localList = ["hello:world", "apple:pie", "hello:sun", "banana:cake"]
  其中，每个元素都是一个字符串，格式为 key:value。我们可以将其作为 Map 函数的输出，用于后续的 Reduce 操作。现在我们想要了解一下经过 Map 阶段后，各个 reducer 所需要处理的数据集合中具体包含哪些键（key）。
  如果使用 printLocalList() 函数，就可以得到如下输出：
  1: 0| hello:world
  1: 1| apple:pie
  1: 2| hello:sun
  1: 3| banana:cake
  这个输出表示在编号为 1 的 reducer 所需要处理的数据集合中，第 0 个元素的键值为 "hello"，第 1 个元素的键值为 "apple"，以此类推。
  这样，通过查看控制台输出，我们就可以快速了解每个 reducer 需要处理的数据集合的内容，有助于后续的 Reduce 操作实现。
*/

/*
  将所有 partition 的内容打印到控制台上。
  具体而言，它会遍历 partitionCollection，该变量是一个字典，其中键（key）为 partition 的编号，值（value）为包含该 partition 中所有元素的列表。
  对于每个 partition，函数都会将其中的所有元素遍历一遍，并输出每个元素的信息，包括该元素所属的 partition 编号、该元素在该 partition 中的编号以及该元素的键（key）。
*/
void Worker::printPartitionCollection()
{
  for ( auto it = partitionCollection.begin(); it != partitionCollection.end(); ++it ) { // 遍历 partitionCollection
    unsigned int c = it->first; // 获取当前 partition 的编号 first(键值对中的键)
    LineList* list = it->second; // 获取当前 partition 中的所有元素 second(键值对中的值)
    unsigned long i = 0; // 用于记录当前元素在当前 partition 中的编号
    for ( auto lit = list->begin(); lit != list->end(); ++lit ) { // 遍历当前 partition 中的所有元素
      cout << rank << ": " << c << "| " << i++ << "| "; // 打印当前元素的 partition 编号和在该 partition 中的编号
      printKey( *lit, conf->getKeySize() ); // 打印当前元素的键（key）
      cout << endl;
    }
  }
}

/*
  例如，如果我们有以下 partitionCollection：
  partitionCollection = {
    0: ["apple", "cat", "dog"],
    1: ["banana", "elephant", "frog"],
    2: ["car", "dog", "elephant"]
  }
  其中，字典中的键为 partition 的编号，值为该 partition 中的元素列表。如果我们使用 printPartitionCollection() 函数，则可以得到如下输出：

  1: 0| 0| banana
  1: 0| 1| elephant
  1: 0| 2| frog
  1: 1| 0| car
  1: 1| 1| dog
  1: 1| 2| elephant
  1: 2| 0| apple
  1: 2| 1| cat
  1: 2| 2| dog
  这个输出表示第一个 reducer 需要处理的 partition 编号为 0，其中包含三个元素 "banana"、"elephant" 和 "frog"。其中，第一个元素 "banana" 在该 partition 中的编号为 0，键为 "banana"。
*/

void Worker::outputLocalList() // 将 Worker 类中 localList 所有元素保存到本地磁盘上的一个文件
{
  char buff[ MAX_FILE_PATH ]; // 定义一个字符数组 buff，用于存放生成的输出文件路径名
  sprintf( buff, "%s_%u", conf->getOutputPath(), rank - 1 ); //使用 sprintf 函数格式化生成输出文件路径名
  /*
    例如，如果输出路径前缀为 "/path/to/output"，当前进程编号为 3，那么这个代码段
    sprintf( buff, "%s_%u", "/path/to/output", 3 - 1 );
    将会把输出文件名格式化为 "/path/to/output_2"。
  */
  ofstream outputFile( buff, ios::out | ios::binary | ios::trunc ); // 打开输出文件 buff 并以二进制格式写入
  for ( auto it = localList.begin(); it != localList.end(); ++it ) { // 遍历 localList
    outputFile.write( ( char* ) *it, conf->getLineSize() ); // 将 localList 中的每个元素写入到输出文件中  
  }
  outputFile.close(); // 关闭输出文件
  //cout << rank << ": outputFile " << buff << " is saved.\n";
}

// 字典树
TrieNode* Worker::buildTrie( PartitionList* partitionList, int lower, int upper, unsigned char* prefix, int prefixSize, int maxDepth )
{
  if ( prefixSize >= maxDepth || lower == upper ) { // 如果当前节点深度 prefixSize 达到了最大深度 maxDepth，或者在当前分区内没有更多的键需要加入到该节点，则创建一个 LeafTrieNode 节点并返回该节点指针
    return new LeafTrieNode( prefixSize, partitionList, lower, upper );
  }
  InnerTrieNode* result = new InnerTrieNode( prefixSize ); //否则，创建一个 InnerTrieNode 节点作为当前节点
  
  int curr = lower; 
  for ( unsigned char ch = 0; ch < 255; ch++ ) {
    prefix[ prefixSize ] = ch;
    lower = curr;
    while( curr < upper ) {
      if( cmpKey( prefix, partitionList->at( curr ), prefixSize + 1 ) ) {
	break;
      }
      curr++;
    }
    result->setChild( ch, buildTrie( partitionList, lower, curr, prefix, prefixSize + 1, maxDepth ) );
  }
  /*
    对于从当前节点开始的每一条边，将会枚举所有可能的字符，对于每一个字符都进行以下操作：
      将字符加入到前缀中。
      统计在当前分区内以该前缀为前缀的键值对，即 curr 到 upper 之间满足前缀相同的键值对个数，并将起始位置赋值给 lower。
      递归构建子树，即递归调用 buildTrie 函数，其中，下一个节点的范围是从 lower 到 curr。
  */
  prefix[ prefixSize ] = 255;//对于当前节点最后一条边（即字符为 255 的边），将字符加入到前缀中并递归构建子树，其中，下一个节点的范围是从 curr 到 upper
  result->setChild( 255, buildTrie( partitionList, curr, upper, prefix, prefixSize + 1, maxDepth ) );
  return result; // 返回当前节点指针
}
