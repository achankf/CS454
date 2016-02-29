#include "common.hpp"
#include "name_service.hpp"
#include "postman.hpp"
#include "rpc.h"
#include "sockets.hpp"
#include <cassert>

#ifndef NDEBUG
#include <iostream>
#endif

TCP::Sockets sockets;
NameService ns;
Postman postman(sockets, ns);

// server
int server_fd = -1;
int server_id = -1;
std::string server_hostname;
Name server_name;

// binder
int binder_fd = -1;
const int binder_id = 0;
std::string binder_hostname;
Name binder_name;

// each client/server needs to greet the binder with a hello request at the beginning
bool hello_sent = false;

static int wait_for_hello_reply()
{
	// busy-wait until "good to see you" reply is back
	Postman::Request ret;

	while(postman.receive_any(ret) < 0);

	assert(ret.message.ns_version > 0);
	if (ret.fd == binder_fd && ret.message.msg_type == Postman::GOOD_TO_SEE_YOU) {
#ifndef NDEBUG
	std::cout << "acknowledge greeting" << std::endl;
#endif
		return 0;
	}
#ifndef NDEBUG
	std::cout << "bad greeting" << std::endl;
#endif
	return -1;
}

int rpcInit()
{
	server_fd = sockets.bind_and_listen();

	if(server_fd < 0)
	{
		// should not happen in the student environment
		assert(false);
		return -1;
	}

	get_hostname(server_fd, server_hostname, server_name);
	binder_fd = connect_to_binder(sockets);
	// should not fail in student environment
	assert(binder_fd >= 0);
	get_peer_info(binder_fd, binder_name);

	if(postman.send_hello(binder_fd) < 0)
	{
		// this must be a bug, since the server is connected to the binder
		assert(false);
		return -1;
	}

	if(wait_for_hello_reply() < 0)
	{
		assert(false);
		return -1;
	}

	hello_sent = true;
	return 0;
}

int rpcCall(char* name, int* argTypes, void** args)
{
	Function func = to_function(name, argTypes);
	return postman.send_call(server_fd, func, args);
}

int rpcCacheCall(char* name, int* argTypes, void** args)
{
	return -1;
}

int rpcRegister(char* name, int* argTypes, skeleton f)
{
	// only the server calls this methods, and hello is sent during init()
	assert(hello_sent);
	Function func = to_function(name, argTypes);
	print_function(func);
	//postman.send_register(binder_fd, server_id, server_port, func);
	return 0;
}

int rpcExecute()
{
	// only the server calls this methods, and hello is sent during init()
	assert(hello_sent);

	while(true)
	{
	}

	return -1;
}

int rpcTerminate()
{
	return -1;
}
