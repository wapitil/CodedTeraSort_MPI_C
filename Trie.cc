
#include <assert.h>
#include "Trie.h"

int InnerTrieNode::findPartition( unsigned char* key )
{
  int level = getLevel();
  assert( level < 10 ); //:断言当前节点的深度小于 10，如果条件为假，则会触发异常终止程序。
  return child[ key[ level & 0xFF ] ]->findPartition( key ); // 递归调用子节点的 findPartition 函数
}

// 在 Trie 树的叶子节点中查找与给定键值 key 匹配的最深的叶子节点所属的分区编号
int LeafTrieNode::findPartition( unsigned char* key ) 
{
  for( int i = lower; i < upper; i++ ) {
    if ( cmpKey( key, partitionList->at( i ), 10 ) ) { // 取当前分区编号 i 对应的分区数据，并将其与给定键值 key 进行比较
      return i;// 如果 cmpKey 函数返回 true，即给定键值 key 等于当前分区数据，则直接返回当前分区编号 i，表示查找成功
    }
  }
  return upper;// 如果 for 循环执行完毕后仍未找到与给定键值 key 匹配的分区编号，则说明给定键值 key 在当前 LeafTrieNode 节点存储的分区数据范围之外，需要继续向上查找。
}
