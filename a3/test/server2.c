#include "rpc.h"
#include "server_function_skels.h"
#include <stdlib.h>

int main(int argc, char *argv[]) {
  
  /* create sockets and connect to the binder */
  rpcInit();

  /* prepare server functions' signatures */
  int argTypes0[1] = {0};

  rpcRegister("fvoid", argTypes0, NULL);
  rpcRegister("fvoid", argTypes0, NULL);
  rpcRegister("fvoid", argTypes0, NULL);
  rpcRegister("fvoid", argTypes0, NULL);
  rpcRegister("fvoid", argTypes0, NULL);
  rpcRegister("fvoid", argTypes0, NULL);
  rpcRegister("fvoid", argTypes0, NULL);
  rpcRegister("fvoid", argTypes0, NULL);
  rpcRegister("fvoid", argTypes0, *fvoid);

  /* call rpcExecute */
  rpcExecute();

  /* return */
  return 0;
}







