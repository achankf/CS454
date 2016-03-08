#include "common.hpp"
#include "rpc.h"
#include <cassert>
#include <stdio.h>

#ifndef NDEBUG

// no server is up
int main() {
	int arg_types[1] = {0};

	int retval;
	retval = rpcCall("", NULL, NULL);
	printf("retval:%d\n", retval);
	assert(retval == FUNCTION_NAME_IS_INVALID);

	retval = rpcCall("a", NULL, NULL);
	printf("retval:%d\n", retval);
	assert(retval == FUNCTION_ARGTYPES_INVALID);

	retval = rpcCall("a", arg_types, NULL);
	printf("retval:%d\n", retval);
	assert(retval == NO_AVAILABLE_SERVER);

	
	retval = rpcCacheCall("a", arg_types, NULL);
	printf("retval:%d\n", retval);
	assert(retval == NO_AVAILABLE_SERVER);

	retval = rpcRegister(NULL, NULL, NULL);
	printf("retval:%d\n", retval);
	assert(retval == NOT_A_SERVER);

	retval = rpcExecute();
	printf("retval:%d\n", retval);
	assert(retval == NOT_A_SERVER);

	retval = rpcInit();
	printf("retval:%d\n", retval);
	assert(retval == NOT_A_SERVER);

	retval = rpcRegister(NULL, NULL, NULL);
	printf("retval:%d\n", retval);
	assert(retval == NOT_A_SERVER);

	retval = rpcExecute();
	printf("retval:%d\n", retval);
	assert(retval == NOT_A_SERVER);
}

#endif
