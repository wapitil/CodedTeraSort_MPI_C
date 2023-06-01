#include <iostream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <assert.h>
#include <string.h>

#include "PartitionSampling.h"
#include "Common.h"

using namespace std;

PartitionSampling::PartitionSampling() // 构造函数
{
  conf = NULL;
}

PartitionSampling::~PartitionSampling()
{
  
}

PartitionList* PartitionSampling::createPartitions() 
{  
  if ( !conf ) { 
    cout << "Configuration has not been provided.\n"; // 配置信息未提供
    assert( false );
  }

  ifstream inputFile( conf->getInputPath(), ios::in | ios::binary | ios::ate ); // 以二进制读写方式打开输入文件
  
  if ( !inputFile.is_open() ) { // 如果文件打开失败
    cout << "Cannot open input file " << conf->getInputPath() << endl; // 无法打开输入文件
    assert( false );
  }

  
  PartitionList keyList; 
  long unsigned int keySize = conf->getKeySize();   // 键的大小
  unsigned long fileSize = inputFile.tellg();     // tellg()表示当前读取位置与输入文件开头之间的字节数 也就是输入文件的大小
  long unsigned int numSamples = min( conf->getNumSamples(), fileSize / conf->getLineSize() ); // 用户设定的采样数和文件的行数相比较，选择小的那个作为采样数 numSamples

  
  // Sample keys 从输入文件中随机抽取指定数量的样本记录，并将这些样本记录存储在一个键列表（keyList）数据结构中。
  inputFile.seekg( 0 ); // 将文件指针移动到文件开头
  long unsigned int gapSkip = ( ( fileSize / conf->getLineSize() ) / numSamples - 1 ) * conf->getLineSize(); 
  /*
    gapSkip 是指两个样本记录之间的跨度，表示一个样本和下一个样本之间在输入文件中的距离。
    fileSize 表示输入文件的大小，conf->getLineSize() 表示每行记录的长度，numSamples 表示要采样的记录数量。
    这个公式实际上是为了保证每个样本之间的间距尽可能相等，从而能够更精确地代表整个数据集。程序使用这个 gapSkip 值来控制在每轮循环读取样本记录时向前移动文件指针的距离。
    因此，每次执行完这条语句后，文件指针会向前移动 conf->getValueSize() + gapSkip 个字节，从而指向下一个样本记录的开头位置，以便程序能够读取和处理下一个样本记录。
    为了让样本记录集合是具有充分代表性
  */
  for ( long unsigned int i = 0; i < numSamples; i++ ) { // 从输入文件中随机抽取指定数量的样本记录
    unsigned char *keyBuff = new unsigned char [ keySize + 1 ];
    if ( keyBuff == NULL ) {
      cout << "Cannot allocate memory to sample keys.\n"; // 防止在从输入文件中抽取样本记录时，因为内存不足而无法为样本记录分配内存空间
      assert( false );
    }
    inputFile.read( (char *) keyBuff, keySize );   // 读取样本记录的键
    keyBuff[ keySize ] = '\0'; // 将样本记录的键的最后一个字符设置为 '\0'
    keyList.push_back( keyBuff ); // 将样本记录的键添加到键列表中
    inputFile.seekg( conf->getValueSize() + gapSkip , ios_base::cur ); // 将文件指针向前移动 conf->getValueSize() + gapSkip 个字节
  }
  inputFile.close(); // 关闭输入文件
  
    
  // Sort sampled keys 按照键的字典序对样本记录的键进行排序
  stable_sort( keyList.begin(), keyList.end(), cmpKey ); // `stable_sort()` 函数是一个稳定的排序函数，它能够保证排序后的相对位置没有发生改变
  
  // Partition keys 一个键值列表 keyList 划分为多个子列表，每个子列表称作一个 partition，每个 partition 中的键值对都会被发送到同一个 reducer 节点上进行处理。 
  PartitionList* partitions = new PartitionList; 
  long unsigned int numPartitions = conf->getNumReducer(); //从配置信息中读取 reducer 的数量（即划分的 partition 数量）
  long unsigned int sizePartition = round( numSamples / numPartitions ); // 每个 partition 中包含的样本记录数量
  for ( unsigned long int i = 1; i < numPartitions; i++ ) { 
    unsigned char *keyBuff = new unsigned char [ keySize + 1 ]; // 为每个 partition 分配内存
    if ( keyBuff == NULL ) { // 如果分配内存失败
      cout << "Cannot allocate memory for partition keys.\n";
      assert( false );
    }
    memcpy( keyBuff, keyList.at( i * sizePartition ), keySize + 1 ); // 将每个 partition 的第一个元素存储在 keyBuff 中
    partitions->push_back( keyBuff ); // 将 keyBuff 添加到 partitions 中
  }

  // The partition list will be broadcast
  // Save partitions to partition list file
  // ofstream partitionFile( conf->getPartitionPath(), ios::out | ios::binary | ios::trunc );
  // for ( auto k = partitions->begin(); k != partitions->end(); ++k ) {
  //   partitionFile.write( (char *) *k, keySize );
  // }
  // partitionFile.close();
  
  // Clean up
  for ( auto k = keyList.begin(); k != keyList.end(); ++k ) { // 释放 keyList 中每个元素所占用的内存
    delete [] (*k);
  }

  return partitions; 
}

// 在 MapReduce 计算过程中对键进行排序(分割)。保证每个 reducer 节点处理的键值对数量大致相等。
// 具体来说，该函数会遍历两个字符串的每一个字符，并进行比较。如果遇到两个不同的字符，则根据它们在字典序中的大小关系返回 true 或者 false，
// 否则继续比较下一个字符，直到其中一个字符串结束。如果两个字符串相等，则认为前一个字符串小于后一个字符串，返回 true。
bool PartitionSampling::cmpKey( const unsigned char* keyl, const unsigned char* keyr )
{
  for ( unsigned long i = 0; keyl[i] != '\0'; i++ ) {
    if ( keyl[ i ] < keyr[ i ] ) {
      return true;
    }
    else if ( keyl[ i ] > keyr[ i ] ) {
      return false;
    }
  }
  return true;
}


void PartitionSampling::printKeys( const PartitionList& keyList ) const // 打印键列表中的所有键
{
  unsigned long int i = 0; // 记录键的序号
  for ( auto k = keyList.begin(); k != keyList.end(); ++k ) { // 遍历键列表中的每个键
    cout << "Key " << dec << i << ": "; // 打印键的序号
    for ( unsigned long int j = 0; j < conf->getKeySize(); j++ ) { // 打印键的每个字符
      cout << setw( 2 ) << setfill( '0' ) << hex << (int) (*k)[j] << " "; // 将键的每个字符转换为十六进制并打印
    }
    cout << dec << endl;// 打印换行符
    i++;// 更新键的序号
  }  
}
