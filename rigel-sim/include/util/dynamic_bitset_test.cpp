#include "util/dynamic_bitset.h"
#include <cstdio>
#include <cstdlib>
#include <set>

/// for testing the dynamic_bitset class

int main(int argc, char *argv[])
{
  DynamicBitset a(123);
  for(int i = 0; i < 32; i++)
    if(i % 2 == 0)
      a.set(i);

  a.print(stdout);
  printf("\n");
  a.toggleAll();
  a.print(stdout);
  printf("\n");
  a.clearAll();
  a.print(stdout);
  printf("\n");
  a.setAll();
  a.print(stdout);
  printf("\n");

  a.clearAll();
  
  a.set(0);
  a.set(5);
  a.set(16);
  a.set(30);
  a.print(stdout);
  printf(" (%zu bits set)\n", a.getNumSetBits());
  a.toggle(0);
  a.toggle(1);
  a.print(stdout);
  printf(" (%zu bits set)\n", a.getNumSetBits());
  
  printf("All clear: %01d, All set: %01d\n", a.allClear(), a.allSet());
  a.clearAll();
  printf("All clear: %01d, All set: %01d\n", a.allClear(), a.allSet());
  a.setAll();
  printf("All clear: %01d, All set: %01d\n", a.allClear(), a.allSet());
  a.clearAll();
  for(int i = 0; i < 32; i++)
    a.set(i);
  printf("All clear: %01d, All set: %01d\n", a.allClear(), a.allSet());
  
  a.clearAll();
  a.set(0);
  a.set(3);
  a.set(12);
  a.set(25);
  a.set(27);
  a.set(47);
  a.set(72);
  
  std::set<size_t> setBits;
  a.findAllSet(setBits);
  
  printf("Set bits: ");
  for(std::set<size_t>::const_iterator it = setBits.begin(), end = setBits.end(); it != end; ++it)
  {
    printf("%0zu, ", *it);
  }
  printf("\n");
  
  printf("Set bits: ");
  size_t curPos = a.findFirstSet();
  while(curPos != -1)
  {
    printf("%0zu", curPos);
    curPos = a.findNextSet(curPos);
    if(curPos != -1)
      printf(", ");
  }
  printf("\n");

  //return 0;
  
  std::set<size_t> clearBits;
  a.findAllClear(clearBits);
  printf("Clear bits: ");
  for(std::set<size_t>::const_iterator it = clearBits.begin(), end = clearBits.end(); it != end; ++it)
  {
    printf("%0zu, ", *it);
  }
  printf("\n");
  
  printf("Clear bits: ");
  curPos = a.findFirstClear();
  while(curPos != -1)
  {
    printf("%0zu", curPos);
    curPos = a.findNextClear(curPos);
    if(curPos != -1)
      printf(", ");
  }
  printf("\n");
  
  
  return 0;
}
