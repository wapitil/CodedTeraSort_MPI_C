#include <iostream>
#include <iomanip>

#include "Utility.h"
#include "Common.h"

// 该函数将给定键值 key 按照指定的大小 size 输出到标准输出流中，其中使用 setw 和 setfill 函数设定了输出格式，hex 函数指定了输出为十六进制数。
void printKey( const unsigned char* key, unsigned int size )
{
  for ( unsigned long int i = 0; i < size; i++ ) {
    cout << setw( 2 ) << setfill( '0' ) << hex << (int) key[ i ] << " ";
  }
  cout << dec;
}

// 该函数比较两个键值的大小，如果 keyA 小于 keyB，则返回 true，否则返回 false。
bool cmpKey( const unsigned char* keyA, const unsigned char* keyB, unsigned int size )
{
  for ( unsigned long i = 0; i < size; i++ ) {
    if ( keyA[ i ] < keyB[ i ] ) {
      return true;
    }
    else if ( keyA[ i ] > keyB[ i ] ) {
      return false;
    }
  }
  return false;
}
