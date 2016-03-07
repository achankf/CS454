#include "common.hpp"
#include "rpc.h"
#include <cassert>

#ifndef NDEBUG

#include <iostream>
// no server is up
int main() {
	int arg_types[1] = {0};

	assert(rpcCall("a", NULL, NULL) == FUNCTION_ARGS_ARE_INVALID);
	assert(rpcCall("a", arg_types, NULL) == NO_AVAILABLE_SERVER);
	assert(rpcCacheCall("a", arg_types, NULL) == NO_AVAILABLE_SERVER);

	assert(rpcRegister(NULL, NULL, NULL) == NOT_A_SERVER);
	assert(rpcExecute() == NOT_A_SERVER);

	// OK: can be promoted to a server
	assert(rpcInit() == OK);
	assert(rpcExecute() == EXECUTE_WITHOUT_REGISTER);
	assert(rpcExecute() == HAS_RUN_EXECUTE);

	// of course, the program can no longer call client codes
	assert(rpcCall("a", arg_types, NULL) == NOT_A_CLIENT);
	assert(rpcCacheCall("a", arg_types, NULL) == NOT_A_CLIENT);
	assert(rpcTerminate() == NOT_A_CLIENT);
}

#endif
