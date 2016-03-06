/*
 * client.c
 * 
 * This file is the client program, 
 * which prepares the arguments, calls "rpcCacheCall", and checks the returns.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rpc.h"

#define CHAR_ARRAY_LENGTH 100

int main() {

  /* prepare the arguments for f0 */
  int argTypes0[1] = {0};
  void **args0;
    
  /* rpcCacheCalls */
  printf("retval:%d\n", rpcCall("fvoid", argTypes0, NULL));
  printf("retval:%d\n", rpcCall("fvoid", argTypes0, NULL));
  printf("retval:%d\n", rpcCall("fvoid", argTypes0, NULL));
  printf("retval:%d\n", rpcCall("fvoid", argTypes0, NULL));
  printf("retval:%d\n", rpcCall("fvoid", argTypes0, NULL));
  printf("retval:%d\n", rpcCacheCall("fvoid", argTypes0, NULL));
  printf("retval:%d\n", rpcCacheCall("fvoid", argTypes0, NULL));
  printf("retval:%d\n", rpcCacheCall("fvoid", argTypes0, NULL));
  printf("retval:%d\n", rpcCacheCall("fvoid", argTypes0, NULL));
  printf("retval:%d\n", rpcCacheCall("fvoid", argTypes0, NULL));
  printf("retval:%d\n", rpcCacheCall("fvoid", argTypes0, NULL));
  printf("retval:%d\n", rpcCacheCall("fvoid", argTypes0, NULL));

  /* rpcTerminate */
  printf("\ndo you want to terminate? y/n: ");
  if (getchar() == 'y')
    rpcTerminate();

  /* end of client.c */
  return 0;
}




