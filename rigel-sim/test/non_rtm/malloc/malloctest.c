#include <rigel.h>

// this test checks for large memory allocations with malloc

#define SIZE 4096

int main () {

  if (RigelGetThreadNum () == 0) {

      SIM_SLEEP_OFF(); 

      float *h_A;
      float *h_B;
      float *h_C;

      unsigned int size_A = ((SIZE * SIZE) + (32 * 4));
      unsigned int mem_size_A = (sizeof (float) * size_A);

      unsigned int size_B = (SIZE * (SIZE + (32 * 4)));
      unsigned int mem_size_B = (sizeof (float) * size_B);

      unsigned int size_C = (SIZE * SIZE);
      unsigned int mem_size_C = (sizeof (float) * size_C);

      h_A = (float *) malloc (mem_size_A);
      h_B = (float *) malloc (mem_size_B);
      h_C = (float *) malloc (mem_size_C);

      RigelPrint(h_B - h_A);
      RigelPrint(h_C - h_B);

      free(h_A);
      free(h_B);
      free(h_C);

  }

  return 0;

}
