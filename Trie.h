#ifndef _MR_TRIE
#define _MR_TRIE

#include "Common.h"
#include "Utility.h"

class TrieNode {
 private:
  int level;// 前缀树的层数
  
 public:
  TrieNode( int _level ): level( _level ) {} //接收一个表示节点深度的整型参数 _level，并将其赋值给节点的 level 成员变量。
  virtual ~TrieNode() {} // 虚析构函数
  virtual int findPartition( unsigned char* key ) = 0; // 纯虚函数 表示查找以当前节点为根节点的子树中匹配给定键值 key 的最深的叶子节点所属的分区编号。
  int getLevel() { return level; } //返回当前 TrieNode 的深度
};


class InnerTrieNode: public TrieNode {
 public:
  TrieNode** child;// 指向子节点的指针数组

 public:
 /*
  接收一个表示节点深度的整型参数 _level，并将其传递给基类 TrieNode 的构造函数。
  同时，在构造函数中创建一个长度为 256 的 TrieNode 指针数组，并将其赋值给子节点指针数组 child
 */
 InnerTrieNode( int _level ): TrieNode( _level ) { 
    child = new TrieNode*[ 256 ];
  }
  ~InnerTrieNode() {
    for( int i = 0; i < 256; i++ ) {
      delete child[ i ];
    }
    delete [] child;
  }

  int findPartition( unsigned char* key ); // 重写基类 TrieNode 的 findPartition 函数
  void setChild( int index, TrieNode* _child ) { child[ index ] = _child; } //设置子节点指针数组 child 中索引为 index 的元素指向 _child
};

class LeafTrieNode: public TrieNode {
 private:
  int lower; // 当前 LeafTrieNode 节点存储的分区数据范围的下界
  int upper; // 当前 LeafTrieNode 节点存储的分区数据范围的上界
  PartitionList* partitionList; //指向当前 LeafTrieNode 节点存储的分区数据的 PartitionList 对象的指针
  
 public:
 /*
  LeafTrieNode 的构造函数，接收表示节点深度、节点存储的分区数据、分区数据范围下界和上界的四个参数，
  并将它们分别赋值给其父类 TrieNode 的构造函数和自身的成员变量 lower、upper 和 partitionList。
 */
 LeafTrieNode( int _level, PartitionList* _partitionList, int _lower, int _upper ): TrieNode( _level ), lower( _lower ), upper( _upper ), partitionList( _partitionList ) {}
  ~LeafTrieNode() {}
  int findPartition( unsigned char* key ); //在 LeafTrieNode 中覆盖 TrieNode 的纯虚函数 findPartition，实现查找以当前 LeafTrieNode 节点为根节点的子树中匹配给定键值 key 的最深的叶子节点所属的分区编号。
};


class Sorter {
 private:
  unsigned int length; // 排序的键值的长度
 public:
 Sorter( unsigned int _length ): length( _length ) {} // Sorter 的构造函数，接收表示比较键长度的参数 _length，并将其赋值给 Sorter 对象的 length 成员变量
  /*
    实现了 Sorter 类型的函数调用运算符，用于对两个长度相同的 unsigned char 类型的字符串进行字典序比较，返回结果。
    该函数使用了 cmpKey 函数，即 Utility.h 中定义的字符串比较函数
  */
  bool operator()( const unsigned char* keyl, const unsigned char* keyr ) { return cmpKey( keyl, keyr, length ); }
};


#endif
