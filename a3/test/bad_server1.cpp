#include "common.hpp"
#include "rpc.h"
#include <cassert>

#ifndef NDEBUG

int skel(int *a, void **b) {
	return -1; // not used
}

int main()
{
	int arg_types[1] = {0};

	// before init

	assert(rpcRegister(NULL, NULL, NULL) == NOT_A_SERVER);
	assert(rpcExecute() == NOT_A_SERVER);

	assert(rpcInit() == OK);
	assert(rpcInit() == HAS_ALREADY_INIT_SERVER);

	// after init

	assert(rpcCall(NULL, NULL, NULL) == NOT_A_CLIENT);
	assert(rpcCacheCall(NULL, NULL, NULL) == NOT_A_CLIENT);

	assert(rpcRegister(NULL, NULL, NULL) == FUNCTION_NAME_IS_INVALID);
	assert(rpcRegister("", NULL, NULL) == FUNCTION_NAME_IS_INVALID);
	// name is longer than 63 characters (not including the numm terminator)
	assert(rpcRegister("1111111122222222333333334444444455555555666666667777777788888888", NULL, NULL) == FUNCTION_NAME_IS_INVALID);
	// strlen(name) == 63 is okay
	assert(rpcRegister("111111122222222333333334444444455555555666666667777777788888888", NULL, NULL) == FUNCTION_ARGTYPES_INVALID);
	assert(rpcRegister("bad1", NULL, NULL) == FUNCTION_ARGTYPES_INVALID);
	assert(rpcRegister("bad2", arg_types, NULL) == SKELETON_IS_NULL);

	assert(rpcExecute() == EXECUTE_WITHOUT_REGISTER);


	// after execution

	assert(rpcExecute() == HAS_RUN_EXECUTE);
	assert(rpcRegister("bad2", arg_types, skel) == HAS_RUN_EXECUTE);
	assert(rpcCall(NULL, NULL, NULL) == NOT_A_CLIENT);
	assert(rpcCacheCall(NULL, NULL, NULL) == NOT_A_CLIENT);

	// server cannot call terminate
	assert(rpcTerminate() == NOT_A_CLIENT);
}
#endif
