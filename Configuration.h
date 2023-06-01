#ifndef _MR_CONFIGURATION
#define _MR_CONFIGURATION

class Configuration {

 protected:
  unsigned int numReducer;
  unsigned int numInput;  
  
  const char *inputPath;
  const char *outputPath;
  const char *partitionPath;
  unsigned long numSamples;
  
 public:
  Configuration() {
    numReducer = 3; // 执行计算的 reducer 节点数量
    numInput = numReducer;    // 输入节点数量
    
    inputPath = "./Input/Input10000";
    outputPath = "./Output/Output10000";
    partitionPath = "./Partition/Partition10000";
    numSamples = 10000; // 指定在构建分区列表时的样本数量
  }
  ~Configuration() {}
  const static unsigned int KEY_SIZE = 10; // 键的大小
  const static unsigned int VALUE_SIZE = 90;  // 值的大小
  /*
    在 MapReduce 计算框架中，输入数据是由键值对组成的。其中，键用于标识一个数据记录的关键信息，值则表示这个数据记录的具体内容。
    KEY_SIZE 和 VALUE_SIZE 常量分别表示键和值的大小，它们规定了每个键值对中键和值所占用的字节数。
  */
  
  unsigned int getNumReducer() const { return numReducer; }  // 获取 reducer 节点数量
  unsigned int getNumInput() const { return numInput; }  // 获取输入节点数量
  const char *getInputPath() const { return inputPath; }  // 获取输入路径
  const char *getOutputPath() const { return outputPath; } // 获取输出路径
  const char *getPartitionPath() const { return partitionPath; }  // 获取分区路径
  unsigned int getKeySize() const { return KEY_SIZE; } // 获取键的大小
  unsigned int getValueSize() const { return VALUE_SIZE; } // 获取值的大小
  unsigned int getLineSize() const { return KEY_SIZE + VALUE_SIZE; } // 获取键值对的大小
  unsigned long getNumSamples() const { return numSamples; }  // 获取样本数量
};

#endif
