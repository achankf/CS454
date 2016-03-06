#include "server_functions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int f0_Skel(int *argTypes, void **args) {

  *(int *)args[0] = f0(*(int *)args[1], *(int *)args[2]);
  return 0;
}

int f1_Skel(int *argTypes, void **args) {

  *((long *)*args) = f1( *((char *)(*(args + 1))), 
		        *((short *)(*(args + 2))),
		        *((int *)(*(args + 3))),
		        *((long *)(*(args + 4))) );

  return 0;
}

int f2_Skel(int *argTypes, void **args) {

  /* (char *)*args = f2( *((float *)(*(args + 1))), *((double *)(*(args + 2))) ); */
  char *temp = f2( *((float *)(*(args + 1))), *((double *)(*(args + 2))) );
	memcpy(*args, temp, 100);
	free(temp);

  return 0;
}

int f3_Skel(int *argTypes, void **args) {

  f3((long *)(*args));
  return 0;
}

/* 
 * this skeleton doesn't do anything except returns
 * a negative value to mimic an error during the 
 * server function execution, i.e. file not exist
 */
int f4_Skel(int *argTypes, void **args) {
	puts("f4_skel called");
  return -1; /* can not print the file */
}

int f4_Skel_overload1(int *argTypes, void **args) {
	puts("f4_skel overload 1 called");
  return 0; // just want to make sure this method ran
}
