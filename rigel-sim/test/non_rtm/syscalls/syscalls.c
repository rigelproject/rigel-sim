////////////////////////////////////////////////////////////////////////////////
// syscalls
////////////////////////////////////////////////////////////////////////////////
// simple syscalls sanity check test
////////////////////////////////////////////////////////////////////////////////

#include "rigel.h"
#include "rigel-tasks.h"
#include <spinlock.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#if 0
#define BARRIER_INFO  RigelBarrier_Info
#define BARRIER_INIT  RigelBarrier_Init
#define BARRIER_ENTER RigelBarrier_EnterFull
#endif

#if 1 
#define BARRIER_INFO  RigelBarrierMT_Info
#define BARRIER_INIT  RigelBarrierMT_Init
#define BARRIER_ENTER RigelBarrierMT_EnterFull
#endif


ATOMICFLAG_INIT_CLEAR(Init_flag);
BARRIER_INFO bi;

char *testfile = "testfile.out";

////////////////////////////////////////////////////////////////////////////////
//	START MAIN
////////////////////////////////////////////////////////////////////////////////
int main() {

  if( RigelGetThreadNum() == 0 ) {

    SIM_SLEEP_OFF();

    char teststring[128];

    // printf
    printf("printf() works\n");
    sprintf(teststring,"sprintf() gets a string\n");
    printf(teststring);
    fprintf(stdout,"fprintf() to stdout\n");
    fprintf(stderr,"fprintf() to stderr\n");

    // scanf
    int scanf_int;
    float scanf_float;
    char scanf_string[128];
    scanf("%d", &scanf_int);
    printf("scanf() read int: %d\n", scanf_int);
    scanf("%f", &scanf_float);
    printf("scanf() read float: %f\n", scanf_float);
    scanf("%s", &scanf_string);
    printf("scanf() read string: %s\n", scanf_string);

    // file i/o

    // write to a file
    printf("testing file output\n");
    FILE* outfile;
    if( (outfile = fopen(testfile,"w")) == NULL ) {
      assert(0 && "fopen() failed!");
    }
    fprintf(outfile,"%d\n", scanf_int);
    fprintf(outfile,"%f\n", scanf_float);
    fprintf(outfile,"%s\n", scanf_string);
    if( fclose(outfile) != NULL ) {
      assert(0 && "fclose() failed!");
    }

    // read from a file
    printf("testing file input\n");
    FILE* infile;
    if( (infile = fopen(testfile,"r")) == NULL ) {
      assert(0 && "fopen() failed!");
    }
    int sz = 256;
    char readfile[sz];
    while( fgets(readfile, sz, infile) ) {
      fprintf(stdout, "%s", readfile);
    }

  }

	return 0;
}
